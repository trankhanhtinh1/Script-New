#pragma once

#include "AIControllerHelpers.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace Plugins::KuroAIO::AI::MarksmanControllerHelpers {

inline bool IsImmobile(const AIBaseClient& target) {
    return target.IsValid() &&
        (Engine::IsHardCrowdControlled(target) ||
         target.MoveSpeed() < 50.0f);
}

inline bool IsEscaping(const AIBaseClient& target,
                       float predictionSeconds = 0.35f) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !target.IsValid()) return false;
    const Vector3 predicted = ControllerHelpers::PredictPosition(
        target, predictionSeconds);
    return predicted.IsValid() && !predicted.IsZero() &&
           predicted.Distance2D(player.Position()) >
               target.Position().Distance2D(player.Position()) + 30.0f;
}

inline float ThreatPriority(const AIHeroClient& target) {
    if (!target.IsValid()) return 0.0f;
    const float offense = std::max(
        target.TotalAttackDamage() * 0.58f,
        target.AP() * 0.42f);
    const float rangeBonus = std::max(0.0f, target.AttackRange() - 400.0f) * 0.08f;
    return offense + rangeBonus;
}

inline float AutoDamage(const AIBaseClient& target) {
    const auto player = GameObjects::Player();
    return player.IsValid() && target.IsValid()
        ? SDK::Damage::GetAutoAttackDamage(player, target, true)
        : 0.0f;
}

inline float SpellDamage(int index, const AIBaseClient& target) {
    return index >= 0 && index < 4 && target.IsValid() &&
           Engine::RuntimeSpells[index] &&
           Engine::RuntimeSpells[index]->IsReady()
        ? std::max(0.0f, Engine::RuntimeSpells[index]->GetDamage(target))
        : 0.0f;
}

inline float EstimatedDamage(const AIBaseClient& target,
                             const std::array<bool, 4>& reachable,
                             int expectedAutos = 1) {
    float result = AutoDamage(target) * static_cast<float>(
        std::max(0, expectedAutos));
    for (int index = 0; index < 4; ++index) {
        if (reachable[static_cast<std::size_t>(index)]) {
            result += SpellDamage(index, target);
        }
    }
    return result;
}

inline MarksmanTargeting::TargetContext BaseTargetContext(
    const AIHeroClient& target,
    float estimatedDamage) {
    MarksmanTargeting::TargetContext context{};
    context.Valid = target.IsValid() && !target.IsDead();
    context.Targetable = target.IsTargetable();
    context.DamageImmune =
        ControllerHelpers::IsCommonUntargetableOrImmune(target);
    context.CrowdControlled = IsImmobile(target);
    context.Dashing = target.IsDashing();
    context.Escaping = IsEscaping(target);
    context.SpellShield =
        ControllerHelpers::HasSpellShieldOrImmunity(target);
    context.EffectiveHealth = std::max(
        1.0f, target.Health() + target.AllShield());
    context.EstimatedDamage = std::max(0.0f, estimatedDamage);
    context.Killable = context.EstimatedDamage >= context.EffectiveHealth;
    context.Priority = ThreatPriority(target);
    return context;
}

inline bool PredictionHits(int index,
                           const AIBaseClient& target,
                           SDK::HitChance chance,
                           bool requireNoCollision,
                           SDK::PredictionOutput* output = nullptr) {
    if (index < 0 || index >= 4 || !target.IsValid() ||
        !Engine::RuntimeSpells[index]) {
        return false;
    }
    const auto prediction = Engine::RuntimeSpells[index]->GetPrediction(target);
    if (output) *output = prediction;
    return ControllerHelpers::PredictionAtLeast(prediction, chance) &&
           (!requireNoCollision || prediction.CollisionObjects.empty());
}

inline bool PredictionProjectileWall(int index,
                                     const SDK::PredictionOutput& prediction,
                                     float fallbackRadius) {
    const Vector3 cast = prediction.GetCastPosition();
    const float radius = index >= 0 && index < 4 && Engine::ActiveProfile
        ? std::max(fallbackRadius, Engine::ResolvedSpecs[index].Width)
        : fallbackRadius;
    return cast.IsValid() && !cast.IsZero() &&
           ControllerHelpers::ProjectileWallBlocksFromPlayer(cast, radius);
}

