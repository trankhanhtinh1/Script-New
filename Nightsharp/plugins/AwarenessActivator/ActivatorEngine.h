#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "AwarenessEngine.h"
#include "../../SDK/Enums/BuffType.h"
#include "../../sdk/Enumerations/SpellSlot.h"
#include "../../core/CoreNavGrid.h"

#include <Windows.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <string_view>

namespace NightSharp::Companion {

class ActivatorSettings final {
public:
    ActivatorSettings() {
        modes_.fill(ActionMode::Suggest);
        modes_[0] = ActionMode::Off;
    }

    ActionMode ModeFor(Capability capability) const noexcept {
        const std::size_t index = static_cast<std::size_t>(capability);
        return index < modes_.size() ? modes_[index] : ActionMode::Off;
    }
    void SetMode(Capability capability, ActionMode mode) noexcept {
        const std::size_t index = static_cast<std::size_t>(capability);
        if (index < modes_.size()) modes_[index] = mode;
    }
    bool enabled = true;
    bool summonersEnabled = true;
    bool defensiveItemsEnabled = true;
    bool supportItemsEnabled = true;
    bool movementItemsEnabled = true;
    bool offensiveItemsEnabled = true;
    bool visionItemsEnabled = true;
    bool potionEnabled = true;
    bool allowPracticeAutomation = false;
    bool doNotInterruptRecall = true;
    bool reserveSmiteCharge = true;
    bool includeDamageOverTime = true;
    bool barrierLethalOnly = false;
    bool ignoreTurretDamage = false;
    bool drawOverlay = true;
    bool drawWorldLayer = true;
    bool drawMinimapLayer = true;
    bool drawAlertCenter = true;
    bool drawEnemyHud = true;
    bool drawCombatState = true;
    bool drawReachableAreas = true;
    bool drawThreats = true;
    bool drawWards = true;
    bool drawJungle = true;
    bool drawStructures = true;
    bool drawObjectives = true;
    bool drawInsights = true;
    bool drawWave = true;
    bool drawActivityHeatmap = false;
    bool drawVisionHeatmap = false;
    bool audioOnly = false;
    bool streamerMode = false;
    bool vietnamese = false;
    bool hudEditor = false;
    int confirmationVirtualKey = VK_MENU;
    float defensiveHorizon = 1.25f;
    float protectionThreshold = 0.32f;
    float allySaveThreshold = 0.38f;
    float offensiveSafetyMargin = 15.0f;
    float cleanseReactionDelay = 0.04f;
    float minimumShieldEfficiency = 0.35f;
    float healMissingHealthThreshold = 0.28f;
    float exhaustDamageThreshold = 0.20f;
    float ghostMinimumTimeGain = 0.35f;
    float alertPanelX = 18.0f;
    float alertPanelY = 22.0f;
    float enemyHudX = 18.0f;
    float enemyHudY = 390.0f;
    int rolePresetIndex = 0;
    std::array<ActionMode, 64> modes_{};
};


class ActivatorEngine final {
public:
    const ActionRequest* Evaluate(AwarenessEngine& awareness,
                                  const ActivatorSettings& settings) {
        ActionArbiter& arbiter = awareness.Arbiter();
        arbiter.Clear();
        if (!settings.enabled) return nullptr;

        const StateStore& store = awareness.Store();
        const PatchRegistry& registry = awareness.Registry();
        const ChampionState* player = FindLocalPlayer(store);
        if (!player || player->dead || !player->visible || player->health.value <= 0.0f) return nullptr;

        const float now = awareness.Now();
        const ThreatForecast selfForecast = CombatPredictionService::Forecast(
            *player, store, now, settings.defensiveHorizon);
        EvaluateCleanse(*player, store, registry, settings, awareness.Mode(), now, arbiter);
        EvaluateProtection(*player, selfForecast, registry, settings, awareness.Mode(), now, arbiter);
        EvaluateAllies(*player, store, registry, settings, awareness.Mode(), now, arbiter);
        EvaluateEnemies(*player, store, selfForecast, registry, settings, awareness.Mode(), now, arbiter);
        EvaluateObjectives(*player, store, registry, settings, awareness.Mode(), now, arbiter);
        EvaluateUtility(*player, store, selfForecast, registry, settings, awareness.Mode(), now, arbiter);
        return arbiter.Resolve(now);
    }

    static bool IsSummonerSpellSlot(int slot) noexcept {
        return slot ==
                   static_cast<int>(SDK::SpellSlot::Summoner1) ||
               slot ==
                   static_cast<int>(SDK::SpellSlot::Summoner2);
    }

    static Capability CapabilityFromSpell(
        const ObservedSpell& spell,
        const PatchRegistry& registry) noexcept {
        if (!IsSummonerSpellSlot(spell.slot)) {
            return Capability::None;
        }
        const SummonerDefinition* definition =
            registry.ResolveAvailableSummoner(
                spell.idHash, spell.name);
        return definition
            ? definition->capability
            : Capability::None;
    }

    static const ObservedSpell* ResolveSummonerSlot(
        const ChampionState& player, Capability capability,
        const PatchRegistry& registry,
        bool requireReady = true) noexcept {
        for (std::size_t i = 0; i < player.spellCount; ++i) {
            const ObservedSpell& spell = player.spells[i];
            if (CapabilityFromSpell(spell, registry) !=
                capability) {
                continue;
            }
            if (requireReady) {
                if (!spell.ready) continue;
                if (spell.maxCharges > 1) {
                    if (spell.charges <= 0) continue;
                } else if (spell.cooldownRemaining > 0.01f) {
                    continue;
                }
            }
            return &spell;
        }
        return nullptr;
    }

    static const ObservedItem* ResolveInventorySlot(
        const ChampionState& player, Capability capability,
        const PatchRegistry& registry,
        bool requireUsable = true) noexcept {
        for (std::size_t i = 0; i < player.itemCount; ++i) {
            const ObservedItem& item = player.items[i];
            if (item.slot < 0 || item.slot > 6 ||
                !item.active) {
                continue;
            }
            const ItemDefinition* definition =
                registry.FindAvailableItem(item.itemId);
            if (!definition ||
                definition->capability != capability) {
                continue;
            }
            if (requireUsable &&
                (!item.usable ||
                 item.cooldownRemaining > 0.01f ||
                 (item.maxCharges > 0 &&
                  item.charges <= 0))) {
                continue;
            }
            return &item;
        }
        return nullptr;
    }

private:
    struct CrowdControlSummary {
        bool cleanseable = false;
        bool qssable = false;
        bool mikaelable = false;
        bool suppression = false;
        bool airborne = false;
        float longestRemaining = 0.0f;
        float latestStartAt = 0.0f;
        int severity = 0;
    };

    static const ChampionState* FindLocalPlayer(const StateStore& store) {
        const ChampionState* result = nullptr;
        store.ForEachChampion([&](const ChampionState& state) {
            if (state.local) result = &state;
        });
        return result;
    }

    static bool Contains(std::string_view value, std::string_view needle) noexcept {
        if (needle.empty() || value.size() < needle.size()) return false;
        for (std::size_t i = 0; i + needle.size() <= value.size(); ++i) {
            bool equal = true;
            for (std::size_t j = 0; j < needle.size(); ++j) {
                if (std::tolower(static_cast<unsigned char>(value[i + j])) !=
                    std::tolower(static_cast<unsigned char>(needle[j]))) {
                    equal = false;
                    break;
                }
            }
            if (equal) return true;
        }
        return false;
    }

