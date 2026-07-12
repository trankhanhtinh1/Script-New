#pragma once

#include "EvadeHelper.h"
#include "EvadeSettings.h"
#include "EvadeSpellData.h"
#include "EvadeSpellDatabase.h"
#include "EvadeUtils.h"

#include "../../../Core/CoreControl.h"
#include "../../../SDK/SDK.h"

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <cmath>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

namespace Plugins::KuroEvade {

struct EvadeSpellConfig {
    bool Enabled = true;
    int DangerLevel = 0;
    int Mode = 2;
};

struct EvadeSpell final {
    using SkillshotList = EvadeHelper::SkillshotList;
    using ConfigResolver = std::function<EvadeSpellConfig(const EvadeSpellData&)>;

    static std::string MenuKey(const EvadeSpellData& data) {
        std::string key = data.ChampionName + "|" + data.Name + "|" + std::to_string(data.ItemId);
        std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return key;
    }

    static bool TryUseBest(const EvadeSettings& settings,
                           const SDK::AIHeroClient& player,
                           const Vec2& bestPosition,
                           bool hasSafeMove,
                           int currentDanger,
                           float lowestHitTime,
                           const SkillshotList& skillshots,
                           const ConfigResolver& configResolver,
                           char* lastEvent,
                           std::size_t lastEventSize) {
        if (!settings.UseEvadeSpells || !player.IsValid()) {
            return false;
        }
        if (settings.CalculateWindupDelay && settings.CurrentWindupDelay > 0.0f) {
            return false;
        }

        const int now = SDK::Variables::TickCount();
        if (now - LastCastTick() < 250) {
            return false;
        }

        const std::string championName = EvadeUtils::GetObjectCharacterName(player);
        auto spells = EvadeSpellDatabase::ForChampion(championName.c_str(), true);
        if (spells.empty()) {
            return false;
        }

        std::stable_sort(spells.begin(), spells.end(), [&](const EvadeSpellData* lhs, const EvadeSpellData* rhs) {
            const auto left = ResolveConfig(*lhs, configResolver);
            const auto right = ResolveConfig(*rhs, configResolver);
            const int leftDanger = left.DangerLevel > 0 ? left.DangerLevel : lhs->DangerLevel;
            const int rightDanger = right.DangerLevel > 0 ? right.DangerLevel : rhs->DangerLevel;
            if (leftDanger != rightDanger) {
                return leftDanger < rightDanger;
            }
            return lhs->Name < rhs->Name;
        });

        currentDanger = std::max(1, currentDanger);
        for (const EvadeSpellData* spellPtr : spells) {
            if (!spellPtr) {
                continue;
            }

            const EvadeSpellData& data = *spellPtr;
            const EvadeSpellConfig config = ResolveConfig(data, configResolver);
            const int configuredDanger = config.DangerLevel > 0 ? config.DangerLevel : data.DangerLevel;
            if (!config.Enabled || !data.IsEnabled || configuredDanger > currentDanger) {
                continue;
            }
            if (!ShouldUseForMode(config.Mode, hasSafeMove, lowestHitTime, settings, data)) {
                continue;
            }

            const SDK::SpellSlot slot = ResolveSlot(player, data);
            if (!IsReady(player, data, slot)) {
                continue;
            }
            if (!MatchesCheckSpellName(player, data, slot)) {
                continue;
            }

            bool casted = false;
            if (data.IsSpecial) {
                casted = TrySpecial(data, slot, settings, player, bestPosition, skillshots);
            }
            if (!casted) {
                casted = TryGeneric(data, slot, settings, player, bestPosition, skillshots);
            }

            if (casted) {
                LastCastTick() = now;
                SetLastEvent(lastEvent, lastEventSize, data.Name.empty() ? "evade spell" : data.Name.c_str());
                return true;
            }
        }

        return false;
    }

private:
    static int& LastCastTick() {
        static int tick = 0;
        return tick;
    }

