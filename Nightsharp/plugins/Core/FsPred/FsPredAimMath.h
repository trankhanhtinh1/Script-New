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