inline bool CastPredictedClear(int index,
                               const AIBaseClient& target,
                               SDK::HitChance chance,
                               bool requireNoCollision,
                               float wallRadius) {
    SDK::PredictionOutput prediction{};
    if (!PredictionHits(index, target, chance,
                        requireNoCollision, &prediction) ||
        PredictionProjectileWall(index, prediction, wallRadius)) {
        return false;
    }
    return Engine::ControllerCastPosition(index, prediction.GetCastPosition());
}

inline bool CanUse(int index,
                   Mode mode,
                   bool allowDuringWindup = false) {
    return ControllerHelpers::ControllerSpellAvailable(
        index, mode, allowDuringWindup);
}

inline bool ManualUltimatePressed() {
    return Key(Engine::AutomaticMenu, "ManualR", false);
}

inline bool LocalAttackAvailable(const AIBaseClient& target,
                                 float bonusRange = 0.0f) {
    return ControllerHelpers::OrbwalkerTargets(target) &&
           ControllerHelpers::InAutoAttackRange(target, bonusRange) &&
           Orbwalker::CanAttack();
}

inline bool OrbwalkerAttackRoute(const AIBaseClient& target,
                                 float bonusRange = 0.0f) {
    return ControllerHelpers::OrbwalkerTargets(target) &&
           ControllerHelpers::InAutoAttackRange(target, bonusRange);
}

// Champion controllers may temporarily steer the orbwalker when an AA has
// explicit spell value (Ezreal W, Caitlyn trap headshot, Varus Blight). The
// caller owns the lifetime state. Clearing verifies ownership first so this
// plugin never erases a newer focus installed elsewhere.
inline bool SetTemporaryOrbwalkerFocus(const AIHeroClient& target,
                                       float allowedRange,
                                       int lifetimeMs,
                                       int& ownedNetworkId,
                                       int& ownedUntilTick) {
    if (!Engine::ValidEnemy(target, allowedRange) ||
        ControllerHelpers::IsCommonUntargetableOrImmune(target)) {
        return false;
    }
    Orbwalker::ForceTarget(AttackableUnit(target.Handle()));
    ownedNetworkId = static_cast<int>(target.NetworkId());
    ownedUntilTick = ControllerHelpers::Now() + std::max(0, lifetimeMs);
    return true;
}

inline void ClearTemporaryOrbwalkerFocus(int& ownedNetworkId,
                                         int& ownedUntilTick) {
    if (ownedNetworkId != 0) {
        const auto current = Orbwalker::ForceTarget();
        if (current.IsValid() &&
            static_cast<int>(current.NetworkId()) == ownedNetworkId) {
            Orbwalker::ForceTarget(AttackableUnit());
        }
    }
    ownedNetworkId = 0;
    ownedUntilTick = 0;
}

inline AIHeroClient OwnedOrbwalkerFocus(int ownedNetworkId,
                                       int ownedUntilTick,
                                       float range) {
    if (ownedNetworkId == 0 ||
        ControllerHelpers::Now() > ownedUntilTick) return {};
    const auto target = ControllerHelpers::HeroByNetworkId(ownedNetworkId);
    return Engine::ValidEnemy(target, range) ? target : AIHeroClient{};
}

inline bool RedirectBeforeAttackToFocus(
    SDK::OrbwalkingActionArgs& args,
    const AIHeroClient& target,
    float bonusRange = 0.0f) {
    if (!Engine::ValidEnemy(target) ||
        ControllerHelpers::IsCommonUntargetableOrImmune(target) ||
        !ControllerHelpers::InAutoAttackRange(target, bonusRange)) {
        return false;
    }
    if (!args.Target.IsValid() ||
        args.Target.NetworkId() != target.NetworkId()) {
        Orbwalker::ForceTarget(AttackableUnit(target.Handle()));
        args.Target = AttackableUnit(target.Handle());
        args.Position = target.Position();
    }
    return true;
}

inline bool CastThrottlePassed(int lastCastTick, int minimumMs) {
    return lastCastTick <= 0 ||
           ControllerHelpers::Now() - lastCastTick >= std::max(0, minimumMs);
}

} // namespace Plugins::KuroAIO::AI::MarksmanControllerHelpers
