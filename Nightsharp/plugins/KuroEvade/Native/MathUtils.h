#pragma once

#include "../../../SDK/SDK.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace Plugins::KuroEvade::MathUtils {

inline constexpr float kEpsilon = 0.0000001f;

inline Vec2 Rotated(const Vec2& value, float angle) {
    const float c = std::cos(angle);
    const float s = std::sin(angle);
    return Vec2(value.x * c - value.y * s, value.x * s + value.y * c);
}

inline std::pair<float, float> LineToLineIntersection(float x1, float y1,
                                                       float x2, float y2,
                                                       float x3, float y3,
                                                       float x4, float y4) {
    const float denominator =
        (y4 - y3) * (x2 - x1) - (x4 - x3) * (y2 - y1);
    if (std::fabs(denominator) <= kEpsilon) {
        const float max = std::numeric_limits<float>::max();
        return { max, max };
    }

    return {
        ((x4 - x3) * (y1 - y3) - (y4 - y3) * (x1 - x3)) / denominator,
        ((x2 - x1) * (y1 - y3) - (y2 - y1) * (x1 - x3)) / denominator
    };
}

inline bool CheckLineIntersectionEx(const Vec2& a, const Vec2& b,
                                    const Vec2& c, const Vec2& d) {
    const auto [first, second] =
        LineToLineIntersection(a.x, a.y, b.x, b.y, c.x, c.y, d.x, d.y);
    return first >= 0.0f && first <= 1.0f &&
           second >= 0.0f && second <= 1.0f;
}

inline bool CheckLineIntersection(const Vec2& a, const Vec2& b,
                                  const Vec2& c, const Vec2& d) {
    return CheckLineIntersectionEx(a, b, c, d);
}

inline Vec2 CheckLineIntersectionEx2(const Vec2& a, const Vec2& b,
                                     const Vec2& c, const Vec2& d) {
    const auto [first, second] =
        LineToLineIntersection(a.x, a.y, b.x, b.y, c.x, c.y, d.x, d.y);
    if (first >= 0.0f && first <= 1.0f &&
        second >= 0.0f && second <= 1.0f) {
        return { first, second };
    }
    return {};
}

inline Vec2 RotateVector(const Vec2& start, const Vec2& end, float angleDegrees) {
    constexpr float kDegreesToRadians = 0.017453292f;
    const float angle = angleDegrees * kDegreesToRadians;
    return start + Rotated(end - start, angle);
}

inline float VectorMovementCollisionEx(Vec2 targetPos, const Vec2& targetDir,
                                       float targetSpeed, const Vec2& sourcePos,
                                       float projectileSpeed, bool& collision,
                                       float extraDelay = 0.0f,
                                       float extraDistance = 0.0f) {
    const Vec2 velocity = targetDir * targetSpeed;
    targetPos = targetPos - velocity * (extraDelay / 1000.0f);
    const Vec2 offset = targetPos - sourcePos;
    const float a = velocity.Dot(velocity) - projectileSpeed * projectileSpeed;
    const float b = 2.0f * velocity.Dot(offset);
    const float c = std::max(0.0f, offset.Dot(offset) + extraDistance * extraDistance);

    if (std::fabs(a) <= kEpsilon) {
        if (std::fabs(b) > kEpsilon) {
            const float time = -c / b;
            collision = time > 0.0f;
            return collision ? time : 0.0f;
        }
        collision = false;
        return 0.0f;
    }

    const float discriminant = b * b - 4.0f * a * c;
    if (discriminant >= 0.0f) {
        const float root = std::sqrt(discriminant);
        const float first = -(b + root) / (2.0f * a);
        const float second = -(b - root) / (2.0f * a);
        collision = true;
        if (first > 0.0f && second > 0.0f) {
            return std::min(first, second);
        }
        if (first > 0.0f) {
            return first;
        }
        if (second > 0.0f) {
            return second;
        }
    }

    collision = false;
    return 0.0f;
}

inline bool PointOnLineSegment(const Vec2& point, const Vec2& start,
                               const Vec2& end) {
    const float projection = (end - start).Dot(point - start);
    return projection >= 0.0f && projection <= start.DistanceSqr(end);
}

inline bool IsPointOnLineSegmentStrict(const Vec2& point, const Vec2& start,
                                       const Vec2& end) {
    return std::max(start.x, end.x) > point.x &&
           point.x > std::min(start.x, end.x) &&
           std::max(start.y, end.y) > point.y &&
           point.y > std::min(start.y, end.y);
}

inline bool isPointOnLineSegment(const Vec2& point, const Vec2& start,
                                 const Vec2& end) {
    return IsPointOnLineSegmentStrict(point, start, end);
}