    static bool HasActiveBuff(const ChampionState& state,
                              std::string_view needle,
                              float now) noexcept {
        for (std::size_t i = 0; i < state.buffCount; ++i) {
            const ObservedBuff& buff = state.buffs[i];
            if (buff.endTime > 0.0f && buff.endTime <= now) {
                continue;
            }
            if (Contains(buff.name, needle)) return true;
        }
        return false;
    }

    static bool HasProtectionImmunity(
        const ChampionState& state, float now) noexcept {
        return state.inStasis || state.invulnerable ||
               !state.targetable ||
               HasActiveBuff(state, "spellshield", now) ||
               HasActiveBuff(state, "invulnerable", now) ||
               HasActiveBuff(state, "undying", now);
    }

    static bool HasGrievousWounds(
        const ChampionState& state, float now) noexcept {
        return HasActiveBuff(state, "grievous", now) ||
               HasActiveBuff(state, "ignite", now) ||
               HasActiveBuff(state, "mortal", now);
    }

    static bool HasHealLockout(
        const ChampionState& state, float now) noexcept {
        return HasActiveBuff(
                   state, "summonerhealcheck", now) ||
               HasActiveBuff(
                   state, "healreduction", now);
    }

    static bool HasMovementBoost(
        const ChampionState& state, float now) noexcept {
        return HasActiveBuff(state, "ghost", now) ||
               HasActiveBuff(state, "shurelya", now) ||
               HasActiveBuff(state, "youmuu", now) ||
               HasActiveBuff(state, "movespeed", now);
    }

    static float EstimateBarrierShield(
        const ChampionState& player) noexcept {
        const int level =
            std::clamp(player.level.value, 1, 18);
        return 120.0f +
               360.0f *
                   static_cast<float>(level - 1) / 17.0f;
    }

    static float EstimateHealAmount(
        const ChampionState& target) noexcept {
        const int level =
            std::clamp(target.level.value, 1, 18);
        return 80.0f +
               238.0f *
                   static_cast<float>(level - 1) / 17.0f;
    }

    static float EffectiveThreatDamage(
        const ThreatForecast& forecast,
        bool includeDamageOverTime,
        bool ignoreTurretDamage) noexcept {
        float damage = forecast.incomingDamage;
        if (!includeDamageOverTime) {
            damage -= forecast.damageOverTime;
        }
        if (ignoreTurretDamage) {
            damage -= forecast.turretDamage;
            if (!includeDamageOverTime) {
                damage += forecast.turretDamageOverTime;
            }
        }
        return std::max(0.0f, damage);
    }

    static float SummonerRange(
        const PatchRegistry& registry, Capability capability,
        float fallback) noexcept {
        const SummonerDefinition* definition =
            registry.FindAvailableSummoner(capability);
        return definition && definition->range > 0.0f
            ? definition->range
            : fallback;
    }

    static float ItemRange(
        const PatchRegistry& registry, Capability capability,
        float fallback) noexcept {
        const ItemDefinition* definition =
            registry.FindAvailableItem(capability);
        return definition && definition->range > 0.0f
            ? definition->range
            : fallback;
    }

    static bool IsUnsafeDestination(
        const Point3& point, const ChampionState& player,
        const StateStore& store, float now) noexcept {
        if (::Globals::base != 0) {
            const auto navGrid = ::CoreNavGrid::Get();
            if (navGrid.IsValid() &&
                !navGrid.IsWalkable(
                    Vec3{ point.x, point.y, point.z })) {
                return true;
            }
        }
        for (std::size_t i = 0;
             i < store.Threats().Size(); ++i) {
            const ThreatState& threat =
                store.Threats().At(i);
            if (CombatPredictionService::IsObservedActionable(
                    threat, now, 1.0f) &&
                CombatPredictionService::Contains(
                    threat, point)) {
                return true;
            }
        }
        for (std::size_t i = 0;
             i < store.Structures().Size(); ++i) {
            const StructureState& structure =
                store.Structures().At(i);
            if (structure.alive && structure.visible &&
                structure.kind == StructureKind::Turret &&
                structure.team != 0 &&
                structure.team != player.team &&
                structure.position.Distance(point) <= 950.0f) {
                return true;
            }
        }
        bool nearbyEnemy = false;
        store.ForEachChampion([&](const ChampionState& enemy) {
            if (!nearbyEnemy &&
                CombatValidationService::IsTargetableEnemy(
                    enemy, player.team) &&
                enemy.position.Distance(point) <= 325.0f) {
                nearbyEnemy = true;
            }
        });
        return nearbyEnemy;
    }

    static Point3 FindFlashDestination(
        const ChampionState& player,
        const ChampionState& source,
        const StateStore& store, float now,
        float range) noexcept {
        const Point3 away =
            Normalize2D(player.position - source.position);
        if (away.IsZero()) return {};
        const float base =
            std::atan2(away.z, away.x);
        static constexpr std::array<float, 7> offsets = {
            0.0f, 0.45f, -0.45f, 0.90f,
            -0.90f, 1.35f, -1.35f
        };
        Point3 best{};
        float bestGain = 0.0f;
        const float before =
            player.position.Distance(source.position);
        for (const float offset : offsets) {
            const float angle = base + offset;
            const Point3 direction{
                std::cos(angle), 0.0f, std::sin(angle)
            };
            const Point3 candidate =
                player.position + direction * range;
            if (IsUnsafeDestination(
                    candidate, player, store, now)) {
                continue;
            }
            const float gain =
                candidate.Distance(source.position) - before;
            if (gain > bestGain) {
                bestGain = gain;
                best = candidate;
            }
        }
        return bestGain >= range * 0.55f ? best : Point3{};
    }

    static ActionMode EffectiveMode(ActionMode requested,
                                    RuntimeMode runtimeMode,
                                    const ActivatorSettings& settings) noexcept {
        if (requested != ActionMode::Auto) return requested;
        if (!settings.allowPracticeAutomation ||
            runtimeMode != RuntimeMode::Practice) {
            return ActionMode::Confirm;
        }
        return ActionMode::Auto;
    }

    static const ObservedSpell* FindSpell(
        const ChampionState& player, Capability capability,
        const PatchRegistry& registry,
        bool requireReady = true) noexcept {
        return ResolveSummonerSlot(
            player, capability, registry, requireReady);
    }

    static const ObservedItem* FindItem(
        const ChampionState& player, Capability capability,
        const PatchRegistry& registry,
        bool requireUsable = true) noexcept {
        return ResolveInventorySlot(
            player, capability, registry, requireUsable);
    }

    static bool BindCapability(const ChampionState& player,
                               const PatchRegistry& registry,
                               Capability capability,
                               ActionRequest& request) noexcept {
        if (const ObservedSpell* spell = FindSpell(player, capability, registry)) {
            request.spellSlot = spell->slot;
            return true;
        }
        if (const ObservedItem* item = FindItem(player, capability, registry)) {
            request.itemId = item->itemId;
            request.itemSlot = item->slot;
            return true;
        }
        return false;
    }

