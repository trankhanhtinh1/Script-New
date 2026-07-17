#pragma once

// Champion-neutral 2D geometry primitives.  Champion geometry files compose
// these primitives into their own hitboxes (Aatrox sweetspots, Ahri return
// interception, Akali cone/ring/dash) instead of reimplementing vector math.

#include "../../../../core/Vector.h"

#include <algorithm>
#include <cmath>

namespace Plugins::KuroAIO::AI::SharedGeometry {

inline constexpr float kPi = 3.14159265358979323846f;

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

} // namespace Plugins::KuroAIO::AI::SharedGeometry

