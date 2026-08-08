#pragma once

#include "../../../core/Vector.h"
#include "../../../sdk/Enumerations/SpellType.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace Plugins::FsPred::AoeMath {
inline constexpr std::size_t kMaxTargets = 5;
enum class ShapeDispatch {
    Line,
    Circle,
    Cone,
    SingleTargetFallback
};

inline ShapeDispatch ResolveShapeDispatch(SDK::SpellType type) {
    if (SDK::IsLineSpellType(type)) {
        return ShapeDispatch::Line;
    }
    if (SDK::IsCircleSpellType(type)) {
        return ShapeDispatch::Circle;
    }
    if (SDK::IsConeSpellType(type)) {
        return ShapeDispatch::Cone;
    }
    return ShapeDispatch::SingleTargetFallback;
}

inline int HitCount(std::uint8_t mask) {
    int count = 0;
    while (mask != 0) {
        count += mask & 1u;
        mask >>= 1u;
    }
    return count;
}

inline bool IsBetterPrimaryCandidate(std::uint8_t hitMask,
                                     float distanceSquared,
                                     int bestHitCount,
                                     float bestDistanceSquared) {
    if ((hitMask & 1u) == 0 || !std::isfinite(distanceSquared)) {
        return false;
    }
    const int count = HitCount(hitMask);
    return count > bestHitCount ||
           (count == bestHitCount &&
            distanceSquared < bestDistanceSquared);
}

inline bool PointHitsLine(const Vec2& point,
                          const Vec2& start,
                          const Vec2& end,
                          float radius) {
    if (!std::isfinite(radius) || radius < 0.0f) {
        return false;
    }
    const Vec2 segment = end - start;
    const float lengthSquared = segment.LengthSqr();
    if (lengthSquared <= 1.0e-6f) {
        return false;
    }
    const float projection = (point - start).Dot(segment) / lengthSquared;
    if (projection < 0.0f || projection > 1.0f) {
        return false;
    }
    const Vec2 closest = start + segment * projection;
    return point.DistanceSqr(closest) <= radius * radius + 1.0e-3f;
}

inline float ConeHalfAngleRadians(float fullAngleDegrees) {
    if (!std::isfinite(fullAngleDegrees) ||
        fullAngleDegrees <= 1.0f ||
        fullAngleDegrees >= 180.0f) {
        return 0.0f;
    }
    return fullAngleDegrees * 3.14159265358979323846f / 360.0f;
}

inline bool PointHitsCone(const Vec2& relativePoint,
                          const Vec2& normalizedDirection,
                          float range,
                          float halfAngleRadians,
                          float targetRadius = 0.0f) {
    if (!std::isfinite(range) || range <= 0.0f ||
        !std::isfinite(halfAngleRadians) || halfAngleRadians <= 0.0f ||
        !std::isfinite(targetRadius) || targetRadius < 0.0f ||
        normalizedDirection.LengthSqr() <= 1.0e-6f) {
        return false;
    }

    const float forward = relativePoint.Dot(normalizedDirection);
    if (forward < -targetRadius ||
        forward > range + targetRadius + 1.0e-3f) {
        return false;
    }
    const float lateral = std::abs(
        normalizedDirection.x * relativePoint.y -
        normalizedDirection.y * relativePoint.x);
    const float centerlineWidth =
        std::max(0.0f, forward) * std::tan(halfAngleRadians);
    return lateral <= centerlineWidth + targetRadius + 1.0e-3f;
}

} // namespace Plugins::FsPred::AoeMath
