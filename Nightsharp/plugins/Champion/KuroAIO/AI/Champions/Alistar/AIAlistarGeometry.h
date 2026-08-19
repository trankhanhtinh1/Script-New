#pragma once

// Deterministic Alistar mechanics. The live controller owns target/ally
// selection and NavMesh queries; this file keeps the easily-confused kit
// math testable: Headbutt's radius-adjusted travel, full versus buffered
// knockback, Pulverize coverage, Trample pulse/AA timing and R mitigation.

#include "../../AIGeometry.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <vector>

namespace Plugins::KuroAIO::AI::Controllers::Alistar::Geometry {

using SharedGeometry::Direction2D;
using SharedGeometry::ProjectPointToSegment2D;

inline constexpr float kHeadbuttBaseSpeed = 1200.0f;
inline constexpr float kHeadbuttDistance = 700.0f;
inline constexpr float kBufferedHeadbuttDistance = 200.0f;
inline constexpr float kPulverizeRadius = 375.0f;
inline constexpr float kTrampleRadius = 350.0f;
inline constexpr float kTramplePulseSeconds = 0.5f;

inline float HeadbuttTravelSeconds(float centerDistance,
                                   float alistarRadius = 80.0f,
                                   float targetRadius = 65.0f,
                                   float baseSpeed = kHeadbuttBaseSpeed) {
    const float exposedDistance = std::max(
        0.0f,
        centerDistance - std::max(0.0f, alistarRadius) -
            std::max(0.0f, targetRadius));
    return exposedDistance / std::max(1.0f, baseSpeed);
}

inline float HeadbuttCenterSpeed(float centerDistance,
                                 float alistarRadius = 80.0f,
                                 float targetRadius = 65.0f,
                                 float baseSpeed = kHeadbuttBaseSpeed) {
    if (centerDistance <= 0.0f) return 0.0f;
    const float travel = HeadbuttTravelSeconds(
        centerDistance, alistarRadius, targetRadius, baseSpeed);
    return travel <= 0.0001f
        ? std::max(1.0f, baseSpeed)
        : centerDistance / travel;
}

inline int BufferedWQImpactMs(float centerDistance,
                              float alistarRadius = 80.0f,
                              float targetRadius = 65.0f,
                              float bufferedQCastSeconds = 0.15f) {
    const float total = HeadbuttTravelSeconds(
        centerDistance, alistarRadius, targetRadius) +
        std::max(0.0f, bufferedQCastSeconds);
    return static_cast<int>(std::ceil(total * 1000.0f));
}

inline Vec3 KnockbackEndpoint(const Vec3& castOrigin,
                              const Vec3& targetAtImpact,
                              bool pulverizeBuffered = false,
                              float fullDistance = kHeadbuttDistance,
                              float bufferedDistance =
                                  kBufferedHeadbuttDistance) {
    const Vec3 direction = Direction2D(castOrigin, targetAtImpact);
    if (direction.IsZero()) return targetAtImpact;
    Vec3 result = targetAtImpact + direction * std::max(
        0.0f, pulverizeBuffered ? bufferedDistance : fullDistance);
    result.y = targetAtImpact.y;
    return result;
}

inline Vec3 StopBeforeWall(const Vec3& targetAtImpact,
                           const Vec3& desiredEndpoint,
                           const Vec3& firstWallPoint,
                           float stopPadding = 18.0f) {
    if (!firstWallPoint.IsValid() || firstWallPoint.IsZero()) {
        return desiredEndpoint;
    }
    const auto projection = ProjectPointToSegment2D(
        firstWallPoint, targetAtImpact, desiredEndpoint);
    if (projection.T <= 0.0f || projection.T >= 1.0f) {
        return desiredEndpoint;
    }
    const Vec3 direction = Direction2D(targetAtImpact, desiredEndpoint);
    Vec3 result = firstWallPoint - direction * std::max(0.0f, stopPadding);
    result.y = targetAtImpact.y;
    return result;
}

inline float TowardPointGain(const Vec3& before,
                             const Vec3& after,
                             const Vec3& desiredPoint) {
    if (!before.IsValid() || !after.IsValid() ||
        !desiredPoint.IsValid()) {
        return -FLT_MAX;
    }
    return before.Distance2D(desiredPoint) -
           after.Distance2D(desiredPoint);
}

inline float PeelSeparationGain(const Vec3& protectedAlly,
                                const Vec3& enemyBefore,
                                const Vec3& enemyAfter) {
    if (!protectedAlly.IsValid() || !enemyBefore.IsValid() ||
        !enemyAfter.IsValid()) {
        return -FLT_MAX;
    }
    return protectedAlly.Distance2D(enemyAfter) -
           protectedAlly.Distance2D(enemyBefore);
}

inline bool PulverizeHits(const Vec3& alistarPosition,
                          const Vec3& targetPosition,
                          float targetRadius = 0.0f,
                          float effectRadius = kPulverizeRadius) {
    return alistarPosition.IsValid() && targetPosition.IsValid() &&
           alistarPosition.Distance2D(targetPosition) <=
               std::max(0.0f, effectRadius) +
               std::clamp(targetRadius, 0.0f, 150.0f);
}

inline int PulverizeHitCount(const Vec3& alistarPosition,
                             const std::vector<Vec3>& targets,
                             float targetRadius = 0.0f,
                             float effectRadius = kPulverizeRadius) {
    int count = 0;
    for (const auto& target : targets) {
        if (PulverizeHits(
                alistarPosition, target, targetRadius, effectRadius)) {
            ++count;
        }
    }
    return count;
}

inline int TramplePulseCount(float elapsedSeconds,
                             float pulseSeconds = kTramplePulseSeconds,
                             int maximumPulses = 10) {
    if (elapsedSeconds < 0.0f || maximumPulses <= 0) return 0;
    const float cadence = std::max(0.05f, pulseSeconds);
    return std::clamp(
        1 + static_cast<int>(std::floor(elapsedSeconds / cadence)),
        0,
        maximumPulses);
}

inline int TrampleStacksFromContinuousContact(
    float elapsedSeconds,
    int maximumStacks = 5) {
    return std::min(
        std::max(0, maximumStacks),
        TramplePulseCount(elapsedSeconds));
}

inline int MillisecondsToNextTramplePulse(
    int elapsedMs,
    int cadenceMs = 500) {
    if (elapsedMs < 0) return 0;
    const int cadence = std::max(1, cadenceMs);
    const int remainder = elapsedMs % cadence;
    return remainder == 0 ? 0 : cadence - remainder;
}

// A high-level Alistar trick: begin the attack at four stacks when its impact
// occurs after the fifth pulse. The on-hit then consumes the newly-created
// empowered attack without adding an avoidable half-second gap.
inline bool FourStackAttackWillStun(int observedStacks,
                                    int millisecondsToNextPulse,
                                    int attackImpactMs,
                                    bool targetRemainsInTrample) {
    if (observedStacks >= 5) return true;
    return observedStacks == 4 && targetRemainsInTrample &&
           attackImpactMs >= std::max(0, millisecondsToNextPulse);
}

inline float EmpoweredTrampleRawDamage(int championLevel) {
    const int level = std::clamp(championLevel, 1, 18);
    return 20.0f + 15.0f * static_cast<float>(level - 1);
}

inline float UltimateDamageReduction(int ultimateRank) {
    static constexpr float reductions[] = {
        0.0f, 0.55f, 0.65f, 0.75f,
    };
    return reductions[std::clamp(ultimateRank, 0, 3)];
}

inline float DamageTakenWithUltimate(int ultimateRank,
                                     float mitigableDamage,
                                     float trueDamage = 0.0f) {
    const float reduction = UltimateDamageReduction(ultimateRank);
    return std::max(0.0f, mitigableDamage) * (1.0f - reduction) +
           std::max(0.0f, trueDamage);
}

inline Vec3 AveragePoint(const std::vector<Vec3>& points) {
    Vec3 result{};
    int count = 0;
    for (const auto& point : points) {
        if (!point.IsValid()) continue;
        result = result + point;
        ++count;
    }
    if (count <= 0) return {};
    result = result / static_cast<float>(count);
    return result;
}

} // namespace Plugins::KuroAIO::AI::Controllers::Alistar::Geometry
