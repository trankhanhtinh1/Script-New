#pragma once

// Pure Kai'Sa mechanics for Riot 26.15 / CommunityDragon 16.15.
// Runtime prediction, buff polling, NavMesh and casts remain in the controller.
#include "../AIGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace Plugins::KuroAIO::AI::Controllers::Kaisa::Geometry {

using SharedGeometry::Direction2D;
using SharedGeometry::RankValue;

inline constexpr float kQRange = 600.0f;
inline constexpr float kWRange = 3000.0f;
inline constexpr int kBaseQMissiles = 6;
inline constexpr int kEvolvedQMissiles = 12;
inline constexpr int kMaximumPlasmaStacks = 4;
inline constexpr float kRMinRange = 2000.0f;
inline constexpr float kRMaxRange = 4500.0f;

inline int ClampPlasmaStacks(int stacks) {
    return std::clamp(stacks, 0, kMaximumPlasmaStacks);
}

struct PlasmaState {
    int Stacks = 0;
    int ExpireTick = 0;

    void Observe(int stacks, int expireTick = 0) {
        Stacks = ClampPlasmaStacks(stacks);
        ExpireTick = Stacks > 0 ? std::max(0, expireTick) : 0;
    }
    void Add(int now, int amount = 1) {
        Stacks = std::min(kMaximumPlasmaStacks, Stacks + std::max(0, amount));
        ExpireTick = Stacks > 0 ? now + 4000 : 0;
    }
    void Consume() { Stacks = 0; ExpireTick = 0; }
    void Poll(int now, int observedStacks, bool buffPresent,
              int observedExpireTick = 0) {
        if (buffPresent) {
            Observe(observedStacks, observedExpireTick > now
                ? observedExpireTick : now + 4000);
        } else if (ExpireTick > 0 && now >= ExpireTick) {
            Consume();
        }
    }
};

struct EvolutionState {
    bool QEvolved = false;
    bool WEvolved = false;
    bool EEvolved = false;

    void Poll(bool q, bool w, bool e) {
        QEvolved = q;
        WEvolved = w;
        EEvolved = e;
    }
    bool Any() const { return QEvolved || WEvolved || EEvolved; }
};

inline bool EvolutionAvailable(float bonusAttackDamage,
                               float abilityPower,
                               float bonusAttackSpeedPercent,
                               bool q = false, bool w = false, bool e = false) {
    return (q && bonusAttackDamage >= 100.0f) ||
           (w && abilityPower >= 100.0f) ||
           (e && bonusAttackSpeedPercent >= 100.0f);
}

inline int QMissiles(bool evolved) {
    return evolved ? kEvolvedQMissiles : kBaseQMissiles;
}

// Isolated Q assigns all missiles to one champion.  Subsequent missiles deal
// 25% of the first missile's damage; ordinary multi-target casts only deliver
// one missile to a given unit.
inline float QDamageForTarget(float perMissileDamage,
                              int missileCount,
                              bool isolated) {
    const float first = std::max(0.0f, perMissileDamage);
    if (!isolated) return missileCount > 0 ? first : 0.0f;
    const int count = std::clamp(missileCount, 0, kEvolvedQMissiles);
    if (count <= 0) return 0.0f;
    return first * (1.0f + 0.25f * static_cast<float>(count - 1));
}

// Level-scaled passive explosion model from the pinned CDragon data.  Missing
// health is clamped because live target telemetry can be stale or over 100%.
inline float PlasmaDamage(int level, int stacks,
                          float abilityPower,
                          float targetMissingHealthPercent) {
    const int clampedLevel = std::clamp(level, 1, 18);
    const int clampedStacks = ClampPlasmaStacks(stacks);
    if (clampedStacks <= 0) return 0.0f;
    const float levelT = static_cast<float>(clampedLevel - 1) / 17.0f;
    const float missing = std::clamp(targetMissingHealthPercent, 0.0f, 100.0f);
    const float ap = std::max(0.0f, abilityPower);
    const float base = 4.0f + 26.0f * levelT + ap * 0.12f;
    const float perStack = 1.0f + 7.0f * levelT + ap * 0.03f;
    const float execute = missing * (0.15f + ap * 0.0006f);
    return base + perStack * static_cast<float>(clampedStacks) + execute;
}

inline float RRange(int rank) {
    static constexpr std::array<float, 4> ranges = {
        0.0f, 2000.0f, 2500.0f, 3000.0f,
    };
    // Runtime data has 2000/2500/3000 effective cast ranges.  Rank zero stays
    // zero so an unlearned ultimate cannot be treated as castable.
    return std::clamp(RankValue(ranges, rank), 0.0f, kRMaxRange);
}

inline bool RTargetReachable(float distance, float rankRange,
                             float targetRadius = 0.0f) {
    const float range = std::clamp(rankRange, 0.0f, kRMaxRange);
    return std::isfinite(distance) && distance >= 0.0f && range > 0.0f &&
           distance <= range + std::max(0.0f, targetRadius);
}

inline Vec3 RDashEndpoint(const Vec3& origin,
                          const Vec3& target,
                          float offset = 225.0f) {
    const Vec3 direction = Direction2D(origin, target);
    if (direction.IsZero()) return {};
    return target - direction * std::max(0.0f, offset);
}

struct REndpointContext {
    bool Valid = false;
    bool MarkedTarget = false;
    bool TerrainSafe = false;
    bool TurretSafe = false;
    bool EnemyCountSafe = false;
    bool AllowTurret = false;
    int NearbyEnemies = 0;
    int MaximumEnemies = 2;
};

inline bool SafeREndpoint(const REndpointContext& context) {
    return context.Valid && context.MarkedTarget && context.TerrainSafe &&
           (context.AllowTurret || context.TurretSafe) &&
           context.NearbyEnemies <= std::max(0, context.MaximumEnemies);
}

inline bool ShouldCastR(bool manual, bool lethal, bool peel,
                        bool alliedFollowup, bool endpointSafe) {
    if (!endpointSafe) return false;
    return manual || lethal || peel || (alliedFollowup && !lethal);
}

} // namespace Plugins::KuroAIO::AI::Controllers::Kaisa::Geometry