inline float GetCollisionTime(const Vec2& pa, const Vec2& pb,
                              const Vec2& va, const Vec2& vb,
                              float ra, float rb, bool& collision) {
    const Vec2 relativePosition = pa - pb;
    const Vec2 relativeVelocity = va - vb;
    const float a = relativeVelocity.Dot(relativeVelocity);
    const float b = 2.0f * relativePosition.Dot(relativeVelocity);
    const float radius = ra + rb;
    const float c = relativePosition.Dot(relativePosition) - radius * radius;

    if (a <= kEpsilon) {
        collision = c <= 0.0f;
        return 0.0f;
    }

    const float discriminant = b * b - 4.0f * a * c;
    float time = 0.0f;
    if (discriminant < 0.0f) {
        time = -b / (2.0f * a);
        collision = false;
    } else {
        const float root = std::sqrt(discriminant);
        const float first = (-b + root) / (2.0f * a);
        const float second = (-b - root) / (2.0f * a);
        time = first >= 0.0f && second >= 0.0f
            ? std::min(first, second)
            : std::max(first, second);
        collision = time >= 0.0f;
    }
    return std::max(0.0f, time);
}

inline float GetCollisionDistanceEx(const Vec2& pa, const Vec2& va, float ra,
                                    const Vec2& pb, const Vec2& vb, float rb,
                                    Vec2& outPa, Vec2& outPb) {
    bool collision = false;
    const float time = GetCollisionTime(pa, pb, va, vb, ra, rb, collision);
    if (!collision) {
        outPa = {};
        outPb = {};
        return std::numeric_limits<float>::max();
    }

    outPa = pa + va * time;
    outPb = pb + vb * time;
    return outPa.Distance(outPb);
}

inline Vec2 ProjectOnSegment(const Vec2& point, const Vec2& start,
                             const Vec2& end, bool* onSegment = nullptr) {
    const Vec2 segment = end - start;
    const float lengthSquared = segment.LengthSqr();
    if (lengthSquared <= kEpsilon) {
        if (onSegment) {
            *onSegment = false;
        }
        return start;
    }

    const float amount = (point - start).Dot(segment) / lengthSquared;
    if (onSegment) {
        *onSegment = amount >= 0.0f && amount <= 1.0f;
    }
    return start + segment * std::clamp(amount, 0.0f, 1.0f);
}

inline float GetCollisionDistance(const Vec2& pa, const Vec2& paEnd,
                                  const Vec2& va, float ra,
                                  const Vec2& pb, const Vec2& pbEnd,
                                  const Vec2& vb, float rb) {
    bool collision = false;
    const float time = GetCollisionTime(pa, pb, va, vb, ra, rb, collision);
    if (!collision) {
        return std::numeric_limits<float>::max();
    }

    const Vec2 first = ProjectOnSegment(pa + va * time, pa, paEnd);
    const Vec2 second = ProjectOnSegment(pb + vb * time, pb, pbEnd);
    return first.Distance(second);
}

inline int FindLineCircleIntersections(const Vec2& center, float radius,
                                       const Vec2& from, const Vec2& to,
                                       Vec2& first, Vec2& second) {
    const Vec2 direction = to - from;
    const float a = direction.Dot(direction);
    const float b = 2.0f * direction.Dot(from - center);
    const Vec2 relative = from - center;
    const float c = relative.Dot(relative) - radius * radius;
    const float discriminant = b * b - 4.0f * a * c;
    const float nan = std::numeric_limits<float>::quiet_NaN();

    if (a <= kEpsilon || discriminant < 0.0f) {
        first = { nan, nan };
        second = { nan, nan };
        return 0;
    }

    if (std::fabs(discriminant) <= kEpsilon) {
        const float amount = -b / (2.0f * a);
        first = from + direction * amount;
        second = { nan, nan };
        return amount >= 0.0f && amount <= 1.0f ? 1 : 0;
    }

    const float root = std::sqrt(discriminant);
    const float firstAmount = (-b + root) / (2.0f * a);
    const float secondAmount = (-b - root) / (2.0f * a);
    first = from + direction * firstAmount;
    second = from + direction * secondAmount;
    const bool firstOnSegment = firstAmount >= 0.0f && firstAmount <= 1.0f;
    const bool secondOnSegment = secondAmount >= 0.0f && secondAmount <= 1.0f;
    if (firstOnSegment && secondOnSegment) {
        return 2;
    }
    if (firstOnSegment) {
        return 1;
    }
    if (secondOnSegment) {
        first = second;
        return 1;
    }
    return 0;
}

} // namespace Plugins::KuroEvade::MathUtils
