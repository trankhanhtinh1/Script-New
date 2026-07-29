#pragma once

// Runtime/event helpers shared by full one-trick controllers.  This file is
// intentionally limited to champion-neutral plumbing.  Spell ordering,
// state transitions, hitbox semantics and matchup decisions remain owned by
// each AI<Champion>Controller.

#include "AIChampionEngine.h"
#include "AIGeometry.h"
#include "AIMarksmanTargeting.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <vector>

namespace Plugins::KuroAIO::AI::ControllerHelpers {

inline bool IsLocalPlayer(const ::Core::Events::ObjectInfo& sender) {
    const auto player = GameObjects::Player();
    return player.IsValid() && sender.IsValid() &&
           sender.NetworkId == static_cast<std::uint32_t>(player.NetworkId());
}

inline int Now() {
    return SDK::Variables::TickCount();
}

// Controllers attach different champion meaning to an enemy spell window,
// but the bounded record lookup/expiry reuse is identical. Record is expected
// to expose NetworkId, CommittedUntil and HardCrowdControlSpentUntil; any
// additional champion fields remain untouched and controller-owned.
template <typename Record, std::size_t Capacity>
inline Record* FindEnemyCastWindow(
    std::array<Record, Capacity>& records,
    int networkId,
    bool create = false) {
    if (networkId == 0) return nullptr;
    for (auto& record : records) {
        if (record.NetworkId == networkId) return &record;
    }
    if (!create) return nullptr;
    const int now = Now();
    for (auto& record : records) {
        if (record.NetworkId == 0 ||
            (record.CommittedUntil < now &&
             record.HardCrowdControlSpentUntil < now)) {
            record = {};
            record.NetworkId = networkId;
            return &record;
        }
    }
    return nullptr;
}

template <typename Record>
inline bool EnemyCastWindowCommitted(const Record* record) {
    return record && record->CommittedUntil >= Now();
}

template <typename Record>
inline bool EnemyCastWindowHardCrowdControlSpent(const Record* record) {
    return record && record->HardCrowdControlSpentUntil >= Now();
}

template <typename Record, std::size_t Capacity>
inline const Record* EnemyCastWindowById(
    const std::array<Record, Capacity>& records,
    int networkId) {
    if (networkId == 0) return nullptr;
    for (const auto& record : records) {
        if (record.NetworkId == networkId) return &record;
    }
    return nullptr;
}

template <typename Record, std::size_t Capacity>
inline bool EnemyCastWindowCommitted(
    const std::array<Record, Capacity>& records,
    int networkId) {
    return EnemyCastWindowCommitted(
        EnemyCastWindowById(records, networkId));
}

template <typename Record, std::size_t Capacity>
inline bool EnemyCastWindowHardCrowdControlSpent(
    const std::array<Record, Capacity>& records,
    int networkId) {
    return EnemyCastWindowHardCrowdControlSpent(
        EnemyCastWindowById(records, networkId));
}

// Pure record lookup used by geometry planners whose test records expose an
// Id and Valid bit. It avoids cloning a linear scan for each spell shape.
template <typename Record>
inline const Record* FindValidRecordById(
    const std::vector<Record>& records,
    int id) {
    for (const auto& record : records) {
        if (record.Id == id && record.Valid) return &record;
    }
    return nullptr;
}

inline float CurrentResource(float maximum = FLT_MAX) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return 0.0f;
    return std::min(
        std::max(0.0f, player.Mana()),
        std::max(0.0f, maximum));
}

// Champion controllers frequently need the local player's normalized mana
// for their own policy thresholds. Keep the player lookup and invalid-player
// fallback in one place; each controller still owns the actual threshold.
inline float PlayerManaPercent() {
    return Engine::ManaPercent(GameObjects::Player());
}

inline bool PlayerMobilityLocked() {
    const auto player = GameObjects::Player();
    return player.IsValid() &&
        (SDK::HasBuffOfType(player, SDK::BuffType::Grounded) ||
         SDK::HasBuffOfType(player, SDK::BuffType::Snare) ||
         SDK::HasBuffOfType(player, SDK::BuffType::Stun) ||
         SDK::HasBuffOfType(player, SDK::BuffType::Knockup) ||
         SDK::HasBuffOfType(player, SDK::BuffType::Knockback) ||
         SDK::HasBuffOfType(player, SDK::BuffType::Suppression));
}

inline float AutoAttackRange(const AIBaseClient& target,
                             float bonusRange = 0.0f) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !target.IsValid()) return 0.0f;
    return player.AttackRange() + target.BoundingRadius() + std::max(0.0f, bonusRange);
}

inline bool InAutoAttackRange(const AIBaseClient& target,
                              float bonusRange = 0.0f) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !target.IsValid()) return false;
    const float range = AutoAttackRange(target, bonusRange);
    return player.Position().DistanceSqr2D(target.Position()) <=
           range * range;
}

inline bool CaptureAfterAttack(const SDK::OrbwalkingActionArgs& args,
                               int& targetNetworkId,
                               int& attackTick) {
    if (!args.Target.IsValid()) return false;
    targetNetworkId = static_cast<int>(args.Target.NetworkId());
    attackTick = Now();
    return true;
}

// Before-attack and after-attack callbacks expose the same neutral target/tick
// payload. Keep separate names for auditable call sites while sharing the
// capture semantics rather than cloning them in individual controllers.
inline bool CaptureBeforeAttack(const SDK::OrbwalkingActionArgs& args,
                                int& targetNetworkId,
                                int& attackTick) {
    return CaptureAfterAttack(args, targetNetworkId, attackTick);
}

