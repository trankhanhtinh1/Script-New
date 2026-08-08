#pragma once

#include "../../core/Vector.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstddef>
#include <optional>
#include <span>

namespace SDK::MovingProjectileCollision {
namespace detail {

inline constexpr double kGeometryEpsilon = 1.0e-8;

inline Vec2 PositionAtTime(const Vec3& start,
                           float movementSpeed,
                           std::span<const Vec3> path,
                           double targetTime) {
    Vec2 current = start.To2D();
    if (!(targetTime > 0.0) || !std::isfinite(movementSpeed) || movementSpeed <= 0.0f) {
        return current;
    }

    double elapsed = 0.0;
    for (const Vec3& waypoint3D : path) {
        if (!waypoint3D.IsValid()) {
            continue;
        }

        const Vec2 waypoint = waypoint3D.To2D();
        const Vec2 delta = waypoint - current;
        const double distance = static_cast<double>(delta.Length());
        if (distance <= kGeometryEpsilon) {
            current = waypoint;
            continue;
        }

        const double duration = distance / static_cast<double>(movementSpeed);
        if (targetTime <= elapsed + duration) {
            const double fraction = std::clamp((targetTime - elapsed) / duration, 0.0, 1.0);
            return current + delta * static_cast<float>(fraction);
        }

        elapsed += duration;
        current = waypoint;
    }

    return current;
}

inline double DistanceSquaredToSegment(const Vec2& point,
                                       const Vec2& start,
                                       const Vec2& end) {
    const Vec2 segment = end - start;
    const double lengthSquared = static_cast<double>(segment.LengthSqr());
    if (lengthSquared <= kGeometryEpsilon) {
        return static_cast<double>(point.DistanceSqr(start));
    }

    const Vec2 fromStart = point - start;
    const double projection = std::clamp(
        static_cast<double>(fromStart.Dot(segment)) / lengthSquared,
        0.0,
        1.0);
    const Vec2 closest = start + segment * static_cast<float>(projection);
    return static_cast<double>(point.DistanceSqr(closest));
}

inline std::optional<double> FirstContactInInterval(
    const Vec2& relativeAtStart,
    const Vec2& relativeVelocity,
    double intervalStart,
    double intervalEnd,
    double radius) {
    if (intervalEnd + kGeometryEpsilon < intervalStart) {
        return std::nullopt;
    }

    const double radiusSquared = radius * radius;
    const double relativeX = static_cast<double>(relativeAtStart.x);
    const double relativeY = static_cast<double>(relativeAtStart.y);
    const double velocityX = static_cast<double>(relativeVelocity.x);
    const double velocityY = static_cast<double>(relativeVelocity.y);
    const double c = relativeX * relativeX + relativeY * relativeY - radiusSquared;
    if (c <= kGeometryEpsilon) {
        return intervalStart;
    }

    const double a = velocityX * velocityX + velocityY * velocityY;
    if (a <= kGeometryEpsilon) {
        return std::nullopt;
    }

    const double b = relativeX * velocityX + relativeY * velocityY;
    double discriminant = b * b - a * c;
    if (discriminant < -kGeometryEpsilon) {
        return std::nullopt;
    }
    discriminant = std::max(0.0, discriminant);

    const double root = std::sqrt(discriminant);
    const double enter = (-b - root) / a;
    const double leave = (-b + root) / a;
    const double duration = std::max(0.0, intervalEnd - intervalStart);
    if (leave < -kGeometryEpsilon || enter > duration + kGeometryEpsilon) {
        return std::nullopt;
    }

    return intervalStart + std::clamp(enter, 0.0, duration);
}

} // namespace detail

// Returns absolute seconds from now to the first projectile/blocker contact.
// The projectile exists only from delay until it reaches segmentEnd. FLT_MAX is
// the SDK's delay-only sentinel: the blocker is advanced to delay and tested
// against the complete finite capsule at that instant.
inline std::optional<float> FirstContactTime(
    const Vec3& segmentStart,
    const Vec3& segmentEnd,
    float delaySeconds,
    float projectileSpeed,
    float combinedRadius,
    const Vec3& blockerStart,
    float blockerSpeed,
    std::span<const Vec3> blockerPath) {
    if (!segmentStart.IsValid() || !segmentEnd.IsValid() || !blockerStart.IsValid() ||
        !std::isfinite(delaySeconds) || !std::isfinite(combinedRadius) ||
        combinedRadius < 0.0f) {
        return std::nullopt;
    }

    const double delay = std::max(0.0, static_cast<double>(delaySeconds));
    const double radius = static_cast<double>(combinedRadius);
    const Vec2 projectileStart = segmentStart.To2D();
    const Vec2 projectileEnd = segmentEnd.To2D();

    if (projectileSpeed == FLT_MAX) {
        const Vec2 blockerAtDelay = detail::PositionAtTime(
            blockerStart,
            blockerSpeed,
            blockerPath,
            delay);
        if (detail::DistanceSquaredToSegment(
                blockerAtDelay,
                projectileStart,
                projectileEnd) <= radius * radius + detail::kGeometryEpsilon) {
            return static_cast<float>(delay);
        }
        return std::nullopt;
    }

    if (!std::isfinite(projectileSpeed) || projectileSpeed <= 0.0f) {
        return std::nullopt;
    }

    const Vec2 projectileDelta = projectileEnd - projectileStart;
    const double projectileDistance = static_cast<double>(projectileDelta.Length());
    if (projectileDistance <= detail::kGeometryEpsilon) {
        const Vec2 blockerAtDelay = detail::PositionAtTime(
            blockerStart,
            blockerSpeed,
            blockerPath,
            delay);
        if (static_cast<double>(blockerAtDelay.DistanceSqr(projectileStart)) <=
            radius * radius + detail::kGeometryEpsilon) {
            return static_cast<float>(delay);
        }
        return std::nullopt;
    }

    const double travelDuration = projectileDistance / static_cast<double>(projectileSpeed);
    const double projectileEndTime = delay + travelDuration;
    if (!std::isfinite(projectileEndTime)) {
        return std::nullopt;
    }

    const Vec2 projectileVelocity = projectileDelta *
        static_cast<float>(static_cast<double>(projectileSpeed) / projectileDistance);
    const double movementSpeed = std::isfinite(blockerSpeed) && blockerSpeed > 0.0f
        ? static_cast<double>(blockerSpeed)
        : 0.0;

    Vec2 blockerPosition = blockerStart.To2D();
    double blockerSegmentStart = 0.0;

    if (movementSpeed > 0.0) {
        for (const Vec3& waypoint3D : blockerPath) {
            if (!waypoint3D.IsValid()) {
                continue;
            }

            const Vec2 waypoint = waypoint3D.To2D();
            const Vec2 blockerDelta = waypoint - blockerPosition;
            const double blockerDistance = static_cast<double>(blockerDelta.Length());
            if (blockerDistance <= detail::kGeometryEpsilon) {
                blockerPosition = waypoint;
                continue;
            }

            const double blockerDuration = blockerDistance / movementSpeed;
            const double blockerSegmentEnd = blockerSegmentStart + blockerDuration;
            const double overlapStart = std::max(delay, blockerSegmentStart);
            const double overlapEnd = std::min(projectileEndTime, blockerSegmentEnd);
            if (overlapStart <= overlapEnd + detail::kGeometryEpsilon) {
                const Vec2 blockerVelocity = blockerDelta *
                    static_cast<float>(movementSpeed / blockerDistance);
                const Vec2 projectileAtOverlap = projectileStart + projectileVelocity *
                    static_cast<float>(overlapStart - delay);
                const Vec2 blockerAtOverlap = blockerPosition + blockerVelocity *
                    static_cast<float>(overlapStart - blockerSegmentStart);
                const auto contact = detail::FirstContactInInterval(
                    projectileAtOverlap - blockerAtOverlap,
                    projectileVelocity - blockerVelocity,
                    overlapStart,
                    overlapEnd,
                    radius);
                if (contact.has_value()) {
                    return static_cast<float>(*contact);
                }
            }

            blockerSegmentStart = blockerSegmentEnd;
            blockerPosition = waypoint;
            if (blockerSegmentStart > projectileEndTime) {
                return std::nullopt;
            }
        }
    }

    const double tailStart = std::max(delay, blockerSegmentStart);
    if (tailStart <= projectileEndTime + detail::kGeometryEpsilon) {
        const Vec2 projectileAtTail = projectileStart + projectileVelocity *
            static_cast<float>(tailStart - delay);
        const auto contact = detail::FirstContactInInterval(
            projectileAtTail - blockerPosition,
            projectileVelocity,
            tailStart,
            projectileEndTime,
            radius);
        if (contact.has_value()) {
            return static_cast<float>(*contact);
        }
    }

    return std::nullopt;
}

} // namespace SDK::MovingProjectileCollision

namespace SDK::Collision {

inline bool ExceedsCollisionAllowance(std::size_t blockerCount,
                                      float maxCollisionCount) {
    if (blockerCount == 0) {
        return false;
    }
    const float allowance = std::isfinite(maxCollisionCount)
        ? std::max(0.0f, maxCollisionCount)
        : 0.0f;
    return static_cast<float>(blockerCount) > allowance;
}

} // namespace SDK::Collision
