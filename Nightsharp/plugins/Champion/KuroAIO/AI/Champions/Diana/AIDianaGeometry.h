#pragma once

// Pure Diana mechanics: Moonlight marks, Crescent Strike prediction windows,
// Pale Cascade shield/orb timing, Lunar Rush endpoints and Moonfall safety.
// Runtime object, buff and terrain observations remain in the controller.
#include "../../AIGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace Plugins::KuroAIO::AI::Controllers::Diana::Geometry {

using SharedGeometry::Direction2D;
using SharedGeometry::ProjectPointToSegment2D;

inline constexpr float kQRange = 825.0f;
inline constexpr float kQWidth = 70.0f;
inline constexpr float kQCastDelay = 0.25f;
inline constexpr float kQSpeed = 1400.0f;
inline constexpr float kQMarkSeconds = 3.0f;
inline constexpr float kWRange = 200.0f;
inline constexpr float kWOrbRadius = 200.0f;
inline constexpr float kWShieldSeconds = 5.0f;
inline constexpr float kERange = 825.0f;
inline constexpr float kEImpactRadius = 160.0f;
inline constexpr float kRRadius = 475.0f;
inline constexpr float kRPullSeconds = 1.0f;
inline constexpr float kRResetSeconds = 1.5f;

struct MoonlightMark {
    int TargetId = 0;
    int AppliedTick = 0;
    int ExpiresTick = 0;
    bool Active = false;
};

inline MoonlightMark ApplyMoonlightMark(int targetId, int nowMs) {
    if (targetId == 0) return {};
    return {targetId, nowMs, nowMs + static_cast<int>(kQMarkSeconds * 1000.0f), true};
}

inline bool MarkActive(const MoonlightMark& mark, int targetId, int nowMs) {
    return mark.Active && mark.TargetId == targetId &&
           nowMs <= mark.ExpiresTick;
}

inline MoonlightMark ConsumeMoonlightMark(MoonlightMark mark, int targetId) {
    if (mark.TargetId == targetId) mark = {};
    return mark;
}

inline MoonlightMark ReconcileMark(MoonlightMark mark, bool buffPresent,
                                   int targetId, int nowMs) {
    if (buffPresent && targetId != 0) {
        mark.TargetId = targetId;
        mark.ExpiresTick = std::max(mark.ExpiresTick,
            nowMs + static_cast<int>(kQMarkSeconds * 1000.0f));
        mark.AppliedTick = nowMs;
        mark.Active = true;
    } else if (!buffPresent && mark.TargetId == targetId) {
        mark = {};
    } else if (mark.Active && nowMs > mark.ExpiresTick) {
        mark = {};
    }
    return mark;
}

inline int PassiveAttackIndex(int completedAttacks) {
    return ((completedAttacks % 3) + 3) % 3 + 1;
}

inline bool PassiveReady(const int completedAttacks) {
    return PassiveAttackIndex(completedAttacks) == 3;
}

inline void RecordBasicAttack(int& completedAttacks) {
    completedAttacks = (std::max(0, completedAttacks) + 1) % 3;
}

inline float PassiveRawDamage(int level, float totalAttackDamage,
                             float abilityPower) {
    static constexpr std::array<float, 4> adRatio{0.0f, 0.40f, 0.55f, 0.70f};
    static constexpr std::array<float, 4> apRatio{0.0f, 0.20f, 0.30f, 0.40f};
    const int tier = level >= 16 ? 3 : level >= 11 ? 2 : level >= 6 ? 1 : 0;
    const float base = level >= 16 ? 80.0f : level >= 11 ? 60.0f :
                       level >= 6 ? 40.0f : 20.0f;
    return base + std::max(0.0f, totalAttackDamage) * adRatio[tier] +
           std::max(0.0f, abilityPower) * apRatio[tier];
}

inline float QRawDamage(int rank, float abilityPower) {
    static constexpr std::array<float, 6> base{0.0f, 60.0f, 85.0f, 110.0f,
                                                135.0f, 160.0f};
    return base[std::clamp(rank, 0, 5)] + std::max(0.0f, abilityPower) * 0.70f;
}

inline float WRawDamagePerOrb(int rank, float abilityPower) {
    static constexpr std::array<float, 6> base{0.0f, 18.0f, 30.0f, 42.0f,
                                                54.0f, 66.0f};
    return base[std::clamp(rank, 0, 5)] + std::max(0.0f, abilityPower) * 0.15f;
}

inline float WRawShield(int rank, float abilityPower) {
    static constexpr std::array<float, 6> base{0.0f, 40.0f, 55.0f, 70.0f,
                                                85.0f, 100.0f};
    return base[std::clamp(rank, 0, 5)] + std::max(0.0f, abilityPower) * 0.30f;
}

inline float ERawDamage(int rank, float abilityPower) {
    static constexpr std::array<float, 6> base{0.0f, 50.0f, 70.0f, 90.0f,
                                                110.0f, 130.0f};
    return base[std::clamp(rank, 0, 5)] + std::max(0.0f, abilityPower) * 0.45f;
}

inline float RRawDamage(int rank, float abilityPower, int enemyCount,
                        bool targetMarked) {
    static constexpr std::array<float, 4> base{0.0f, 200.0f, 300.0f, 400.0f};
    const float additional = static_cast<float>(std::max(0, enemyCount - 1)) *
                             (targetMarked ? 40.0f : 25.0f);
    return base[std::clamp(rank, 0, 3)] + std::max(0.0f, abilityPower) * 0.60f +
           additional;
}

