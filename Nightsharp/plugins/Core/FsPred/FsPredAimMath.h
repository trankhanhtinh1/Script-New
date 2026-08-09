#pragma once

#include "../../../core/Vector.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <optional>
#include <span>

namespace Plugins::FsPred::AimMath {

inline float EffectiveDelay(float inputDelaySeconds,
                            int pingMilliseconds,
                            int extraDelayMilliseconds,
                            bool firstTick) {
    if (!firstTick) {
        return inputDelaySeconds;
    }
    return std::max(0.0f, inputDelaySeconds) +
        static_cast<float>(std::max(0, pingMilliseconds)) / 2000.0f +
        static_cast<float>(std::clamp(extraDelayMilliseconds, 0, 100)) / 1000.0f;
}

inline std::optional<float> ProjectileTravelTime(float distance,
                                                 float projectileSpeed) {
    if (!std::isfinite(distance) || distance < 0.0f) {
        return std::nullopt;
    }
    if (projectileSpeed == FLT_MAX) {
        return 0.0f;
    }
    if (!std::isfinite(projectileSpeed) || projectileSpeed <= 0.0f) {
        return std::nullopt;
    }
    return distance / projectileSpeed;
}

struct PathProjectionResult {
    Vec3 Position{};
    float DistanceSquared = FLT_MAX;
    bool HasProjection = false;
};

inline PathProjectionResult ProjectPositionOnPath(
    const Vec3& position,
    std::span<const Vec3> path) {
    PathProjectionResult result{};
    if (!position.IsValid() || path.size() < 2) {
        return result;
    }

    Vec3 previous{};
    bool hasPrevious = false;
    for (const Vec3& point : path) {
        if (!point.IsValid()) {
            hasPrevious = false;
            continue;
        }
        if (!hasPrevious) {
            previous = point;
            hasPrevious = true;
            continue;
        }

        const float segmentX = point.x - previous.x;
        const float segmentZ = point.z - previous.z;
        const float segmentLengthSquared =
            segmentX * segmentX + segmentZ * segmentZ;
        if (segmentLengthSquared <= 1.0e-6f) {
            previous = point;
            continue;
        }

        const float projection = std::clamp(
            ((position.x - previous.x) * segmentX +
             (position.z - previous.z) * segmentZ) /
                segmentLengthSquared,
            0.0f,
            1.0f);
        const Vec3 projected{
            previous.x + segmentX * projection,
            previous.y + (point.y - previous.y) * projection,
            previous.z + segmentZ * projection
        };
        const float distanceSquared =
            position.DistanceSqr2D(projected);
        if (!result.HasProjection ||
            distanceSquared < result.DistanceSquared) {
            result.Position = projected;
            result.DistanceSquared = distanceSquared;
            result.HasProjection = true;
        }
        previous = point;
    }
    return result;
}

inline Vec3 SelectHybridUnitPosition(
    const Vec3& serverPosition,
    const Vec3& clientPosition,
    bool isMoving,
    bool isCasting,
    bool isDashing,
    std::span<const Vec3> path) {
    const bool hasServerPosition =
        serverPosition.IsValid() && !serverPosition.IsZero();
    const bool hasClientPosition =
        clientPosition.IsValid() && !clientPosition.IsZero();

    if (!isMoving || isCasting || isDashing) {
        return hasServerPosition
            ? serverPosition
            : (hasClientPosition ? clientPosition : Vec3{});
    }
    if (!hasClientPosition) {
        return hasServerPosition ? serverPosition : Vec3{};
    }

    const PathProjectionResult projected =
        ProjectPositionOnPath(clientPosition, path);
    return projected.HasProjection ? projected.Position : clientPosition;
}

inline Vec3 SelectCastOrigin(
    const Vec3& explicitOrigin,
    const Vec3& playerClientPosition,
    const Vec3& playerServerPosition) {
    if (explicitOrigin.IsValid() && !explicitOrigin.IsZero()) {
        return explicitOrigin;
    }
    if (playerClientPosition.IsValid() &&
        !playerClientPosition.IsZero()) {
        return playerClientPosition;
    }
    return playerServerPosition.IsValid() &&
           !playerServerPosition.IsZero()
        ? playerServerPosition
        : Vec3{};
}

inline bool ArrivesDuringDash(float arrivalSeconds,
                              float remainingDashSeconds,
                              int toleranceMilliseconds = 80) {
    return std::isfinite(arrivalSeconds) &&
        std::isfinite(remainingDashSeconds) &&
        arrivalSeconds >= 0.0f &&
        remainingDashSeconds >= 0.0f &&
        arrivalSeconds <= remainingDashSeconds +
            static_cast<float>(std::max(0, toleranceMilliseconds)) / 1000.0f;
}

inline bool IsImmobileAtImpact(float arrivalSeconds,
                               double remainingImmobileSeconds,
                               double radiusWindowSeconds) {
    return std::isfinite(arrivalSeconds) &&
        std::isfinite(remainingImmobileSeconds) &&
        std::isfinite(radiusWindowSeconds) &&
        arrivalSeconds >= 0.0f &&
        remainingImmobileSeconds >= 0.0 &&
        radiusWindowSeconds >= 0.0 &&
        static_cast<double>(arrivalSeconds) <=
            remainingImmobileSeconds + radiusWindowSeconds;
}


struct PathAdvanceResult {
    Vec3 Position{};
    bool HasRoute = false;
    bool ReachedRequestedDistance = false;
};

inline PathAdvanceResult AdvancePath(const Vec3& start,
                                     float movementSpeed,
                                     float seconds,
                                     std::span<const Vec3> path) {
    PathAdvanceResult result{};
    result.Position = start;
    if (!start.IsValid() || !std::isfinite(movementSpeed) || movementSpeed <= 0.0f ||
        !std::isfinite(seconds) || seconds < 0.0f) {
        return result;
    }

    float remainingDistance = movementSpeed * seconds;
    Vec3 current = start;
    for (const Vec3& waypoint : path) {
        if (!waypoint.IsValid()) {
            continue;
        }
        const float segmentLength = current.Distance2D(waypoint);
        if (segmentLength <= 1.0e-3f) {
            current = waypoint;
            continue;
        }

        result.HasRoute = true;
        if (remainingDistance <= segmentLength) {
            result.Position = current.Extend(waypoint, remainingDistance);
            result.ReachedRequestedDistance = true;
            return result;
        }
        remainingDistance -= segmentLength;
        current = waypoint;
    }

    result.Position = current;
    result.ReachedRequestedDistance = remainingDistance <= 1.0e-3f;
    return result;
}

} // namespace Plugins::FsPred::AimMath