    static EvadeSpellConfig ResolveConfig(const EvadeSpellData& data,
                                          const ConfigResolver& resolver) {
        EvadeSpellConfig config;
        config.Enabled = data.IsEnabled;
        config.DangerLevel = data.DangerLevel;
        config.Mode = data.IndexUseWhen;
        if (resolver) {
            const EvadeSpellConfig resolved = resolver(data);
            config.Enabled = resolved.Enabled;
            config.DangerLevel = resolved.DangerLevel > 0 ? resolved.DangerLevel : config.DangerLevel;
            config.Mode = std::clamp(resolved.Mode, 0, 2);
        }
        return config;
    }

    static bool ShouldUseForMode(int mode,
                                 bool hasSafeMove,
                                 float lowestHitTime,
                                 const EvadeSettings& settings,
                                 const EvadeSpellData& data) {
        mode = std::clamp(mode, 0, 2);
        if (mode == 0 && hasSafeMove && !settings.PreferEvadeSpells) {
            return false;
        }
        if (mode == 1 && lowestHitTime != FLT_MAX) {
            const float activation = static_cast<float>(settings.SpellActivationTime) +
                settings.ExtraDelay + static_cast<float>(SDK::Game::Ping()) +
                static_cast<float>(std::max(0, data.Delay));
            if (lowestHitTime > activation) {
                return false;
            }
        }
        return true;
    }

    static bool SameText(const std::string& lhs, const char* rhs) {
        return rhs && rhs[0] && !lhs.empty() && _stricmp(lhs.c_str(), rhs) == 0;
    }

    static SDK::SpellSlot ResolveSlot(const SDK::AIHeroClient& player,
                                      const EvadeSpellData& data) {
        if (data.IsSummonerSpell && !data.CheckSpellName.empty()) {
            const SDK::SpellSlot slots[] = { SDK::SpellSlot::Summoner1, SDK::SpellSlot::Summoner2 };
            for (SDK::SpellSlot slot : slots) {
                const auto spell = player.GetSpell(slot);
                if (spell.IsValid() &&
                    (SameText(spell.Name(), data.CheckSpellName.c_str()) ||
                     SameText(spell.ScriptName(), data.CheckSpellName.c_str()) ||
                     SameText(spell.IconName(), data.CheckSpellName.c_str()))) {
                    return slot;
                }
            }
            return SDK::SpellSlot::Unknown;
        }
        return data.Slot;
    }

    static bool IsReady(const SDK::AIHeroClient& player,
                        const EvadeSpellData& data,
                        SDK::SpellSlot slot) {
        if (data.IsItem) {
            return data.ItemId > 0 && SDK::CanUseItem(player, data.ItemId);
        }
        if (slot == SDK::SpellSlot::Unknown) {
            return false;
        }

        const auto spell = player.GetSpell(slot);
        if (!spell.IsValid() || !spell.Learned()) {
            return false;
        }
        return player.Spellbook().CanUseSpell(slot) == SDK::CoreSpellBook::State_Ready;
    }

    static bool IsSlotReady(const SDK::AIHeroClient& player, SDK::SpellSlot slot) {
        if (slot == SDK::SpellSlot::Unknown) {
            return false;
        }
        const auto spell = player.GetSpell(slot);
        return spell.IsValid() && spell.Learned() &&
            player.Spellbook().CanUseSpell(slot) == SDK::CoreSpellBook::State_Ready;
    }

    static bool MatchesCheckSpellName(const SDK::AIHeroClient& player,
                                      const EvadeSpellData& data,
                                      SDK::SpellSlot slot) {
        if (data.CheckSpellName.empty() || data.IsItem) {
            return true;
        }
        if (slot == SDK::SpellSlot::Unknown) {
            return false;
        }
        const auto spell = player.GetSpell(slot);
        return SameText(spell.Name(), data.CheckSpellName.c_str()) ||
               SameText(spell.ScriptName(), data.CheckSpellName.c_str()) ||
               SameText(spell.IconName(), data.CheckSpellName.c_str());
    }

