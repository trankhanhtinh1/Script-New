#pragma once

#include "AIControllerHelpers.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>

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

inline float SpellProjectileWallRadius(int index, float fallbackRadius) {
    if (index < 0 || index >= 4 || !Engine::ActiveProfile) {
        return fallbackRadius;
    }
    const auto& spec = Engine::ResolvedSpecs[index];
    if (!spec.ProjectileWall) return fallbackRadius;
    // SpellSpec::Width is the prediction diameter; collision consumes the
    // missile radius. Ground/area widths never enter this path.
    return spec.Kind == CastKind::Line
        ? std::max(fallbackRadius, spec.Width * 0.5f)
        : fallbackRadius;
}

inline bool PositionProjectileWall(int index,
                                   const Vector3& destination,
                                   float fallbackRadius) {
    return destination.IsValid() && !destination.IsZero() &&
           Engine::ActiveProfile && index >= 0 && index < 4 &&
           Engine::ResolvedSpecs[index].ProjectileWall &&
           ControllerHelpers::ProjectileWallBlocksFromPlayer(
               destination,
               SpellProjectileWallRadius(index, fallbackRadius));
}

inline bool TargetProjectileWall(int index,
                                 const AIBaseClient& target,
                                 float fallbackRadius) {
    return target.IsValid() &&
           PositionProjectileWall(index, target.Position(), fallbackRadius);
}

inline bool PredictionProjectileWall(int index,
                                     const SDK::PredictionOutput& prediction,
                                     float fallbackRadius) {
    return PositionProjectileWall(
        index, prediction.GetCastPosition(), fallbackRadius);
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

// The current marksman controllers below all use projectile basic attacks.
// Keep route scoring and temporary focus aligned with the orbwalker's own
// projectile-wall rejection so a wall cannot turn a forced target into a
// dead focus. Non-projectile marksmen keep their champion-specific handling.
inline bool OrbwalkerAttackProjectileBlocked(const AIBaseClient& target) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !target.IsValid()) return false;
    static constexpr std::array<SDK::ChampionId, 24> projectileMarksmen = {
        SDK::ChampionId::Aphelios, SDK::ChampionId::Ashe,
        SDK::ChampionId::Caitlyn, SDK::ChampionId::Corki,
        SDK::ChampionId::Draven, SDK::ChampionId::Ezreal,
        SDK::ChampionId::Jhin, SDK::ChampionId::Jinx,
        SDK::ChampionId::Kaisa, SDK::ChampionId::Kalista,
        SDK::ChampionId::KogMaw, SDK::ChampionId::Lucian,
        SDK::ChampionId::MissFortune, SDK::ChampionId::Senna,
        SDK::ChampionId::Sivir, SDK::ChampionId::Smolder,
        SDK::ChampionId::Tristana, SDK::ChampionId::Twitch,
        SDK::ChampionId::Varus, SDK::ChampionId::Vayne,
        SDK::ChampionId::Xayah, SDK::ChampionId::Yunara,
        SDK::ChampionId::Graves, SDK::ChampionId::Quinn,
    };
    const SDK::ChampionId playerChampionId =
        SDK::ChampionIdFromName(player.CharacterName().c_str());
    for (const SDK::ChampionId champion : projectileMarksmen) {
        if (playerChampionId == champion) {
            return ControllerHelpers::ProjectileWallBlocksFromPlayer(
                target.Position(), 0.0f);
        }
    }
    return false;
}

inline bool LocalAttackAvailable(const AIBaseClient& target,
                                 float bonusRange = 0.0f) {
    return ControllerHelpers::OrbwalkerTargets(target) &&
           ControllerHelpers::InAutoAttackRange(target, bonusRange) &&
           !OrbwalkerAttackProjectileBlocked(target) &&
           Orbwalker::CanAttack();
}

// A spell should not steal a high-value attack merely because the orbwalker
// is a few milliseconds short of CanAttack().  This is deliberately tied to
// the orbwalker's real target; an unrelated unit must not block a legal cast.
inline bool LocalAttackReadySoon(const AIBaseClient& target,
                                 int horizonMs = 220,
                                 float bonusRange = 0.0f) {
    if (!ControllerHelpers::OrbwalkerTargets(target) ||
        !ControllerHelpers::InAutoAttackRange(target, bonusRange) ||
        OrbwalkerAttackProjectileBlocked(target)) {
        return false;
    }
    if (Orbwalker::CanAttack()) return true;
    const int remaining = Orbwalker::AttackCooldownRemaining();
    return remaining > 0 && remaining <= std::max(0, horizonMs);
}

inline float BuffRemainingMs(const AIBaseClient& target,
                             const char* buffName) {
    if (!target.IsValid() || !buffName || !buffName[0] ||
        !target.HasBuff(buffName)) {
        return 0.0f;
    }
    return std::max(0.0f, CoreBuffs::GetBuffRemainingTime(
        target.Address(), buffName, SDK::Game::Time()) * 1000.0f);
}

