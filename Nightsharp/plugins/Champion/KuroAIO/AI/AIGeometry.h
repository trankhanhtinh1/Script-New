#pragma once

// Champion-neutral 2D geometry primitives.  Champion geometry files compose
// these primitives into their own hitboxes (Aatrox sweetspots, Ahri return
// interception, Akali cone/ring/dash) instead of reimplementing vector math.

#include "../../../../core/Vector.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace Plugins::KuroAIO::AI::SharedGeometry {

inline constexpr float kPi = 3.14159265358979323846f;

// Rank-indexed live data appears in nearly every champion geometry file.
// Clamp and index it once here so controllers do not grow subtly different
// off-by-one handling for unlearned or malformed spell ranks.
template <typename Value, std::size_t N>
inline Value RankValue(const std::array<Value, N>& values, int rank) {
    static_assert(N > 0, "RankValue requires at least one entry");
    return values[static_cast<std::size_t>(
        std::clamp(rank, 0, static_cast<int>(N) - 1))];
}

inline float Cross2D(const Vec3& left, const Vec3& right) {
    return left.x * right.z - left.z * right.x;
}

inline Vec3 Direction2D(const Vec3& from, const Vec3& to) {
    Vec3 direction = to - from;
    direction.y = 0.0f;
    const float length = direction.Length2D();
    if (length <= 0.001f || !std::isfinite(length)) return {};
    return direction / length;
}

inline Vec3 Rotate2D(const Vec3& direction, float radians) {
    const float cosine = std::cos(radians);
    const float sine = std::sin(radians);
    Vec3 rotated{
        direction.x * cosine - direction.z * sine,
        0.0f,
        direction.x * sine + direction.z * cosine,
    };
    const float length = rotated.Length2D();
    return length > 0.001f && std::isfinite(length)
        ? rotated / length
        : Vec3{};
}

struct SegmentProjection {
    float T = 0.0f;
    float Distance = 0.0f;
    Vec3 Closest = {};
};

inline SegmentProjection ProjectPointToSegment2D(const Vec3& point,
                                                  const Vec3& start,
                                                  const Vec3& end) {
    Vec3 segment = end - start;
    segment.y = 0.0f;
    Vec3 relative = point - start;
    relative.y = 0.0f;
    const float lengthSquared = segment.Dot(segment);
    if (lengthSquared <= 0.001f || !std::isfinite(lengthSquared)) {
        return { 0.0f, point.Distance2D(start), start };
    }
    const float t = std::clamp(relative.Dot(segment) / lengthSquared, 0.0f, 1.0f);
    Vec3 closest = start + segment * t;
    closest.y = start.y;
    return { t, point.Distance2D(closest), closest };
}

// Solve the first contact between two circles moving at constant relative
// velocity in the X/Z plane. Projectile champions still own their cast delay,
// travel segment, endpoint and lollipop semantics; this neutral quadratic is
// shared so every first-body solver does not grow a subtly different root and
// malformed-input policy.
inline bool SolveMovingCircleContactTime2D(const Vec3& relativePosition,
                                           const Vec3& relativeVelocity,
                                           float combinedRadius,
                                           float maximumSeconds,
                                           float& resultSeconds) {
    resultSeconds = 0.0f;
    if (!std::isfinite(combinedRadius) ||
        !std::isfinite(maximumSeconds) || maximumSeconds < 0.0f) {
        return false;
    }
    const float radius = std::max(0.0f, combinedRadius);
    const float c = relativePosition.LengthSqr2D() - radius * radius;
    if (c <= 0.0f) return true;

    const float a = relativeVelocity.LengthSqr2D();
    if (a <= 0.0001f || !std::isfinite(a)) return false;
    const float b = 2.0f * relativePosition.Dot(relativeVelocity);
    const float discriminant = b * b - 4.0f * a * c;
    if (discriminant < 0.0f || !std::isfinite(discriminant)) return false;

    const float root = std::sqrt(discriminant);
    const float first = (-b - root) / (2.0f * a);
    const float second = (-b + root) / (2.0f * a);
    const float candidate = first >= 0.0f ? first : second;
    if (candidate < 0.0f || candidate > maximumSeconds ||
        !std::isfinite(candidate)) {
        return false;
    }
    resultSeconds = candidate;
    return true;
}

} // namespace Plugins::KuroAIO::AI::SharedGeometry
