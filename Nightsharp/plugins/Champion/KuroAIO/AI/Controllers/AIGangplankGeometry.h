#pragma once

#include "../AIGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace Plugins::KuroAIO::AI::Controllers::Gangplank::Geometry {

inline constexpr float kQRange = 625.0f;
inline constexpr float kEPlacementRange = 1000.0f;
inline constexpr float kBarrelExplosionRadius = 325.0f;
inline constexpr float kBarrelChainRange = 650.0f;
inline constexpr float kRRadius = 525.0f;
inline constexpr int kBarrelLifetimeMs = 25000;
inline constexpr int kPassiveCooldownMs = 15000;
inline constexpr int kRDurationMs = 8000;
inline constexpr int kRWaveCount = 12;
inline constexpr std::size_t kMaximumTrackedBarrels = 8;

inline int BarrelTickPeriodMs(int level) {
    if (level >= 13) return 500;
    if (level >= 7) return 1000;
    return 2000;
}

inline int BarrelMaximumAmmo(int rank) {
    static constexpr std::array<int, 6> values{ 0, 3, 3, 4, 4, 5 };
    return values[std::clamp(rank, 0, 5)];
}

inline int BarrelRechargeMs(int rank) {
    static constexpr std::array<int, 6> values{ 0, 17000, 16000, 15000, 14000, 13000 };
    return values[std::clamp(rank, 0, 5)];
}

struct TrialByFireState {
    bool Ready = true;
    int ReadyAt = 0;
};

inline TrialByFireState NormalizeTrialByFire(TrialByFireState state, int now) {
    if (!state.Ready && state.ReadyAt > 0 && now >= state.ReadyAt) return { true, 0 };
    return state;
}

inline TrialByFireState ConsumeTrialByFire(int now) {
    return { false, now + kPassiveCooldownMs };
}

inline TrialByFireState RefreshTrialByFireFromBarrel() { return { true, 0 }; }

inline float TrialByFireRawDamage(int level, float bonusAttackDamage) {
    const float t = static_cast<float>(std::clamp(level, 1, 18) - 1) / 17.0f;
    return 50.0f + 200.0f * t + std::max(0.0f, bonusAttackDamage);
}

struct BarrelState {
    int NetworkId = 0;
    Vec3 Position{};
    int SpawnTick = 0;
    int LastSeenTick = 0;
    int ObservedHealth = 3;
    int ObservedHealthTick = 0;
    bool Allied = false;
    bool Alive = false;
};

inline int PredictedBarrelHealth(const BarrelState& barrel, int level, int atTick) {
    if (!barrel.Alive || barrel.NetworkId == 0 || atTick < barrel.SpawnTick ||
        atTick >= barrel.SpawnTick + kBarrelLifetimeMs) return 0;
    const int anchor = barrel.ObservedHealthTick > 0 ? barrel.ObservedHealthTick : barrel.SpawnTick;
    const int anchorHealth = std::clamp(barrel.ObservedHealth, 1, 3);
    if (atTick <= anchor) return anchorHealth;
    const int ticks = (atTick - anchor) / BarrelTickPeriodMs(level);
    return std::max(1, anchorHealth - ticks);
}

inline int MillisecondsUntilBarrelOne(const BarrelState& barrel, int level, int now) {
    if (!barrel.Alive) return 0;
    if (PredictedBarrelHealth(barrel, level, now) <= 1) return 0;
    const int period = BarrelTickPeriodMs(level);
    const int anchor = barrel.ObservedHealthTick > 0 ? barrel.ObservedHealthTick : barrel.SpawnTick;
    const int health = std::clamp(barrel.ObservedHealth, 1, 3);
    return std::max(0, anchor + (health - 1) * period - now);
}

inline bool BarrelConnected(const BarrelState& left, const BarrelState& right) {
    return left.Alive && right.Alive && left.NetworkId != right.NetworkId &&
        left.Position.IsValid() && right.Position.IsValid() &&
        left.Position.Distance2D(right.Position) <= kBarrelChainRange;
}

struct BarrelChain {
    std::array<bool, kMaximumTrackedBarrels> Reached{};
    int Count = 0;
};

inline BarrelChain BuildBarrelChain(
    const std::array<BarrelState, kMaximumTrackedBarrels>& barrels,
    std::size_t source) {
    BarrelChain result{};
    if (source >= barrels.size() || !barrels[source].Alive) return result;
    result.Reached[source] = true;
    result.Count = 1;
    bool changed = true;
    while (changed) {
        changed = false;
        for (std::size_t i = 0; i < barrels.size(); ++i) {
            if (!result.Reached[i]) continue;
            for (std::size_t j = 0; j < barrels.size(); ++j) {
                if (result.Reached[j] || !BarrelConnected(barrels[i], barrels[j])) continue;
                result.Reached[j] = true;
                ++result.Count;
                changed = true;
            }
        }
    }
    return result;
}

inline bool ChainHitsPosition(
    const std::array<BarrelState, kMaximumTrackedBarrels>& barrels,
    const BarrelChain& chain,
    const Vec3& position,
    float targetRadius = 0.0f) {
    if (!position.IsValid()) return false;
    for (std::size_t i = 0; i < barrels.size(); ++i) {
        if (chain.Reached[i] && barrels[i].Alive &&
            barrels[i].Position.Distance2D(position) <=
                kBarrelExplosionRadius + std::max(0.0f, targetRadius)) return true;
    }
    return false;
}

