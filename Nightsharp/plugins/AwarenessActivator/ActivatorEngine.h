#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "../../SectionProfiler.h"

#include "AwarenessEngine.h"
#include "../../SDK/Enums/BuffType.h"
#include "../../sdk/Enumerations/SpellSlot.h"
#include "../../core/CoreNavGrid.h"
#include "../../SDK/UI/UI.h"

#include <Windows.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <string_view>

namespace NightSharp::Companion {

struct CapabilitySafety {
    bool allowSuggest = true;
    bool allowConfirm = true;
    bool allowPracticeAutomation = true;
};

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
    CapabilitySafety& SafetyFor(Capability capability) noexcept {
        const std::size_t index =
            static_cast<std::size_t>(capability);
        return safety_[index < safety_.size() ? index : 0];
    }
    const CapabilitySafety& SafetyFor(
        Capability capability) const noexcept {
        const std::size_t index =
            static_cast<std::size_t>(capability);
        return safety_[index < safety_.size() ? index : 0];
    }
    void SetSafety(Capability capability,
                   CapabilitySafety safety) noexcept {
        const std::size_t index =
            static_cast<std::size_t>(capability);
        if (index < safety_.size()) safety_[index] = safety;
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
    bool doNotUseWhileTyping = true;
    bool reserveSmiteCharge = true;
    bool includeDamageOverTime = true;
    bool barrierLethalOnly = false;
    bool drawOverlay = true;
    bool drawIcons = true;

    // World-space drawing can be controlled independently from the minimap.
    bool drawWorldLayer = true;
    bool drawWorldChampions = true;
    bool drawCombatState = true;
    bool drawReachableAreas = true;
    bool drawThreats = true;
    bool drawWards = true;
    bool drawJungle = true;
    bool drawObjectives = true;
    bool drawInsights = true;
    bool drawWave = true;

    // Minimap drawing has its own feature gates so disabling one category
    // avoids both its iteration and its draw/icon work.
    bool drawMinimapLayer = true;
    bool drawMinimapChampions = true;
    bool drawMinimapWards = true;
    bool drawMinimapJungle = true;
    bool drawMinimapObjectives = true;
    bool drawMinimapLabels = true;
    // Draws only path destinations that were observed while the champion was
    // visible. The observation expires after the expected travel window.
    bool drawPathTargets = true;

    bool drawAlertCenter = true;
    bool drawEnemyHud = true;
    // Compact notifications for contested epic objectives (dragon/baron/etc.).
    bool drawObjectiveAttackNotifications = true;
    // Draw short-lived enemy world icons from observed casts/missiles.
    // Hidden memory events are accepted in every runtime mode; while the
    // evidence window is active, rendering uses the enemy's live position.
    bool drawObservedEnemyWorldIcons = true;
    bool drawActivityHeatmap = false;
    bool drawVisionHeatmap = false;
    // Keeps expensive world-space drawing and decision scans bounded on
    // high-refresh-rate clients. Minimap information remains global.
    bool performanceMode = true;
    // Optional per-stage profiler. Disabled by default so render/update paths
    // do not take a clock or accumulate counters when diagnostics are unused.
    bool diagnosticsEnabled = false;
    bool diagnosticsConsoleLog = false;
    bool diagnosticsVerbose = true;
    float diagnosticsReportInterval = 60.0f;
    float diagnosticsSlowFrameMs = 8.0f;
    bool audioOnly = false;
    bool streamerMode = false;
    bool vietnamese = false;
    // 0 = vertical cards, 1 = horizontal cards.
    int hudLayoutIndex = 0;
    // User multiplier applied after each renderer-defined base icon size.
    float iconScale = 1.0f;
    int confirmationVirtualKey = VK_MENU;
    float defensiveHorizon = 1.25f;
    float protectionThreshold = 0.32f;
    float allySaveThreshold = 0.38f;
    float offensiveSafetyMargin = 15.0f;
    float cleanseReactionDelay = 0.04f;
    float reactionDebounceSeconds = 0.25f;
    float minimumShieldEfficiency = 0.35f;
    float healMissingHealthThreshold = 0.28f;
    float exhaustDamageThreshold = 0.20f;
    float ghostMinimumTimeGain = 0.35f;
    float worldDrawDistance = 4500.0f;
    float reachableAreaMaxRadius = 2500.0f;
    // Negative coordinates request a smart viewport-relative default.
    float alertPanelX = -1.0f;
    float alertPanelY = -1.0f;
    float objectivePanelX = -1.0f;
    float objectivePanelY = -1.0f;
    float insightPanelX = -1.0f;
    float insightPanelY = -1.0f;
    // Screen-space offset for the minimal enemy-ready icon row.
    float enemyHudOffsetX = 0.0f;
    float enemyHudOffsetY = -62.0f;
    int rolePresetIndex = 0;
    std::array<ActionMode, 64> modes_{};
    std::array<CapabilitySafety, 64> safety_{};
};


class ActivatorEngine final {
public:
    const ActionRequest* Evaluate(
        AwarenessEngine& awareness,
        const ActivatorSettings& settings) {
        NS_PROFILE("Activator.Evaluate");
        ActionArbiter& arbiter = awareness.Arbiter();
        arbiter.SetDebounceSeconds(
            settings.reactionDebounceSeconds);
        arbiter.Clear();
        hasSelfForecast_ = false;
        selfForecast_ = {};
        if (!settings.enabled) return nullptr;

        const StateStore& store = awareness.Store();
        const PatchRegistry& registry = awareness.Registry();
        const ChampionState* player = FindLocalPlayer(store);
        if (!player || player->dead || !player->visible ||
            player->health.value <= 0.0f) {
            return nullptr;
        }
        if ((settings.doNotInterruptRecall &&
             player->recalling) ||
            player->channeling ||
            (settings.doNotUseWhileTyping &&
             SDK::UI::g_KeybindInputBlocked)) {
            return nullptr;
        }

        const float now = awareness.Now();
        selfForecast_ = CombatPredictionService::Forecast(
            *player, store, now, settings.defensiveHorizon);
        hasSelfForecast_ = true;
        EvaluateCleanse(
            *player, store, registry, settings,
            awareness.Mode(), now, arbiter);
        EvaluateProtection(
            *player, selfForecast_, registry, settings,
            awareness.Mode(), now, arbiter);
        EvaluateAllies(
            *player, store, registry, settings,
            awareness.Mode(), now, arbiter);
        EvaluateEnemies(
            *player, store, selfForecast_, registry, settings,
            awareness.Mode(), now, arbiter);
        EvaluateObjectives(
            *player, store, registry, settings,
            awareness.Mode(), now, arbiter);
        EvaluateUtility(
            *player, store, selfForecast_, registry, settings,
            awareness.Mode(), now, arbiter);
        return arbiter.Resolve(now);
    }

