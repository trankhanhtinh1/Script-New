#pragma once

#include "../../core/Vector.h"

#include <cfloat>
#include <utility>
#include <vector>

namespace SDK::Geometry {

struct MovementCollisionResult {
    float Time = FLT_MAX;
    Vec2 Position = {};

    bool IsValid() const {
        return Time < FLT_MAX && Position.IsValid();
    }
};

inline float PathLength(const std::vector<Vec2>& path) {
    float total = 0.0f;
    for (size_t i = 1; i < path.size(); ++i) {
        total += path[i - 1].Distance(path[i]);
    }
    return total;
}

inline Vec3 PositionAlongPath(const std::vector<Vec3>& path, float distance) {
    return ::Geometry::PositionOnPath(path, distance);
}

inline Vec2 PositionAlongPath(const std::vector<Vec2>& path, float distance) {
    if (path.empty()) {
        return {};
    }
    if (path.size() == 1) {
        return path.front();
    }

    float remaining = distance;
    for (size_t i = 1; i < path.size(); ++i) {
        const float segLen = path[i - 1].Distance(path[i]);
        if (remaining <= segLen) {
            return path[i - 1].Extend(path[i], remaining);
        }
        remaining -= segLen;
    }

    return path.back();
}

inline std::vector<Vec2> CutPath(const std::vector<Vec2>& path, float distance) {
    if (path.size() <= 1 || distance <= 0.0f) {
        return path;
    }

    std::vector<Vec2> result = {};
    float remaining = distance;

    for (size_t i = 0; i + 1 < path.size(); ++i) {
        const float segLen = path[i].Distance(path[i + 1]);
        if (remaining > segLen) {
            remaining -= segLen;
            continue;
        }

        result.push_back(path[i].Extend(path[i + 1], remaining));
        for (size_t j = i + 1; j < path.size(); ++j) {
            result.push_back(path[j]);
        }
        break;
    }

    if (result.empty()) {
        result.push_back(path.back());
    }
    return result;
}

inline MovementCollisionResult VectorMovementCollision(const Vec2& startPos1,
                                                       const Vec2& endPos1,
                                                       float speed1,
                                                       const Vec2& startPos2,
                                                       float speed2,
                                                       float delay = 0.0f) {
    MovementCollisionResult result = {};
    if (speed1 <= 0.0f || speed2 <= 0.0f) {
        return result;
    }

    const Vec2 delta = endPos1 - startPos1;
    const float dist = delta.Length();
    if (dist < 0.0001f) {
        return result;
    }

    const float maxTime = dist / speed1;
    const Vec2 direction = delta / dist;
    const Vec2 origin = startPos1 - (direction * speed1 * delay);
    const Vec2 relative = origin - startPos2;
    const Vec2 velocity = direction * speed1;

    const float a = velocity.Dot(velocity) - (speed2 * speed2);
    const float b = 2.0f * relative.Dot(velocity);
    const float c = relative.Dot(relative);

    auto acceptSolution = [&](float t) {
        if (t < delay || t > (delay + maxTime)) {
            return false;
        }
        result.Time = t;
        result.Position = origin + (velocity * t);
        return true;
    };

    if (std::fabs(a) < 0.0001f) {
        if (std::fabs(b) < 0.0001f) {
            return result;
        }
        acceptSolution(-c / b);
        return result;
    }

    const float disc = (b * b) - (4.0f * a * c);
    if (disc < 0.0f) {
        return result;
    }

    const float sqrtDisc = std::sqrt(disc);
    const float t1 = (-b - sqrtDisc) / (2.0f * a);
    const float t2 = (-b + sqrtDisc) / (2.0f * a);
    if (acceptSolution(t1)) {
        return result;
    }
    acceptSolution(t2);
    return result;
}

} // namespace SDK::Geometry