    static ActionRequest MakeRequest(const ChampionState& player,
                                     const PatchRegistry& registry,
                                     const ActivatorSettings& settings,
                                     RuntimeMode runtimeMode,
                                     Capability capability,
                                     ActionPriority priority,
                                     float now,
                                     std::string_view reason,
                                     Confidence confidence = Confidence::High) {
        ActionRequest request{};
        request.capability = capability;
        request.mode = EffectiveMode(settings.ModeFor(capability), runtimeMode, settings);
        if (capability == Capability::Herald ||
            capability == Capability::KnightsVow) {
            request.mode = request.mode == ActionMode::Off
                ? ActionMode::Off
                : ActionMode::Suggest;
        } else if ((capability == Capability::Flash ||
                    capability == Capability::Teleport) &&
                   request.mode == ActionMode::Auto) {
            request.mode = ActionMode::Confirm;
        }
        request.priority = priority;
        request.resourceMask = ResourceFor(capability);
        request.conflictMask = request.resourceMask;
        request.createdAt = now;
        request.expiresAt = now + 0.35f;
        request.confidence = confidence;
        CopyText(request.reason, reason);
        CopyText(request.sourceModule, "activator");
        if (!BindCapability(player, registry, capability, request)) request.capability = Capability::None;
        return request;
    }


    static CrowdControlSummary SummarizeControl(const ChampionState& state,
                                                const PatchRegistry& registry,
                                                float now) noexcept {
        CrowdControlSummary summary{};
        for (std::size_t i = 0; i < state.buffCount; ++i) {
            const ObservedBuff& buff = state.buffs[i];
            if (buff.endTime > 0.0f && buff.endTime <= now) continue;
            summary.latestStartAt = std::max(summary.latestStartAt, buff.startTime);
            const float remaining = buff.endTime > 0.0f
                ? std::max(0.0f, buff.endTime - now)
                : 0.5f;
            if (const BuffDefinition* definition =
                    registry.ResolveBuff(buff.idHash, buff.name)) {
                if (remaining < definition->minimumDuration) continue;
                summary.cleanseable = summary.cleanseable || definition->cleanseable;
                summary.qssable = summary.qssable || definition->qssable;
                summary.mikaelable = summary.mikaelable || definition->mikaelable;
                summary.suppression = summary.suppression ||
                                      definition->control == CrowdControl::Suppression;
                summary.airborne = summary.airborne ||
                                   definition->control == CrowdControl::Airborne;
                summary.severity = std::max(
                    summary.severity, static_cast<int>(std::ceil(definition->danger)));
                summary.longestRemaining = std::max(summary.longestRemaining, remaining);
                continue;
            }
            const auto type = static_cast<SDK::BuffType>(buff.type);
            switch (type) {
            case SDK::BuffType::Stun: case SDK::BuffType::Silence: case SDK::BuffType::Taunt:
            case SDK::BuffType::Polymorph: case SDK::BuffType::Snare: case SDK::BuffType::Fear:
            case SDK::BuffType::Charm: case SDK::BuffType::Blind: case SDK::BuffType::Grounded:
            case SDK::BuffType::Drowsy: case SDK::BuffType::Asleep:
                summary.cleanseable = true; summary.qssable = true; summary.mikaelable = true;
                summary.severity = std::max(summary.severity, 7); break;
            case SDK::BuffType::Suppression:
                summary.qssable = true; summary.suppression = true;
                summary.severity = std::max(summary.severity, 10); break;
            case SDK::BuffType::Knockup: case SDK::BuffType::Knockback:
                summary.airborne = true; summary.severity = std::max(summary.severity, 10); break;
            default: continue;
            }
            summary.longestRemaining = std::max(summary.longestRemaining, remaining);
        }
        return summary;
    }

    static void EvaluateCleanse(const ChampionState& player,
                                const StateStore& store,
                                const PatchRegistry& registry,
                                const ActivatorSettings& settings,
                                RuntimeMode runtimeMode,
                                float now,
                                ActionArbiter& arbiter) {
        if (!settings.summonersEnabled &&
            !settings.defensiveItemsEnabled) {
            return;
        }
        const CrowdControlSummary control =
            SummarizeControl(player, registry, now);
        if (control.airborne ||
            control.longestRemaining * 1000.0f < 250.0f ||
            now - control.latestStartAt <
                settings.cleanseReactionDelay) {
            return;
        }

        bool removed = false;
        if (settings.summonersEnabled &&
            control.cleanseable && !control.suppression) {
            ActionRequest cleanse = MakeRequest(
                player, registry, settings, runtimeMode,
                Capability::Cleanse,
                ActionPriority::BreakHardCc, now,
                "observed Cleanse-compatible crowd control");
            cleanse.expectedValue =
                static_cast<float>(control.severity) +
                control.longestRemaining;
            removed = arbiter.Submit(cleanse);
        }
        if (!removed && settings.defensiveItemsEnabled &&
            control.qssable) {
            ActionRequest qss = MakeRequest(
                player, registry, settings, runtimeMode,
                Capability::Qss,
                ActionPriority::BreakHardCc, now,
                control.suppression
                    ? "observed suppression; QSS is valid"
                    : "observed removable hard crowd control");
            if (qss.capability == Capability::None ||
                qss.mode == ActionMode::Off) {
                qss = MakeRequest(
                    player, registry, settings, runtimeMode,
                    Capability::Mercurial,
                    ActionPriority::BreakHardCc, now,
                    "observed removable hard crowd control");
            }
            qss.expectedValue =
                static_cast<float>(control.severity) +
                control.longestRemaining;
            arbiter.Submit(qss);
        }
        (void)store;
    }