inline Vec3 ChainPlacement(const Vec3& anchor, const Vec3& desired) {
    if (!anchor.IsValid() || !desired.IsValid()) return {};
    const Vec3 direction = SharedGeometry::Direction2D(anchor, desired);
    if (direction.IsZero()) return anchor;
    const float distance = std::min(kBarrelChainRange - 20.0f,
                                    anchor.Distance2D(desired));
    return anchor + direction * std::max(0.0f, distance);
}

struct BarrelTriggerContext {
    bool BarrelAlive = false;
    bool InTriggerRange = false;
    bool AttackWindingUp = false;
    bool ExactAttackTarget = false;
    bool TargetHitByChain = false;
    bool EnemyCanWinRace = false;
    bool FarmValue = false;
    bool Lethal = false;
    int PredictedHealthAtImpact = 3;
};

inline bool MayTriggerBarrel(const BarrelTriggerContext& context) {
    if (!context.BarrelAlive || !context.InTriggerRange ||
        context.PredictedHealthAtImpact != 1 || context.EnemyCanWinRace ||
        (!context.TargetHitByChain && !context.FarmValue)) return false;
    if (context.AttackWindingUp && !context.ExactAttackTarget && !context.Lethal) return false;
    return true;
}

struct CleanseContext {
    bool Ready = false;
    bool RemovableCrowdControl = false;
    bool Airborne = false;
    bool InStasis = false;
    bool AttackWindingUp = false;
    bool LethalDanger = false;
    int RemainingCrowdControlMs = 0;
    int MinimumCrowdControlMs = 550;
};

inline bool ShouldCastRemoveScurvy(const CleanseContext& context) {
    if (!context.Ready || context.InStasis || context.Airborne ||
        !context.RemovableCrowdControl) return false;
    if (context.RemainingCrowdControlMs < std::max(0, context.MinimumCrowdControlMs) &&
        !context.LethalDanger) return false;
    return !context.AttackWindingUp || context.LethalDanger;
}

inline float RemoveScurvyRawHeal(int rank, float abilityPower, float missingHealth) {
    static constexpr std::array<float, 6> base{ 0.0f, 45.0f, 70.0f, 95.0f, 120.0f, 145.0f };
    return base[std::clamp(rank, 0, 5)] + 0.90f * std::max(0.0f, abilityPower) +
           0.13f * std::max(0.0f, missingHealth);
}

inline float ParrrleyRawDamage(int rank, float totalAttackDamage) {
    static constexpr std::array<float, 6> base{ 0.0f, 10.0f, 40.0f, 70.0f, 100.0f, 130.0f };
    return base[std::clamp(rank, 0, 5)] + std::max(0.0f, totalAttackDamage);
}

inline float PowderKegRawChampionDamage(int rank, float triggeringAttackRawDamage) {
    static constexpr std::array<float, 6> bonus{ 0.0f, 75.0f, 95.0f, 115.0f, 135.0f, 155.0f };
    return std::max(0.0f, triggeringAttackRawDamage) + bonus[std::clamp(rank, 0, 5)];
}

struct UltimateUpgrades {
    bool DeathsDaughter = false;
    bool FireAtWill = false;
    bool RaiseMorale = false;
};

inline float CannonBarrageWaveRawDamage(int rank, float abilityPower) {
    static constexpr std::array<float, 4> base{ 0.0f, 40.0f, 70.0f, 100.0f };
    return base[std::clamp(rank, 0, 3)] + 0.10f * std::max(0.0f, abilityPower);
}

inline float CannonBarrageConservativeRawDamage(int rank, float abilityPower,
                                                int expectedWaves,
                                                UltimateUpgrades upgrades = {}) {
    const int waves = std::clamp(expectedWaves, 0, kRWaveCount);
    float damage = CannonBarrageWaveRawDamage(rank, abilityPower) * waves;
    if (upgrades.DeathsDaughter && waves > 0) {
        static constexpr std::array<float, 4> base{ 0.0f, 120.0f, 210.0f, 300.0f };
        damage += base[std::clamp(rank, 0, 3)] + 0.30f * std::max(0.0f, abilityPower);
    }
    return damage;
}

struct RPolicyContext {
    bool Ready = false;
    bool PositionValid = false;
    bool ManualOwnership = false;
    bool TargetInLocalCombat = false;
    bool TargetImmobile = false;
    bool TargetLikelyToRemain = false;
    bool Lethal = false;
    bool AllyInDanger = false;
    bool ObjectiveFight = false;
    int EnemiesInZone = 0;
    int AlliesInZone = 0;
    int MinimumEnemies = 2;
};

inline bool ShouldCastCannonBarrage(const RPolicyContext& context) {
    if (!context.Ready || !context.PositionValid || context.ManualOwnership ||
        context.EnemiesInZone <= 0) return false;
    if (context.Lethal && (context.TargetImmobile || context.TargetLikelyToRemain)) return true;
    if (context.AllyInDanger && context.AlliesInZone > 0 &&
        context.EnemiesInZone >= context.AlliesInZone) return true;
    if (context.ObjectiveFight && context.EnemiesInZone >= 2) return true;
    return !context.TargetInLocalCombat &&
        context.EnemiesInZone >= std::max(2, context.MinimumEnemies) &&
        (context.TargetImmobile || context.TargetLikelyToRemain);
}

} // namespace Plugins::KuroAIO::AI::Controllers::Gangplank::Geometry
