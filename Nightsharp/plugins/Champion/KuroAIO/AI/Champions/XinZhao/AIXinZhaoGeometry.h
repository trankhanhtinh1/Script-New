#pragma once

// Deterministic Xin Zhao mechanics. The live controller supplies prediction,
// target value, spell/buff events and map safety; this file owns the passive
// cadence, Q chain, piercing W line, targeted E endpoint and Crescent Guard
// isolation arithmetic so those decisions remain independently testable.

#include "../../AIGeometry.h"

#include <algorithm>
#include <cmath>

namespace Plugins::KuroAIO::AI::Controllers::XinZhao::Geometry {

using SharedGeometry::Direction2D;

inline constexpr float kWRange = 1000.0f;
inline constexpr float kWThrustHalfWidth = 25.0f;
inline constexpr float kWSlashRange = 475.0f;
inline constexpr float kWSlashHalfWidth = 130.0f;
inline constexpr float kENormalRange = 650.0f;
inline constexpr float kEChallengeRange = 1100.0f;
inline constexpr float kEImpactRadius = 250.0f;
inline constexpr float kRSweepRadius = 500.0f;
inline constexpr float kRGuardRadius = 450.0f;

inline int PassiveAttackIndex(int completedAttacks) {
    const int normalized = ((completedAttacks % 3) + 3) % 3;
    return normalized + 1;
}

inline bool PassiveProcsOnNextAttack(int completedAttacks) {
    return PassiveAttackIndex(completedAttacks) == 3;
}

inline float PassiveDamageRatio(int level) {
    if (level >= 16) return 0.60f;
    if (level >= 11) return 0.45f;
    if (level >= 6) return 0.30f;
    return 0.15f;
}

inline float PassiveApDamageRatio(int level) {
    if (level >= 16) return 0.20f;
    if (level >= 11) return 0.15f;
    if (level >= 6) return 0.10f;
    return 0.05f;
}

inline float PassiveRawBonusDamage(int level,
                                   float totalAttackDamage,
                                   float abilityPower) {
    return std::max(0.0f, totalAttackDamage) * PassiveDamageRatio(level) +
           std::max(0.0f, abilityPower) * PassiveApDamageRatio(level);
}

inline float PassiveMaximumHealthRatio(int level) {
    if (level >= 11) return 0.05f;
    if (level >= 6) return 0.035f;
    return 0.02f;
}

inline float PassiveHealApRatio(int level) {
    if (level >= 11) return 0.70f;
    if (level >= 6) return 0.50f;
    return 0.40f;
}

inline float PassiveRawHealing(int level,
                               float maximumHealth,
                               float abilityPower) {
    return std::max(0.0f, maximumHealth) * PassiveMaximumHealthRatio(level) +
           std::max(0.0f, abilityPower) * PassiveHealApRatio(level);
}

struct QChainState {
    int Strikes = 0;
    bool Active = false;
};

inline QChainState BeginQChain() { return { 0, true }; }

inline QChainState AdvanceQChain(QChainState state,
                                 bool completedBasicAttack) {
    if (!state.Active || !completedBasicAttack) return state;
    state.Strikes = std::clamp(state.Strikes + 1, 0, 3);
    if (state.Strikes >= 3) state.Active = false;
    return state;
}

inline bool QKnocksUpOnNextAttack(const QChainState& state) {
    return state.Active && state.Strikes == 2;
}

inline float QCooldownAfterHits(float remainingSeconds, int completedHits) {
    return std::max(0.0f, remainingSeconds -
        static_cast<float>(std::clamp(completedHits, 0, 3)));
}

inline float QRawBonusDamage(int rank, float bonusAttackDamage) {
    static constexpr float base[] = {
        0.0f, 15.0f, 30.0f, 45.0f, 60.0f, 75.0f,
    };
    return base[std::clamp(rank, 0, 5)] +
           std::max(0.0f, bonusAttackDamage) * 0.40f;
}

inline float RayAlong(const Vec3& origin,
                      const Vec3& direction,
                      const Vec3& point) {
    Vec3 relative = point - origin;
    relative.y = 0.0f;
    return relative.Dot(direction);
}

inline float RayPerpendicular(const Vec3& origin,
                              const Vec3& direction,
                              const Vec3& point) {
    const float along = RayAlong(origin, direction, point);
    Vec3 closest = origin + direction * along;
    closest.y = origin.y;
    return point.Distance2D(closest);
}

inline bool PiercingLineHits(const Vec3& origin,
                             const Vec3& direction,
                             const Vec3& point,
                             float targetRadius,
                             float range,
                             float halfWidth) {
    if (!origin.IsValid() || direction.IsZero() || !point.IsValid()) {
        return false;
    }
    const float radius = std::clamp(targetRadius, 0.0f, 250.0f);
    const float along = RayAlong(origin, direction, point);
    if (along < -radius || along > std::max(0.0f, range) + radius) {
        return false;
    }
    return RayPerpendicular(origin, direction, point) <=
           std::max(0.0f, halfWidth) + radius;
}

inline bool WThrustHits(const Vec3& origin,
                        const Vec3& direction,
                        const Vec3& point,
                        float targetRadius = 0.0f) {
    return PiercingLineHits(origin, direction, point, targetRadius,
                            kWRange, kWThrustHalfWidth);
}

inline bool WSlashHits(const Vec3& origin,
                       const Vec3& direction,
                       const Vec3& point,
                       float targetRadius = 0.0f) {
    return PiercingLineHits(origin, direction, point, targetRadius,
                            kWSlashRange, kWSlashHalfWidth);
}

inline float WSlashRawDamage(int rank, float totalAttackDamage) {
    static constexpr float base[] = {
        0.0f, 30.0f, 40.0f, 50.0f, 60.0f, 70.0f,
    };
    return base[std::clamp(rank, 0, 5)] +
           std::max(0.0f, totalAttackDamage) * 0.30f;
}

inline float WThrustRawDamage(int rank,
                              float totalAttackDamage,
                              float abilityPower,
                              float criticalStrikeChance = 0.0f) {
    static constexpr float base[] = {
        0.0f, 50.0f, 85.0f, 120.0f, 155.0f, 190.0f,
    };
    const float raw = base[std::clamp(rank, 0, 5)] +
        std::max(0.0f, totalAttackDamage) * 0.90f +
        std::max(0.0f, abilityPower) * 0.65f;
    return raw * (1.0f + std::clamp(criticalStrikeChance, 0.0f, 1.0f) /
                            3.0f);
}

inline float WRawDamage(int rank,
                        float totalAttackDamage,
                        float abilityPower,
                        float criticalStrikeChance = 0.0f,
                        bool slashAlsoHits = true) {
    return WThrustRawDamage(rank, totalAttackDamage, abilityPower,
                            criticalStrikeChance) +
           (slashAlsoHits ? WSlashRawDamage(rank, totalAttackDamage) : 0.0f);
}

inline float ECastRange(bool challenged) {
    return challenged ? kEChallengeRange : kENormalRange;
}

inline float EExposedDashDistance(float centerDistance,
                                  float playerRadius,
                                  float targetRadius) {
    return std::max(0.0f, centerDistance -
        std::max(0.0f, playerRadius) - std::max(0.0f, targetRadius));
}

inline Vec3 EDashEndpoint(const Vec3& origin,
                          const Vec3& target,
                          float playerRadius,
                          float targetRadius,
                          float extraStandoff = 0.0f) {
    const Vec3 direction = Direction2D(origin, target);
    if (direction.IsZero()) return origin;
    const float gap = std::max(0.0f, playerRadius) +
                      std::max(0.0f, targetRadius) +
                      std::max(0.0f, extraStandoff);
    const float distance = origin.Distance2D(target);
    if (distance <= gap) return origin;
    return target - direction * gap;
}

inline bool ECanReach(float centerDistance,
                      float playerRadius,
                      float targetRadius,
                      bool challenged) {
    return EExposedDashDistance(centerDistance, playerRadius, targetRadius) <=
           ECastRange(challenged);
}

inline float ERawDamage(int rank, float abilityPower) {
    static constexpr float base[] = {
        0.0f, 50.0f, 75.0f, 100.0f, 125.0f, 150.0f,
    };
    return base[std::clamp(rank, 0, 5)] +
           std::max(0.0f, abilityPower) * 1.20f;
}

inline bool CircleHits(const Vec3& center,
                       const Vec3& target,
                       float targetRadius,
                       float effectRadius) {
    return center.IsValid() && target.IsValid() &&
           center.Distance2D(target) <= std::max(0.0f, effectRadius) +
               std::clamp(targetRadius, 0.0f, 250.0f);
}

inline bool RSweepHits(const Vec3& center,
                       const Vec3& target,
                       float targetRadius = 0.0f) {
    return CircleHits(center, target, targetRadius, kRSweepRadius);
}

inline bool RBlocksDamageSource(const Vec3& center,
                                const Vec3& source,
                                float sourceRadius = 0.0f) {
    if (!center.IsValid() || !source.IsValid()) return false;
    return center.Distance2D(source) >
           kRGuardRadius + std::clamp(sourceRadius, 0.0f, 250.0f);
}

inline float RRawDamage(int rank,
                        float bonusAttackDamage,
                        float abilityPower,
                        float targetCurrentHealth) {
    static constexpr float base[] = {
        0.0f, 75.0f, 175.0f, 275.0f,
    };
    return base[std::clamp(rank, 0, 3)] +
           std::max(0.0f, bonusAttackDamage) +
           std::max(0.0f, abilityPower) * 1.10f +
           std::max(0.0f, targetCurrentHealth) * 0.15f;
}

struct IsolationContext {
    bool ChallengedTargetInside = false;
    int NonChallengedInside = 0;
    int EnemiesRemainingInside = 0;
    int AlliesNearTarget = 0;
    int OutsideDamageThreats = 0;
    bool UnderEnemyTurret = false;
    bool ChallengedTargetKillable = false;
    float PlayerHealthPercent = 100.0f;
};

inline float IsolationSafetyScore(const IsolationContext& context) {
    if (!context.ChallengedTargetInside) return -10000.0f;
    float score = 160.0f;
    score += static_cast<float>(std::max(0, context.NonChallengedInside)) *
             180.0f;
    score += static_cast<float>(std::max(0, context.OutsideDamageThreats)) *
             145.0f;
    score += static_cast<float>(std::max(0, context.AlliesNearTarget)) *
             95.0f;
    score -= static_cast<float>(std::max(0, context.EnemiesRemainingInside - 1)) *
             260.0f;
    if (context.UnderEnemyTurret) score -= 900.0f;
    if (context.ChallengedTargetKillable) score += 260.0f;
    if (context.PlayerHealthPercent < 35.0f) score += 120.0f;
    return score;
}

inline bool SafeIsolation(const IsolationContext& context,
                          float minimumScore = 180.0f) {
    return IsolationSafetyScore(context) >= minimumScore;
}

} // namespace Plugins::KuroAIO::AI::Controllers::XinZhao::Geometry