// Record only the neutral facts common to local basic-attack callbacks.
// What the attack means (passive consumption, a weave window, an empowered
// hit, and so on) remains the owning champion controller's responsibility.
inline bool CaptureLocalAutoAttack(
    const SDK::Events::ProcessSpellEventArgs& args,
    int& targetNetworkId,
    int& castTick) {
    if (!IsLocalPlayer(args.Sender) || !args.IsAutoAttack) return false;
    const std::uint32_t observedTarget = args.TargetNetworkId != 0
        ? args.TargetNetworkId
        : args.Target.NetworkId;
    targetNetworkId = static_cast<int>(observedTarget);
    castTick = Now();
    return true;
}

template <int* TargetNetworkId, int* CaptureTick>
inline void CaptureLocalAutoAttackEvent(
    const SDK::Events::ProcessSpellEventArgs& args) {
    (void)CaptureLocalAutoAttack(args, *TargetNetworkId, *CaptureTick);
}

template <int* TargetNetworkId, int* CaptureTick>
inline void CaptureAfterAttackEvent(SDK::OrbwalkingActionArgs& args) {
    (void)CaptureAfterAttack(args, *TargetNetworkId, *CaptureTick);
}

template <void (*UpdateState)(
              const SDK::Events::BuffEventArgs&, bool),
          bool Added>
inline void ForwardBuffStateEvent(
    const SDK::Events::BuffEventArgs& args) {
    UpdateState(args, Added);
}

template <void (*OnLocal)(
              const SDK::Events::ProcessSpellEventArgs&),
          void (*OnOther)(
              const SDK::Events::ProcessSpellEventArgs&)>
inline void DispatchLocalOrOtherSpellEvent(
    const SDK::Events::ProcessSpellEventArgs& args) {
    if (IsLocalPlayer(args.Sender)) {
        OnLocal(args);
    } else {
        OnOther(args);
    }
}

inline bool SpellEnabled(int index, Mode mode) {
    return Engine::MenuSpellEnabled(Engine::MenuForMode(mode), index, true);
}

inline bool ControllerSpellAvailable(int index,
                                     Mode mode,
                                     bool allowDuringWindup = false) {
    if (index < 0 || index >= 4 || !Engine::ActiveProfile ||
        !Engine::RuntimeSpells[index] ||
        !Engine::RuntimeSpells[index]->IsReady() ||
        !SpellEnabled(index, mode)) {
        return false;
    }
    const auto& spec = Engine::ResolvedSpecs[index];
    if (!Engine::ModeEnabled(spec, mode) ||
        !Engine::ResourceOkay(spec, mode)) {
        return false;
    }
    return allowDuringWindup ||
           !Engine::ShouldPreserveAttack(spec, StepRule::None);
}

inline bool RuntimeNameContains(int index, const char* token) {
    return index >= 0 && index < 4 && token && token[0] &&
           Engine::TextContains(Engine::RuntimeSpellNames[index].c_str(), token);
}

inline bool NameEquals(const char* left, const char* right) {
    return left && right && left[0] && right[0] &&
           _stricmp(left, right) == 0;
}

inline bool TextContainsAny(
    const char* value,
    std::initializer_list<const char*> tokens) {
    if (!value) return false;
    for (const char* token : tokens) {
        if (token && token[0] && Engine::TextContains(value, token)) {
            return true;
        }
    }
    return false;
}

inline bool AnyTextContains(
    std::initializer_list<const char*> values,
    std::initializer_list<const char*> tokens) {
    for (const char* value : values) {
        if (TextContainsAny(value, tokens)) return true;
    }
    return false;
}

inline bool HasAnyBuff(
    const AIBaseClient& unit,
    std::initializer_list<const char*> names) {
    if (!unit.IsValid()) return false;
    for (const char* name : names) {
        if (name && name[0] && unit.HasBuff(name)) return true;
    }
    return false;
}

// Buff-alias telemetry often exposes the same stack manager under mixed-case
// or legacy names. Keep only the champion vocabulary at call sites and share
// the validity/max-count plumbing here.
inline int MaximumBuffCount(
    const AIBaseClient& unit,
    std::initializer_list<const char*> names) {
    if (!unit.IsValid()) return 0;
    int maximum = 0;
    for (const char* name : names) {
        if (name && name[0]) {
            maximum = std::max(maximum, unit.GetBuffCount(name));
        }
    }
    return maximum;
}

inline int SpellRank(int index) {
    if (index < 0 || index >= 4 || !Engine::ActiveProfile) return 0;
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return 0;
    const auto spell = player.Spellbook().GetSpell(
        Engine::ActiveProfile->Spells[index].Slot);
    return spell.IsValid() ? std::max(0, spell.Level()) : 0;
}

inline float SpellCost(int index) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || index < 0 || index >= 4 || !Engine::ActiveProfile) {
        return 0.0f;
    }
    const auto spell = player.Spellbook().GetSpell(
        Engine::ActiveProfile->Spells[index].Slot);
    const float cost = spell.IsValid() ? spell.ManaCost() : 0.0f;
    return std::isfinite(cost) && cost > 0.0f ? cost : 0.0f;
}

inline bool SpellInstanceContains(
    const SDK::SpellDataInstClient& spell,
    const char* token) {
    return spell.IsValid() && token && token[0] &&
        (Engine::TextContains(spell.Name().c_str(), token) ||
         Engine::TextContains(spell.ScriptName().c_str(), token) ||
         Engine::TextContains(spell.IconName().c_str(), token));
}

