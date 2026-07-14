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

class SourceEvadeSpell final {
public:
    using ConfigResolver =
        std::function<EvadeSpellConfig(const Database::EvadeSpellData&)>;
    using AllyShieldResolver =
        std::function<bool(const SDK::AIBaseClient&)>;

    static std::string MenuKey(const Database::EvadeSpellData& data) {
        std::string result = data.ChampionName + "|" + data.Name + "|" +
            std::to_string(data.ItemId);
        std::transform(result.begin(), result.end(), result.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return result;
    }

    static bool TryUseBest(const EvadeSettings& settings,
                           const SDK::AIHeroClient& player,
                           const Vec2& bestPosition,
                           bool hasSafeMove,
                           int currentDanger,
                           float lowestHitTime,
                           const SourceSkillshotList& skillshots,
                           const ConfigResolver& resolver,
                           const AllyShieldResolver& allyShieldResolver,
                           char* lastEvent,
                           std::size_t lastEventSize) {
        if (!player.IsValid()) {
            return false;
        }
        (void)hasSafeMove;
        (void)lowestHitTime;
        const int now = SDK::Variables::TickCount();
        if (now - LastCastTick() < 250) {
            return false;
        }

        auto spells = Database::EvadeSpellDatabase::ForChampion(
            EvadeUtils::GetObjectCharacterName(player).c_str(), true);
        std::stable_sort(spells.begin(), spells.end(), [&](const auto* lhs, const auto* rhs) {
            const EvadeSpellConfig left = Resolve(*lhs, resolver);
            const EvadeSpellConfig right = Resolve(*rhs, resolver);
            const int leftDanger = left.DangerLevel > 0 ? left.DangerLevel : lhs->DangerLevel;
            const int rightDanger = right.DangerLevel > 0 ? right.DangerLevel : rhs->DangerLevel;
            return leftDanger != rightDanger
                ? leftDanger < rightDanger
                : lhs->Name < rhs->Name;
        });

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

            if (Cast(*data, slot, settings, player, bestPosition, skillshots,
                     config.WardJump, allyShieldResolver)) {
                LastCastTick() = now;
                SetLastEvent(lastEvent, lastEventSize,
                    data->Name.empty() ? "evade spell" : data->Name.c_str());
                return true;
            }
        }
        return false;
    }

    // Port of Program's independent ally-shield pass. It runs before the
    // player's own evade decision, so a threatened ally can be shielded even
    // when the local player is standing in a safe position.
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

        const auto spells = Database::EvadeSpellDatabase::ForChampion(
            EvadeUtils::GetObjectCharacterName(player).c_str(), false);
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

    static bool Cast(const Database::EvadeSpellData& data,
                     SDK::SpellSlot slot,
                     const EvadeSettings& settings,
                     const SDK::AIHeroClient& player,
                     const Vec2& bestPosition,
                     const SourceSkillshotList& skillshots,
                     bool wardJump,
                     const AllyShieldResolver& allyShieldResolver) {
        CoreEvadeState::SpellCastBypassScope evadeCastScope;
        if (data.EvadeTypeValue == EvadeType::WindWall) {
            return CastWindWall(slot, player, skillshots, settings);
        }
        if (data.IsItem) {
            const bool used = SDK::UseItem(player, data.ItemId);
            if (used && data.EvadeTypeValue == EvadeType::MovementSpeedBuff) {
                MoveTo(player, bestPosition);
            }
            return used;
        }

        if (data.IsSpecial) {
            const bool special = CastSpecial(
                data, slot, settings, player, bestPosition, skillshots,
                wardJump, allyShieldResolver);
            if (special) {
                return true;
            }
        }

        switch (data.CastTypeValue) {
        case CastType::Self:
            if (player.Spellbook().CastSpell(slot)) {
                if (data.EvadeTypeValue == EvadeType::MovementSpeedBuff) {
                    MoveTo(player, bestPosition);
                }
                return true;
            }
            return false;
        case CastType::Target:
            return CastTarget(data, slot, settings, player, bestPosition,
                              skillshots, wardJump, allyShieldResolver);
        case CastType::Position:
        default:
            if (CastPosition(data, slot, settings, player,
                             bestPosition, skillshots)) {
                if (data.EvadeTypeValue == EvadeType::MovementSpeedBuff) {
                    MoveTo(player, bestPosition);
                }
                return true;
            }
            return false;
        }
    }

