#pragma once

// Deterministic Vi mechanics. The live controller owns prediction, NavMesh,
// target selection and cast arbitration; this file keeps charge, Denting Blows,
// Relentless Force and lock-on path math independently testable.

#include "../../AIGeometry.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <vector>

namespace Plugins::KuroAIO::AI::Controllers::Vi::Geometry {

using SharedGeometry::Direction2D;
using SharedGeometry::ProjectPointToSegment2D;

inline constexpr float kQMinimumRange = 250.0f;
inline constexpr float kQMaximumRange = 725.0f;
inline constexpr float kQFullChargeSeconds = 1.25f;
inline constexpr float kQHalfWidth = 55.0f;
inline constexpr float kEExtraAttackRange = 50.0f;
inline constexpr float kEConeRange = 535.0f;
inline constexpr float kEConeHalfAngleDegrees = 17.5f;
inline constexpr float kRRange = 800.0f;
inline constexpr float kRPathHalfWidth = 100.0f;
inline constexpr float kRBaseSpeed = 800.0f;
inline constexpr float kRWindupSeconds = 0.25f;

inline float QChargeFraction(float elapsedSeconds) {
    return std::clamp(
        elapsedSeconds / kQFullChargeSeconds, 0.0f, 1.0f);
}

inline float QChargeRange(float elapsedSeconds) {
    return kQMinimumRange +
        (kQMaximumRange - kQMinimumRange) *
            QChargeFraction(elapsedSeconds);
}

inline float QDamageMultiplier(float elapsedSeconds) {
    // CommunityDragon 16.15: minimum damage grows linearly to 2.5x.
    return 1.0f + 1.5f * QChargeFraction(elapsedSeconds);
}

inline float QRawDamage(int rank,
                        float bonusAttackDamage,
                        float elapsedSeconds) {
    const int level = std::clamp(rank, 0, 5);
    const float minimum = level > 0
        ? 20.0f + 20.0f * static_cast<float>(level)
        : 0.0f;
    return (minimum + 0.60f * std::max(0.0f, bonusAttackDamage)) *
        QDamageMultiplier(elapsedSeconds);
}

inline Vec3 QEndpoint(const Vec3& origin,
                      const Vec3& aim,
                      float elapsedSeconds) {
    const Vec3 direction = Direction2D(origin, aim);
    if (direction.IsZero()) return origin;
    Vec3 result = origin + direction * QChargeRange(elapsedSeconds);
    result.y = origin.y;
    return result;
}

struct QCollisionCandidate {
    int Id = 0;
    Vec3 Position = {};
    float Radius = 0.0f;
    bool Valid = true;
};

struct QCollisionResult {
    int Id = 0;
    float Along = FLT_MAX;
    Vec3 Contact = {};
    bool Hit = false;
};

inline QCollisionResult FirstQCollision(
    const Vec3& origin,
    const Vec3& endpoint,
    const std::vector<QCollisionCandidate>& candidates,
    float qHalfWidth = kQHalfWidth) {
    QCollisionResult result{};
    const float segmentLength = origin.Distance2D(endpoint);
    if (segmentLength <= 0.001f) return result;
    for (const auto& candidate : candidates) {
        if (!candidate.Valid || !candidate.Position.IsValid()) continue;
        const auto projection = ProjectPointToSegment2D(
            candidate.Position, origin, endpoint);
        if (projection.T < 0.0f || projection.T > 1.0f) continue;
        const float allowed = std::max(0.0f, qHalfWidth) +
            std::clamp(candidate.Radius, 0.0f, 200.0f);
        if (candidate.Position.Distance2D(projection.Closest) > allowed) {
            continue;
        }
        const float along = projection.T * segmentLength;
        if (along < result.Along) {
            result.Id = candidate.Id;
            result.Along = along;
            result.Contact = projection.Closest;
            result.Hit = true;
        }
    }
    return result;
}

inline bool QEndpointPolicy(bool endpointWalkable,
                            bool endpointUnderEnemyTurret,
                            bool dashHazard,
                            int enemiesAtEndpoint,
                            int maximumEnemies,
                            bool lethalCommit,
                            bool fleeing) {
    if (!endpointWalkable || dashHazard) return false;
    if (!fleeing && endpointUnderEnemyTurret && !lethalCommit) return false;
    return fleeing || enemiesAtEndpoint <= std::max(1, maximumEnemies) ||
        lethalCommit;
}

inline int NextDentingBlowsStacks(int currentStacks, bool qualifyingHit) {
    if (!qualifyingHit) return std::clamp(currentStacks, 0, 2);
    const int next = std::clamp(currentStacks, 0, 2) + 1;
    return next >= 3 ? 0 : next;
}

inline bool DentingBlowsProcs(int currentStacks, bool qualifyingHit) {
    return qualifyingHit && std::clamp(currentStacks, 0, 2) >= 2;
}

inline float DentingBlowsPercentMaxHealth(int rank,
                                         float bonusAttackDamage) {
    const int level = std::clamp(rank, 0, 5);
    if (level <= 0) return 0.0f;
    return (3.0f + static_cast<float>(level) +
            0.035f * std::max(0.0f, bonusAttackDamage)) /
        100.0f;
}

inline float DentingBlowsRawDamage(int rank,
                                   float bonusAttackDamage,
                                   float targetMaximumHealth,
                                   bool monster = false) {
    const float raw = std::max(0.0f, targetMaximumHealth) *
        DentingBlowsPercentMaxHealth(rank, bonusAttackDamage);
    return monster ? std::min(raw, 300.0f) : raw;
}

inline float ArmorAfterDentingBlows(float armor) {
    return armor * 0.80f;
}

inline float BlastShieldAmount(float maximumHealth) {
    return 0.12f * std::max(0.0f, maximumHealth);
}

inline float BlastShieldCooldownAfterProc(float remainingSeconds) {
    return std::max(0.0f, remainingSeconds - 4.0f);
}

inline bool EEmpoweredAttackInRange(float centerDistance,
                                    float ordinaryAttackRange,
                                    float targetRadius = 0.0f) {
    return centerDistance <= std::max(0.0f, ordinaryAttackRange) +
        kEExtraAttackRange + std::max(0.0f, targetRadius);
}

inline bool EConeHits(const Vec3& viPosition,
                      const Vec3& primaryPosition,
                      const Vec3& candidatePosition,
                      float candidateRadius = 0.0f,
                      float coneRange = kEConeRange,
                      float halfAngleDegrees = kEConeHalfAngleDegrees) {
    const Vec3 direction = Direction2D(viPosition, primaryPosition);
    const Vec3 toCandidate = Direction2D(primaryPosition, candidatePosition);
    const float distance = primaryPosition.Distance2D(candidatePosition);
    if (direction.IsZero() || toCandidate.IsZero() ||
        distance > std::max(0.0f, coneRange) + std::max(0.0f, candidateRadius)) {
        return false;
    }
    constexpr float pi = 3.14159265358979323846f;
    const float angle = std::clamp(halfAngleDegrees, 0.0f, 89.0f) * pi / 180.0f;
    const float angularPadding = distance > 1.0f
        ? std::asin(std::clamp(candidateRadius / distance, 0.0f, 1.0f))
        : pi * 0.5f;
    return direction.Dot(toCandidate) >= std::cos(angle + angularPadding);
}

inline float RTravelSeconds(float centerDistance) {
    return kRWindupSeconds +
        std::max(0.0f, centerDistance) / kRBaseSpeed;
}

inline bool RPathIntersects(const Vec3& origin,
                            const Vec3& target,
                            const Vec3& other,
                            float otherRadius = 0.0f,
                            float pathHalfWidth = kRPathHalfWidth) {
    const auto projection = ProjectPointToSegment2D(other, origin, target);
    if (projection.T <= 0.0f || projection.T >= 1.0f) return false;
    return other.Distance2D(projection.Closest) <=
        std::max(0.0f, pathHalfWidth) + std::max(0.0f, otherRadius);
}

struct RPathContext {
    bool TargetValid = false;
    bool TargetProtected = false;
    bool EndpointWalkable = false;
    bool EndpointUnderEnemyTurret = false;
    bool EndpointDashHazard = false;
    bool PlayerUnderEnemyTurret = false;
    int EnemiesAtEndpoint = 0;
    int PathEnemies = 0;
    int AlliedFollowup = 0;
    int MaximumEnemies = 2;
    bool TargetLethal = false;
    bool Defensive = false;
    bool ManualConsent = false;
};

inline bool RLockOnPathSafe(const RPathContext& context) {
    if (!context.TargetValid || context.TargetProtected ||
        !context.EndpointWalkable || context.EndpointDashHazard) {
        return false;
    }
    if (context.Defensive) {
        return context.EnemiesAtEndpoint <=
            std::max(1, context.MaximumEnemies + 1);
    }
    if (context.EndpointUnderEnemyTurret && !context.PlayerUnderEnemyTurret &&
        !context.TargetLethal && !context.ManualConsent) {
        return false;
    }
    if (context.EnemiesAtEndpoint > std::max(1, context.MaximumEnemies) &&
        !context.TargetLethal) {
        return false;
    }
    if (context.PathEnemies >= 2 && context.AlliedFollowup <= 0 &&
        !context.TargetLethal && !context.ManualConsent) {
        return false;
    }
    return context.AlliedFollowup > 0 || context.TargetLethal ||
        context.Defensive || context.ManualConsent;
}

} // namespace Plugins::KuroAIO::AI::Controllers::Vi::Geometry