inline bool HeroHasSummonerSpellToken(const AIHeroClient& hero,
                                      const char* token) {
    if (!hero.IsValid() || !token || !token[0]) return false;
    const auto first = hero.Spellbook().GetSpell(
        SDK::SpellSlot::Summoner1);
    const auto second = hero.Spellbook().GetSpell(
        SDK::SpellSlot::Summoner2);
    return SpellInstanceContains(first, token) ||
           SpellInstanceContains(second, token);
}

inline bool HeroHasSmite(const AIHeroClient& hero) {
    return HeroHasSummonerSpellToken(hero, "smite");
}

inline AIHeroClient HeroByNetworkId(int networkId) {
    return networkId != 0
        ? Engine::EnemyByNetworkId(networkId)
        : AIHeroClient{};
}

// Unlike HeroByNetworkId, these raw lookups deliberately retain hidden,
// untargetable and transitional heroes. Event reconciliation and protected-
// ally state often need identity after gameplay-valid target selection ends.
inline AIHeroClient RawEnemyHeroByNetworkId(int networkId) {
    if (networkId == 0) return {};
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (static_cast<int>(enemy.NetworkId()) == networkId) return enemy;
    }
    return {};
}

inline AIHeroClient RawAllyHeroByNetworkId(int networkId) {
    if (networkId == 0) return {};
    const auto player = GameObjects::Player();
    if (player.IsValid() &&
        static_cast<int>(player.NetworkId()) == networkId) return player;
    for (const auto& ally : GameObjects::AllyHeroes()) {
        if (static_cast<int>(ally.NetworkId()) == networkId) return ally;
    }
    return {};
}

inline bool IsCommonUntargetableOrImmune(const AIBaseClient& target);

inline AIHeroClient OrbwalkerHeroTarget(float range = FLT_MAX) {
    const auto orbTarget = Orbwalker::GetTarget();
    if (!orbTarget.IsValid() || !orbTarget.IsHero()) return {};
    const AIHeroClient hero(orbTarget.Handle());
    return Engine::ValidEnemy(hero, range) ? hero : AIHeroClient{};
}

inline bool OrbwalkerTargets(const AIBaseClient& target,
                             float range = FLT_MAX) {
    if (!target.IsValid()) return false;
    const auto hero = OrbwalkerHeroTarget(range);
    return hero.IsValid() && hero.NetworkId() == target.NetworkId();
}

inline AIHeroClient PlayerSelectedEnemy(float range = FLT_MAX) {
    if (auto* selector = SDK::TargetSelector::Instance()) {
        const auto selected = selector->GetSelectedTarget();
        if (Engine::ValidEnemy(selected, range)) return selected;
    }
    return {};
}

inline AIHeroClient NearestEnemyToPlayer(const AIHeroClient& fallback = {},
                                         float range = 1400.0f) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return {};
    AIHeroClient best = Engine::ValidEnemy(fallback, range)
        ? fallback
        : AIHeroClient{};
    float bestDistance = best.IsValid()
        ? player.Position().Distance2D(best.Position())
        : FLT_MAX;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy, range)) continue;
        const float distance = player.Position().Distance2D(enemy.Position());
        if (distance < bestDistance) {
            best = enemy;
            bestDistance = distance;
        }
    }
    return best;
}

// The builder supplies champion-specific reach facts (prediction, collision,
// marks, charge state and real combo damage). The shared loop guarantees that
// a selected or locked target never wins merely by preference when no legal
// damage route can actually reach it.
template <typename ContextBuilder>
inline AIHeroClient SelectReachableEnemy(
    const AIHeroClient& preferred,
    float searchRange,
    ContextBuilder&& buildContext,
    MarksmanTargeting::TargetEvaluation* chosenEvaluation = nullptr) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || searchRange <= 0.0f) return {};

    AIHeroClient best{};
    MarksmanTargeting::TargetEvaluation bestEvaluation{};
    const int preferredId = Engine::ValidEnemy(preferred)
        ? static_cast<int>(preferred.NetworkId()) : 0;
    const auto orbTarget = OrbwalkerHeroTarget(searchRange);
    const int orbTargetId = orbTarget.IsValid()
        ? static_cast<int>(orbTarget.NetworkId()) : 0;

    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy, searchRange) ||
            IsCommonUntargetableOrImmune(enemy)) {
            continue;
        }
        auto context = buildContext(enemy);
        context.Valid = context.Valid && enemy.IsValid() && !enemy.IsDead();
        context.Targetable = context.Targetable && enemy.IsTargetable();
        context.Distance = player.Position().Distance2D(enemy.Position());
        context.MaximumReach = searchRange;
        context.HealthPercent = enemy.HealthPercent();
        context.EffectiveHealth = std::max(
            context.EffectiveHealth,
            std::max(1.0f, enemy.Health()));
        context.Selected = context.Selected ||
            (preferredId != 0 &&
             static_cast<int>(enemy.NetworkId()) == preferredId);
        context.Locked = context.Locked ||
            (Engine::LockedTargetNetworkId != 0 &&
             static_cast<int>(enemy.NetworkId()) ==
                 Engine::LockedTargetNetworkId);
        context.OrbwalkerTarget = context.OrbwalkerTarget ||
            (orbTargetId != 0 &&
             static_cast<int>(enemy.NetworkId()) == orbTargetId);

        const auto evaluation = MarksmanTargeting::EvaluateTarget(context);
        if (MarksmanTargeting::BetterTarget(evaluation, bestEvaluation)) {
            best = enemy;
            bestEvaluation = evaluation;
        }
    }

    if (chosenEvaluation) *chosenEvaluation = bestEvaluation;
    return best;
}