    static void EvaluateProtection(const ChampionState& player,
                                   const ThreatForecast& forecast,
                                   const PatchRegistry& registry,
                                   const ActivatorSettings& settings,
                                   RuntimeMode runtimeMode,
                                   float now,
                                   ActionArbiter& arbiter) {
        if (forecast.threatCount <= 0 ||
            forecast.incomingDamage <= 0.0f ||
            HasProtectionImmunity(player, now)) {
            return;
        }
        const float maxHealth =
            std::max(1.0f, player.maxHealth.value);
        const float existingShield =
            std::max(0.0f, player.allShield);
        const float damage = EffectiveThreatDamage(
            forecast, settings.includeDamageOverTime, false);
        const float barrierDamage = EffectiveThreatDamage(
            forecast, settings.includeDamageOverTime,
            settings.ignoreTurretDamage);
        const bool lethal =
            damage >= player.health.value + existingShield;
        const bool burst =
            damage >= maxHealth *
                settings.protectionThreshold;

        if (settings.summonersEnabled) {
            const float barrierShield =
                EstimateBarrierShield(player);
            const float barrierNeeded = std::max(
                0.0f, barrierDamage - existingShield);
            const float barrierEfficiency =
                barrierShield > 0.0f
                    ? std::min(barrierShield, barrierNeeded) /
                          barrierShield
                    : 0.0f;
            const bool barrierLethal =
                barrierDamage >=
                    player.health.value + existingShield;
            const bool barrierBurst =
                barrierDamage >= maxHealth *
                    settings.protectionThreshold;
            const bool barrierSaves =
                !barrierLethal ||
                barrierDamage <
                    player.health.value + existingShield +
                        barrierShield;
            if ((barrierLethal || barrierBurst) &&
                (!settings.barrierLethalOnly ||
                 barrierLethal) &&
                barrierSaves &&
                barrierEfficiency >=
                    settings.minimumShieldEfficiency) {
                ActionRequest barrier = MakeRequest(
                    player, registry, settings, runtimeMode,
                    Capability::Barrier,
                    ActionPriority::PreventDeath, now,
                    barrierLethal
                        ? "Barrier prevents lethal observed damage"
                        : "observed burst efficiently consumes Barrier",
                    forecast.confidence);
                barrier.expectedValue =
                    std::min(barrierShield, barrierNeeded);
                arbiter.Submit(barrier);
            }

            const float missingHealth =
                std::max(0.0f, maxHealth - player.health.value);
            const float rawHeal = EstimateHealAmount(player);
            const float effectiveHeal =
                rawHeal *
                (HasGrievousWounds(player, now)
                     ? 0.6f
                     : 1.0f);
            const float usableHeal =
                std::min(missingHealth, effectiveHeal);
            const bool healSaves =
                !lethal ||
                damage < player.health.value +
                    existingShield + usableHeal;
            const bool sufficientlyMissing =
                missingHealth / maxHealth >=
                    settings.healMissingHealthThreshold;
            if ((lethal || burst) && healSaves &&
                sufficientlyMissing &&
                !HasHealLockout(player, now) &&
                usableHeal >= effectiveHeal * 0.35f) {
                ActionRequest heal = MakeRequest(
                    player, registry, settings, runtimeMode,
                    Capability::Heal,
                    ActionPriority::PreventDeath, now,
                    lethal
                        ? "Heal prevents lethal observed damage"
                        : "incoming burst and missing health justify Heal",
                    forecast.confidence);
                heal.expectedValue = usableHeal;
                arbiter.Submit(heal);
            }
        }
        if (settings.defensiveItemsEnabled &&
            (lethal || burst)) {
            if (lethal &&
                forecast.firstImpactAt <= now + 2.5f) {
                ActionRequest stasis = MakeRequest(
                    player, registry, settings, runtimeMode,
                    Capability::Zhonya,
                    ActionPriority::PreventDeath, now,
                    "lethal observed impact enters the stasis window",
                    forecast.confidence);
                if (stasis.capability == Capability::None ||
                    stasis.mode == ActionMode::Off) {
                    stasis = MakeRequest(
                        player, registry, settings, runtimeMode,
                        Capability::Seeker,
                        ActionPriority::PreventDeath, now,
                        "lethal observed impact enters the stasis window",
                        forecast.confidence);
                }
                stasis.expectedValue = damage + maxHealth;
                arbiter.Submit(stasis);
            }
            if (damage > existingShield) {
                ActionRequest seraph = MakeRequest(
                    player, registry, settings, runtimeMode,
                    Capability::Seraph,
                    ActionPriority::PreventDeath, now,
                    lethal
                        ? "observed lethal damage justifies Seraph shield"
                        : "observed burst justifies Seraph shield",
                    forecast.confidence);
                seraph.expectedValue =
                    damage - existingShield;
                arbiter.Submit(seraph);
            }
        }
    }

    static void EvaluateAllies(const ChampionState& player,
                               const StateStore& store,
                               const PatchRegistry& registry,
                               const ActivatorSettings& settings,
                               RuntimeMode runtimeMode,
                               float now,
                               ActionArbiter& arbiter) {
        if (!settings.supportItemsEnabled &&
            !settings.summonersEnabled) {
            return;
        }
        const float healRange =
            SummonerRange(registry, Capability::Heal, 850.0f);
        const float mikaelRange =
            ItemRange(registry, Capability::Mikael, 750.0f);
        const float locketRange =
            ItemRange(registry, Capability::Locket, 700.0f);
        const float redemptionRange =
            ItemRange(registry, Capability::Redemption, 5500.0f);
        const float knightsVowRange =
            ItemRange(
                registry, Capability::KnightsVow, 1000.0f);
        int nearbyThreatenedAllies = 0;
        const ChampionState* lowest = nullptr;
        ThreatForecast lowestForecast{};
        float lowestRatio = 1.0f;
        const ChampionState* vowTarget = nullptr;
        float vowScore = -1.0f;

        store.ForEachChampion([&](const ChampionState& ally) {
            if (!ally.ally || ally.local || ally.dead ||
                !ally.visible || !ally.targetable ||
                ally.inStasis || ally.invulnerable ||
                ally.maxHealth.value <= 0.0f) {
                return;
            }
            const float ratio =
                ally.health.value / ally.maxHealth.value;
            const float distance =
                ally.position.Distance(player.position);
            if (settings.supportItemsEnabled &&
                distance <= knightsVowRange &&
                !HasActiveBuff(ally, "knightsvow", now)) {
                const float candidateScore =
                    std::max(0.0f, ally.totalGold.value) +
                    static_cast<float>(
                        std::max(1, ally.level.value)) *
                        500.0f;
                if (candidateScore > vowScore) {
                    vowScore = candidateScore;
                    vowTarget = &ally;
                }
            }
            const ThreatForecast forecast =
                CombatPredictionService::Forecast(
                    ally, store, now,
                    settings.defensiveHorizon);
            const float incoming = EffectiveThreatDamage(
                forecast, settings.includeDamageOverTime,
                false);
            const bool threatened =
                forecast.threatCount > 0 &&
                incoming > std::max(0.0f, ally.allShield);

            if (distance <= locketRange && threatened &&
                ratio < settings.allySaveThreshold) {
                ++nearbyThreatenedAllies;
            }
            if (distance <= redemptionRange &&
                ratio < lowestRatio &&
                (threatened || ratio < 0.20f)) {
                lowestRatio = ratio;
                lowest = &ally;
                lowestForecast = forecast;
            }

            if (settings.supportItemsEnabled) {
                const CrowdControlSummary control =
                    SummarizeControl(ally, registry, now);
                if (control.mikaelable &&
                    !control.airborne &&
                    distance <= mikaelRange &&
                    control.longestRemaining >= 0.25f &&
                    now - control.latestStartAt >=
                        settings.cleanseReactionDelay) {
                    ActionRequest request = MakeRequest(
                        player, registry, settings, runtimeMode,
                        Capability::Mikael,
                        ActionPriority::SaveAlly, now,
                        "nearby ally has observed Mikael-compatible crowd control");
                    request.targetId = ally.networkId;
                    request.expectedValue =
                        static_cast<float>(control.severity) +
                        control.longestRemaining;
                    arbiter.Submit(request);
                }
            }

            if (settings.summonersEnabled && threatened &&
                distance <= healRange &&
                ratio < settings.allySaveThreshold &&
                !HasHealLockout(player, now) &&
                !HasHealLockout(ally, now)) {
                const float rawHeal = EstimateHealAmount(player);
                const float effectiveHeal =
                    rawHeal *
                    (HasGrievousWounds(ally, now)
                         ? 0.6f
                         : 1.0f);
                const float missingHealth =
                    ally.maxHealth.value - ally.health.value;
                const float usableHeal =
                    std::min(missingHealth, effectiveHeal);
                const bool lethal =
                    incoming >= ally.health.value +
                        std::max(0.0f, ally.allShield);
                const bool saves =
                    !lethal ||
                    incoming < ally.health.value +
                        std::max(0.0f, ally.allShield) +
                        usableHeal;
                if (saves &&
                    usableHeal >= effectiveHeal * 0.35f) {
                    ActionRequest heal = MakeRequest(
                        player, registry, settings, runtimeMode,
                        Capability::Heal,
                        ActionPriority::SaveAlly, now,
                        lethal
                            ? "Heal prevents lethal observed ally damage"
                            : "nearby ally has low health and observed damage",
                        forecast.confidence);
                    heal.expectedValue = usableHeal;
                    arbiter.Submit(heal);
                }
            }
        });

        if (settings.supportItemsEnabled && vowTarget &&
            !HasActiveBuff(player, "knightsvow", now)) {
            ActionRequest vow = MakeRequest(
                player, registry, settings, runtimeMode,
                Capability::KnightsVow,
                ActionPriority::Utility, now,
                "highest-value visible ally is available for Knight's Vow");
            vow.targetId = vowTarget->networkId;
            vow.expectedValue = vowScore;
            arbiter.Submit(vow);
        }

        if (settings.supportItemsEnabled &&
            nearbyThreatenedAllies >= 2) {
            ActionRequest locket = MakeRequest(
                player, registry, settings, runtimeMode,
                Capability::Locket,
                ActionPriority::SaveAlly, now,
                "multiple nearby allies can efficiently consume Locket");
            locket.expectedValue =
                static_cast<float>(nearbyThreatenedAllies) *
                100.0f;
            arbiter.Submit(locket);
        }
        if (settings.supportItemsEnabled && lowest &&
            lowestRatio < settings.allySaveThreshold &&
            lowestForecast.threatCount > 0) {
            ActionRequest redemption = MakeRequest(
                player, registry, settings, runtimeMode,
                Capability::Redemption,
                ActionPriority::SaveAlly, now,
                "visible threatened ally has high missing-health value",
                lowestForecast.confidence);
            redemption.position = lowest->position;
            redemption.expectedValue =
                (1.0f - lowestRatio) *
                    lowest->maxHealth.value +
                lowestForecast.incomingDamage;
            arbiter.Submit(redemption);
        }
    }

