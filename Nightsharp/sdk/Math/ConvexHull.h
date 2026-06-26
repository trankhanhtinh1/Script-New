#pragma once

#include "../../core/Vector.h"

#include <algorithm>
#include <cmath>
#include <cfloat>
#include <limits>
#include <vector>

namespace SDK::ConvexHull {

struct MecCircle {
    Vec2 Center = {};
    float Radius = 0.0f;
};

namespace detail {

inline float Cross(const Vec2& o, const Vec2& a, const Vec2& b) {
    return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x);
}

inline float DistanceSquared(const Vec2& a, const Vec2& b) {
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    return dx * dx + dy * dy;
}

inline bool CircleEnclosesPoint(const Vec2& center, float radiusSq, const Vec2& point) {
    return DistanceSquared(center, point) <= (radiusSq + 1e-3f);
}

inline bool CircleEnclosesPoints(const Vec2& center, float radiusSq, const std::vector<Vec2>& points) {
    for (const auto& point : points) {
        if (!CircleEnclosesPoint(center, radiusSq, point)) {
            return false;
        }
    }
    return true;
}

inline bool FindCircle(const Vec2& a, const Vec2& b, const Vec2& c, Vec2& center, float& radiusSq) {
    const float aSq = a.x * a.x + a.y * a.y;
    const float bSq = b.x * b.x + b.y * b.y;
    const float cSq = c.x * c.x + c.y * c.y;
    const float d = 2.0f * (a.x * (b.y - c.y) + b.x * (c.y - a.y) + c.x * (a.y - b.y));

    if (std::fabs(d) < 1e-6f) {
        center = {};
        radiusSq = FLT_MAX;
        return false;
    }

    center.x = (aSq * (b.y - c.y) + bSq * (c.y - a.y) + cSq * (a.y - b.y)) / d;
    center.y = (aSq * (c.x - b.x) + bSq * (a.x - c.x) + cSq * (b.x - a.x)) / d;
    radiusSq = DistanceSquared(center, a);
    return true;
}

} // namespace detail

inline std::vector<Vec2> MakeConvexHull(std::vector<Vec2> points) {
    if (points.size() <= 1) {
        return points;
    }

    std::sort(points.begin(), points.end(), [](const Vec2& lhs, const Vec2& rhs) {
        if (lhs.x != rhs.x) {
            return lhs.x < rhs.x;
        }
        return lhs.y < rhs.y;
    });

    points.erase(std::unique(points.begin(), points.end(), [](const Vec2& lhs, const Vec2& rhs) {
        return std::fabs(lhs.x - rhs.x) < 1e-4f && std::fabs(lhs.y - rhs.y) < 1e-4f;
    }), points.end());

    if (points.size() <= 2) {
        return points;
    }

    std::vector<Vec2> lower = {};
    for (const auto& point : points) {
        while (lower.size() >= 2 &&
               detail::Cross(lower[lower.size() - 2], lower.back(), point) <= 0.0f) {
            lower.pop_back();
        }
        lower.push_back(point);
    }

    std::vector<Vec2> upper = {};
    for (auto it = points.rbegin(); it != points.rend(); ++it) {
        while (upper.size() >= 2 &&
               detail::Cross(upper[upper.size() - 2], upper.back(), *it) <= 0.0f) {
            upper.pop_back();
        }
        upper.push_back(*it);
    }

    lower.pop_back();
    upper.pop_back();
    lower.insert(lower.end(), upper.begin(), upper.end());
    return lower;
}

inline void FindMinimalBoundingCircle(const std::vector<Vec2>& points, Vec2& center, float& radius) {
    center = {};
    radius = 0.0f;
    if (points.empty()) {
        return;
    }

    Vec2 bestCenter = points.front();
    float bestRadiusSq = FLT_MAX;

    if (points.size() == 1) {
        center = points.front();
        radius = 0.0f;
        return;
    }

    for (size_t i = 0; i + 1 < points.size(); ++i) {
        for (size_t j = i + 1; j < points.size(); ++j) {
            const Vec2 testCenter((points[i].x + points[j].x) * 0.5f, (points[i].y + points[j].y) * 0.5f);
            const float testRadiusSq = detail::DistanceSquared(testCenter, points[i]);
            if (testRadiusSq >= bestRadiusSq) {
                continue;
            }
            if (!detail::CircleEnclosesPoints(testCenter, testRadiusSq, points)) {
                continue;
            }
            bestCenter = testCenter;
            bestRadiusSq = testRadiusSq;
        }
    }

    for (size_t i = 0; i + 2 < points.size(); ++i) {
        for (size_t j = i + 1; j + 1 < points.size(); ++j) {
            for (size_t k = j + 1; k < points.size(); ++k) {
                Vec2 testCenter = {};
                float testRadiusSq = FLT_MAX;
                if (!detail::FindCircle(points[i], points[j], points[k], testCenter, testRadiusSq)) {
                    continue;
                }
                if (testRadiusSq >= bestRadiusSq) {
                    continue;
                }
                if (!detail::CircleEnclosesPoints(testCenter, testRadiusSq, points)) {
                    continue;
                }
                bestCenter = testCenter;
                bestRadiusSq = testRadiusSq;
            }
        }
    }

    if (bestRadiusSq == FLT_MAX) {
        bestCenter = points.front();
        bestRadiusSq = 0.0f;
        for (const auto& point : points) {
            bestRadiusSq = std::max(bestRadiusSq, detail::DistanceSquared(bestCenter, point));
        }
    }

    center = bestCenter;
    radius = std::sqrt(bestRadiusSq);
}

inline MecCircle GetMec(const std::vector<Vec2>& points) {
    MecCircle circle = {};
    const auto hull = MakeConvexHull(points);
    FindMinimalBoundingCircle(hull.empty() ? points : hull, circle.Center, circle.Radius);
    return circle;
}

} // namespace SDK::ConvexHull