inline bool HasEnemyChampionNear(float range) {
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (Engine::ValidEnemy(enemy, range)) return true;
    }
    return false;
}

inline int CountAlliedFollowup(const Vector3& position,
                               float range,
                               bool includePlayer = false) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !position.IsValid()) return 0;
    const float rangeSqr = std::max(0.0f, range) *
                           std::max(0.0f, range);
    int count = 0;
    for (const auto& ally : GameObjects::AllyHeroes()) {
        if (!Engine::ValidAlly(ally) ||
            (!includePlayer && ally.NetworkId() == player.NetworkId()) ||
            ally.Position().DistanceSqr2D(position) > rangeSqr) {
            continue;
        }
        ++count;
    }
    return count;
}

// Champion-neutral estimate of which ally is most expensive to leave
// unprotected. Controllers still decide what constitutes a threat and which
// spell can peel it; this helper only avoids cloning the same damage/range/HP
// ranking loop in every support or vanguard.
inline float AllyProtectionPriority(const AIHeroClient& ally) {
    if (!Engine::ValidAlly(ally)) return -FLT_MAX;
    const float offense = std::max(
        ally.TotalAttackDamage() * 0.85f,
        ally.AP() * 0.62f);
    const float rangeValue =
        std::max(0.0f, ally.AttackRange() - 175.0f) * 0.22f;
    const float vulnerability =
        (100.0f - ally.HealthPercent()) * 1.35f;
    return offense + rangeValue + vulnerability;
}

inline AIHeroClient SelectProtectionAlly(
    float searchRange,
    int recentlyTargetedNetworkId = 0,
    int recentlyTargetedUntilTick = 0,
    float nearbyThreatWeight = 240.0f,
    float targetedWeight = 520.0f) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return {};
    AIHeroClient best{};
    float bestScore = -FLT_MAX;
    for (const auto& ally : GameObjects::AllyHeroes()) {
        if (!Engine::ValidAlly(ally, searchRange) ||
            ally.NetworkId() == player.NetworkId()) {
            continue;
        }
        float score = AllyProtectionPriority(ally) +
            static_cast<float>(Engine::CountEnemiesAt(
                ally.Position(), 700.0f)) * nearbyThreatWeight;
        if (static_cast<int>(ally.NetworkId()) ==
                recentlyTargetedNetworkId &&
            Now() <= recentlyTargetedUntilTick) {
            score += targetedWeight;
        }
        if (score > bestScore) {
            best = ally;
            bestScore = score;
        }
    }
    return best;
}

// Spell-shield state is champion-neutral. Controllers still decide whether
// consuming it is worthwhile and append their own parry/untargetable states.
inline bool HasSpellShieldOrImmunity(const AIBaseClient& target) {
    return target.IsValid() &&
        (SDK::HasBuffOfType(target, SDK::BuffType::SpellShield) ||
         SDK::HasBuffOfType(target, SDK::BuffType::SpellImmunity) ||
         target.HasBuff("SivirE") ||
         target.HasBuff("NocturneShroudofDarkness") ||
         target.HasBuff("MorganaE") ||
         target.HasBuff("BlackShield") ||
         target.HasBuff("BansheesVeil") ||
         target.HasBuff("EdgeOfNight"));
}

inline bool IsCommonUntargetableOrImmune(const AIBaseClient& target) {
    return !target.IsValid() || target.IsDead() ||
           target.IsInvulnerable() || !target.IsTargetable() ||
           SDK::HasBuffOfType(target, SDK::BuffType::SpellImmunity) ||
           SDK::HasBuffOfType(target, SDK::BuffType::Invulnerability) ||
           target.HasBuff("FioraW") ||
           target.HasBuff("VladimirSanguinePool") ||
           target.HasBuff("FizzE") || target.HasBuff("FizzEIcon") ||
           target.HasBuff("EliseSpiderE") ||
           target.HasBuff("zhonyasringshield") ||
           target.HasBuff("BardRStasis") || target.HasBuff("KayleR");
}

inline bool NearTerrain(const Vector3& position,
                        float radius = 175.0f,
                        int samples = 16) {
    if (!position.IsValid() || position.IsZero()) return false;
    if (SDK::NavMesh::IsWall(position)) return true;
    const int count = std::clamp(samples, 4, 64);
    for (int i = 0; i < count; ++i) {
        const float angle = 2.0f * SharedGeometry::kPi *
                            static_cast<float>(i) /
                            static_cast<float>(count);
        const Vector3 sample{
            position.x + std::cos(angle) * radius,
            position.y,
            position.z + std::sin(angle) * radius,
        };
        if (SDK::NavMesh::IsWall(sample)) return true;
    }
    return false;
}

inline AIBaseClient UnitByNetworkId(int networkId) {
    return networkId != 0
        ? GameObjects::GetUnitByNetworkId<AIBaseClient>(networkId)
        : AIBaseClient{};
}

inline bool ValidHostileUnit(const AIBaseClient& unit,
                             float range = FLT_MAX) {
    const auto player = GameObjects::Player();
    return unit.IsValid() && !unit.IsDead() && unit.IsEnemy() &&
           unit.IsTargetable() && player.IsValid() &&
           player.Position().Distance2D(unit.Position()) <= range;
}