    static bool TryGeneric(const EvadeSpellData& data,
                           SDK::SpellSlot slot,
                           const EvadeSettings& settings,
                           const SDK::AIHeroClient& player,
                           const Vec2& bestPosition,
                           const SkillshotList& skillshots) {
        if (data.EvadeTypeValue == EvadeType::WindWall) {
            return CastWindWall(data, slot, player, skillshots);
        }

        if (data.IsItem) {
            if (data.EvadeTypeValue == EvadeType::MovementSpeedBuff) {
                if (SDK::UseItem(player, data.ItemId)) {
                    MoveToBest(player, bestPosition);
                    return true;
                }
                return false;
            }
            return SDK::UseItem(player, data.ItemId);
        }

        switch (data.CastTypeValue) {
        case CastType::Self:
            if (player.Spellbook().CastSpell(slot)) {
                if (data.EvadeTypeValue == EvadeType::MovementSpeedBuff) {
                    MoveToBest(player, bestPosition);
                }
                return true;
            }
            return false;
        case CastType::Target:
            return CastTarget(data, slot, settings, player, bestPosition, skillshots);
        case CastType::Position:
        default:
            if (CastPosition(data, slot, player, bestPosition)) {
                if (data.EvadeTypeValue == EvadeType::MovementSpeedBuff) {
                    MoveToBest(player, bestPosition);
                }
                return true;
            }
            return false;
        }
    }

    static bool TrySpecial(const EvadeSpellData& data,
                           SDK::SpellSlot slot,
                           const EvadeSettings& settings,
                           const SDK::AIHeroClient& player,
                           const Vec2& bestPosition,
                           const SkillshotList& skillshots) {
        if (_stricmp(data.Name.c_str(), "Ekko E Attack") == 0) {
            if (!player.HasBuff("ekkoeattackbuff")) {
                return false;
            }
            return CastTarget(data, slot, settings, player, bestPosition, skillshots);
        }

        if (_stricmp(data.Name.c_str(), "Ekko R") == 0) {
            for (const auto& minion : SDK::GameObjects::AllyMinions()) {
                if (!minion.IsValid() || minion.IsDead() || _stricmp(EvadeUtils::GetObjectName(minion).c_str(), "Ekko") != 0) {
                    continue;
                }
                if (IsSafeAt(settings, minion.ServerPosition().To2D(), player.BoundingRadius(), skillshots)) {
                    return player.Spellbook().CastSpell(slot);
                }
            }
            return false;
        }

        if (_stricmp(data.Name.c_str(), "Elise E") == 0) {
            if (SameText(player.GetSpell(SDK::SpellSlot::E).Name(), "EliseSpiderEInitial")) {
                return player.Spellbook().CastSpell(slot, player.Address());
            }
            if (IsSlotReady(player, SDK::SpellSlot::R)) {
                player.Spellbook().CastSpell(SDK::SpellSlot::R);
            }
            return false;
        }

        if (_stricmp(data.Name.c_str(), "Katarina E") == 0) {
            SDK::AIBaseClient target;
            Vec2 landing;
            if (!FindTarget(data, settings, player, bestPosition, skillshots, target, landing)) {
                return false;
            }
            return player.Spellbook().CastSpell(slot, Vec3::From2D(target.ServerPosition().To2D(), player.ServerPosition().y));
        }

        if (_stricmp(data.Name.c_str(), "Nidalee W") == 0) {
            if (SameText(player.GetSpell(SDK::SpellSlot::W).Name(), "Pounce")) {
                return CastPosition(data, slot, player, bestPosition);
            }
            if (IsSlotReady(player, SDK::SpellSlot::R)) {
                player.Spellbook().CastSpell(SDK::SpellSlot::R);
            }
            return false;
        }

        if (_stricmp(data.Name.c_str(), "Riven Q") == 0) {
            MoveToBest(player, bestPosition);
            return CastPosition(data, slot, player, bestPosition);
        }

        if (_stricmp(data.Name.c_str(), "Yuumi E") == 0) {
            if (SameText(player.GetSpell(SDK::SpellSlot::W).Name(), "YuumiW") &&
                player.Spellbook().CastSpell(slot)) {
                MoveToBest(player, bestPosition);
                return true;
            }
            return false;
        }

        return false;
    }