    static void EvaluateEnemies(const ChampionState& player,
                                const StateStore& store,
                                const ThreatForecast& selfForecast,
                                const PatchRegistry& registry,
                                const ActivatorSettings& settings,
                                RuntimeMode runtimeMode,
                                float now,
                                ActionArbiter& arbiter) {
        const float igniteRange =
            SummonerRange(registry, Capability::Ignite, 600.0f);
        const ChampionState* closest = nullptr;
        float closestDistance = 100000.0f;

        store.ForEachChampion([&](const ChampionState& enemy) {
            if (!CombatValidationService::IsTargetableEnemy(
                    enemy, player.team)) {
                return;
            }
            const float distance =
                enemy.position.Distance(player.position);
            if (distance < closestDistance) {
                closestDistance = distance;
                closest = &enemy;
            }

            if (!settings.summonersEnabled ||
                distance > igniteRange ||
                HasProtectionImmunity(enemy, now) ||
                HasActiveBuff(enemy, "summonerdot", now) ||
                HasActiveBuff(enemy, "ignite", now)) {
                return;
            }
            const int level =
                std::clamp(player.level.value, 1, 18);
            const float igniteDamage =
                50.0f + 20.0f * static_cast<float>(level);
            const ThreatForecast targetForecast =
                CombatPredictionService::Forecast(
                    enemy, store, now, 5.0f);
            const float committedDamage =
                targetForecast.incomingDamage;
            const float projectedHealth =
                enemy.health.value +
                std::max(0.0f, enemy.allShield) +
                std::max(0.0f, enemy.healthRegen) * 2.5f -
                committedDamage;
            const float healthRatio =
                enemy.health.value /
                std::max(1.0f, enemy.maxHealth.value);
            const float healingPressure =
                std::max(0.0f, enemy.healthRegen) * 5.0f;
            const bool execute =
                projectedHealth <= igniteDamage -
                    settings.offensiveSafetyMargin;
            const bool antiHeal =
                healthRatio <= 0.55f &&
                projectedHealth <= igniteDamage * 2.0f &&
                (healingPressure >= igniteDamage * 0.20f ||
                 HasActiveBuff(enemy, "lifesteal", now) ||
                 HasActiveBuff(enemy, "omnivamp", now) ||
                 HasActiveBuff(enemy, "regeneration", now));
            if (!execute && !antiHeal) return;

            ActionRequest ignite = MakeRequest(
                player, registry, settings, runtimeMode,
                Capability::Ignite,
                ActionPriority::Execute, now,
                execute
                    ? "projected visible health is inside Ignite execute margin"
                    : "low visible target has observed high healing pressure");
            ignite.targetId = enemy.networkId;
            ignite.expectedValue = execute
                ? igniteDamage - projectedHealth
                : healingPressure;
            arbiter.Submit(ignite);
        });
        if (!closest) return;

        const ChampionState* primary =
            store.FindChampion(selfForecast.primarySource);
        if (primary &&
            !CombatValidationService::IsTargetableEnemy(
                *primary, player.team)) {
            primary = nullptr;
        }
        const float damage = EffectiveThreatDamage(
            selfForecast, settings.includeDamageOverTime,
            false);

        if (settings.summonersEnabled && primary) {
            const float exhaustRange =
                SummonerRange(
                    registry, Capability::Exhaust, 650.0f);
            const float primaryDistance =
                primary->position.Distance(player.position);
            const float reducibleDamage = std::max(
                0.0f, damage -
                    selfForecast.incomingTrueDamage);
            if (primaryDistance <= exhaustRange &&
                reducibleDamage >=
                    player.maxHealth.value *
                        settings.exhaustDamageThreshold &&
                !HasActiveBuff(*primary, "exhaust", now)) {
                ActionRequest exhaust = MakeRequest(
                    player, registry, settings, runtimeMode,
                    Capability::Exhaust,
                    ActionPriority::Disengage, now,
                    "visible source contributes substantial reducible burst",
                    selfForecast.confidence);
                exhaust.targetId = primary->networkId;
                exhaust.expectedValue = reducibleDamage;
                arbiter.Submit(exhaust);
            }
        }

        if (settings.summonersEnabled &&
            !HasMovementBoost(player, now)) {
            const CrowdControlSummary control =
                SummarizeControl(player, registry, now);
            const float speed =
                std::max(1.0f, player.moveSpeed);
            const bool disengage =
                primary &&
                damage >=
                    player.maxHealth.value * 0.15f &&
                primary->position.Distance(player.position) <=
                    1400.0f;
            const float closestRatio =
                closest->health.value /
                std::max(1.0f, closest->maxHealth.value);
            const bool chase =
                !disengage &&
                selfForecast.threatCount == 0 &&
                closestRatio <= 0.35f &&
                closestDistance >= 500.0f &&
                closestDistance <= 1800.0f;
            const float travelDistance = disengage
                ? std::min(
                      primary->position.Distance(player.position),
                      1800.0f)
                : std::min(closestDistance, 1800.0f);
            const float timeGain =
                travelDistance / speed -
                travelDistance / (speed * 1.25f);
            if ((disengage || chase) &&
                control.severity < 4 &&
                timeGain >= settings.ghostMinimumTimeGain) {
                ActionRequest ghost = MakeRequest(
                    player, registry, settings, runtimeMode,
                    Capability::Ghost,
                    disengage
                        ? ActionPriority::Disengage
                        : ActionPriority::Mobility,
                    now,
                    disengage
                        ? "Ghost creates meaningful separation from visible danger"
                        : "Ghost creates meaningful chase time against a low target",
                    selfForecast.confidence);
                ghost.expectedValue = timeGain;
                arbiter.Submit(ghost);
            }
        }

        if (settings.offensiveItemsEnabled &&
            !HasProtectionImmunity(*closest, now)) {
            const ThreatForecast targetForecast =
                CombatPredictionService::Forecast(
                    *closest, store, now, 1.0f);
            const float committedDamage =
                targetForecast.incomingDamage;
            const float targetEffectiveHealth =
                closest->health.value +
                std::max(0.0f, closest->allShield);
            const ItemDefinition* gunbladeDefinition =
                registry.FindAvailableItem(
                    Capability::Gunblade);
            const float gunbladeRange =
                ItemRange(
                    registry, Capability::Gunblade, 700.0f);
            const float gunbladeDamage =
                gunbladeDefinition &&
                gunbladeDefinition->effectValues[0] > 0.0f
                    ? gunbladeDefinition->effectValues[0]
                    : 175.0f;
            if (closestDistance <= gunbladeRange &&
                committedDamage < targetEffectiveHealth &&
                targetEffectiveHealth - committedDamage <=
                    gunbladeDamage) {
                ActionRequest gunblade = MakeRequest(
                    player, registry, settings, runtimeMode,
                    Capability::Gunblade,
                    ActionPriority::Execute, now,
                    "active damage is needed to finish the visible target");
                gunblade.targetId = closest->networkId;
                gunblade.expectedValue =
                    gunbladeDamage -
                    (targetEffectiveHealth -
                     committedDamage);
                arbiter.Submit(gunblade);
            }

            const float rocketbeltDash =
                ItemRange(
                    registry, Capability::Rocketbelt,
                    275.0f);
            const float rocketbeltReach =
                rocketbeltDash + 450.0f;
            if (settings.ModeFor(
                    Capability::Rocketbelt) !=
                    ActionMode::Off &&
                FindItem(
                    player, Capability::Rocketbelt,
                    registry) != nullptr &&
                closestDistance >= 300.0f &&
                closestDistance <= rocketbeltReach &&
                damage < player.health.value * 0.50f) {
                const Point3 direction = Normalize2D(
                    closest->position - player.position);
                const Point3 endpoint =
                    player.position +
                    direction * rocketbeltDash;
                if (!direction.IsZero() &&
                    !IsUnsafeDestination(
                        endpoint, player, store, now)) {
                    ActionRequest rocketbelt = MakeRequest(
                        player, registry, settings,
                        runtimeMode,
                        Capability::Rocketbelt,
                        ActionPriority::Engage, now,
                        "safe dash endpoint closes distance to a visible target");
                    rocketbelt.position =
                        closest->position;
                    rocketbelt.expectedValue =
                        closestDistance - rocketbeltDash;
                    arbiter.Submit(rocketbelt);
                }
            }

            const std::array<Capability, 5> cleaves = {
                Capability::Stridebreaker,
                Capability::Tiamat,
                Capability::RavenousHydra,
                Capability::TitanicHydra,
                Capability::ProfaneHydra
            };
            for (const Capability capability : cleaves) {
                const float range =
                    ItemRange(registry, capability, 450.0f);
                int count = 0;
                store.ForEachChampion(
                    [&](const ChampionState& enemy) {
                        if (CombatValidationService::
                                IsTargetableEnemy(
                                    enemy, player.team) &&
                            enemy.position.Distance(
                                player.position) <= range) {
                            ++count;
                        }
                    });
                const bool executeActive =
                    capability == Capability::ProfaneHydra &&
                    closest->health.value /
                            std::max(
                                1.0f,
                                closest->maxHealth.value) <=
                        0.40f;
                if (count <= 0 ||
                    (capability == Capability::ProfaneHydra &&
                     count < 2 && !executeActive)) {
                    continue;
                }
                ActionRequest cleave = MakeRequest(
                    player, registry, settings, runtimeMode,
                    capability,
                    executeActive
                        ? ActionPriority::Execute
                        : ActionPriority::Engage,
                    now,
                    executeActive
                        ? "low visible champion is inside execute-active radius"
                        : "visible champions are inside cleave-active radius");
                cleave.expectedValue =
                    static_cast<float>(count) * 20.0f +
                    (executeActive ? 30.0f : 0.0f);
                arbiter.Submit(cleave);
            }
        }

        if (settings.defensiveItemsEnabled) {
            const float randuinRange =
                ItemRange(
                    registry, Capability::Randuin, 500.0f);
            int nearbyDivers = 0;
            store.ForEachChampion(
                [&](const ChampionState& enemy) {
                    if (CombatValidationService::
                            IsTargetableEnemy(
                                enemy, player.team) &&
                        enemy.position.Distance(
                            player.position) <= randuinRange) {
                        ++nearbyDivers;
                    }
                });
            if (nearbyDivers >= 2) {
                ActionRequest randuin = MakeRequest(
                    player, registry, settings, runtimeMode,
                    Capability::Randuin,
                    ActionPriority::Disengage, now,
                    "multiple visible divers are inside Randuin range");
                randuin.expectedValue =
                    static_cast<float>(nearbyDivers) * 25.0f;
                arbiter.Submit(randuin);
            }
        }

        if (settings.summonersEnabled && primary &&
            settings.ModeFor(Capability::Flash) !=
                ActionMode::Off &&
            FindSpell(
                player, Capability::Flash,
                registry) != nullptr &&
            selfForecast.threatCount > 0 &&
            damage >= player.health.value +
                std::max(0.0f, player.allShield) &&
            static_cast<int>(selfForecast.confidence) >=
                static_cast<int>(Confidence::High)) {
            bool targetedThreat = false;
            for (std::size_t i = 0;
                 i < store.Threats().Size(); ++i) {
                const ThreatState& threat =
                    store.Threats().At(i);
                if (threat.sourceId == primary->networkId &&
                    threat.targetId == player.networkId &&
                    CombatPredictionService::
                        IsObservedActionable(
                            threat, now,
                            settings.defensiveHorizon)) {
                    targetedThreat = true;
                    break;
                }
            }
            if (targetedThreat) {
                const float flashRange =
                    SummonerRange(
                        registry, Capability::Flash, 425.0f);
                const Point3 destination =
                    FindFlashDestination(
                        player, *primary, store, now,
                        flashRange);
                if (!destination.IsZero()) {
                    ActionRequest flash = MakeRequest(
                        player, registry, settings, runtimeMode,
                        Capability::Flash,
                        ActionPriority::PreventDeath, now,
                        "lethal targeted threat has a validated Flash destination",
                        selfForecast.confidence);
                    flash.position = destination;
                    flash.expectedValue = damage;
                    arbiter.Submit(flash);
                }
            }
        }
    }

