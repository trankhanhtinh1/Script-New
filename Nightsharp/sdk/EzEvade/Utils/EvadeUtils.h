#pragma once
#include "sdk/SDK.h"
#include <chrono>
#include <random>
#include <vector>

namespace EzEvade {
namespace EvadeUtils {

inline std::mt19937& Rng() {
    static std::mt19937 gen((unsigned int)std::chrono::high_resolution_clock::now().time_since_epoch().count());
    return gen;
}

inline int RandomInt(int minVal, int maxVal) {
    if (maxVal < minVal) std::swap(minVal, maxVal);
    std::uniform_int_distribution<int> dist(minVal, maxVal);
    return dist(Rng());
}

inline float RandomFloat(float minVal, float maxVal) {
    if (maxVal < minVal) std::swap(minVal, maxVal);
    std::uniform_real_distribution<float> dist(minVal, maxVal);
    return dist(Rng());
}

inline float TickCount() {
    return (float)SDK::Game::GetTickCount();
}

inline std::vector<Vec2> PathToVector2(const std::vector<Vec3>& path) {
    std::vector<Vec2> out;
    out.reserve(path.size());
    for (const auto& p : path) {
        out.push_back(p.To2D());
    }
    return out;
}

inline std::vector<Vec2> CutPath(const std::vector<Vec2>& path, float distance) {
    std::vector<Vec2> result;
    if (path.empty()) return result;

    result.push_back(path.front());
    float remaining = distance;

    for (size_t i = 0; i + 1 < path.size(); ++i) {
        const float dist = path[i].Distance(path[i + 1]);
        if (dist > remaining) {
            result.push_back(path[i] + (path[i + 1] - path[i]).Normalized() * remaining);
            break;
        }

        result.push_back(path[i + 1]);
        remaining -= dist;
    }

    if (result.empty()) {
        result.push_back(path.back());
    }
    return result;
}

inline std::vector<Vec2> CutPathPrev(const std::vector<Vec2>& path, float distance) {
    std::vector<Vec2> result;
    if (path.empty()) return result;

    float remaining = distance;
    for (size_t i = 0; i + 1 < path.size(); ++i) {
        const float dist = path[i].Distance(path[i + 1]);
        if (dist > remaining) {
            result.push_back(path[i] + (path[i + 1] - path[i]).Normalized() * remaining);
            for (size_t j = i + 1; j < path.size(); ++j) {
                result.push_back(path[j]);
            }
            break;
        }
        remaining -= dist;
    }

    if (result.empty()) {
        result.push_back(path.back());
    }
    return result;
}

inline std::vector<Vec2> CutPath(const std::vector<Vec2>& path, const SDK::GameObject& unit, float delay, float speed = 0.0f) {
    float moveSpeed = (speed > 0.0f) ? speed : unit.GetMoveSpeed();
    const float dist = moveSpeed * delay / 1000.0f;
    return CutPath(path, dist);
}

inline std::vector<Vec2> CutPathPrev(const std::vector<Vec2>& path, const SDK::GameObject& unit, float delay) {
    const float dist = unit.GetMoveSpeed() * delay / 1000.0f;
    return CutPathPrev(path, dist);
}

inline Vec2 GetGamePosition(const SDK::GameObject& hero, float delay = 0.0f) {
    if (!hero.IsValid()) return Vec2();

    if (hero.IsMoving()) {
        std::vector<Vec2> path;
        path.push_back(hero.GetServerPosition().To2D());
        auto wps = hero.GetWaypoints();
        for (const auto& p : wps) {
            path.push_back(p.To2D());
        }

        auto finalPath = CutPath(path, hero, delay);
        if (!finalPath.empty()) {
            return finalPath.back();
        }
    }

    return hero.GetServerPosition().To2D();
}

inline Vec2 ExtendDir(const Vec2& pos, const Vec2& dir, float distance) {
    return pos + dir * distance;
}

inline Vec3 ExtendDir(const Vec3& pos, const Vec3& dir, float distance) {
    return pos + dir * distance;
}

} // namespace EvadeUtils
} // namespace EzEvade