// Cast-range validation differs from a center-to-center proximity query by
// the target's gameplay radius.  Lane and jungle controllers should share
// this exact rule instead of cloning a local ValidFarmUnit wrapper.
inline bool ValidHostileUnitInGameplayRange(const AIBaseClient& unit,
                                            float range) {
    const auto player = GameObjects::Player();
    return unit.IsValid() && !unit.IsDead() && unit.IsEnemy() &&
           unit.IsTargetable() && player.IsValid() &&
           player.Position().Distance2D(unit.Position()) <=
               range + unit.BoundingRadius();
}

inline bool HasNearbyJungleTarget(float range) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    for (const auto& monster : GameObjects::Jungle()) {
        if (monster.IsValid() && !monster.IsDead() &&
            monster.IsTargetable() &&
            player.Position().Distance2D(monster.Position()) <= range) {
            return true;
        }
    }
    return false;
}

inline bool ObjectEventIsAllied(
    const SDK::Events::ObjectEventArgs& args) {
    const auto player = GameObjects::Player();
    return player.IsValid() &&
        (args.Sender.Team == 0 ||
         args.Sender.Team == static_cast<std::uint32_t>(player.Team()));
}

inline bool IsEpicMonster(const AIBaseClient& unit) {
    if (!unit.IsValid()) return false;
    const AIMinionClient monster(unit.Address());
    if (!monster.IsValid()) return false;
    const SDK::JungleType type = monster.GetJungleType();
    return type == SDK::JungleType::Legendary ||
           type == SDK::JungleType::Epic;
}

// Nearby epic-objective presence is a champion-neutral map observation.
// Controllers retain all setup, contest and spell-commit policy locally.
inline bool HasNearbyEpicMonster(float range) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    for (const auto& monster : GameObjects::Jungle()) {
        if (monster.IsValid() && !monster.IsDead() &&
            IsEpicMonster(monster) &&
            player.Position().Distance2D(monster.Position()) <= range) {
            return true;
        }
    }
    return false;
}

// Siege and super minions are the shared high-value lane bodies used by
// formation, execute and wave-preservation policies.  Keep the SDK flag
// interpretation here so full controllers do not each clone it.
inline bool IsLargeLaneMinion(const AIMinionClient& minion) {
    if (!minion.IsValid()) return false;
    const MinionTypes type = minion.GetMinionType();
    return HasFlag(type, MinionTypes::Siege) ||
           HasFlag(type, MinionTypes::Super);
}

inline AIMinionClient SelectJungleTarget(
    float range,
    float currentHealthWeight = 0.15f,
    float epicBonus = 100000.0f) {
    const auto player = GameObjects::Player();
    AIMinionClient best{};
    if (!player.IsValid()) return best;
    float bestScore = -FLT_MAX;
    for (const auto& monster : GameObjects::Jungle()) {
        if (!monster.IsValid() || monster.IsDead() ||
            !monster.IsTargetable() ||
            player.Position().Distance2D(monster.Position()) > range) {
            continue;
        }
        float score = monster.MaxHealth() +
                      monster.Health() * currentHealthWeight;
        if (IsEpicMonster(monster)) score += epicBonus;
        if (score > bestScore) {
            best = monster;
            bestScore = score;
        }
    }
    return best;
}

inline bool Ready(int index) {
    return index >= 0 && index < 4 && Engine::RuntimeSpells[index] &&
           Engine::RuntimeSpells[index]->IsReady();
}

inline bool HasCurrentResource(float amount) {
    return CurrentResource() + 0.5f >= std::max(0.0f, amount);
}

inline float ReadySpellResource(std::initializer_list<int> indices) {
    float total = 0.0f;
    for (const int index : indices) {
        if (index >= 0 && index < 4 && Ready(index)) {
            total += SpellCost(index);
        }
    }
    return total;
}

inline bool HasResourceFor(std::initializer_list<int> indices,
                           float reserve = 0.0f) {
    return HasCurrentResource(
        ReadySpellResource(indices) + std::max(0.0f, reserve));
}

inline bool CastThrottleReady(int index,
                              int defaultHumanizerMs,
                              int fastFollowupMs = -1) {
    if (!Ready(index)) {
        return false;
    }
    const int minimum = fastFollowupMs >= 0
        ? std::max(0, fastFollowupMs)
        : std::max(18, Slider(
              Engine::HumanMenu, "Humanizer", std::max(18, defaultHumanizerMs)));
    const int now = SDK::Variables::TickCount();
    return Engine::LastActionTick <= 0 ||
           now - Engine::LastActionTick >= minimum;
}

// Shared responsive policy for champions whose recast/arrival branches need
// a zero-delay follow-up while ordinary casts still use the 38 ms baseline.
inline bool CastThrottleReady(int index, bool fastFollowup = false) {
    return CastThrottleReady(index, 38, fastFollowup ? 0 : -1);
}

inline Vector3 PredictPosition(const AIBaseClient& target, float delaySeconds) {
    if (!target.IsValid()) return {};
    if (target.IsDashing() && target.PathEnd().IsValid() &&
        !target.PathEnd().IsZero()) {
        return target.PathEnd();
    }
    const auto prediction = SDK::Prediction::GetPrediction(
        target, std::max(0.0f, delaySeconds),
        std::max(25.0f, target.BoundingRadius()));
    const Vector3 predicted = prediction.GetUnitPosition();
    return predicted.IsValid() && !predicted.IsZero()
        ? predicted
        : target.Position();
}

inline bool CursorDirectionAgrees(const Vector3& destination,
                                  float minimumDot = -0.08f) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    const Vector3 desired = SharedGeometry::Direction2D(
        player.Position(), destination);
    const Vector3 cursor = SharedGeometry::Direction2D(
        player.Position(), Game::CursorPos());
    return desired.IsZero() || cursor.IsZero() ||
           desired.Dot(cursor) >= minimumDot;
}