inline bool OrbwalkerAttackRoute(const AIBaseClient& target,
                                 float bonusRange = 0.0f) {
    // Reachability and current selection are different facts.  Any clean unit
    // inside the live attack range is a route the orbwalker can take; the
    // current orbwalker target receives its own stickiness bonus in
    // SelectReachableEnemy and must not be the only attackable candidate.
    return Orbwalker::AttackEnabled() && target.IsValid() &&
           !target.IsDead() && target.IsEnemy() && target.IsTargetable() &&
           !ControllerHelpers::IsCommonUntargetableOrImmune(target) &&
           !OrbwalkerAttackProjectileBlocked(target) &&
           ControllerHelpers::InAutoAttackRange(target, bonusRange);
}

inline bool ImmediateAttackKillRoute(const AIHeroClient& target) {
    return OrbwalkerAttackRoute(target) && Orbwalker::CanAttack() &&
           AutoDamage(target) >= target.Health() + target.AllShield();
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
        ControllerHelpers::IsCommonUntargetableOrImmune(target) ||
        OrbwalkerAttackProjectileBlocked(target)) {
        return false;
    }
    const auto current = Orbwalker::ForceTarget();
    if (!current.IsValid() || current.NetworkId() != target.NetworkId()) {
        Orbwalker::ForceTarget(AttackableUnit(target.Handle()));
    }
    ownedNetworkId = static_cast<int>(target.NetworkId());
    ownedUntilTick = ControllerHelpers::Now() + std::max(0, lifetimeMs);
    return true;
}

inline bool ForceImmediateAttackKill(const AIHeroClient& target,
                                     int lifetimeMs,
                                     int& ownedNetworkId,
                                     int& ownedUntilTick) {
    return ImmediateAttackKillRoute(target) &&
        SetTemporaryOrbwalkerFocus(
            target, ControllerHelpers::AutoAttackRange(target), lifetimeMs,
            ownedNetworkId, ownedUntilTick);
}

inline void ClearTemporaryOrbwalkerFocus(int& ownedNetworkId,
                                         int& ownedUntilTick) {
    if (ownedNetworkId != 0) {
        const auto current = Orbwalker::ForceTarget();
        if (!current.IsValid() ||
            static_cast<int>(current.NetworkId()) == ownedNetworkId) {
            Orbwalker::ForceTarget(AttackableUnit());
        }
    }
    ownedNetworkId = 0;
    ownedUntilTick = 0;
}

inline AIHeroClient OwnedOrbwalkerFocus(int& ownedNetworkId,
                                       int& ownedUntilTick,
                                       float range) {
    if (ownedNetworkId == 0) return {};
    if (ControllerHelpers::Now() > ownedUntilTick) {
        ClearTemporaryOrbwalkerFocus(ownedNetworkId, ownedUntilTick);
        return {};
    }
    const auto target = ControllerHelpers::HeroByNetworkId(ownedNetworkId);
    if (!Engine::ValidEnemy(target, range) ||
        ControllerHelpers::IsCommonUntargetableOrImmune(target) ||
        OrbwalkerAttackProjectileBlocked(target)) {
        ClearTemporaryOrbwalkerFocus(ownedNetworkId, ownedUntilTick);
        return {};
    }
    return target;
}

inline bool RedirectBeforeAttackToFocus(
    SDK::OrbwalkingActionArgs& args,
    const AIHeroClient& target,
    float bonusRange = 0.0f) {
    if (!Engine::ValidEnemy(target) ||
        ControllerHelpers::IsCommonUntargetableOrImmune(target) ||
        OrbwalkerAttackProjectileBlocked(target) ||
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

inline bool RecentlyAttackedTarget(const AIBaseClient& target,
                                   int lastTargetNetworkId,
                                   int lastAttackTick,
                                   int windowMs) {
    return target.IsValid() && lastAttackTick > 0 &&
           ControllerHelpers::Now() - lastAttackTick <=
               std::max(0, windowMs) &&
           lastTargetNetworkId ==
               static_cast<int>(target.NetworkId());
}

inline void CaptureAfterAttackAndReleaseOwnedFocus(
    SDK::OrbwalkingActionArgs& args,
    int& lastTargetNetworkId,
    int& lastAttackTick,
    int& ownedTargetNetworkId,
    int& ownedUntilTick) {
    (void)ControllerHelpers::CaptureAfterAttack(
        args, lastTargetNetworkId, lastAttackTick);
    if (lastTargetNetworkId == ownedTargetNetworkId) {
        ClearTemporaryOrbwalkerFocus(
            ownedTargetNetworkId, ownedUntilTick);
    }
}

template <int* LastTargetNetworkId,
          int* LastAttackTick,
          int* OwnedTargetNetworkId,
          int* OwnedUntilTick>
inline void CaptureAfterAttackAndReleaseOwnedFocusEvent(
    SDK::OrbwalkingActionArgs& args) {
    CaptureAfterAttackAndReleaseOwnedFocus(
        args, *LastTargetNetworkId, *LastAttackTick,
        *OwnedTargetNetworkId, *OwnedUntilTick);
}

inline bool CastThrottlePassed(int lastCastTick, int minimumMs) {
    return lastCastTick <= 0 ||
           ControllerHelpers::Now() - lastCastTick >= std::max(0, minimumMs);
}

} // namespace Plugins::KuroAIO::AI::MarksmanControllerHelpers
