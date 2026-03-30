#pragma once

#include "../Math/Prediction/GamePath.h"
#include "../Math/Polygons/Polygon.h"

#include <algorithm>
#include <vector>

namespace SDK::Utils::MathUtils {

inline std::vector<Vector2> CutPath(const std::vector<Vector2>& path, float distance) {
    if (path.empty()) {
        return {};
    }

    std::vector<Vector2> result = {};
    for (size_t i = 0; i + 1 < path.size(); ++i) {
        const float dist = path[i].Distance(path[i + 1]);
        if (dist > distance) {
            result.push_back(path[i] + ((path[i + 1] - path[i]).Normalized() * distance));
            for (size_t j = i + 1; j < path.size(); ++j) {
                result.push_back(path[j]);
            }
            return result;
        }
        distance -= dist;
    }

    result.push_back(path.back());
    return result;
}

inline std::vector<Vector2> GetWaypoints(const AIBaseClient& unit) {
    if (!unit.IsValid()) {
        return {};
    }

    if (unit.IsVisible()) {
        std::vector<Vector2> out = {};
        for (const auto& point : unit.GetWaypoints()) {
            out.push_back(point.To2D());
        }
        return out;
    }

    const auto stored = GamePath::PathTracker::GetStoredPaths(unit, 10.0);
    if (!stored.empty()) {
        return stored.back().Path;
    }

    return {};
}

inline Vector2 PositionAfter(const std::vector<Vector2>& path, int time, int speed, int delay = 0) {
    if (path.empty()) {
        return {};
    }

    int distance = std::max(0, time - delay) * speed / 1000;
    for (size_t i = 0; i + 1 < path.size(); ++i) {
        const auto from = path[i];
        const auto to = path[i + 1];
        const int segment = static_cast<int>(from.Distance(to));
        if (segment > distance) {
            return from + ((to - from).Normalized() * static_cast<float>(distance));
        }
        distance -= segment;
    }

    return path.back();
}

inline std::vector<SDK::Geometry::Polygon> ToPolygons(const std::vector<SDK::Geometry::Polygon>& polygons) {
    return polygons;
}

} // namespace SDK::Utils::MathUtils