inline bool PredictionAtLeast(const SDK::PredictionOutput& prediction,
                              SDK::HitChance chance) {
    return static_cast<int>(prediction.Hitchance) >=
           static_cast<int>(chance);
}

// Projectile denial is champion-neutral geometry. Controllers still decide
// whether a missile is worth casting, while this helper centralizes the
// shared validity and wall-collision query.
inline bool ProjectileWallBlocks(const Vector3& source,
                                 const Vector3& destination,
                                 float missileRadius) {
    return source.IsValid() && destination.IsValid() &&
           !source.IsZero() && !destination.IsZero() &&
           SDK::Collision::HasProjectileWallCollision(
               source, destination, std::max(0.0f, missileRadius));
}

inline bool ProjectileWallBlocksFromPlayer(const Vector3& destination,
                                           float missileRadius) {
    const auto player = GameObjects::Player();
    return player.IsValid() && ProjectileWallBlocks(
        player.Position(), destination, missileRadius);
}

// Some endpoint spells detonate at the first projectile-intercept barrier
// instead of being deleted. The SDK exposes a segment predicate but no common
// contact point, so bisect the monotone path prefix once here rather than
// cloning Yasuo/Samira/Mel-specific scans in champion controllers.
inline bool ProjectileWallFirstContact(const Vector3& source,
                                       const Vector3& destination,
                                       float missileRadius,
                                       Vector3& contact,
                                       int iterations = 18) {
    contact = {};
    if (!ProjectileWallBlocks(source, destination, missileRadius)) {
        return false;
    }
    const Vector3 delta = destination - source;
    float clearPrefix = 0.0f;
    float blockedPrefix = 1.0f;
    for (int step = 0; step < std::clamp(iterations, 8, 26); ++step) {
        const float middle = (clearPrefix + blockedPrefix) * 0.5f;
        const Vector3 probe = source + delta * middle;
        if (ProjectileWallBlocks(source, probe, missileRadius)) {
            blockedPrefix = middle;
        } else {
            clearPrefix = middle;
        }
    }
    contact = source + delta * blockedPrefix;
    contact.y = destination.y;
    return contact.IsValid() && !contact.IsZero();
}

inline bool ProjectileWallFirstContactFromPlayer(
    const Vector3& destination,
    float missileRadius,
    Vector3& contact,
    int iterations = 18) {
    const auto player = GameObjects::Player();
    return player.IsValid() && ProjectileWallFirstContact(
        player.Position(), destination, missileRadius,
        contact, iterations);
}

inline bool SpellEventNameContains(
    const SDK::Events::ProcessSpellEventArgs& args,
    const char* token) {
    return Engine::TextContains(args.SpellName, token) ||
           Engine::TextContains(args.ScriptName, token) ||
           Engine::TextContains(args.PayloadSpellName, token);
}

inline bool SpellEventNameContainsAny(
    const SDK::Events::ProcessSpellEventArgs& args,
    std::initializer_list<const char*> tokens) {
    for (const char* token : tokens) {
        if (token && token[0] && SpellEventNameContains(args, token)) {
            return true;
        }
    }
    return false;
}

inline bool SpellSlotOrEventNameContainsAny(
    const SDK::Events::ProcessSpellEventArgs& args,
    SDK::SpellSlot slot,
    std::initializer_list<const char*> tokens) {
    return args.Slot == static_cast<int>(slot) ||
           SpellEventNameContainsAny(args, tokens);
}

inline bool ChampionIs(const AIHeroClient& target, const char* name) {
    return target.IsValid() && name && name[0] &&
           _stricmp(target.CharacterName().c_str(), name) == 0;
}

inline bool EnemySpellReady(const AIHeroClient& target, SDK::SpellSlot slot) {
    if (!target.IsValid()) return false;
    const auto spell = target.Spellbook().GetSpell(slot);
    return spell.IsValid() && spell.Level() > 0 &&
           spell.RemainingCooldown(Game::Time()) <= 0.08f;
}

inline bool EnemyFlashReady(const AIHeroClient& target) {
    if (!target.IsValid()) return false;
    for (const SDK::SpellSlot slot : {
             SDK::SpellSlot::Summoner1, SDK::SpellSlot::Summoner2 }) {
        const auto spell = target.Spellbook().GetSpell(slot);
        if (spell.IsValid() &&
            Engine::TextContains(spell.Name().c_str(), "flash") &&
            spell.RemainingCooldown(Game::Time()) <= 0.08f) {
            return true;
        }
    }
    return false;
}

// Common anti-dash zones.  This is a danger query, not a claim that every
// listed effect cancels every dash already in flight.
inline bool HasReadyDashHazardAt(const Vector3& position,
                                 float searchRange = 650.0f) {
    if (!position.IsValid() || position.IsZero()) return false;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy) ||
            enemy.Position().Distance2D(position) > searchRange) {
            continue;
        }
        if ((ChampionIs(enemy, "Poppy") &&
             (enemy.HasBuff("PoppyWZone") ||
              EnemySpellReady(enemy, SDK::SpellSlot::W))) ||
            (ChampionIs(enemy, "Taliyah") &&
             EnemySpellReady(enemy, SDK::SpellSlot::E)) ||
            (ChampionIs(enemy, "Cassiopeia") &&
             EnemySpellReady(enemy, SDK::SpellSlot::W))) {
            return true;
        }
    }
    return false;
}

