#pragma once

// Evade-spell execution for the replacement engine.  Selection follows the
// supplied Program.TryToEvade order (walking, shields/walls, movement buffs,
// dashes/blinks), while the retained KuroEvade database supplies champion and
// item metadata.

#include "Evader.h"
#include "../Database/EvadeSpellData.h"
#include "../Database/EvadeSpellDatabase.h"
#include "../Helpers/Utils.h"

#include "../../../../Core/CoreControl.h"
#include "../../../../SDK/SDK.h"

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <climits>
#include <cstddef>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

namespace Plugins::KuroEvade {

struct EvadeSpellConfig {
    bool Enabled = true;
    int DangerLevel = 0;
    bool WardJump = true;
};

struct EvadeSpellUseResult {
    bool Used = false;
    bool MovementSpeed = false;
    bool Displacement = false;
    int BaselineThreats = 0;
    int RemainingThreats = INT_MAX;
    int AvoidedThreats = 0;
};

class SourceEvadeSpell final {
public:
    using ConfigResolver =
        std::function<EvadeSpellConfig(const Database::EvadeSpellData&)>;
    using AllyShieldResolver =
        std::function<bool(const SDK::AIBaseClient&)>;

    static std::string MenuKey(const Database::EvadeSpellData& data) {
        const char* championName = data.IsGlobal
            ? "AllChampions"
            : SDK::ChampionName(data.ChampionId);
        std::string result = std::string(championName) + "|" + data.Name + "|" +
            std::to_string(data.ItemId);
        std::transform(result.begin(), result.end(), result.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return result;
    }

    static EvadeSpellUseResult TryUseBest(
            const EvadeSettings& settings,
            const SDK::AIHeroClient& player,
            const Vec2& bestPosition,
            int baselineThreats,
            int currentDanger,
            float lowestHitTime,
            const SourceSkillshotList& incomingThreats,
            const SourceSkillshotList& skillshots,
            const ConfigResolver& resolver,
            const AllyShieldResolver& allyShieldResolver,
            char* lastEvent,
            std::size_t lastEventSize) {
        EvadeSpellUseResult result;
        result.BaselineThreats = std::max(0, baselineThreats);
        if (!player.IsValid() || baselineThreats <= 0) {
            return result;
        }
        const int now = SDK::Variables::TickCount();
        if (now - LastCastTick() < 250) {
            return result;
        }

        const std::string playerName =
            EvadeUtils::GetObjectCharacterName(player);
        const SDK::ChampionId playerChampionId =
            SDK::ChampionIdFromName(playerName.c_str());
        auto spells = Database::EvadeSpellDatabase::ForChampion(
            playerChampionId, true);

        struct RankedCandidate {
            const Database::EvadeSpellData* Data = nullptr;
            SDK::SpellSlot Slot = SDK::SpellSlot::Unknown;
            int RemainingThreats = INT_MAX;
            int RequiredDanger = 0;
            bool WardJump = true;
        };
        std::vector<RankedCandidate> candidates;
        candidates.reserve(spells.size());

        currentDanger = std::max(1, currentDanger);
        for (const Database::EvadeSpellData* data : spells) {
            if (!data) {
                continue;
            }
            const EvadeSpellConfig config = Resolve(*data, resolver);
            const int danger = config.DangerLevel > 0
                ? config.DangerLevel
                : data->DangerLevel;
            if (!config.Enabled || !data->IsEnabled || danger > currentDanger) {
                continue;
            }

            const SDK::SpellSlot slot = ResolveSlot(player, *data);
            if (!IsReady(player, *data, slot) ||
                !MatchesSpellName(player, *data, slot)) {
                continue;
            }

            const int remaining = EstimateRemainingThreats(
                *data, slot, settings, player, bestPosition,
                baselineThreats, lowestHitTime, incomingThreats,
                skillshots, config.WardJump, allyShieldResolver);
            if (remaining >= baselineThreats) {
                continue;
            }
            candidates.push_back(
                { data, slot, std::max(0, remaining), danger,
                  config.WardJump });
        }

        std::stable_sort(candidates.begin(), candidates.end(),
            [](const RankedCandidate& lhs, const RankedCandidate& rhs) {
                if (lhs.RemainingThreats != rhs.RemainingThreats) {
                    return lhs.RemainingThreats < rhs.RemainingThreats;
                }
                if (lhs.RequiredDanger != rhs.RequiredDanger) {
                    return lhs.RequiredDanger < rhs.RequiredDanger;
                }
                return lhs.Data->Name < rhs.Data->Name;
            });

        for (const RankedCandidate& candidate : candidates) {
            const Database::EvadeSpellData& data = *candidate.Data;
            if (Cast(data, candidate.Slot, settings, player, bestPosition,
                     incomingThreats, skillshots, candidate.WardJump,
                     allyShieldResolver, candidate.RemainingThreats)) {
                LastCastTick() = now;
                SetLastEvent(lastEvent, lastEventSize,
                    data.Name.empty() ? "evade spell" : data.Name.c_str());
                result.Used = true;
                result.MovementSpeed =
                    data.EvadeTypeValue == EvadeType::MovementSpeedBuff;
                result.Displacement = IsDisplacementSpell(data);
                result.RemainingThreats = candidate.RemainingThreats;
                result.AvoidedThreats = std::max(
                    0, baselineThreats - candidate.RemainingThreats);
                return result;
            }
        }
        return result;
    }

    // Port of Program's independent ally-shield pass. The engine calls it when
    // the local player is safe, preventing an ally cast from consuming the
    // shared cast window needed for self-evade.
    static bool TryShieldAllies(
            const EvadeSettings& settings,
            const SDK::AIHeroClient& player,
            const SourceSkillshotList& skillshots,
            const ConfigResolver& resolver,
            const AllyShieldResolver& allyShieldResolver,
            char* lastEvent,
            std::size_t lastEventSize) {
        if (!settings.Enabled || !player.IsValid()) {
            return false;
        }
        const int now = SDK::Variables::TickCount();
        if (now - LastCastTick() < 250) {
            return false;
        }

        const std::string playerName =
            EvadeUtils::GetObjectCharacterName(player);
        const SDK::ChampionId playerChampionId =
            SDK::ChampionIdFromName(playerName.c_str());
        const auto spells = Database::EvadeSpellDatabase::ForChampion(
            playerChampionId, false);
        for (const auto& ally : SDK::GameObjects::AllyHeroes()) {
            if (!ally.IsValid() || ally.IsDead() || ally.IsMe()) {
                continue;
            }
            const SDK::AIBaseClient allyBase(ally.Handle());
            if (allyShieldResolver && !allyShieldResolver(allyBase)) {
                continue;
            }

            for (const Database::EvadeSpellData* data : spells) {
                if (!data || !data->CanShieldAllies || !data->IsEnabled) {
                    continue;
                }
                const EvadeSpellConfig config = Resolve(*data, resolver);
                if (!config.Enabled) {
                    continue;
                }
                const SDK::SpellSlot slot = ResolveSlot(player, *data);
                if (!IsReady(player, *data, slot) ||
                    !MatchesSpellName(player, *data, slot)) {
                    continue;
                }

                float range = data->MaxRange > 0.0f
                    ? data->MaxRange
                    : player.GetSpell(slot).CastRange();
                range = range > 0.0f ? range : 800.0f;
                if (player.ServerPosition().To2D().Distance(
                        ally.ServerPosition().To2D()) >
                    range + ally.BoundingRadius()) {
                    continue;
                }

                int dangerLevel = 0;
                for (const SourceSkillshotPtr& skillshot : skillshots) {
                    if (SourceEvader::ShouldConsider(skillshot, settings) &&
                        skillshot->IsAboutToHit(
                            DelayMs(*data), allyBase, settings)) {
                        dangerLevel = std::max(
                            dangerLevel, skillshot->Data.DangerValue);
                    }
                }
                const int requiredDanger = config.DangerLevel > 0
                    ? config.DangerLevel
                    : data->DangerLevel;
                if (dangerLevel < requiredDanger) {
                    continue;
                }

                CoreEvadeState::SpellCastBypassScope evadeCastScope;
                if (player.Spellbook().CastSpell(slot, ally.Address())) {
                    LastCastTick() = now;
                    SetLastEvent(lastEvent, lastEventSize, "ally shield");
                    return true;
                }
            }
        }
        return false;
    }

private:
    static int& LastCastTick() {
        static int value = 0;
        return value;
    }

    static EvadeSpellConfig Resolve(const Database::EvadeSpellData& data,
                                    const ConfigResolver& resolver) {
        if (resolver) {
            return resolver(data);
        }
        return { data.IsEnabled, data.DangerLevel, true };
    }

    static SDK::SpellSlot ResolveSlot(const SDK::AIHeroClient& player,
                                      const Database::EvadeSpellData& data) {
        if (data.IsSummonerSpell && !data.CheckSpellName.empty()) {
            for (SDK::SpellSlot slot : { SDK::SpellSlot::Summoner1,
                                         SDK::SpellSlot::Summoner2 }) {
                const auto spell = player.GetSpell(slot);
                if (spell.IsValid() && SameText(spell.Name(), data.CheckSpellName)) {
                    return slot;
                }
            }
        }
        if (data.Slot == SDK::SpellSlot::Unknown) {
            for (SDK::SpellSlot slot : { SDK::SpellSlot::Q, SDK::SpellSlot::W,
                                         SDK::SpellSlot::E, SDK::SpellSlot::R }) {
                const auto spell = player.GetSpell(slot);
                if (spell.IsValid() &&
                    (SameText(spell.Name(), data.CheckSpellName) ||
                     SameText(spell.ScriptName(), data.CheckSpellName) ||
                     SameText(spell.IconName(), data.CheckSpellName))) {
                    return slot;
                }
            }
            if (IsNamed(data, "PhaseDive2", "EkkoEAttack")) {
                return SDK::SpellSlot::E;
            }
        }
        return data.Slot;
    }

    static bool IsReady(const SDK::AIHeroClient& player,
                        const Database::EvadeSpellData& data,
                        SDK::SpellSlot slot) {
        if (data.IsItem) {
            return data.ItemId > 0 && SDK::CanUseItem(player, data.ItemId);
        }
        if (slot == SDK::SpellSlot::Unknown) {
            return false;
        }
        const auto spell = player.GetSpell(slot);
        return spell.IsValid() && spell.Learned() &&
            player.Spellbook().CanUseSpell(slot) == SDK::CoreSpellBook::State_Ready;
    }

    static bool SameText(const std::string& lhs, const std::string& rhs) {
        return !lhs.empty() && !rhs.empty() &&
            _stricmp(lhs.c_str(), rhs.c_str()) == 0;
    }

    static bool MatchesSpellName(const SDK::AIHeroClient& player,
                                 const Database::EvadeSpellData& data,
                                 SDK::SpellSlot slot) {
        if (data.CheckSpellName.empty() || data.IsItem) {
            return true;
        }
        const auto spell = player.GetSpell(slot);
        return SameText(spell.Name(), data.CheckSpellName) ||
               SameText(spell.ScriptName(), data.CheckSpellName) ||
               SameText(spell.IconName(), data.CheckSpellName);
    }

    static bool IsSelfProtectiveTarget(
            const Database::EvadeSpellData& data) {
        return std::find(data.ValidTargets.begin(), data.ValidTargets.end(),
                         SpellTargets::AllyChampions) !=
                   data.ValidTargets.end() &&
            (data.EvadeTypeValue == EvadeType::SpellShield ||
             data.EvadeTypeValue == EvadeType::MovementSpeedBuff ||
             (data.EvadeTypeValue == EvadeType::Shield &&
              data.Speed <= 0.0f));
    }

    static bool IsDisplacementSpell(
            const Database::EvadeSpellData& data) {
        return data.EvadeTypeValue == EvadeType::Dash ||
            data.EvadeTypeValue == EvadeType::Blink ||
            (data.CastTypeValue == CastType::Target && data.Speed > 0.0f &&
             !IsSelfProtectiveTarget(data));
    }

    static float BoostedMoveSpeed(
            const Database::EvadeSpellData& data,
            SDK::SpellSlot slot,
            const SDK::AIHeroClient& player) {
        const float current = std::max(50.0f, player.MoveSpeed());
        float percent = data.Speed;
        if (!data.SpeedArray.empty()) {
            int rank = 1;
            if (!data.IsItem && slot != SDK::SpellSlot::Unknown) {
                rank = std::max(1, player.GetSpell(slot).Level());
            }
            const std::size_t index = static_cast<std::size_t>(std::clamp(
                rank - 1, 0, static_cast<int>(data.SpeedArray.size()) - 1));
            percent = data.SpeedArray[index];
        }
        if (percent <= 0.0f) {
            return current;
        }
        // SpeedArray stores percentage bonuses in the retained database. Keep
        // the prediction bounded because temporary champion mechanics can
        // report values above the normal movement cap.
        return std::clamp(
            current * (1.0f + percent / 100.0f),
            current + 15.0f, current * 2.25f);
    }

    static bool ResolvePositionDestination(
            const Database::EvadeSpellData& data,
            SDK::SpellSlot slot,
            const SDK::AIHeroClient& player,
            const Vec2& bestPosition,
            Vec2& outPosition) {
        const Vec2 hero = player.ServerPosition().To2D();
        Vec2 direction = (bestPosition - hero).Normalized();
        if (direction.IsZero()) {
            direction = (SDK::Game::CursorPos().To2D() - hero).Normalized();
        }
        if (direction.IsZero()) {
            direction = player.Direction().To2D().Normalized();
        }
        if (direction.IsZero()) {
            return false;
        }

        float range = data.MaxRange > 0.0f
            ? data.MaxRange
            : player.GetSpell(slot).CastRange();
        range = std::max(50.0f, range);
        float distance = data.FixedRange
            ? range
            : std::min(range, std::max(0.0f, hero.Distance(bestPosition)));
        if (data.MinRange > 0.0f) {
            distance = std::max(distance, data.MinRange);
        }
        if (distance <= 1.0f) {
            distance = range;
        }
        outPosition = data.IsReversed
            ? hero - direction * distance
            : hero + direction * distance;
        return !outPosition.IsZero() && SourceGeometry::IsNavigable(
            outPosition, player.ServerPosition().y);
    }

    static int CountBlinkThreats(
            const SDK::AIHeroClient& player,
            const Vec2& landing,
            int delay,
            const SourceSkillshotList& skillshots,
            const EvadeSettings& settings) {
        const float radius = std::max(1.0f, player.BoundingRadius());
        delay = std::max(0, delay);
        int count = 0;

        for (const SourceSkillshotPtr& skillshot : skillshots) {
            if (!SourceEvader::ShouldConsider(skillshot, settings)) {
                continue;
            }

            bool hit = delay > 0 && skillshot->IsAboutToHit(
                delay, SDK::AIBaseClient(player.Handle()), settings);
            const int horizon = std::clamp(
                skillshot->EndTick() - SDK::Variables::TickCount(),
                delay, delay + 1500);
            const int step = skillshot->IsFiniteMissile() ? 18 : 25;
            for (int time = delay; !hit && time <= horizon; time += step) {
                hit = skillshot->ContainsAt(
                    landing, radius, time, settings);
            }
            if (!hit && horizon >= delay) {
                hit = skillshot->ContainsAt(
                    landing, radius, horizon, settings);
            }
            if (hit) {
                ++count;
            }
        }
        return count;
    }

    static int CountDisplacementThreats(
            const Database::EvadeSpellData& data,
            const SDK::AIHeroClient& player,
            const Vec2& landing,
            const SourceSkillshotList& skillshots,
            const EvadeSettings& settings) {
        const Vec2 hero = player.ServerPosition().To2D();
        const int delay = DelayMs(data);
        const bool travelsPath = data.EvadeTypeValue == EvadeType::Dash ||
            (data.CastTypeValue == CastType::Target && data.Speed > 0.0f &&
             !IsSelfProtectiveTarget(data));
        if (travelsPath) {
            if (!SourceGeometry::SegmentIsNavigable(
                    hero, landing, player.ServerPosition().y, 25.0f)) {
                return INT_MAX;
            }
            const float dashSpeed = data.Speed > 0.0f
                ? data.Speed
                : std::max(1000.0f, player.MoveSpeed());
            return SourceEvader::CountPathThreats(
                { hero, landing }, 80, dashSpeed, delay,
                player.BoundingRadius(), skillshots, settings);
        }
        return CountBlinkThreats(
            player, landing, delay, skillshots, settings);
    }

    static int EstimateProtectionRemaining(
            const Database::EvadeSpellData& data,
            const SDK::AIHeroClient& player,
            int baselineThreats,
            float lowestHitTime,
            const SourceSkillshotList& incomingThreats,
            const EvadeSettings& settings) {
        const int window = std::clamp(
            DelayMs(data) + std::max(0, SDK::Game::Ping()) / 2 + 70,
            120, 1000);
        if (lowestHitTime != FLT_MAX && lowestHitTime > window) {
            return baselineThreats;
        }

        int protectable = 0;
        const Vec2 hero = player.ServerPosition().To2D();
        for (const SourceSkillshotPtr& skillshot : incomingThreats) {
            if (!SourceEvader::ShouldConsider(skillshot, settings) ||
                !skillshot->ContainsStatic(
                    hero, player.BoundingRadius(), settings) ||
                !skillshot->IsAboutToHit(
                    window, SDK::AIBaseClient(player.Handle()), settings)) {
                continue;
            }
            ++protectable;
        }

        const bool protectsMultiple = data.Untargetable ||
            IsNamed(data, "Rappel", "EliseSpiderEInitial") ||
            IsNamed(data, "Hallowed Mist", "GwenW") ||
            IsNamed(data, "Apotheosis", "NilahR") ||
            IsNamed(data, "Intervention", "JudicatorIntervention") ||
            IsNamed(data, "Hourglass", "ZhonyasHourglass") ||
            IsNamed(data, "Witchcap", "Witchcap");
        if (data.EvadeTypeValue == EvadeType::SpellShield &&
            !protectsMultiple) {
            protectable = std::min(1, protectable);
        }
        return std::max(0, baselineThreats - protectable);
    }

    static int EstimateWindWallRemaining(
            const Database::EvadeSpellData& data,
            const SDK::AIHeroClient& player,
            int baselineThreats,
            const SourceSkillshotList& incomingThreats,
            const EvadeSettings& settings) {
        const Vec2 hero = player.ServerPosition().To2D();
        const int window = std::clamp(
            DelayMs(data) + std::max(0, SDK::Game::Ping()) / 2 + 100,
            150, 900);
        Vec2 wallDirection;
        float firstHit = FLT_MAX;
        for (const SourceSkillshotPtr& skillshot : incomingThreats) {
            if (!SourceEvader::ShouldConsider(skillshot, settings) ||
                !skillshot->HasMissile() ||
                !skillshot->IsAboutToHit(
                    window, SDK::AIBaseClient(player.Handle()), settings)) {
                continue;
            }
            const float hit = skillshot->HitTime(hero, settings);
            if (hit < firstHit) {
                firstHit = hit;
                wallDirection =
                    (skillshot->MissilePosition(0) - hero).Normalized();
            }
        }
        if (wallDirection.IsZero()) {
            return baselineThreats;
        }

        int blocked = 0;
        for (const SourceSkillshotPtr& skillshot : incomingThreats) {
            if (!SourceEvader::ShouldConsider(skillshot, settings) ||
                !skillshot->HasMissile() ||
                !skillshot->IsAboutToHit(
                    window, SDK::AIBaseClient(player.Handle()), settings)) {
                continue;
            }
            const Vec2 direction =
                (skillshot->MissilePosition(0) - hero).Normalized();
            if (!direction.IsZero() && direction.Dot(wallDirection) >= 0.45f) {
                ++blocked;
            }
        }
        return std::max(0, baselineThreats - blocked);
    }

    static int EstimateChronobreakRemaining(
            const Database::EvadeSpellData& data,
            const SDK::AIHeroClient& player,
            int baselineThreats,
            const SourceSkillshotList& skillshots,
            const EvadeSettings& settings) {
        int best = baselineThreats;
        for (const auto& minion : GameObjects::Get<SDK::AIMinionClient>()) {
            const std::string objectName = EvadeUtils::GetObjectName(minion);
            const std::string characterName =
                EvadeUtils::GetObjectCharacterName(minion);
            if (!minion.IsValid() || minion.IsDead() ||
                (!ContainsInsensitive(objectName, "Ekko") &&
                 !ContainsInsensitive(characterName, "Ekko"))) {
                continue;
            }
            best = std::min(best, CountBlinkThreats(
                player, minion.ServerPosition().To2D(), DelayMs(data),
                skillshots, settings));
        }
        return best;
    }

    static int EstimateRemainingThreats(
            const Database::EvadeSpellData& data,
            SDK::SpellSlot slot,
            const EvadeSettings& settings,
            const SDK::AIHeroClient& player,
            const Vec2& bestPosition,
            int baselineThreats,
            float lowestHitTime,
            const SourceSkillshotList& incomingThreats,
            const SourceSkillshotList& skillshots,
            bool wardJump,
            const AllyShieldResolver& allyShieldResolver) {
        if (data.EvadeTypeValue == EvadeType::MovementSpeedBuff) {
            const Vec2 hero = player.ServerPosition().To2D();
            if (bestPosition.IsZero() ||
                !SourceGeometry::SegmentIsNavigable(
                    hero, bestPosition, player.ServerPosition().y, 25.0f)) {
                return baselineThreats;
            }
            return SourceEvader::CountPathThreats(
                { hero, bestPosition },
                std::clamp(settings.EvadingFirstTimeOffset, 0, 500),
                BoostedMoveSpeed(data, slot, player), 0,
                player.BoundingRadius(), skillshots, settings);
        }
        if (data.EvadeTypeValue == EvadeType::WindWall) {
            return EstimateWindWallRemaining(
                data, player, baselineThreats, incomingThreats, settings);
        }
        if (IsNamed(data, "Chronobreak", "EkkoR")) {
            return EstimateChronobreakRemaining(
                data, player, baselineThreats, skillshots, settings);
        }
        if (data.CastTypeValue == CastType::Target &&
            !IsSelfProtectiveTarget(data)) {
            SDK::AIBaseClient target;
            Vec2 landing;
            int remaining = INT_MAX;
            return FindTarget(
                data, settings, player, bestPosition, skillshots,
                wardJump, allyShieldResolver, INT_MAX,
                target, landing, remaining)
                ? remaining
                : baselineThreats;
        }
        if (data.EvadeTypeValue == EvadeType::Invulnerability ||
            data.EvadeTypeValue == EvadeType::Shield ||
            data.EvadeTypeValue == EvadeType::SpellShield) {
            return EstimateProtectionRemaining(
                data, player, baselineThreats, lowestHitTime,
                incomingThreats, settings);
        }
        if (data.CastTypeValue == CastType::Position) {
            Vec2 landing;
            if (!ResolvePositionDestination(
                    data, slot, player, bestPosition, landing)) {
                return baselineThreats;
            }
            return CountDisplacementThreats(
                data, player, landing, skillshots, settings);
        }
        return baselineThreats;
    }

    static bool PrepareMovementSpeed(
            const SDK::AIHeroClient& player,
            const Vec2& bestPosition) {
        if (bestPosition.IsZero()) {
            return false;
        }
        const Vec2 pathEnd = player.PathEnd().To2D();
        if ((player.IsMoving() || player.HasPath()) && !pathEnd.IsZero() &&
            pathEnd.DistanceSqr(bestPosition) <= 45.0f * 45.0f) {
            return true;
        }
        return MoveTo(player, bestPosition);
    }

    static bool Cast(const Database::EvadeSpellData& data,
                     SDK::SpellSlot slot,
                     const EvadeSettings& settings,
                     const SDK::AIHeroClient& player,
                     const Vec2& bestPosition,
                     const SourceSkillshotList& incomingThreats,
                     const SourceSkillshotList& skillshots,
                     bool wardJump,
                     const AllyShieldResolver& allyShieldResolver,
                     int maxRemainingThreats) {
        CoreEvadeState::SpellCastBypassScope evadeCastScope;
        if (data.EvadeTypeValue == EvadeType::WindWall) {
            return CastWindWall(slot, player, incomingThreats, settings);
        }

        // A speed buff is the one evade spell whose protection depends on an
        // already active route.  Queue that route first, then activate the
        // buff; never cast first and hope a later MoveTo survives arbitration.
        if (data.EvadeTypeValue == EvadeType::MovementSpeedBuff &&
            !PrepareMovementSpeed(player, bestPosition)) {
            return false;
        }
        if (data.IsItem) {
            return SDK::UseItem(player, data.ItemId);
        }

        if (data.IsSpecial) {
            const bool special = CastSpecial(
                data, slot, settings, player, bestPosition, skillshots,
                wardJump, allyShieldResolver, maxRemainingThreats);
            if (special) {
                return true;
            }
            if (IsNamed(data, "PhaseDive2", "EkkoEAttack") ||
                IsNamed(data, "Chronobreak", "EkkoR") ||
                IsNamed(data, "Rappel", "EliseSpiderEInitial") ||
                IsNamed(data, "Pounce", "Pounce") ||
                IsNamed(data, "BrokenWings", "RivenTriCleave")) {
                return false;
            }
        }

        switch (data.CastTypeValue) {
        case CastType::Self:
            return player.Spellbook().CastSpell(slot);
        case CastType::Target:
            return CastTarget(data, slot, settings, player, bestPosition,
                              skillshots, wardJump, allyShieldResolver,
                              maxRemainingThreats);
        case CastType::Position:
        default:
            return CastPosition(data, slot, settings, player,
                                bestPosition, skillshots,
                                maxRemainingThreats);
        }
    }

    static bool CastSpecial(const Database::EvadeSpellData& data,
                            SDK::SpellSlot slot,
                            const EvadeSettings& settings,
                            const SDK::AIHeroClient& player,
                            const Vec2& bestPosition,
                            const SourceSkillshotList& skillshots,
                            bool wardJump,
                            const AllyShieldResolver& allyShieldResolver,
                            int maxRemainingThreats) {
        if (IsNamed(data, "PhaseDive2", "EkkoEAttack")) {
            return player.HasBuff("ekkoeattackbuff") &&
                CastTarget(data, slot, settings, player, bestPosition,
                           skillshots, wardJump, allyShieldResolver,
                           maxRemainingThreats);
        }
        if (IsNamed(data, "Chronobreak", "EkkoR")) {
            for (const auto& minion : GameObjects::Get<SDK::AIMinionClient>()) {
                const std::string objectName = EvadeUtils::GetObjectName(minion);
                const std::string characterName =
                    EvadeUtils::GetObjectCharacterName(minion);
                if (minion.IsValid() && !minion.IsDead() &&
                    (ContainsInsensitive(objectName, "Ekko") ||
                     ContainsInsensitive(characterName, "Ekko")) &&
                    CountBlinkThreats(
                        player, minion.ServerPosition().To2D(), DelayMs(data),
                        skillshots, settings) <= maxRemainingThreats) {
                    return player.Spellbook().CastSpell(slot);
                }
            }
            return false;
        }
        if (IsNamed(data, "Rappel", "EliseSpiderEInitial")) {
            if (SameText(player.GetSpell(SDK::SpellSlot::E).Name(), "EliseSpiderEInitial")) {
                return player.Spellbook().CastSpell(slot, player.Address());
            }
            return false;
        }
        if (IsNamed(data, "Pounce", "Pounce")) {
            if (SameText(player.GetSpell(SDK::SpellSlot::W).Name(), "Pounce")) {
                return CastPosition(data, slot, settings, player,
                                    bestPosition, skillshots,
                                    maxRemainingThreats);
            }
            return false;
        }
        if (IsNamed(data, "BrokenWings", "RivenTriCleave")) {
            MoveTo(player, bestPosition);
            return CastPosition(data, slot, settings, player,
                                bestPosition, skillshots,
                                maxRemainingThreats);
        }
        return false;
    }

    static bool CastPosition(const Database::EvadeSpellData& data,
                             SDK::SpellSlot slot,
                             const EvadeSettings& settings,
                             const SDK::AIHeroClient& player,
                             const Vec2& bestPosition,
                             const SourceSkillshotList& skillshots,
                             int maxRemainingThreats) {
        Vec2 position;
        if (!ResolvePositionDestination(
                data, slot, player, bestPosition, position)) {
            return false;
        }
        const int remaining = CountDisplacementThreats(
            data, player, position, skillshots, settings);
        if (remaining > maxRemainingThreats) {
            return false;
        }
        return player.Spellbook().CastSpell(
            slot, Vec3::From2D(position, player.ServerPosition().y));
    }

    static bool CastWindWall(SDK::SpellSlot slot,
                             const SDK::AIHeroClient& player,
                             const SourceSkillshotList& skillshots,
                             const EvadeSettings& settings) {
        const Vec2 hero = player.ServerPosition().To2D();
        const SourceSkillshot* closest = nullptr;
        float hitTime = FLT_MAX;
        for (const SourceSkillshotPtr& skillshot : skillshots) {
            if (!SourceEvader::ShouldConsider(skillshot, settings) ||
                !skillshot->HasMissile()) {
                continue;
            }
            const float current = skillshot->HitTime(hero, settings);
            if (current < hitTime && skillshot->ContainsStatic(
                    hero, player.BoundingRadius() + 60.0f, settings)) {
                hitTime = current;
                closest = skillshot.get();
            }
        }
        if (!closest) {
            return false;
        }
        Vec2 direction = (closest->MissilePosition(0) - hero).Normalized();
        if (direction.IsZero()) {
            direction = (closest->Start() - hero).Normalized();
        }
        return !direction.IsZero() && player.Spellbook().CastSpell(
            slot, Vec3::From2D(hero + direction * 100.0f,
                               player.ServerPosition().y));
    }

    static bool CastTarget(const Database::EvadeSpellData& data,
                           SDK::SpellSlot slot,
                           const EvadeSettings& settings,
                           const SDK::AIHeroClient& player,
                           const Vec2& bestPosition,
                           const SourceSkillshotList& skillshots,
                           bool wardJump,
                           const AllyShieldResolver& allyShieldResolver,
                           int maxRemainingThreats) {
        SDK::AIBaseClient target;
        Vec2 landing;
        int remaining = INT_MAX;
        if (!FindTarget(data, settings, player, bestPosition,
                        skillshots, wardJump, allyShieldResolver,
                        maxRemainingThreats, target, landing, remaining) ||
            !player.Spellbook().CastSpell(slot, target.Address())) {
            return false;
        }
        return true;
    }

    static bool FindTarget(const Database::EvadeSpellData& data,
                           const EvadeSettings& settings,
                           const SDK::AIHeroClient& player,
                           const Vec2& bestPosition,
                           const SourceSkillshotList& skillshots,
                           bool wardJump,
                           const AllyShieldResolver& allyShieldResolver,
                           int maxRemainingThreats,
                           SDK::AIBaseClient& outTarget,
                           Vec2& outLanding,
                           int& outRemainingThreats) {
        if (IsSelfProtectiveTarget(data)) {
            outTarget = SDK::AIBaseClient(player.Handle());
            outLanding = player.ServerPosition().To2D();
            outRemainingThreats = 0;
            return outTarget.IsValid();
        }

        std::vector<SDK::AIBaseClient> candidates;
        const auto add = [&](const SDK::AIBaseClient& unit) {
            if (!unit.IsValid() || unit.IsDead() ||
                !unit.IsTargetable() ||
                unit.NetworkId() == player.NetworkId()) {
                return;
            }
            if (data.CanShieldAllies && unit.IsAlly() &&
                allyShieldResolver && !allyShieldResolver(unit)) {
                return;
            }
            float range = data.MaxRange > 0
                ? static_cast<float>(data.MaxRange)
                : player.GetSpell(data.Slot).CastRange();
            range = range > 0.0f ? range : 700.0f;
            if (player.ServerPosition().To2D().Distance(unit.ServerPosition().To2D()) <=
                range + unit.BoundingRadius() + 75.0f) {
                candidates.push_back(unit);
            }
        };
        for (SpellTargets type : data.ValidTargets) {
            switch (type) {
            case SpellTargets::AllyChampions:
                for (const auto& unit : SDK::GameObjects::AllyHeroes()) add(SDK::AIBaseClient(unit.Handle()));
                break;
            case SpellTargets::EnemyChampions:
                for (const auto& unit : SDK::GameObjects::EnemyHeroes()) add(SDK::AIBaseClient(unit.Handle()));
                break;
            case SpellTargets::AllyMinions:
                for (const auto& unit : SDK::GameObjects::AllyMinions()) add(SDK::AIBaseClient(unit.Handle()));
                break;
            case SpellTargets::EnemyMinions:
                for (const auto& unit : SDK::GameObjects::EnemyMinions()) add(SDK::AIBaseClient(unit.Handle()));
                break;
            case SpellTargets::Targetables:
                for (const auto& unit : GameObjects::Get<SDK::AIMinionClient>()) {
                    add(SDK::AIBaseClient(unit.Handle()));
                }
                for (const auto& unit : GameObjects::Get<SDK::AIHeroClient>()) {
                    add(SDK::AIBaseClient(unit.Handle()));
                }
                if (wardJump) {
                    for (const auto& unit : SDK::GameObjects::AllyWards()) {
                        add(SDK::AIBaseClient(unit.Handle()));
                    }
                }
                break;
            }
        }

        float bestScore = FLT_MAX;
        int bestRemaining = INT_MAX;
        for (const SDK::AIBaseClient& unit : candidates) {
            const Vec2 landing = LandingPosition(data, player, unit);
            if (!SourceGeometry::IsNavigable(
                    landing, player.ServerPosition().y)) {
                continue;
            }
            const int remaining = CountDisplacementThreats(
                data, player, landing, skillshots, settings);
            if (remaining > maxRemainingThreats) {
                continue;
            }
            const float score = landing.Distance(bestPosition);
            if (remaining < bestRemaining ||
                (remaining == bestRemaining && score < bestScore)) {
                bestRemaining = remaining;
                bestScore = score;
                outTarget = unit;
                outLanding = landing;
                outRemainingThreats = remaining;
            }
        }
        return outTarget.IsValid();
    }

    static Vec2 LandingPosition(const Database::EvadeSpellData& data,
                                const SDK::AIHeroClient& player,
                                const SDK::AIBaseClient& target) {
        const Vec2 hero = player.ServerPosition().To2D();
        const Vec2 position = target.ServerPosition().To2D();
        Vec2 direction = (position - hero).Normalized();
        if (direction.IsZero()) {
            direction = player.Direction().To2D().Normalized();
        }
        const float offset = std::max(75.0f,
            target.BoundingRadius() + player.BoundingRadius() * 0.5f);
        if (data.IsBehindTarget) {
            return position + direction * offset;
        }
        if (data.IsInfrontTarget) {
            return position - direction * offset;
        }
        return position;
    }

    static bool MoveTo(const SDK::AIHeroClient& player, const Vec2& position) {
        return !position.IsZero() && CoreControl::IssueMove(
            Vec3::From2D(position, player.ServerPosition().y), true);
    }

    static bool ContainsInsensitive(const std::string& text,
                                    const char* value) {
        if (text.empty() || !value || !value[0]) {
            return false;
        }
        const std::string_view n(value);
        auto it = std::search(
            text.begin(), text.end(),
            n.begin(), n.end(),
            [](char c1, char c2) {
                return std::tolower(static_cast<unsigned char>(c1)) ==
                       std::tolower(static_cast<unsigned char>(c2));
            }
        );
        return it != text.end();
    }

    static bool IsNamed(const Database::EvadeSpellData& data,
                        const char* name,
                        const char* spellName) {
        return SameText(data.Name, name) ||
               SameText(data.Name, spellName) ||
               SameText(data.CheckSpellName, name) ||
               SameText(data.CheckSpellName, spellName);
    }

    static void SetLastEvent(char* buffer,
                             std::size_t bufferSize,
                             const char* value) {
        if (buffer && bufferSize > 0) {
            strncpy_s(buffer, bufferSize, value ? value : "", _TRUNCATE);
        }
    }

    static int DelayMs(const Database::EvadeSpellData& data) {
        return static_cast<int>(std::clamp(
            data.Delay, 0.0f, static_cast<float>(INT_MAX)));
    }
};

} // namespace Plugins::KuroEvade
