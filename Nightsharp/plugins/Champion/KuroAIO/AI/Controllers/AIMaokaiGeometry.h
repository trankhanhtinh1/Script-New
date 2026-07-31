#pragma once

// Maokai-owned geometry and deterministic mechanics.  The controller owns live
// object/event state; this header owns sapling zones, Q displacement, W arrival,
// E missile timing and the accelerating R root-wave model.

#include "../AIGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cfloat>

namespace Plugins::KuroAIO::AI::Controllers::Maokai::Geometry {

using SharedGeometry::Direction2D;
using SharedGeometry::ProjectPointToSegment2D;
using Vec3 = ::Vec3;

inline constexpr float kQRange = 650.0f;
inline constexpr float kQHalfWidth = 55.0f;
inline constexpr float kQKnockbackDistance = 400.0f;
inline constexpr float kQKnockbackSpeed = 950.0f;
inline constexpr float kWRange = 525.0f;
inline constexpr float kWDashSpeed = 1300.0f;
inline constexpr float kEMaxRange = 1100.0f;
inline constexpr float kEMissileWidth = 120.0f;
inline constexpr float kEMissileTravelSeconds = 0.85f;
inline constexpr float kSaplingDetectionRadius = 550.0f;
inline constexpr float kSaplingLifetimeSeconds = 30.0f;
inline constexpr float kEmpoweredZoneSeconds = 2.0f;
inline constexpr float kRMaxRange = 3000.0f;
inline constexpr float kRHalfWidth = 60.0f;
inline constexpr float kRExtraHalfWidth = 75.0f;
inline constexpr float kRInitialSpeed = 100.0f;
inline constexpr float kRAcceleration = 300.0f;
inline constexpr float kRMaxSpeed = 750.0f;
inline constexpr float kRMinRootSeconds = 0.75f;
inline constexpr float kRMaxRootSeconds = 2.25f;

struct SaplingState {
    int Id = 0;
    Vec3 Position = {};
    int SpawnTick = 0;
    int ExpireTick = 0;
    int EmpowerExpireTick = 0;
    bool Active = false;
    bool Empowered = false;
    bool Triggered = false;
};

inline bool SaplingLive(const SaplingState& sapling, int now) {
    return sapling.Active && sapling.Position.IsValid() &&
           now <= sapling.ExpireTick;
}

inline bool SaplingZoneLive(const SaplingState& sapling, int now) {
    return SaplingLive(sapling, now) && sapling.Triggered &&
           sapling.EmpowerExpireTick >= now;
}

inline bool InSaplingDetectionZone(const SaplingState& sapling,
                                   const Vec3& target,
                                   float targetRadius = 0.0f) {
    return sapling.Active && sapling.Position.IsValid() && target.IsValid() &&
           sapling.Position.Distance2D(target) <=
               kSaplingDetectionRadius + std::max(0.0f, targetRadius);
}

inline bool EBrushEmpowers(const Vec3& brushPosition,
                           const Vec3& targetPosition,
                           float targetRadius = 0.0f) {
    return brushPosition.IsValid() && targetPosition.IsValid() &&
           brushPosition.Distance2D(targetPosition) <=
               kSaplingDetectionRadius + std::max(0.0f, targetRadius);
}

inline float SaplingPriority(float distanceToTarget,
                             bool inBrush,
                             bool enemyTurret,
                             int nearbyEnemies,
                             int nearbyAllies) {
    if (!std::isfinite(distanceToTarget) || distanceToTarget < 0.0f) {
        return -FLT_MAX;
    }
    float score = 900.0f - distanceToTarget;
    if (inBrush) score += 340.0f;
    score += static_cast<float>(std::max(0, nearbyAllies)) * 95.0f;
    score -= static_cast<float>(std::max(0, nearbyEnemies)) * 125.0f;
    if (enemyTurret) score -= 700.0f;
    return score;
}

struct QPlan {
    Vec3 Origin = {};
    Vec3 Direction = {};
    Vec3 TargetAtImpact = {};
    Vec3 KnockbackEndpoint = {};
    float TargetDistance = 0.0f;
    float ImpactSeconds = 0.0f;
    bool Valid = false;
};

inline QPlan BuildQPlan(const Vec3& origin,
                        const Vec3& target,
                        float playerRadius = 55.0f,
                        float targetRadius = 65.0f) {
    QPlan plan{};
    plan.Origin = origin;
    plan.TargetAtImpact = target;
    if (!origin.IsValid() || !target.IsValid()) return plan;
    plan.Direction = Direction2D(origin, target);
    if (plan.Direction.IsZero()) return plan;
    plan.TargetDistance = origin.Distance2D(target);
    if (!std::isfinite(plan.TargetDistance) || plan.TargetDistance > kQRange + targetRadius) {
        return plan;
    }
    plan.KnockbackEndpoint = target + plan.Direction * kQKnockbackDistance;
    plan.KnockbackEndpoint.y = target.y;
    plan.ImpactSeconds = std::max(0.0f,
        (plan.TargetDistance - std::max(0.0f, playerRadius) -
         std::max(0.0f, targetRadius)) / kQKnockbackSpeed);
    plan.Valid = true;
    return plan;
}

inline bool QHits(const Vec3& origin,
                  const Vec3& direction,
                  const Vec3& target,
                  float targetRadius = 0.0f) {
    if (!origin.IsValid() || !direction.IsValid() || direction.IsZero() ||
        !target.IsValid()) return false;
    const Vec3 unit = Direction2D(Vec3{}, direction);
    if (unit.IsZero()) return false;
    const auto projection = ProjectPointToSegment2D(
        target, origin, origin + unit * kQRange);
    return projection.T < 1.0f + 0.0001f &&
           projection.Distance <= kQHalfWidth + std::max(0.0f, targetRadius);
}

inline bool QEndpointSafe(const Vec3& endpoint,
                          bool enemyTurret,
                          bool readyDashHazard,
                          int enemiesAtEndpoint,
                          int alliesAtEndpoint,
                          int maximumEnemies,
                          bool lethal) {
    if (!endpoint.IsValid() || enemyTurret || readyDashHazard) return false;
    if (!lethal && enemiesAtEndpoint > std::max(0, maximumEnemies)) return false;
    return alliesAtEndpoint > 0 || enemiesAtEndpoint <= 1 || lethal;
}

struct WDashPlan {
    Vec3 Origin = {};
    Vec3 TargetAtImpact = {};
    Vec3 Endpoint = {};
    float Distance = 0.0f;
    float ArrivalSeconds = 0.0f;
    bool Valid = false;
};

inline WDashPlan BuildWDashPlan(const Vec3& origin,
                                const Vec3& target,
                                float targetRadius = 65.0f) {
    WDashPlan plan{};
    plan.Origin = origin;
    plan.TargetAtImpact = target;
    if (!origin.IsValid() || !target.IsValid()) return plan;
    plan.Distance = origin.Distance2D(target);
    if (!std::isfinite(plan.Distance) || plan.Distance > kWRange + targetRadius) {
        return plan;
    }
    plan.Endpoint = target;
    plan.Endpoint.y = origin.y;
    plan.ArrivalSeconds = plan.Distance / kWDashSpeed;
    plan.Valid = true;
    return plan;
}

inline bool WEndpointSafe(const Vec3& endpoint,
                          bool enemyTurret,
                          bool readyDashHazard,
                          int enemies,
                          int allies,
                          int maximumEnemies,
                          bool lethal) {
    if (!endpoint.IsValid() || enemyTurret || readyDashHazard) return false;
    if (!lethal && enemies > std::max(0, maximumEnemies)) return false;
    return allies > 0 || enemies <= 1 || lethal;
}

inline bool WEndpointSafe(const WDashPlan& plan,
                          bool enemyTurret,
                          bool readyDashHazard,
                          int enemies,
                          int allies,
                          int maximumEnemies,
                          bool lethal) {
    return plan.Valid && WEndpointSafe(plan.Endpoint, enemyTurret,
        readyDashHazard, enemies, allies, maximumEnemies, lethal);
}

inline bool EProjectileClear(const Vec3& origin,
                             const Vec3& destination,
                             float blockerRadius,
                             const Vec3& blocker) {
    if (!origin.IsValid() || !destination.IsValid()) return false;
    const auto projection = ProjectPointToSegment2D(blocker, origin, destination);
    return projection.Distance > kEMissileWidth * 0.5f +
               std::max(0.0f, blockerRadius);
}

inline float EImpactSeconds(const Vec3& origin, const Vec3& destination) {
    if (!origin.IsValid() || !destination.IsValid()) return FLT_MAX;
    return kEMissileTravelSeconds;
}

inline float RWaveDistance(float elapsedSeconds) {
    if (!std::isfinite(elapsedSeconds) || elapsedSeconds <= 0.0f) return 0.0f;
    const float t = elapsedSeconds;
    const float accelTime = (kRMaxSpeed - kRInitialSpeed) / kRAcceleration;
    if (t <= accelTime) return kRInitialSpeed * t + 0.5f * kRAcceleration * t * t;
    return kRInitialSpeed * accelTime + 0.5f * kRAcceleration * accelTime * accelTime +
           kRMaxSpeed * (t - accelTime);
}

inline float RWaveTravelSeconds(float distance) {
    if (!std::isfinite(distance) || distance < 0.0f || distance > kRMaxRange) return FLT_MAX;
    const float accelTime = (kRMaxSpeed - kRInitialSpeed) / kRAcceleration;
    const float accelDistance = RWaveDistance(accelTime);
    if (distance <= accelDistance) {
        const float discriminant = kRInitialSpeed * kRInitialSpeed +
            2.0f * kRAcceleration * distance;
        return (-kRInitialSpeed + std::sqrt(std::max(0.0f, discriminant))) /
               kRAcceleration;
    }
    return accelTime + (distance - accelDistance) / kRMaxSpeed;
}

inline bool RWaveHits(const Vec3& origin,
                      const Vec3& direction,
                      const Vec3& target,
                      float targetRadius = 0.0f,
                      bool extraForm = false) {
    if (!origin.IsValid() || !target.IsValid()) return false;
    const Vec3 unit = Direction2D(origin, origin + direction);
    if (unit.IsZero()) return false;
    const auto projection = ProjectPointToSegment2D(
        target, origin, origin + unit * kRMaxRange);
    const float width = extraForm ? kRExtraHalfWidth : kRHalfWidth;
    return projection.T < 1.0f + 0.0001f &&
           projection.Distance <= width + std::max(0.0f, targetRadius);
}

inline float RRootDuration(float distanceFromOrigin) {
    const float ratio = std::clamp(distanceFromOrigin / kRMaxRange, 0.0f, 1.0f);
    return kRMinRootSeconds + (kRMaxRootSeconds - kRMinRootSeconds) * ratio;
}

struct RWaveState {
    Vec3 Origin = {};
    Vec3 Direction = {};
    int CastTick = 0;
    int ResolveTick = 0;
    int TargetId = 0;
    bool Active = false;
};

inline bool RWaveActive(const RWaveState& state, int now) {
    return state.Active && state.Origin.IsValid() && !state.Direction.IsZero() &&
           now <= state.ResolveTick;
}

inline bool RShouldCast(int predictedHits,
                        float qualityScore,
                        int minimumHits,
                        float minimumScore,
                        bool peelUrgent,
                        bool lethalSingle,
                        bool unsafeMobility) {
    if (unsafeMobility) return false;
    if (peelUrgent) return predictedHits >= 1;
    return predictedHits >= std::max(1, minimumHits) ||
           (lethalSingle && predictedHits >= 1) ||
           qualityScore >= minimumScore;
}

inline float QRawDamage(int rank, float abilityPower, bool monster) {
    static constexpr std::array<float, 6> base{0, 30, 75, 120, 165, 210};
    static constexpr std::array<float, 6> monsterModifier{
        0.0f, 1.50f, 1.60f, 1.70f, 1.80f, 1.90f};
    const int r = std::clamp(rank, 0, 5);
    const float modifier = monster ? monsterModifier[r] : 1.0f;
    return (base[r] + std::max(0.0f, abilityPower) * 0.50f) * modifier;
}

inline float ERawDamage(int rank, float abilityPower, bool empowered) {
    static constexpr std::array<float, 6> base{0, 25, 50, 75, 100, 125};
    static constexpr std::array<float, 6> emp{0, 50, 100, 150, 200, 250};
    const int r = std::clamp(rank, 0, 5);
    return (empowered ? emp[r] : base[r]) +
           std::max(0.0f, abilityPower) * (empowered ? 0.50f : 0.25f);
}

} // namespace Plugins::KuroAIO::AI::Controllers::Maokai::Geometry