inline bool MissileEventIsLocal(const SDK::Events::ObjectEventArgs& args) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    const std::uint32_t playerId = static_cast<std::uint32_t>(player.NetworkId());
    if (args.SourceNetworkId != 0) return args.SourceNetworkId == playerId;
    if (args.Source.NetworkId != 0) return args.Source.NetworkId == playerId;
    if (args.Sender.Ptr != 0) {
        const MissileClient missile(args.Sender.Ptr);
        return missile.IsValid() && missile.CasterNetworkId() == player.NetworkId();
    }
    return false;
}

inline int NormalizedCastDelayMs(float castDelay,
                                 int fallbackMs = 250) {
    if (!std::isfinite(castDelay) || castDelay <= 0.0f) {
        return std::max(0, fallbackMs);
    }
    // The event bridge historically exposed seconds, while a few spell
    // payloads arrive already expressed in milliseconds.  Normalize once so
    // champion controllers do not each grow a subtly different conversion.
    return static_cast<int>(castDelay * (castDelay > 10.0f ? 1.0f : 1000.0f));
}

inline bool LikelyHardCrowdControlSpell(
    const SDK::Events::ProcessSpellEventArgs& args) {
    static constexpr std::array<const char*, 18> tokens = {
        "hook", "grab", "knock", "pulverize", "charm", "stun",
        "snare", "root", "bind", "bandagetoss", "rocketgrab",
        "deathsentence", "dredgeline", "darkbinding", "lightbinding",
        "zenithblade", "enchantedcrystalarrow", "curseofthesadmummy",
    };
    for (const char* token : tokens) {
        if (Engine::TextContains(args.SpellName, token) ||
            Engine::TextContains(args.ScriptName, token) ||
            Engine::TextContains(args.PayloadSpellName, token)) {
            return true;
        }
    }
    return false;
}

struct EnemyCastAnalysis {
    AIHeroClient Enemy = {};
    bool Valid = false;
    bool TargetsPlayer = false;
    bool Committed = false;
    bool CrossesPlayer = false;
    bool LikelyHardCrowdControl = false;
    float LineDistance = FLT_MAX;
    int CommitmentUntilTick = 0;
    int LineThreatUntilTick = 0;
};

inline EnemyCastAnalysis AnalyzeEnemyCast(
    const SDK::Events::ProcessSpellEventArgs& args,
    float minimumLineLength = 240.0f,
    float linePadding = 105.0f,
    int commitmentExtraMs = 250,
    int commitmentFallbackMs = 250,
    int commitmentMinimumMs = 240,
    int commitmentMaximumMs = 1500,
    int lineThreatWindowMs = 420) {
    EnemyCastAnalysis result{};
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !args.Sender.IsValid() ||
        IsLocalPlayer(args.Sender)) {
        return result;
    }

    result.Enemy = Engine::EnemyByNetworkId(
        static_cast<int>(args.Sender.NetworkId));
    if (!Engine::ValidEnemy(result.Enemy)) return result;

    result.Valid = true;
    const std::uint32_t playerId =
        static_cast<std::uint32_t>(player.NetworkId());
    result.TargetsPlayer = args.TargetNetworkId == playerId ||
                           args.Target.NetworkId == playerId;
    result.Committed = result.TargetsPlayer || args.IsAutoAttack ||
                       args.CastDelay >= 0.20f;
    const int now = SDK::Variables::TickCount();
    if (result.Committed) {
        const int castMs = NormalizedCastDelayMs(
            args.CastDelay, commitmentFallbackMs);
        result.CommitmentUntilTick = now + std::clamp(
            castMs + commitmentExtraMs,
            commitmentMinimumMs,
            commitmentMaximumMs);
    }

    if (!args.IsAutoAttack && args.StartPosition.IsValid() &&
        args.EndPosition.IsValid() && !args.StartPosition.IsZero() &&
        !args.EndPosition.IsZero() &&
        args.StartPosition.Distance2D(args.EndPosition) > minimumLineLength) {
        const auto projection = SharedGeometry::ProjectPointToSegment2D(
            player.Position(), args.StartPosition, args.EndPosition);
        result.LineDistance = projection.Distance;
        result.CrossesPlayer = projection.Distance <=
            player.BoundingRadius() + std::max(0.0f, linePadding);
        if (result.CrossesPlayer) {
            result.LikelyHardCrowdControl =
                LikelyHardCrowdControlSpell(args);
            result.LineThreatUntilTick = now +
                std::max(0, lineThreatWindowMs);
        }
    }
    return result;
}

enum class ThreatCondition : std::uint8_t {
    None,
    AnnieStunReady,
    TwistedFateGoldCard,
};

struct PointClickThreatRule {
    const char* Champion = "";
    SDK::SpellSlot Slot = SDK::SpellSlot::Unknown;
    float Range = 0.0f;
    ThreatCondition Condition = ThreatCondition::None;
    bool RequireSpellReady = true;
};

inline bool ThreatConditionMet(const AIHeroClient& enemy,
                               ThreatCondition condition) {
    switch (condition) {
    case ThreatCondition::AnnieStunReady:
        return enemy.HasBuff("anniepassiveprimed") ||
               enemy.HasBuff("pyromania_particle");
    case ThreatCondition::TwistedFateGoldCard:
        return enemy.HasBuff("GoldCardPreAttack");
    default:
        return true;
    }
}