inline float QTravelSeconds(float distance) {
    return kQCastDelay + std::clamp(distance, 0.0f, kQRange) / kQSpeed;
}

inline bool CrescentStrikeHits(const Vec3& origin, const Vec3& aim,
                              const Vec3& target, float targetRadius) {
    if (!origin.IsValid() || !aim.IsValid() || !target.IsValid()) return false;
    const auto projection = ProjectPointToSegment2D(target, origin, aim);
    return origin.Distance2D(aim) <= kQRange + 1.0f &&
           projection.Distance <= kQWidth * 0.5f + std::clamp(targetRadius, 0.0f, 250.0f);
}

inline Vec3 CrescentAim(const Vec3& origin, const Vec3& predicted) {
    const Vec3 direction = Direction2D(origin, predicted);
    return direction.IsZero() ? Vec3{} : origin + direction * kQRange;
}

struct OrbState {
    int CastTick = 0;
    int OrbsRemaining = 3;
    bool Active = false;
    bool ShieldActive = false;
};

inline OrbState BeginPaleCascade(int nowMs) { return {nowMs, 3, true, true}; }

inline OrbState OrbHit(OrbState state) {
    if (!state.Active) return state;
    state.OrbsRemaining = std::max(0, state.OrbsRemaining - 1);
    if (state.OrbsRemaining == 0) state.Active = false;
    return state;
}

inline OrbState ReconcileOrbs(OrbState state, int nowMs, bool shieldBuff,
                              int observedOrbs = -1) {
    if (!shieldBuff || (state.CastTick > 0 &&
        nowMs > state.CastTick + static_cast<int>(kWShieldSeconds * 1000.0f))) {
        return {};
    }
    state.ShieldActive = true;
    if (observedOrbs >= 0) state.OrbsRemaining = std::clamp(observedOrbs, 0, 3);
    state.Active = state.OrbsRemaining > 0;
    return state;
}

inline bool WOrbHits(const Vec3& center, const Vec3& target, float targetRadius) {
    return center.IsValid() && target.IsValid() &&
           center.Distance2D(target) <= kWOrbRadius + std::max(0.0f, targetRadius);
}

inline Vec3 EDashEndpoint(const Vec3& origin, const Vec3& target,
                          float playerRadius, float targetRadius) {
    const Vec3 direction = Direction2D(origin, target);
    if (direction.IsZero()) return origin;
    const float standoff = std::max(0.0f, playerRadius) +
                           std::max(0.0f, targetRadius);
    return target - direction * standoff;
}

inline bool EReachable(const Vec3& origin, const Vec3& target,
                       float playerRadius, float targetRadius,
                       bool marked) {
    if (!origin.IsValid() || !target.IsValid()) return false;
    const float gap = std::max(0.0f, playerRadius) + std::max(0.0f, targetRadius);
    return origin.Distance2D(target) - gap <= kERange + (marked ? 1.0f : 0.0f);
}

inline bool EEndpointSafe(const Vec3& endpoint, bool wall, bool turret,
                         int nearbyEnemies, int maximumEnemies, bool lethal) {
    if (!endpoint.IsValid() || endpoint.IsZero() || wall) return false;
    if (turret && !lethal) return false;
    return nearbyEnemies <= std::max(1, maximumEnemies) || lethal;
}

struct MoonfallContext {
    int EnemiesInside = 0;
    int AlliesInside = 0;
    int OutsideThreats = 0;
    bool UnderEnemyTurret = false;
    bool TerrainBlocked = false;
    bool TargetMarked = false;
    bool Lethal = false;
    float PlayerHealthPercent = 100.0f;
};

inline float MoonfallSafetyScore(const MoonfallContext& context) {
    if (context.TerrainBlocked) return -10000.0f;
    float score = static_cast<float>(std::max(0, context.EnemiesInside)) * 170.0f;
    score += static_cast<float>(std::max(0, context.AlliesInside)) * 85.0f;
    score += static_cast<float>(std::max(0, context.OutsideThreats)) * 115.0f;
    score -= static_cast<float>(std::max(0, context.EnemiesInside - 2)) * 210.0f;
    if (context.TargetMarked) score += 150.0f;
    if (context.Lethal) score += 300.0f;
    if (context.UnderEnemyTurret) score -= context.Lethal ? 500.0f : 1400.0f;
    if (context.PlayerHealthPercent < 35.0f) score -= context.Lethal ? 0.0f : 220.0f;
    return score;
}

inline bool MoonfallSafe(const MoonfallContext& context, float minimumScore,
                         int minimumEnemies) {
    return !context.TerrainBlocked && context.EnemiesInside >= minimumEnemies &&
           MoonfallSafetyScore(context) >= minimumScore;
}

inline bool MoonfallPulls(const Vec3& center, const Vec3& target, float radius,
                          float targetRadius) {
    return center.IsValid() && target.IsValid() &&
           center.Distance2D(target) <= std::max(0.0f, radius) +
               std::max(0.0f, targetRadius);
}

} // namespace Plugins::KuroAIO::AI::Controllers::Diana::Geometry