    static float EstimateSmiteDamage(
        const ChampionState& player,
        const SummonerDefinition* definition) noexcept {
        const std::array<float, 3> fallback = {
            600.0f, 1000.0f, 1400.0f
        };
        int stage = 0;
        if (player.roleQuest.completed ||
            player.roleQuestSpellUpgrade) {
            stage = 2;
        } else if (player.roleQuest.progress >= 15) {
            stage = 1;
        }
        if (definition &&
            definition->effectValues[
                static_cast<std::size_t>(stage)] > 0.0f) {
            return definition->effectValues[
                static_cast<std::size_t>(stage)];
        }
        return fallback[static_cast<std::size_t>(stage)];
    }

    static void EvaluateObjectives(const ChampionState& player,
                                   const StateStore& store,
                                   const PatchRegistry& registry,
                                   const ActivatorSettings& settings,
                                   RuntimeMode runtimeMode,
                                   float now,
                                   ActionArbiter& arbiter) {
        if (!settings.summonersEnabled) return;
        const ObservedSpell* smiteSpell =
            FindSpell(player, Capability::Smite, registry);
        if (!smiteSpell) return;
        const SummonerDefinition* definition =
            registry.ResolveAvailableSummoner(
                smiteSpell->idHash, smiteSpell->name);
        const float smiteDamage =
            EstimateSmiteDamage(player, definition);
        const float smiteRange =
            definition && definition->range > 0.0f
                ? definition->range
                : 500.0f;

        for (std::size_t i = 0;
             i < store.Objectives().Size(); ++i) {
            const ObjectiveState& objective =
                store.Objectives().At(i);
            if (!objective.visible ||
                objective.status == ObjectiveStatus::Dead ||
                objective.status == ObjectiveStatus::Disabled ||
                objective.health <= 0.0f ||
                objective.position.Distance(player.position) >
                    smiteRange) {
                continue;
            }
            if (settings.reserveSmiteCharge &&
                objective.kind == ObjectiveKind::Scuttle &&
                smiteSpell->maxCharges > 1 &&
                smiteSpell->charges <= 1) {
                continue;
            }

            float recentDamage = 0.0f;
            store.Damage().ForEach(
                [&](const DamageRecord& record) {
                    if (record.targetId ==
                            objective.networkId &&
                        record.at >= now - 1.25f &&
                        record.amount > 0.0f &&
                        static_cast<int>(
                            record.evidence.confidence) >=
                            static_cast<int>(
                                Confidence::High)) {
                        recentDamage += record.amount;
                    }
                });
            const float damagePerSecond =
                recentDamage / 1.25f;
            const float reactionWindow = 0.12f;
            const float projectedHealth =
                objective.health -
                damagePerSecond * reactionWindow;
            if (projectedHealth <= 0.0f ||
                projectedHealth > smiteDamage) {
                continue;
            }

            const bool epic =
                objective.kind == ObjectiveKind::Baron ||
                objective.kind == ObjectiveKind::ElderDragon ||
                objective.kind == ObjectiveKind::DragonSoul ||
                objective.kind ==
                    ObjectiveKind::ElementalDragon ||
                objective.kind == ObjectiveKind::RiftHerald;
            ActionRequest smite = MakeRequest(
                player, registry, settings, runtimeMode,
                Capability::Smite,
                ActionPriority::SecureObjective, now,
                "visible objective enters the patch-aware Smite window",
                Confidence::Confirmed);
            smite.targetId = objective.networkId;
            smite.expectedValue =
                smiteDamage - projectedHealth +
                (epic ? 1000.0f : 250.0f);
            arbiter.Submit(smite);
        }
    }