// Shared, auditable point-click lockdown registry.  Skillshots such as
// Warwick R and Skarner R deliberately do not belong here; their live paths
// are handled by each controller's incoming-line logic.
inline constexpr std::array<PointClickThreatRule, 18> PointClickThreats = {
    PointClickThreatRule{ "Malzahar", SDK::SpellSlot::R, 700.0f },
    PointClickThreatRule{ "Lissandra", SDK::SpellSlot::R, 550.0f },
    PointClickThreatRule{ "Vi", SDK::SpellSlot::R, 800.0f },
    PointClickThreatRule{ "Nautilus", SDK::SpellSlot::R, 825.0f },
    PointClickThreatRule{ "Pantheon", SDK::SpellSlot::W, 600.0f },
    PointClickThreatRule{ "Maokai", SDK::SpellSlot::W, 525.0f },
    PointClickThreatRule{ "FiddleSticks", SDK::SpellSlot::Q, 575.0f },
    PointClickThreatRule{ "Renekton", SDK::SpellSlot::W, 275.0f },
    PointClickThreatRule{ "Lulu", SDK::SpellSlot::W, 650.0f },
    PointClickThreatRule{ "Rammus", SDK::SpellSlot::E, 325.0f },
    PointClickThreatRule{ "Alistar", SDK::SpellSlot::W, 650.0f },
    PointClickThreatRule{ "Poppy", SDK::SpellSlot::E, 475.0f },
    PointClickThreatRule{ "Sett", SDK::SpellSlot::R, 400.0f },
    PointClickThreatRule{ "Camille", SDK::SpellSlot::R, 475.0f },
    PointClickThreatRule{ "Mordekaiser", SDK::SpellSlot::R, 650.0f },
    PointClickThreatRule{ "Annie", SDK::SpellSlot::Q, 625.0f,
                          ThreatCondition::AnnieStunReady, true },
    PointClickThreatRule{ "Annie", SDK::SpellSlot::R, 600.0f,
                          ThreatCondition::AnnieStunReady, true },
    PointClickThreatRule{ "TwistedFate", SDK::SpellSlot::W, 575.0f,
                          ThreatCondition::TwistedFateGoldCard, false },
};

inline bool HasReadyPointClickThreatAt(const Vector3& position) {
    if (!position.IsValid() || position.IsZero()) return false;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!Engine::ValidEnemy(enemy)) continue;
        for (const auto& rule : PointClickThreats) {
            if (!ChampionIs(enemy, rule.Champion) ||
                position.Distance2D(enemy.Position()) > rule.Range ||
                !ThreatConditionMet(enemy, rule.Condition)) {
                continue;
            }
            if (!rule.RequireSpellReady || EnemySpellReady(enemy, rule.Slot)) {
                return true;
            }
        }
    }
    return false;
}

// Three-stage mobility champions commonly need the same gapcloser capture
// rule but retain champion-specific reactions.  This helper records only the
// neutral event facts; it never decides whether to cast, peel, or disengage.
inline bool CaptureGapcloser(
    const SDK::Events::Gapcloser::GapCloserEventArgs& args,
    int& targetNetworkId,
    Vector3& endpoint,
    int& expireTick,
    float nearbyEndpointRange,
    int lifetimeMs) {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) return false;
    if (!args.IsDirectedToPlayer &&
        (!args.End.IsValid() || args.End.IsZero() ||
         args.End.Distance2D(player.Position()) > nearbyEndpointRange)) {
        return false;
    }
    targetNetworkId = static_cast<int>(args.NetworkId);
    endpoint = args.End;
    expireTick = SDK::Variables::TickCount() + std::max(0, lifetimeMs);
    return true;
}

template <int* TargetNetworkId,
          Vector3* Endpoint,
          int* ExpireTick,
          int NearbyEndpointRange,
          int LifetimeMs>
inline void CaptureGapcloserEvent(
    const SDK::Events::Gapcloser::GapCloserEventArgs& args) {
    (void)CaptureGapcloser(
        args, *TargetNetworkId, *Endpoint, *ExpireTick,
        static_cast<float>(NearbyEndpointRange), LifetimeMs);
}

inline int RemainingMilliseconds(float endTime,
                                 int fallbackMs,
                                 int minimumMs,
                                 int maximumMs) {
    const int raw = endTime > Game::Time()
        ? static_cast<int>((endTime - Game::Time()) * 1000.0f)
        : fallbackMs;
    return std::clamp(raw, minimumMs, maximumMs);
}

inline int BuffExpireTick(const SDK::Events::BuffEventArgs& args,
                          int fallbackMs) {
    return Now() + RemainingMilliseconds(
        args.EndTime, fallbackMs, 80, std::max(5000, fallbackMs * 2));
}

// Interruptable events expose the same target/lifetime facts to every
// champion. Controllers still decide whether they can interrupt at all and
// which spell/sequence is safe to commit.
inline void CaptureInterruptable(
    const SDK::Events::InterruptableSpell::InterruptableTargetEventArgs& args,
    int& targetNetworkId,
    int& expireTick,
    int fallbackMs = 900,
    int minimumMs = 250,
    int maximumMs = 5000) {
    targetNetworkId = static_cast<int>(args.NetworkId);
    expireTick = Now() + RemainingMilliseconds(
        args.EndTime, fallbackMs, minimumMs, maximumMs);
}

template <int* TargetNetworkId,
          int* ExpireTick,
          int FallbackMs = 900,
          int MinimumMs = 250,
          int MaximumMs = 5000>
inline void CaptureInterruptableEvent(
    const SDK::Events::InterruptableSpell::InterruptableTargetEventArgs& args) {
    CaptureInterruptable(
        args, *TargetNetworkId, *ExpireTick,
        FallbackMs, MinimumMs, MaximumMs);
}

} // namespace Plugins::KuroAIO::AI::ControllerHelpers
