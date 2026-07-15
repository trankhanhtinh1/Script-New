#pragma once

// Geometry primitives used by the replacement Evade engine.  The candidate
// construction and polygon-edge projection mirror Geometry.cs/Evader.cs from
// the supplied Evade source while using NightSharp's native Vec2/SDK types.

#include "../../../../Core/CoreNavGrid.h"
#include "../../../../SDK/SDK.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

namespace Plugins::KuroEvade::SourceGeometry {

inline constexpr float Pi = 3.14159265358979323846f;
inline constexpr float Epsilon = 0.0001f;

struct Projection {
    Vec2 LinePoint;
    Vec2 SegmentPoint;
    bool IsOnSegment = false;
    float T = 0.0f;
};

struct Intersection {
    bool Intersects = false;
    Vec2 Point;
};

struct PolygonProjection {
    bool Valid = false;
    bool Inside = false;
    Vec2 Point;
    float Distance = std::numeric_limits<float>::max();
};

// A small sampled signed-distance field for the navigation mesh. Clearance is
// the nearest walkable distance before a wall/building is reached and
// EscapeDirection is the local negative gradient (away from blocked cells).
// This is deliberately sampled in a handful of rays instead of performing a
// dense polar search for every evade candidate.
struct NavigationProbe {
    float Clearance = 0.0f;
    Vec2 EscapeDirection;
    int BlockedRays = 0;
};

inline Vec2 Perpendicular(const Vec2& value) {
    return Vec2(-value.y, value.x);
}

inline Vec2 Rotate(const Vec2& value, float radians) {
    const float c = std::cos(radians);
    const float s = std::sin(radians);
    return Vec2(value.x * c - value.y * s,
                value.x * s + value.y * c);
}

inline float Cross(const Vec2& lhs, const Vec2& rhs) {
    return lhs.x * rhs.y - lhs.y * rhs.x;
}

inline Projection ProjectOn(const Vec2& point,
                            const Vec2& segmentStart,
                            const Vec2& segmentEnd) {
    Projection result;
    const Vec2 segment = segmentEnd - segmentStart;
    const float lengthSqr = segment.LengthSqr();
    if (lengthSqr <= Epsilon) {
        result.LinePoint = segmentStart;
        result.SegmentPoint = segmentStart;
        return result;
    }

    result.T = (point - segmentStart).Dot(segment) / lengthSqr;
    result.LinePoint = segmentStart + segment * result.T;
    result.IsOnSegment = result.T >= 0.0f && result.T <= 1.0f;
    result.SegmentPoint = segmentStart + segment * std::clamp(result.T, 0.0f, 1.0f);
    return result;
}

inline Intersection SegmentIntersection(const Vec2& a,
                                        const Vec2& b,
                                        const Vec2& c,
                                        const Vec2& d) {
    Intersection result;
    const Vec2 r = b - a;
    const Vec2 s = d - c;
    const float denominator = Cross(r, s);
    if (std::fabs(denominator) <= Epsilon) {
        return result;
    }

    const float t = Cross(c - a, s) / denominator;
    const float u = Cross(c - a, r) / denominator;
    if (t < -Epsilon || t > 1.0f + Epsilon ||
        u < -Epsilon || u > 1.0f + Epsilon) {
        return result;
    }

    result.Intersects = true;
    result.Point = a + r * std::clamp(t, 0.0f, 1.0f);
    return result;
}

inline float PointSegmentDistance(const Vec2& point,
                                  const Vec2& start,
                                  const Vec2& end) {
    return point.Distance(ProjectOn(point, start, end).SegmentPoint);
}

inline float SegmentDistance(const Vec2& a,
                             const Vec2& b,
                             const Vec2& c,
                             const Vec2& d) {
    if (SegmentIntersection(a, b, c, d).Intersects) {
        return 0.0f;
    }
    return std::min({
        PointSegmentDistance(a, c, d),
        PointSegmentDistance(b, c, d),
        PointSegmentDistance(c, a, b),
        PointSegmentDistance(d, a, b),
    });
}

inline std::vector<Vec2> ToPoints(const std::vector<SDK::Clipper::IntPoint>& path) {
    std::vector<Vec2> result;
    result.reserve(path.size());
    for (const auto& point : path) {
        result.emplace_back(static_cast<float>(point.X), static_cast<float>(point.Y));
    }
    return result;
}

inline bool PointInPolygon(const Vec2& point, const std::vector<Vec2>& polygon) {
    if (polygon.size() < 3) {
        return false;
    }

    bool inside = false;
    for (std::size_t i = 0, j = polygon.size() - 1; i < polygon.size(); j = i++) {
        const Vec2& a = polygon[i];
        const Vec2& b = polygon[j];
        if (PointSegmentDistance(point, a, b) <= 0.75f) {
            return true;
        }
        const bool crosses = ((a.y > point.y) != (b.y > point.y)) &&
            point.x < (b.x - a.x) * (point.y - a.y) /
                ((b.y - a.y) + std::numeric_limits<float>::epsilon()) + a.x;
        if (crosses) {
            inside = !inside;
        }
    }
    return inside;
}

inline float DistanceToPolygon(const Vec2& point, const std::vector<Vec2>& polygon) {
    if (polygon.empty()) {
        return std::numeric_limits<float>::max();
    }
    if (PointInPolygon(point, polygon)) {
        return 0.0f;
    }

    float best = std::numeric_limits<float>::max();
    for (std::size_t i = 0; i < polygon.size(); ++i) {
        best = std::min(best, PointSegmentDistance(
            point, polygon[i], polygon[(i + 1) % polygon.size()]));
    }
    return best;
}

inline PolygonProjection ClosestPointOnPolygon(
        const Vec2& point,
        const std::vector<Vec2>& polygon) {
    PolygonProjection result;
    if (polygon.size() < 2) {
        return result;
    }

    result.Inside = PointInPolygon(point, polygon);
    for (std::size_t i = 0; i < polygon.size(); ++i) {
        const Vec2 projected = ProjectOn(
            point, polygon[i], polygon[(i + 1) % polygon.size()]).SegmentPoint;
        const float distance = point.Distance(projected);
        if (distance < result.Distance) {
            result.Valid = true;
            result.Point = projected;
            result.Distance = distance;
        }
    }
    return result;
}

inline bool SegmentIntersectsPolygon(const Vec2& start,
                                     const Vec2& end,
                                     const std::vector<Vec2>& polygon,
                                     float extraRadius = 0.0f) {
    if (polygon.size() < 3) {
        return false;
    }
    if (PointInPolygon(start, polygon) || PointInPolygon(end, polygon)) {
        return true;
    }
    for (std::size_t i = 0; i < polygon.size(); ++i) {
        const Vec2& a = polygon[i];
        const Vec2& b = polygon[(i + 1) % polygon.size()];
        if (SegmentDistance(start, end, a, b) <= extraRadius) {
            return true;
        }
    }
    return false;
}

inline std::vector<Vec2> CirclePoints(const Vec2& center,
                                      float radius,
                                      int quality = 32) {
    std::vector<Vec2> points;
    quality = std::clamp(quality, 8, 96);
    points.reserve(static_cast<std::size_t>(quality));
    for (int i = 0; i < quality; ++i) {
        const float angle = 2.0f * Pi * static_cast<float>(i) /
            static_cast<float>(quality);
        points.emplace_back(center.x + radius * std::cos(angle),
                            center.y + radius * std::sin(angle));
    }
    return points;
}

inline std::vector<Vec2> RectanglePoints(const Vec2& start,
                                         const Vec2& end,
                                         float halfWidth) {
    Vec2 direction = (end - start).Normalized();
    if (direction.IsZero()) {
        direction = Vec2(1.0f, 0.0f);
    }
    const Vec2 side = Perpendicular(direction) * halfWidth;
    return { start + side, end + side, end - side, start - side };
}

inline std::vector<Vec2> CapsulePoints(const Vec2& start,
                                       const Vec2& end,
                                       float radius,
                                       int segments = 18) {
    radius = std::max(0.0f, radius);
    const Vec2 direction = (end - start).Normalized();
    if (direction.IsZero() || start.DistanceSqr(end) <= 0.01f) {
        return CirclePoints(start, radius, std::max(8, segments));
    }

    const int half = std::max(3, segments / 2);
    const float heading = std::atan2(direction.y, direction.x);
    std::vector<Vec2> result;
    result.reserve(static_cast<std::size_t>((half + 1) * 2));
    for (int index = 0; index <= half; ++index) {
        const float angle = heading + Pi * 0.5f +
            Pi * static_cast<float>(index) / static_cast<float>(half);
        result.emplace_back(start.x + std::cos(angle) * radius,
                            start.y + std::sin(angle) * radius);
    }
    for (int index = 0; index <= half; ++index) {
        const float angle = heading - Pi * 0.5f +
            Pi * static_cast<float>(index) / static_cast<float>(half);
        result.emplace_back(end.x + std::cos(angle) * radius,
                            end.y + std::sin(angle) * radius);
    }
    return result;
}

inline std::vector<Vec2> SectorPoints(const Vec2& center,
                                      const Vec2& direction,
                                      float angleRadians,
                                      float radius,
                                      float padding = 0.0f,
                                      int quality = 28) {
    std::vector<Vec2> points;
    Vec2 normalized = direction.Normalized();
    if (normalized.IsZero()) {
        normalized = Vec2(1.0f, 0.0f);
    }
    const float expandedRadius = std::max(1.0f, radius + padding);
    const float angularPadding = padding > 0.0f && radius > 1.0f
        ? std::asin(std::clamp(padding / radius, 0.0f, 1.0f))
        : 0.0f;
    const float half = std::max(0.01f, angleRadians * 0.5f + angularPadding);
    quality = std::clamp(quality, 8, 64);
    points.reserve(static_cast<std::size_t>(quality + 2));
    points.push_back(center);
    for (int i = 0; i <= quality; ++i) {
        const float angle = -half + (2.0f * half * static_cast<float>(i) /
                                     static_cast<float>(quality));
        points.push_back(center + Rotate(normalized, angle) * expandedRadius);
    }
    return points;
}

inline Vec2 PositionAfter(const std::vector<Vec2>& path,
                          float distance) {
    if (path.empty()) {
        return {};
    }
    if (path.size() == 1 || distance <= 0.0f) {
        return path.front();
    }

    float remaining = distance;
    for (std::size_t i = 1; i < path.size(); ++i) {
        const float length = path[i - 1].Distance(path[i]);
        if (length <= Epsilon) {
            continue;
        }
        if (remaining <= length) {
            return path[i - 1] + (path[i] - path[i - 1]) * (remaining / length);
        }
        remaining -= length;
    }
    return path.back();
}

inline float PathLength(const std::vector<Vec2>& path) {
    float result = 0.0f;
    for (std::size_t i = 1; i < path.size(); ++i) {
        result += path[i - 1].Distance(path[i]);
    }
    return result;
}

inline bool IsNavigable(const Vec2& point, float height) {
    if (point.IsZero() || !std::isfinite(point.x) || !std::isfinite(point.y)) {
        return false;
    }
    const Vec3 world = Vec3::From2D(point, height);
    return !CoreNavGrid::IsWall(world) && !CoreNavGrid::IsBuilding(world);
}

inline bool SegmentIsNavigable(const Vec2& start,
                               const Vec2& end,
                               float height,
                               float step = 35.0f) {
    const float distance = start.Distance(end);
    const int samples = std::max(1, static_cast<int>(std::ceil(distance / std::max(10.0f, step))));
    for (int i = 1; i <= samples; ++i) {
        const Vec2 point = start + (end - start) *
            (static_cast<float>(i) / static_cast<float>(samples));
        if (!IsNavigable(point, height)) {
            return false;
        }
    }
    return true;
}

// Finds the first map-wall/building contact on a route and returns the moving
// object's centre at contact. The forward semicircle probes the full collision
// radius instead of only the centre line, while omitting the rear half avoids
// a false impact when a projectile is launched away from a wall behind it.
// Binary refinement keeps the returned contact stable.
inline bool FirstTerrainCollision(const Vec2& start,
                                  const Vec2& end,
                                  float height,
                                  float clearance,
                                  Vec2& collisionPoint,
                                  float step = 18.0f) {
    collisionPoint = {};
    const Vec2 direction = (end - start).Normalized();
    const float length = start.Distance(end);
    if (direction.IsZero() || length <= 1.0f) {
        return false;
    }

    step = std::clamp(step, 5.0f, 60.0f);
    clearance = std::max(0.0f, clearance);
    const auto footprintIsNavigable = [&](float distance) {
        const Vec2 center = start + direction *
            std::clamp(distance, 0.0f, length);
        if (!IsNavigable(center, height)) {
            return false;
        }
        if (clearance <= 1.0f) {
            return true;
        }
        for (int ray = -2; ray <= 2; ++ray) {
            const float angle = static_cast<float>(ray) * Pi * 0.25f;
            if (!IsNavigable(center + Rotate(direction, angle) * clearance,
                             height)) {
                return false;
            }
        }
        return true;
    };
    float previousDistance = 0.0f;
    for (float distance = step; distance <= length + step; distance += step) {
        const float sampledDistance = std::min(length, distance);
        if (footprintIsNavigable(sampledDistance)) {
            previousDistance = sampledDistance;
            if (sampledDistance >= length) {
                break;
            }
            continue;
        }

        float low = previousDistance;
        float high = sampledDistance;
        for (int iteration = 0; iteration < 7; ++iteration) {
            const float middle = (low + high) * 0.5f;
            if (footprintIsNavigable(middle)) {
                low = middle;
            } else {
                high = middle;
            }
        }
        collisionPoint = start + direction *
            std::clamp(high, 0.0f, length);
        return true;
    }
    return false;
}

inline NavigationProbe ProbeNavigation(const Vec2& point,
                                        float height,
                                        float maxDistance = 160.0f,
                                        int rayCount = 8,
                                        int radialSteps = 4) {
    NavigationProbe result;
    maxDistance = std::max(20.0f, maxDistance);
    rayCount = std::clamp(rayCount, 4, 16);
    radialSteps = std::clamp(radialSteps, 2, 6);
    result.Clearance = maxDistance;

    if (!IsNavigable(point, height)) {
        result.Clearance = 0.0f;
    }

    Vec2 escape;
    for (int ray = 0; ray < rayCount; ++ray) {
        const float angle = 2.0f * Pi * static_cast<float>(ray) /
            static_cast<float>(rayCount);
        const Vec2 direction(std::cos(angle), std::sin(angle));
        float openDistance = 0.0f;
        bool blocked = false;

        for (int step = 1; step <= radialSteps; ++step) {
            const float distance = maxDistance * static_cast<float>(step) /
                static_cast<float>(radialSteps);
            if (IsNavigable(point + direction * distance, height)) {
                openDistance = distance;
                continue;
            }

            blocked = true;
            ++result.BlockedRays;
            result.Clearance = std::min(result.Clearance, openDistance);
            const float proximity = 1.0f -
                std::clamp(openDistance / maxDistance, 0.0f, 1.0f);
            // A blocked sample in +d contributes a derivative towards -d.
            escape = escape - direction * (proximity * proximity);
            break;
        }

        if (!blocked) {
            result.Clearance = std::min(result.Clearance, maxDistance);
        }
    }

    result.EscapeDirection = escape.Normalized();
    return result;
}

} // namespace Plugins::KuroEvade::SourceGeometry