    static bool HasPotionBuff(
        const ChampionState& player, float now) noexcept {
        return HasActiveBuff(player, "potion", now) ||
               HasActiveBuff(player, "flask", now) ||
               HasActiveBuff(
                   player, "itemcrystalflask", now);
    }

    static void EvaluateUtility(const ChampionState& player,
                                const StateStore& store,
                                const ThreatForecast& forecast,
                                const PatchRegistry& registry,
                                const ActivatorSettings& settings,
                                RuntimeMode runtimeMode,
                                float now,
                                ActionArbiter& arbiter) {
        const float healthRatio =
            player.maxHealth.value > 0.0f
                ? player.health.value /
                      player.maxHealth.value
                : 1.0f;
        const float damage = EffectiveThreatDamage(
            forecast, settings.includeDamageOverTime,
            false);
        if (settings.potionEnabled &&
            healthRatio < 0.65f && healthRatio > 0.15f &&
            damage < player.health.value +
                std::max(0.0f, player.allShield) &&
            !HasPotionBuff(player, now) &&
            !HasProtectionImmunity(player, now)) {
            ActionRequest potion = MakeRequest(
                player, registry, settings, runtimeMode,
                Capability::Potion,
                ActionPriority::Utility, now,
                "meaningful missing health and no potion effect is active");
            potion.expectedValue =
                (1.0f - healthRatio) * 100.0f;
            arbiter.Submit(potion);
        }

        if (settings.movementItemsEnabled &&
            forecast.threatCount > 0 &&
            damage >=
                player.maxHealth.value * 0.12f &&
            !HasMovementBoost(player, now)) {
            const CrowdControlSummary control =
                SummarizeControl(player, registry, now);
            if (control.severity < 4) {
                int nearbyAllies = 0;
                store.ForEachChampion(
                    [&](const ChampionState& ally) {
                        if (ally.ally && !ally.local &&
                            !ally.dead && ally.visible &&
                            ally.position.Distance(
                                player.position) <= 1000.0f) {
                            ++nearbyAllies;
                        }
                    });
                const Capability movementCapability =
                    nearbyAllies > 0
                        ? Capability::Shurelya
                        : Capability::Youmuu;
                ActionRequest movement = MakeRequest(
                    player, registry, settings, runtimeMode,
                    movementCapability,
                    ActionPriority::Disengage, now,
                    nearbyAllies > 0
                        ? "visible nearby allies share the disengage speed value"
                        : "self movement active creates observed disengage value",
                    forecast.confidence);
                movement.expectedValue =
                    damage +
                    static_cast<float>(nearbyAllies) * 25.0f;
                if (!arbiter.Submit(movement) &&
                    movementCapability ==
                        Capability::Shurelya) {
                    ActionRequest youmuu = MakeRequest(
                        player, registry, settings,
                        runtimeMode, Capability::Youmuu,
                        ActionPriority::Disengage, now,
                        "self movement active creates observed disengage value",
                        forecast.confidence);
                    youmuu.expectedValue = damage;
                    arbiter.Submit(youmuu);
                }
            }
        }

        if (settings.offensiveItemsEnabled &&
            player.maxMana.value > 0.0f &&
            !HasActiveBuff(player, "actualizer", now)) {
            float plannedMana = 0.0f;
            int readyCombatSpells = 0;
            for (std::size_t i = 0;
                 i < player.spellCount; ++i) {
                const ObservedSpell& spell =
                    player.spells[i];
                if (spell.slot >= 0 && spell.slot <= 3 &&
                    spell.ready) {
                    plannedMana +=
                        std::max(0.0f, spell.manaCost);
                    ++readyCombatSpells;
                }
            }
            bool visibleEnemy = false;
            store.ForEachChampion(
                [&](const ChampionState& enemy) {
                    if (!visibleEnemy &&
                        CombatValidationService::
                            IsTargetableEnemy(
                                enemy, player.team) &&
                        enemy.position.Distance(
                            player.position) <= 1200.0f) {
                        visibleEnemy = true;
                    }
                });
            const float requiredMana = std::max(
                plannedMana * 1.5f,
                player.maxMana.value * 0.30f);
            if (visibleEnemy && readyCombatSpells >= 2 &&
                player.mana.value >= requiredMana &&
                player.mana.value - requiredMana >=
                    player.maxMana.value * 0.20f) {
                ActionRequest actualizer = MakeRequest(
                    player, registry, settings, runtimeMode,
                    Capability::Actualizer,
                    ActionPriority::Engage, now,
                    "observed mana sustains the planned eight-second spell window");
                actualizer.expectedValue =
                    player.mana.value - requiredMana;
                arbiter.Submit(actualizer);
            }
        }

        if (settings.visionItemsEnabled) {
            const float oracleRange =
                ItemRange(
                    registry, Capability::Oracle, 750.0f);
            int nearbyFriendlyWards = 0;
            int nearbyEnemyWards = 0;
            store.Wards().ForEach(
                [&](const WardState& ward) {
                    if (ward.destroyed) return;
                    const float distance =
                        ward.position.Distance(player.position);
                    if (ward.enemy && ward.visible &&
                        distance <= oracleRange) {
                        ++nearbyEnemyWards;
                    }
                    if (ward.ally && distance <= 900.0f) {
                        ++nearbyFriendlyWards;
                    }
                });
            if (nearbyEnemyWards > 0) {
                ActionRequest oracle = MakeRequest(
                    player, registry, settings, runtimeMode,
                    Capability::Oracle,
                    ActionPriority::Utility, now,
                    "observed enemy vision is inside Oracle Lens range",
                    Confidence::Confirmed);
                oracle.expectedValue =
                    static_cast<float>(nearbyEnemyWards) *
                    10.0f;
                arbiter.Submit(oracle);
            } else if (forecast.threatCount == 0 &&
                       nearbyFriendlyWards == 0 &&
                       !player.lastDirection.IsZero()) {
                const Point3 direction =
                    Normalize2D(player.lastDirection);
                const float wardRange =
                    ItemRange(
                        registry, Capability::Ward, 600.0f);
                ActionRequest vision = MakeRequest(
                    player, registry, settings, runtimeMode,
                    Capability::Ward,
                    ActionPriority::Utility, now,
                    "no observed friendly vision covers the current route");
                vision.position =
                    player.position +
                    direction * std::min(wardRange, 600.0f);
                vision.expectedValue = 2.0f;
                if (!arbiter.Submit(vision)) {
                    const float farsightRange =
                        ItemRange(
                            registry,
                            Capability::Farsight, 4000.0f);
                    ActionRequest farsight = MakeRequest(
                        player, registry, settings, runtimeMode,
                        Capability::Farsight,
                        ActionPriority::Utility, now,
                        "Farsight can scout the uncovered observed route");
                    farsight.position =
                        player.position +
                        direction *
                            std::min(farsightRange, 3500.0f);
                    farsight.expectedValue = 1.0f;
                    arbiter.Submit(farsight);
                }
            }
        }

        if (settings.offensiveItemsEnabled) {
            const float heraldRange =
                ItemRange(
                    registry, Capability::Herald, 1200.0f);
            for (std::size_t i = 0;
                 i < store.Structures().Size(); ++i) {
                const StructureState& structure =
                    store.Structures().At(i);
                if (!structure.alive || !structure.visible ||
                    structure.team == player.team ||
                    structure.kind != StructureKind::Turret ||
                    structure.position.Distance(
                        player.position) > heraldRange) {
                    continue;
                }
                ActionRequest herald = MakeRequest(
                    player, registry, settings, runtimeMode,
                    Capability::Herald,
                    ActionPriority::Utility, now,
                    "visible enemy turret permits a Herald deployment suggestion",
                    structure.evidence.confidence);
                herald.position = structure.position;
                herald.expectedValue =
                    structure.maxHealth > 0.0f
                        ? 1.0f -
                              structure.health /
                                  structure.maxHealth
                        : 1.0f;
                arbiter.Submit(herald);
                break;
            }
        }

        if (settings.summonersEnabled &&
            forecast.threatCount == 0 &&
            !player.channeling && !player.recalling) {
            bool enemyNearPlayer = false;
            store.ForEachChampion(
                [&](const ChampionState& enemy) {
                    if (!enemyNearPlayer &&
                        CombatValidationService::
                            IsTargetableEnemy(
                                enemy, player.team) &&
                        enemy.position.Distance(
                            player.position) <= 1200.0f) {
                        enemyNearPlayer = true;
                    }
                });
            const float teleportRange =
                SummonerRange(
                    registry, Capability::Teleport,
                    25000.0f);
            const StructureState* destination = nullptr;
            float destinationScore = 0.0f;
            if (!enemyNearPlayer) {
                for (std::size_t i = 0;
                     i < store.Structures().Size(); ++i) {
                    const StructureState& structure =
                        store.Structures().At(i);
                    const float distance =
                        structure.position.Distance(
                            player.position);
                    if (!structure.alive ||
                        !structure.visible ||
                        structure.team != player.team ||
                        structure.networkId == 0 ||
                        distance < 2500.0f ||
                        distance > teleportRange ||
                        static_cast<int>(
                            structure.evidence.confidence) <
                            static_cast<int>(
                                Confidence::High)) {
                        continue;
                    }
                    int enemies = 0;
                    int allies = 0;
                    store.ForEachChampion(
                        [&](const ChampionState& champion) {
                            if (champion.dead ||
                                !champion.visible) {
                                return;
                            }
                            if (champion.position.Distance(
                                    structure.position) >
                                1000.0f) {
                                return;
                            }
                            if (champion.enemy) ++enemies;
                            if (champion.ally &&
                                !champion.local) {
                                ++allies;
                            }
                        });
                    if (enemies <= 0 ||
                        (allies <= 0 && enemies < 2)) {
                        continue;
                    }
                    const float score =
                        static_cast<float>(enemies) * 100.0f +
                        static_cast<float>(allies) * 40.0f +
                        distance * 0.001f;
                    if (score > destinationScore) {
                        destinationScore = score;
                        destination = &structure;
                    }
                }
            }
            if (destination) {
                ActionRequest teleport = MakeRequest(
                    player, registry, settings, runtimeMode,
                    Capability::Teleport,
                    ActionPriority::Mobility, now,
                    "visible distant fight supports a Teleport confirmation",
                    destination->evidence.confidence);
                teleport.targetId =
                    destination->networkId;
                teleport.position =
                    destination->position;
                teleport.expectedValue = destinationScore;
                arbiter.Submit(teleport);
            }
        }
    }
};

} // namespace NightSharp::Companion