    bool HasSelfForecast() const noexcept {
        return hasSelfForecast_;
    }
    const ThreatForecast& SelfForecast() const noexcept {
        return selfForecast_;
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
        if (!IsSummonerSpellSlot(spell.slot) ||
            spell.idHash == 0 || !spell.name[0]) {
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
            if (!definition || definition->capability != capability) {
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
    static bool PathSupports(
        const ChampionState& state,
        const Point3& destination) noexcept {
        const Point3 direction =
            Normalize2D(state.lastDirection);
        const Point3 route =
            Normalize2D(destination - state.position);
        if (direction.IsZero() || route.IsZero()) return true;
        const float alignment =
            direction.x * route.x + direction.z * route.z;
        return alignment >= -0.25f;
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
        bool includeDamageOverTime) noexcept {
        float damage = forecast.incomingDamage;
        if (!includeDamageOverTime) {
            damage -= forecast.damageOverTime;
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
        if (capability == Capability::KnightsVow) {
            request.mode = request.mode == ActionMode::Off
                ? ActionMode::Off
                : ActionMode::Suggest;
        } else if (capability == Capability::Flash ||
                   capability == Capability::Teleport) {
            if (request.mode != ActionMode::Off) {
                request.mode = ActionMode::Confirm;
            }
        }
        const CapabilitySafety& safety =
            settings.SafetyFor(capability);
        if (request.mode == ActionMode::Auto &&
            !safety.allowPracticeAutomation) {
            request.mode = safety.allowConfirm
                ? ActionMode::Confirm
                : (safety.allowSuggest
                       ? ActionMode::Suggest
                       : ActionMode::Off);
        }
        if (request.mode == ActionMode::Confirm &&
            !safety.allowConfirm) {
            request.mode = safety.allowSuggest
                ? ActionMode::Suggest
                : ActionMode::Off;
        }
        if (request.mode == ActionMode::Suggest &&
            !safety.allowSuggest) {
            request.mode = ActionMode::Off;
        }
        request.priority = priority;
        request.resourceMask = ResourceFor(capability);
        request.conflictMask = ConflictFor(capability);
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
            forecast, settings.includeDamageOverTime);
        const float barrierDamage = damage;
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

    void EvaluateAllies(const ChampionState& player,
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
        const bool canKnightsVow =
            settings.supportItemsEnabled &&
            settings.ModeFor(Capability::KnightsVow) !=
                ActionMode::Off &&
            FindItem(
                player, Capability::KnightsVow,
                registry) != nullptr &&
            !HasActiveBuff(player, "knightsvow", now) &&
            !player.recalling &&
            !player.channeling;
        int nearbyThreatenedAllies = 0;
        const ChampionState* lowest = nullptr;
        ThreatForecast lowestForecast{};
        float lowestRatio = 1.0f;
        const ChampionState* vowTarget = nullptr;
        const ChampionState* bestVowTarget = nullptr;
        const ChampionState* stickyVowTarget = nullptr;
        float vowScore = -1.0f;
        float bestVowScore = -1.0f;
        float stickyVowScore = -1.0f;

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
            if (canKnightsVow &&
                distance <= knightsVowRange &&
                !HasActiveBuff(ally, "knightsvow", now)) {
                const float candidateScore =
                    std::max(0.0f, ally.totalGold.value) +
                    static_cast<float>(
                        std::max(1, ally.level.value)) *
                        500.0f +
                    std::max(0.0f, 1.0f - ratio) * 250.0f;
                if (candidateScore > bestVowScore) {
                    bestVowScore = candidateScore;
                    bestVowTarget = &ally;
                }
                if (ally.networkId == knightsVowTargetId_) {
                    stickyVowScore = candidateScore;
                    stickyVowTarget = &ally;
                }
            }
            const ThreatForecast forecast =
                CombatPredictionService::Forecast(
                    ally, store, now,
                    settings.defensiveHorizon);
            const float incoming = EffectiveThreatDamage(
                forecast, settings.includeDamageOverTime);
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
                    heal.targetId = ally.networkId;
                    heal.expectedValue = usableHeal;
                    arbiter.Submit(heal);
                }
            }
        });
        if (canKnightsVow) {
            const bool keepSticky =
                stickyVowTarget &&
                (bestVowTarget == stickyVowTarget ||
                 bestVowScore <=
                     stickyVowScore * 1.20f + 250.0f);
            vowTarget = keepSticky
                ? stickyVowTarget
                : bestVowTarget;
            vowScore = keepSticky
                ? stickyVowScore
                : bestVowScore;
            knightsVowTargetId_ =
                vowTarget ? vowTarget->networkId : 0;
        }

        if (canKnightsVow && vowTarget) {
            ActionRequest vow = MakeRequest(
                player, registry, settings, runtimeMode,
                Capability::KnightsVow,
                ActionPriority::Utility, now,
                "stable highest-value visible ally is available for Knight's Vow");
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
            const ItemDefinition* redemptionDefinition =
                registry.FindAvailableItem(Capability::Redemption);
            const float redemptionDelay =
                redemptionDefinition &&
                        redemptionDefinition->effectValues[0] > 0.0f
                    ? redemptionDefinition->effectValues[0]
                    : 2.5f;
            if (lowestForecast.firstImpactAt > now) {
                redemption.earliestAt = std::max(
                    now, lowestForecast.firstImpactAt - redemptionDelay);
                redemption.expiresAt = redemption.earliestAt + 0.35f;
            }
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
            selfForecast, settings.includeDamageOverTime);

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
            const Point3 escapeRoute = disengage
                ? player.position +
                      Normalize2D(player.position - primary->position) *
                          100.0f
                : closest->position;
            const Point3 chaseRoute =
                closest->position +
                Normalize2D(closest->position - player.position) *
                    100.0f;
            const bool pathSupports =
                PathSupports(player, escapeRoute) &&
                (!chase || PathSupports(*closest, chaseRoute));
            const float pathConfidence =
                (player.pathBranches > 1 ||
                 closest->pathBranches > 1)
                    ? 0.75f
                    : 1.0f;
            const float timeGain =
                (travelDistance / speed -
                 travelDistance / (speed * 1.25f)) *
                pathConfidence;
            if (pathSupports && (disengage || chase) &&
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

        if (settings.movementItemsEnabled &&
            settings.ModeFor(Capability::Shurelya) != ActionMode::Off &&
            FindItem(player, Capability::Shurelya, registry) != nullptr &&
            !HasMovementBoost(player, now)) {
            const float shurelyaRange =
                ItemRange(registry, Capability::Shurelya, 1000.0f);
            int nearbyAllies = 0;
            int threatenedAllies = 0;
            float teamThreat = 0.0f;
            store.ForEachChampion([&](const ChampionState& ally) {
                if (!CombatValidationService::IsTargetableAlly(
                        ally, player.team) ||
                    ally.networkId == player.networkId) {
                    return;
                }
                const float distance =
                    ally.position.Distance(player.position);
                if (distance > shurelyaRange) return;
                ++nearbyAllies;
                const ThreatForecast forecast =
                    CombatPredictionService::Forecast(
                        ally, store, now, settings.defensiveHorizon);
                const float incoming = EffectiveThreatDamage(
                    forecast, settings.includeDamageOverTime);
                const float unmetDamage =
                    std::max(0.0f, incoming -
                        std::max(0.0f, ally.allShield));
                if (forecast.threatCount > 0 &&
                    unmetDamage > std::max(
                        15.0f, ally.health.value * 0.08f)) {
                    ++threatenedAllies;
                    teamThreat += unmetDamage;
                }
            });

            const CrowdControlSummary control =
                SummarizeControl(player, registry, now);
            const ChampionState* movementTarget =
                primary ? primary : closest;
            if (control.severity < 4 && nearbyAllies > 0) {
                const bool defensive =
                    threatenedAllies >= 2 ||
                    (threatenedAllies > 0 &&
                     damage >= player.health.value * 0.10f);
                const bool engage =
                    !defensive &&
                    selfForecast.threatCount == 0 &&
                    movementTarget &&
                    closestDistance >= 450.0f &&
                    closestDistance <= 1500.0f &&
                    movementTarget->health.value >
                        movementTarget->maxHealth.value * 0.20f &&
                    PathSupports(player, movementTarget->position);
                if (defensive || engage) {
                    ActionRequest shurelya = MakeRequest(
                        player, registry, settings, runtimeMode,
                        Capability::Shurelya,
                        defensive
                            ? ActionPriority::Disengage
                            : ActionPriority::Mobility,
                        now,
                        defensive
                            ? "multiple visible allies need immediate movement to escape observed danger"
                            : "nearby visible ally can convert movement speed into a safer engage",
                        defensive
                            ? (selfForecast.confidence == Confidence::Unknown
                                ? Confidence::High
                                : selfForecast.confidence)
                            : Confidence::High);
                    shurelya.expectedValue = defensive
                        ? teamThreat +
                            static_cast<float>(threatenedAllies) * 100.0f
                        : static_cast<float>(nearbyAllies) *
                            std::max(0.25f,
                                closestDistance /
                                    std::max(1.0f, player.moveSpeed) *
                                    0.20f);
                    shurelya.position = defensive && movementTarget
                        ? player.position +
                            Normalize2D(player.position -
                                        movementTarget->position) *
                                100.0f
                        : (movementTarget
                               ? movementTarget->position
                               : player.position);
                    arbiter.Submit(shurelya);
                }
            }
        }

        if (settings.movementItemsEnabled &&
            settings.ModeFor(Capability::Youmuu) != ActionMode::Off &&
            FindItem(player, Capability::Youmuu, registry) != nullptr &&
            !HasMovementBoost(player, now)) {
            const CrowdControlSummary control =
                SummarizeControl(player, registry, now);
            const ChampionState* movementTarget =
                primary ? primary : closest;
            if (control.severity < 4 && movementTarget) {
                const bool disengage =
                    primary &&
                    damage >= player.maxHealth.value * 0.12f &&
                    primary->position.Distance(player.position) <=
                        1500.0f;
                const bool chase =
                    !disengage &&
                    selfForecast.threatCount == 0 &&
                    closest->health.value /
                            std::max(1.0f, closest->maxHealth.value) <=
                        0.55f &&
                    closestDistance >= 400.0f &&
                    closestDistance <= 1800.0f;
                const Point3 route = disengage
                    ? player.position +
                        Normalize2D(player.position -
                                    movementTarget->position) *
                            100.0f
                    : movementTarget->position;
                const float travelDistance = disengage
                    ? std::min(
                          movementTarget->position.Distance(
                              player.position),
                          1800.0f)
                    : std::min(closestDistance, 1800.0f);
                const bool pathSupports =
                    PathSupports(player, route);
                const float speed =
                    std::max(1.0f, player.moveSpeed);
                const float pathConfidence =
                    player.pathBranches > 1 ? 0.75f : 1.0f;
                const float timeGain =
                    (travelDistance / speed -
                     travelDistance / (speed * 1.20f)) *
                    pathConfidence;
                if ((disengage || chase) && pathSupports &&
                    timeGain >= settings.ghostMinimumTimeGain * 0.75f) {
                    ActionRequest youmuu = MakeRequest(
                        player, registry, settings, runtimeMode,
                        Capability::Youmuu,
                        disengage
                            ? ActionPriority::Disengage
                            : ActionPriority::Mobility,
                        now,
                        disengage
                            ? "Youmuu creates meaningful separation from visible danger"
                            : "Youmuu creates meaningful chase time against a low visible target",
                        selfForecast.confidence == Confidence::Unknown
                            ? Confidence::High
                            : selfForecast.confidence);
                    youmuu.expectedValue = timeGain;
                    youmuu.position = route;
                    arbiter.Submit(youmuu);
                }
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
            if (settings.ModeFor(Capability::Gunblade) !=
                    ActionMode::Off &&
                FindItem(
                    player, Capability::Gunblade,
                    registry) != nullptr &&
                !closest->recalling &&
                !closest->teleporting &&
                closestDistance <= gunbladeRange &&
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
            const CrowdControlSummary rocketbeltControl =
                SummarizeControl(player, registry, now);
            if (settings.ModeFor(
                    Capability::Rocketbelt) !=
                    ActionMode::Off &&
                FindItem(
                    player, Capability::Rocketbelt,
                    registry) != nullptr &&
                !HasMovementBoost(player, now) &&
                rocketbeltControl.severity < 4 &&
                !closest->recalling &&
                !closest->teleporting &&
                closestDistance >= 300.0f &&
                closestDistance <= rocketbeltReach &&
                damage < player.health.value * 0.50f &&
                PathSupports(player, closest->position)) {
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

            const float stridebreakerRange =
                ItemRange(registry, Capability::Stridebreaker, 450.0f);
            if (settings.ModeFor(Capability::Stridebreaker) !=
                    ActionMode::Off &&
                FindItem(
                    player, Capability::Stridebreaker,
                    registry) != nullptr &&
                !HasProtectionImmunity(player, now) &&
                SummarizeControl(player, registry, now).severity < 4) {
                int nearbyEnemies = 0;
                store.ForEachChampion(
                    [&](const ChampionState& enemy) {
                        if (CombatValidationService::
                                IsTargetableEnemy(
                                    enemy, player.team) &&
                            !HasProtectionImmunity(enemy, now) &&
                            enemy.position.Distance(
                                player.position) <=
                                stridebreakerRange) {
                            ++nearbyEnemies;
                        }
                    });
                const bool singleTargetControl =
                    nearbyEnemies == 1 &&
                    closestDistance <= stridebreakerRange &&
                    closest->health.value >
                        closest->maxHealth.value * 0.25f;
                if (nearbyEnemies >= 2 || singleTargetControl) {
                    ActionRequest stridebreaker = MakeRequest(
                        player, registry, settings, runtimeMode,
                        Capability::Stridebreaker,
                        nearbyEnemies >= 2
                            ? ActionPriority::Engage
                            : ActionPriority::Disengage,
                        now,
                        nearbyEnemies >= 2
                            ? "multiple visible enemies can be slowed by Stridebreaker"
                            : "a visible target is in Stridebreaker control range",
                        Confidence::High);
                    stridebreaker.position = closest->position;
                    stridebreaker.expectedValue =
                        static_cast<float>(nearbyEnemies) * 100.0f;
                    arbiter.Submit(stridebreaker);
                }
            }
            const float randuinRange =
                ItemRange(registry, Capability::Randuin, 450.0f);
            if (settings.ModeFor(Capability::Randuin) !=
                    ActionMode::Off &&
                FindItem(
                    player, Capability::Randuin,
                    registry) != nullptr &&
                !HasProtectionImmunity(player, now) &&
                SummarizeControl(player, registry, now).severity < 4) {
                const ThreatForecast randuinForecast =
                    CombatPredictionService::Forecast(
                        player, store, now, 1.0f);
                int nearbyEnemies = 0;
                float pressure = 0.0f;
                const ChampionState* peelEnemy = nullptr;
                float peelScore = -1.0f;
                store.ForEachChampion(
                    [&](const ChampionState& enemy) {
                        if (!CombatValidationService::
                                IsTargetableEnemy(
                                    enemy, player.team) ||
                            HasProtectionImmunity(enemy, now)) {
                            return;
                        }
                        const float distance =
                            enemy.position.Distance(player.position);
                        if (distance > randuinRange) {
                            return;
                        }
                        ++nearbyEnemies;
                        pressure +=
                            std::max(0.0f,
                                1.0f - distance /
                                    std::max(1.0f, randuinRange)) *
                            100.0f;
                        const bool attackingPlayer =
                            enemy.networkId ==
                            randuinForecast.primarySource;
                        const float score =
                            (attackingPlayer ? 1000.0f : 0.0f) +
                            std::max(0.0f,
                                randuinRange - distance) +
                            enemy.moveSpeed * 0.05f;
                        if (score > peelScore) {
                            peelScore = score;
                            peelEnemy = &enemy;
                        }
                    });
                const bool imminentPeel =
                    randuinForecast.threatCount > 0 &&
                    randuinForecast.incomingDamage > 0.0f;
                if (peelEnemy &&
                    (nearbyEnemies >= 2 || imminentPeel)) {
                    ActionRequest randuin = MakeRequest(
                        player, registry, settings, runtimeMode,
                        Capability::Randuin,
                        ActionPriority::Disengage, now,
                        nearbyEnemies >= 2
                            ? "multiple visible enemies are inside Randuin peel range"
                            : "an observed threat makes Randuin peel valuable",
                        imminentPeel
                            ? randuinForecast.confidence
                            : Confidence::High);
                    randuin.targetId = peelEnemy->networkId;
                    randuin.position = player.position;
                    randuin.expectedValue =
                        pressure +
                        randuinForecast.incomingDamage * 0.50f +
                        (randuinForecast.hardCc ? 75.0f : 0.0f);
                    arbiter.Submit(randuin);
                }
            }


            const float titanicRange =
                ItemRange(registry, Capability::TitanicHydra, 450.0f);
            if (settings.ModeFor(Capability::TitanicHydra) !=
                    ActionMode::Off &&
                FindItem(
                    player, Capability::TitanicHydra,
                    registry) != nullptr &&
                !HasProtectionImmunity(player, now) &&
                !HasActiveBuff(player, "titanic", now) &&
                SummarizeControl(player, registry, now).severity < 4 &&
                closestDistance <= titanicRange &&
                !HasProtectionImmunity(*closest, now)) {
                bool recentAttack = false;
                store.Damage().ForEach(
                    [&](const DamageRecord& record) {
                        if (!recentAttack &&
                            record.sourceId == player.networkId &&
                            record.targetId == closest->networkId &&
                            record.amount > 0.0f &&
                            record.at <= now &&
                            record.at >= now - 0.35f) {
                            recentAttack = true;
                        }
                    });
                if (recentAttack) {
                    ActionRequest titanic = MakeRequest(
                        player, registry, settings, runtimeMode,
                        Capability::TitanicHydra,
                        ActionPriority::Engage, now,
                        "a recent visible basic attack can be woven with Titanic Hydra",
                        Confidence::Confirmed);
                    titanic.targetId = closest->networkId;
                    titanic.position = closest->position;
                    titanic.expectedValue =
                        std::max(25.0f,
                            closest->maxHealth.value * 0.04f);
                    arbiter.Submit(titanic);
                }
            }

            const float ravenousRange =
                ItemRange(registry, Capability::RavenousHydra, 450.0f);
            if (settings.ModeFor(Capability::RavenousHydra) !=
                    ActionMode::Off &&
                FindItem(
                    player, Capability::RavenousHydra,
                    registry) != nullptr &&
                !HasProtectionImmunity(player, now) &&
                SummarizeControl(player, registry, now).severity < 4) {
                int nearbyEnemies = 0;
                store.ForEachChampion(
                    [&](const ChampionState& enemy) {
                        if (CombatValidationService::
                                IsTargetableEnemy(
                                    enemy, player.team) &&
                            !HasProtectionImmunity(enemy, now) &&
                            enemy.position.Distance(
                                player.position) <=
                                ravenousRange) {
                            ++nearbyEnemies;
                        }
                    });
                if (nearbyEnemies >= 2) {
                    const float missingHealth =
                        std::max(0.0f,
                            player.maxHealth.value -
                                player.health.value);
                    ActionRequest hydra = MakeRequest(
                        player, registry, settings, runtimeMode,
                        Capability::RavenousHydra,
                        ActionPriority::Engage, now,
                        "multiple visible targets justify Ravenous Hydra damage and healing",
                        Confidence::High);
                    hydra.position = closest->position;
                    hydra.expectedValue =
                        static_cast<float>(nearbyEnemies) * 30.0f +
                        missingHealth * 0.10f;
                    arbiter.Submit(hydra);
                }
            }

            const float profaneRange =
                ItemRange(registry, Capability::ProfaneHydra, 400.0f);
            const ItemDefinition* profaneDefinition =
                registry.FindAvailableItem(
                    Capability::ProfaneHydra);
            const float profaneDamage =
                profaneDefinition &&
                        profaneDefinition->effectValues[0] > 0.0f
                    ? profaneDefinition->effectValues[0]
                    : 100.0f;
            const ThreatForecast profaneForecast =
                CombatPredictionService::Forecast(
                    *closest, store, now, 1.0f);
            const float profaneCommittedDamage =
                EffectiveThreatDamage(
                    profaneForecast,
                    settings.includeDamageOverTime);
            const float profaneHealth =
                closest->health.value +
                std::max(0.0f, closest->allShield);
            const float profaneProjectedHealth =
                profaneHealth - profaneCommittedDamage;
            if (settings.ModeFor(Capability::ProfaneHydra) !=
                    ActionMode::Off &&
                FindItem(
                    player, Capability::ProfaneHydra,
                    registry) != nullptr &&
                !HasProtectionImmunity(player, now) &&
                SummarizeControl(player, registry, now).severity < 4 &&
                closestDistance <= profaneRange &&
                profaneCommittedDamage < profaneHealth &&
                profaneProjectedHealth > 0.0f &&
                profaneProjectedHealth <= profaneDamage) {
                ActionRequest profane = MakeRequest(
                    player, registry, settings, runtimeMode,
                    Capability::ProfaneHydra,
                    ActionPriority::Execute, now,
                    "visible target remains inside Profane Hydra execute damage",
                    profaneForecast.confidence == Confidence::Unknown
                        ? Confidence::High
                        : profaneForecast.confidence);
                profane.targetId = closest->networkId;
                profane.position = closest->position;
                profane.expectedValue =
                    profaneDamage - profaneProjectedHealth;
                arbiter.Submit(profane);
            }

            const std::array<Capability, 1> cleaves = {
                Capability::Tiamat
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
                    (capability == Capability::Tiamat &&
                     count < 2) ||
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

            constexpr float damageHistoryWindow = 1.25f;
            constexpr float reactionWindow = 0.12f;
            float recentDamage = 0.0f;
            store.Damage().ForEach(
                [&](const DamageRecord& record) {
                    if (record.targetId ==
                            objective.networkId &&
                        record.at <= now &&
                        record.at >=
                            now - damageHistoryWindow &&
                        record.amount > 0.0f &&
                        static_cast<int>(
                            record.evidence.confidence) >=
                            static_cast<int>(
                                Confidence::High)) {
                        recentDamage += record.amount;
                    }
                });
            const float damagePerSecond =
                recentDamage / damageHistoryWindow;
            const float projectedHealth =
                std::max(
                    0.0f,
                    objective.health -
                        damagePerSecond * reactionWindow);
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
            forecast, settings.includeDamageOverTime);
        const ObservedItem* potionItem =
            FindItem(
                player, Capability::Potion,
                registry);
        if (settings.potionEnabled &&
            settings.ModeFor(Capability::Potion) !=
                ActionMode::Off &&
            potionItem != nullptr &&
            !player.recalling &&
            !player.channeling &&
            healthRatio < 0.65f &&
            healthRatio > 0.15f &&
            damage < player.health.value +
                std::max(0.0f, player.allShield) &&
            !HasPotionBuff(player, now) &&
            !HasProtectionImmunity(player, now)) {
            ActionRequest potion = MakeRequest(
                player, registry, settings, runtimeMode,
                Capability::Potion,
                ActionPriority::Utility, now,
                "usable potion can restore meaningful missing health safely",
                Confidence::High);
            potion.position = player.position;
            potion.expectedValue =
                (1.0f - healthRatio) * 100.0f +
                (potionItem->maxCharges > 0
                    ? static_cast<float>(
                          potionItem->charges)
                    : 1.0f);
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
            settings.ModeFor(Capability::Tiamat) != ActionMode::Off &&
            FindItem(
                player, Capability::Tiamat,
                registry) != nullptr &&
            forecast.threatCount == 0 &&
            !HasProtectionImmunity(player, now)) {
            const WaveState& wave = store.Wave();
            const float tiamatRange =
                ItemRange(registry, Capability::Tiamat, 450.0f);
            bool enemyNear = false;
            store.ForEachChampion(
                [&](const ChampionState& enemy) {
                    if (!enemyNear &&
                        CombatValidationService::
                            IsTargetableEnemy(
                                enemy, player.team) &&
                        enemy.position.Distance(
                            player.position) <=
                            std::max(tiamatRange, 650.0f)) {
                        enemyNear = true;
                    }
                });
            const bool waveFresh =
                wave.evidence.IsKnown() &&
                !wave.evidence.IsExpired(now) &&
                !wave.center.IsZero();
            const bool waveClear =
                waveFresh &&
                wave.enemyMinions >= 3 &&
                wave.enemyHealth > 0.0f &&
                wave.center.Distance(player.position) <=
                    tiamatRange + 150.0f &&
                !Contains(wave.classification, "freeze");
            if (!enemyNear && waveClear) {
                ActionRequest tiamat = MakeRequest(
                    player, registry, settings, runtimeMode,
                    Capability::Tiamat,
                    ActionPriority::WaveClear, now,
                    "fresh visible enemy wave can be cleared outside combat",
                    wave.evidence.confidence);
                tiamat.position = wave.center;
                tiamat.expectedValue =
                    static_cast<float>(wave.enemyMinions) * 25.0f +
                    std::max(0.0f, wave.enemyHealth) * 0.05f;
                arbiter.Submit(tiamat);
            }
        }


        const ObservedItem* actualizerItem =
            FindItem(
                player, Capability::Actualizer,
                registry);
        if (settings.offensiveItemsEnabled &&
            settings.ModeFor(Capability::Actualizer) !=
                ActionMode::Off &&
            actualizerItem != nullptr &&
            player.maxMana.value > 0.0f &&
            !player.recalling &&
            !player.channeling &&
            !HasProtectionImmunity(player, now) &&
            !HasActiveBuff(player, "actualizer", now)) {
            float plannedMana = 0.0f;
            int readyCombatSpells = 0;
            for (std::size_t i = 0;
                 i < player.spellCount; ++i) {
                const ObservedSpell& spell =
                    player.spells[i];
                if (spell.slot >= 0 && spell.slot <= 3 &&
                    spell.ready &&
                    spell.cooldownRemaining <= 0.01f) {
                    plannedMana +=
                        std::max(0.0f, spell.manaCost);
                    ++readyCombatSpells;
                }
            }
            int visibleEnemies = 0;
            bool lowHealthAlly = false;
            store.ForEachChampion(
                [&](const ChampionState& unit) {
                    const float distance =
                        unit.position.Distance(player.position);
                    if (distance > 1200.0f || unit.dead ||
                        !unit.visible) {
                        return;
                    }
                    if (CombatValidationService::
                            IsTargetableEnemy(
                                unit, player.team)) {
                        ++visibleEnemies;
                    } else if (unit.ally && !unit.local &&
                               unit.maxHealth.value > 0.0f &&
                               unit.health.value /
                                       unit.maxHealth.value <
                                   0.75f) {
                        lowHealthAlly = true;
                    }
                });
            const float windowMana =
                std::max(plannedMana * 1.5f,
                    player.maxMana.value * 0.30f);
            const float reserveMana = std::max(
                plannedMana * 0.25f,
                player.maxMana.value * 0.20f);
            const bool enoughMana =
                player.mana.value >= windowMana + reserveMana;
            const bool hasCombatValue =
                visibleEnemies > 0 || lowHealthAlly;
            if (hasCombatValue &&
                readyCombatSpells >= 2 &&
                enoughMana) {
                ActionRequest actualizer = MakeRequest(
                    player, registry, settings, runtimeMode,
                    Capability::Actualizer,
                    ActionPriority::Engage, now,
                    "usable Actualizer can amplify a sustained eight-second combat window",
                    Confidence::High);
                actualizer.position = player.position;
                actualizer.expectedValue =
                    static_cast<float>(readyCombatSpells) *
                        30.0f +
                    static_cast<float>(visibleEnemies) *
                        20.0f +
                    (lowHealthAlly ? 25.0f : 0.0f) +
                    player.mana.value -
                    (windowMana + reserveMana);
                arbiter.Submit(actualizer);
            }
        }

        if (settings.visionItemsEnabled &&
            !player.recalling &&
            !player.channeling) {
            const bool canOracle =
                settings.ModeFor(Capability::Oracle) !=
                    ActionMode::Off &&
                FindItem(
                    player, Capability::Oracle,
                    registry) != nullptr;
            const bool canWard =
                settings.ModeFor(Capability::Ward) !=
                    ActionMode::Off &&
                FindItem(
                    player, Capability::Ward,
                    registry) != nullptr;
            const bool canFarsight =
                settings.ModeFor(Capability::Farsight) !=
                    ActionMode::Off &&
                FindItem(
                    player, Capability::Farsight,
                    registry) != nullptr;
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
                    if (ward.ally && ward.visible &&
                        distance <= 900.0f) {
                        ++nearbyFriendlyWards;
                    }
                });
            if (canOracle && nearbyEnemyWards > 0) {
                ActionRequest oracle = MakeRequest(
                    player, registry, settings, runtimeMode,
                    Capability::Oracle,
                    ActionPriority::Utility, now,
                    "observed enemy vision is inside Oracle Lens range",
                    Confidence::Confirmed);
                oracle.mode = ActionMode::Suggest;
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
                if (canWard) {
                    ActionRequest vision = MakeRequest(
                        player, registry, settings, runtimeMode,
                        Capability::Ward,
                        ActionPriority::Utility, now,
                        "no observed friendly vision covers the current route",
                        Confidence::High);
                    vision.mode = ActionMode::Suggest;
                    vision.position =
                        player.position +
                        direction * std::min(wardRange, 600.0f);
                    vision.expectedValue = 2.0f;
                    arbiter.Submit(vision);
                } else if (canFarsight) {
                    const float farsightRange =
                        ItemRange(
                            registry,
                            Capability::Farsight, 4000.0f);
                    ActionRequest farsight = MakeRequest(
                        player, registry, settings, runtimeMode,
                        Capability::Farsight,
                        ActionPriority::Utility, now,
                        "Farsight can scout the uncovered observed route",
                        Confidence::High);
                    farsight.mode = ActionMode::Suggest;
                    farsight.position =
                        player.position +
                        direction *
                            std::min(farsightRange, 3500.0f);
                    farsight.expectedValue = 1.0f;
                    arbiter.Submit(farsight);
                }
            }
        }


    }
    ThreatForecast selfForecast_{};
    bool hasSelfForecast_ = false;
    std::uint32_t knightsVowTargetId_ = 0;
};

} // namespace NightSharp::Companion