    static bool CastPosition(const EvadeSpellData& data,
                             SDK::SpellSlot slot,
                             const SDK::AIHeroClient& player,
                             const Vec2& bestPosition) {
        const Vec2 heroPos = player.ServerPosition().To2D();
        Vec2 direction = (bestPosition - heroPos).Normalized();
        if (direction.IsZero()) {
            direction = SDK::Game::CursorPos().To2D() - heroPos;
            direction = direction.Normalized();
        }
        if (direction.IsZero()) {
            direction = player.Direction().To2D().Normalized();
        }
        if (direction.IsZero()) {
            return false;
        }

        const auto spell = player.GetSpell(slot);
        float range = data.MaxRange > 0 ? static_cast<float>(data.MaxRange) : spell.CastRange();
        if (range <= 0.0f) {
            range = std::max(50.0f, heroPos.Distance(bestPosition));
        }

        float distance = data.FixedRange ? range : std::min(range, std::max(0.0f, heroPos.Distance(bestPosition)));
        if (data.MinRange > 0) {
            distance = std::max(distance, static_cast<float>(data.MinRange));
        }
        if (distance <= 1.0f) {
            distance = range;
        }

        const Vec2 castPos = data.IsReversed
            ? heroPos - direction * distance
            : heroPos + direction * distance;
        return player.Spellbook().CastSpell(slot, Vec3::From2D(castPos, player.ServerPosition().y));
    }

    static bool CastWindWall(const EvadeSpellData& data,
                             SDK::SpellSlot slot,
                             const SDK::AIHeroClient& player,
                             const SkillshotList& skillshots) {
        if (data.CastTypeValue != CastType::Position) {
            return false;
        }

        const Vec2 heroPos = player.ServerPosition().To2D();
        const SDK::Skillshot* best = nullptr;
        float bestHitTime = FLT_MAX;
        for (const auto& skillshot : skillshots) {
            if (!skillshot || !skillshot->HasMissile()) {
                continue;
            }
            if (!EvadeHelper::InSkillShot(*skillshot, heroPos, player.BoundingRadius() + 60.0f)) {
                continue;
            }
            const float hitTime = EvadeHelper::SpellHitTime(*skillshot, heroPos);
            if (hitTime < bestHitTime) {
                bestHitTime = hitTime;
                best = skillshot.get();
            }
        }
        if (!best) {
            return false;
        }

        Vec2 direction = (best->StartPosition - heroPos).Normalized();
        if (direction.IsZero()) {
            direction = (best->EndPosition - heroPos).Normalized();
        }
        if (direction.IsZero()) {
            return false;
        }

        const Vec2 castPos = heroPos + direction * 100.0f;
        return player.Spellbook().CastSpell(slot, Vec3::From2D(castPos, player.ServerPosition().y));
    }

    static bool CastTarget(const EvadeSpellData& data,
                           SDK::SpellSlot slot,
                           const EvadeSettings& settings,
                           const SDK::AIHeroClient& player,
                           const Vec2& bestPosition,
                           const SkillshotList& skillshots) {
        SDK::AIBaseClient target;
        Vec2 landing;
        if (!FindTarget(data, settings, player, bestPosition, skillshots, target, landing)) {
            return false;
        }
        return player.Spellbook().CastSpell(slot, target.Address());
    }