    static bool CastSpecial(const Database::EvadeSpellData& data,
                            SDK::SpellSlot slot,
                            const EvadeSettings& settings,
                            const SDK::AIHeroClient& player,
                            const Vec2& bestPosition,
                            const SourceSkillshotList& skillshots,
                            bool wardJump,
                            const AllyShieldResolver& allyShieldResolver) {
        if (IsNamed(data, "PhaseDive2", "EkkoEAttack")) {
            return player.HasBuff("ekkoeattackbuff") &&
                CastTarget(data, slot, settings, player, bestPosition,
                           skillshots, wardJump, allyShieldResolver);
        }
        if (IsNamed(data, "Chronobreak", "EkkoR")) {
            for (const auto& minion : SDK::ObjectManager::Get<SDK::AIMinionClient>()) {
                const std::string objectName = EvadeUtils::GetObjectName(minion);
                const std::string characterName =
                    EvadeUtils::GetObjectCharacterName(minion);
                if (minion.IsValid() && !minion.IsDead() &&
                    (ContainsInsensitive(objectName, "Ekko") ||
                     ContainsInsensitive(characterName, "Ekko")) &&
                    SourceEvader::IsSafePoint(minion.ServerPosition().To2D(),
                        player.BoundingRadius(), skillshots, settings)) {
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
                                    bestPosition, skillshots);
            }
            return false;
        }
        if (IsNamed(data, "BrokenWings", "RivenTriCleave")) {
            MoveTo(player, bestPosition);
            return CastPosition(data, slot, settings, player,
                                bestPosition, skillshots);
        }
        return false;
    }

    static bool CastPosition(const Database::EvadeSpellData& data,
                             SDK::SpellSlot slot,
                             const EvadeSettings& settings,
                             const SDK::AIHeroClient& player,
                             const Vec2& bestPosition,
                             const SourceSkillshotList& skillshots) {
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
        float range = data.MaxRange > 0
            ? static_cast<float>(data.MaxRange)
            : player.GetSpell(slot).CastRange();
        range = std::max(50.0f, range);
        float distance = data.FixedRange
            ? range
            : std::min(range, std::max(0.0f, hero.Distance(bestPosition)));
        if (data.MinRange > 0) {
            distance = std::max(distance, static_cast<float>(data.MinRange));
        }
        if (distance <= 1.0f) {
            distance = range;
        }
        const Vec2 position = data.IsReversed
            ? hero - direction * distance
            : hero + direction * distance;
        const int arrival = DelayMs(data);
        for (const SourceSkillshotPtr& skillshot : skillshots) {
            if (SourceEvader::ShouldConsider(skillshot, settings) &&
                skillshot->ContainsAt(position, player.BoundingRadius(),
                                      arrival, settings)) {
                return false;
            }
        }
        if (data.EvadeTypeValue == EvadeType::Dash) {
            const float dashSpeed = data.Speed > 0
                ? static_cast<float>(data.Speed)
                : std::max(1000.0f, player.MoveSpeed());
            if (!SourceEvader::IsSafePath(
                    { hero, position }, 80, dashSpeed, arrival,
                    player.BoundingRadius(), skillshots, settings).IsSafe) {
                return false;
            }
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
                           const AllyShieldResolver& allyShieldResolver) {
        SDK::AIBaseClient target;
        Vec2 landing;
        if (!FindTarget(data, settings, player, bestPosition,
                        skillshots, wardJump, allyShieldResolver,
                        target, landing) ||
            !player.Spellbook().CastSpell(slot, target.Address())) {
            return false;
        }
        if (data.EvadeTypeValue == EvadeType::MovementSpeedBuff) {
            MoveTo(player, bestPosition);
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
                           SDK::AIBaseClient& outTarget,
                           Vec2& outLanding) {
        const bool selfProtectiveTarget =
            std::find(data.ValidTargets.begin(), data.ValidTargets.end(),
                      SpellTargets::AllyChampions) != data.ValidTargets.end() &&
            (data.EvadeTypeValue == EvadeType::SpellShield ||
             data.EvadeTypeValue == EvadeType::MovementSpeedBuff);
        if (selfProtectiveTarget) {
            outTarget = SDK::AIBaseClient(player.Handle());
            outLanding = player.ServerPosition().To2D();
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
                for (const auto& unit : SDK::ObjectManager::Get<SDK::AIMinionClient>()) {
                    add(SDK::AIBaseClient(unit.Handle()));
                }
                for (const auto& unit : SDK::ObjectManager::Get<SDK::AIHeroClient>()) {
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
        for (const SDK::AIBaseClient& unit : candidates) {
            const Vec2 landing = LandingPosition(data, player, unit);
            if (!SourceEvader::IsSafePoint(landing, player.BoundingRadius(),
                                           skillshots, settings)) {
                continue;
            }
            const float score = landing.Distance(bestPosition);
            if (score < bestScore) {
                bestScore = score;
                outTarget = unit;
                outLanding = landing;
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

    static void MoveTo(const SDK::AIHeroClient& player, const Vec2& position) {
        if (!position.IsZero()) {
            CoreControl::IssueMove(
                Vec3::From2D(position, player.ServerPosition().y), true);
        }
    }

    static bool ContainsInsensitive(const std::string& text,
                                    const char* value) {
        if (text.empty() || !value || !value[0]) {
            return false;
        }
        std::string lhs = text;
        std::string rhs = value;
        std::transform(lhs.begin(), lhs.end(), lhs.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        std::transform(rhs.begin(), rhs.end(), rhs.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return lhs.find(rhs) != std::string::npos;
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