    static bool FindTarget(const EvadeSpellData& data,
                           const EvadeSettings& settings,
                           const SDK::AIHeroClient& player,
                           const Vec2& bestPosition,
                           const SkillshotList& skillshots,
                           SDK::AIBaseClient& outTarget,
                           Vec2& outLanding) {
        std::vector<SDK::AIBaseClient> candidates;
        const auto addCandidate = [&](const SDK::AIBaseClient& unit) {
            if (!unit.IsValid() || unit.NetworkId() == player.NetworkId() || unit.IsDead()) {
                return;
            }
            if (unit.IsHero() && (!unit.IsVisible() || !unit.IsTargetable())) {
                return;
            }

            float range = static_cast<float>(data.MaxRange);
            if (range <= 0.0f) {
                range = player.GetSpell(data.Slot).CastRange();
            }
            if (range <= 0.0f) {
                range = 700.0f;
            }
            const float dist = player.ServerPosition().To2D().Distance(unit.ServerPosition().To2D());
            if (dist > range + unit.BoundingRadius() + 75.0f) {
                return;
            }
            candidates.push_back(unit);
        };

        for (SpellTargets targetType : data.ValidTargets) {
            switch (targetType) {
            case SpellTargets::AllyChampions:
                for (const auto& unit : SDK::GameObjects::AllyHeroes()) {
                    addCandidate(SDK::AIBaseClient(unit.Handle()));
                }
                break;
            case SpellTargets::EnemyChampions:
                for (const auto& unit : SDK::GameObjects::EnemyHeroes()) {
                    addCandidate(SDK::AIBaseClient(unit.Handle()));
                }
                break;
            case SpellTargets::AllyMinions:
                for (const auto& unit : SDK::GameObjects::AllyMinions()) {
                    addCandidate(SDK::AIBaseClient(unit.Handle()));
                }
                break;
            case SpellTargets::EnemyMinions:
                for (const auto& unit : SDK::GameObjects::EnemyMinions()) {
                    addCandidate(SDK::AIBaseClient(unit.Handle()));
                }
                break;
            }
        }

        float bestScore = FLT_MAX;
        const Vec2 heroPos = player.ServerPosition().To2D();
        for (const auto& unit : candidates) {
            const Vec2 landing = LandingPosition(data, player, unit);
            if (!IsSafeAt(settings, landing, player.BoundingRadius(), skillshots)) {
                continue;
            }

            float score = landing.Distance(bestPosition);
            if (settings.PreventEnemy) {
                for (const auto& enemy : SDK::GameObjects::EnemyHeroes()) {
                    if (enemy.IsValid() && enemy.NetworkId() != unit.NetworkId()) {
                        const float enemyDistance = enemy.ServerPosition().To2D().Distance(landing);
                        if (enemyDistance < settings.MinComfortZone) {
                            score += settings.MinComfortZone - enemyDistance;
                        }
                    }
                }
            }
            score += heroPos.Distance(unit.ServerPosition().To2D()) * 0.05f;
            if (score < bestScore) {
                bestScore = score;
                outTarget = unit;
                outLanding = landing;
            }
        }

        return outTarget.IsValid();
    }

    static Vec2 LandingPosition(const EvadeSpellData& data,
                                const SDK::AIHeroClient& player,
                                const SDK::AIBaseClient& target) {
        const Vec2 playerPos = player.ServerPosition().To2D();
        const Vec2 targetPos = target.ServerPosition().To2D();
        Vec2 direction = (targetPos - playerPos).Normalized();
        if (direction.IsZero()) {
            direction = player.Direction().To2D().Normalized();
        }
        const float offset = std::max(75.0f, target.BoundingRadius() + player.BoundingRadius() * 0.5f);
        if (data.IsBehindTarget) {
            return targetPos + direction * offset;
        }
        if (data.IsInfrontTarget) {
            return targetPos - direction * offset;
        }
        return targetPos;
    }

    static bool IsSafeAt(const EvadeSettings& settings,
                         const Vec2& position,
                         float radius,
                         const SkillshotList& skillshots) {
        EvadeHelper helper(settings);
        for (const auto& skillshot : skillshots) {
            if (!skillshot || !helper.ShouldConsiderSpell(*skillshot)) {
                continue;
            }
            if (EvadeHelper::InSkillShot(*skillshot, position, radius + settings.ExtraSpellRadius)) {
                return false;
            }
        }
        return true;
    }

    static void MoveToBest(const SDK::AIHeroClient& player, const Vec2& bestPosition) {
        if (bestPosition.IsZero()) {
            return;
        }
        CoreControl::IssueMove(Vec3::From2D(bestPosition, player.ServerPosition().y), true);
    }

    static void SetLastEvent(char* lastEvent, std::size_t lastEventSize, const char* text) {
        if (!lastEvent || lastEventSize == 0) {
            return;
        }
        strncpy_s(lastEvent, lastEventSize, text ? text : "", _TRUNCATE);
    }
};

} // namespace Plugins::KuroEvade
