#pragma once

#include "../../Core/Game.h"
#include "../../Events/Events.h"
#include "../../GameObjects/GameObjects.h"

#include <algorithm>
#include <cfloat>
#include <limits>
#include <new>
#include <unordered_map>
#include <vector>

namespace SDK::Prediction::GamePath {

struct StoredPath {
    int Tick = 0;
    std::vector<Vec2> Path = {};

    Vec2 StartPoint() const {
        return Path.empty() ? Vec2() : Path.front();
    }

    Vec2 EndPoint() const {
        return Path.empty() ? Vec2() : Path.back();
    }

    double Time() const {
        return static_cast<double>(Game::TickCount() - Tick) / 1000.0;
    }

    int WaypointCount() const {
        return static_cast<int>(Path.size());
    }
};

namespace detail {
    inline std::unordered_map<int, std::vector<StoredPath>>*& PathStore() {
        static auto* storage = new(std::nothrow) std::unordered_map<int, std::vector<StoredPath>>();
        return storage;
    }

    inline bool g_hooksRegistered = false;

    inline std::vector<Vec2> Build2DPath(const AIBaseClient& unit) {
        std::vector<Vec2> out = {};
        if (!unit.IsValid()) {
            return out;
        }

        const auto fullPath = unit.GetWaypoints();
        if (!fullPath.empty()) {
            out.reserve(fullPath.size());
            for (const auto& point : fullPath) {
                out.push_back(point.To2D());
            }
            return out;
        }

        return out;
    }

    inline std::vector<Vec2> To2DPath(const std::vector<Vec3>& path) {
        std::vector<Vec2> out = {};
        out.reserve(path.size());
        for (const auto& point : path) {
            out.push_back(point.To2D());
        }
        return out;
    }

    inline void PushStoredPath(const AIBaseClient& unit, const std::vector<Vec2>& path) {
        if (!unit.IsValid() || path.empty()) {
            return;
        }

        auto* paths = PathStore();
        if (!paths) {
            return;
        }

        auto& history = (*paths)[unit.NetworkId()];
        history.push_back(StoredPath{ Game::TickCount(), path });
        if (history.size() > 50) {
            history.erase(history.begin(), history.begin() + static_cast<long>(history.size() - 50));
        }
    }

    inline void OnNewPath(const ::SDK::Events::NewPathEventArgs& args) {
        if (!args.Sender.IsValid() || args.PathCount <= 0) {
            return;
        }
        std::vector<Vec3> pathVec(args.Path, args.Path + args.PathCount);
        const AIBaseClient sender(args.Sender.Ptr);
        PushStoredPath(sender, To2DPath(pathVec));
    }
}

inline void Initialize() {
    (void)detail::PathStore();
    if (!detail::g_hooksRegistered) {
        ::SDK::Events::hook.OnNewPath += detail::OnNewPath;
        detail::g_hooksRegistered = true;
    }
}

inline void Reset() {
    if (detail::g_hooksRegistered) {
        ::SDK::Events::hook.OnNewPath -= detail::OnNewPath;
        detail::g_hooksRegistered = false;
    }
    if (detail::PathStore()) {
        detail::PathStore()->clear();
    }
}

inline void Update() {
    Initialize();
    auto* paths = detail::PathStore();
    if (!paths) {
        return;
    }

    for (const auto& hero : GameObjects::AllGameObjects()) {
        if (!hero.IsValid() || hero.IsDead() || !hero.IsHero()) {
            continue;
        }

        auto& history = (*paths)[hero.NetworkId()];
        if (history.empty()) {
            detail::PushStoredPath(AIBaseClient(hero.Address()), detail::Build2DPath(AIBaseClient(hero.Address())));
            continue;
        }

        while (!history.empty() && history.front().Time() > 10.0) {
            history.erase(history.begin());
        }
    }
}

inline std::vector<Vec3> GetCurrentPath(const AIBaseClient& unit) {
    std::vector<Vec3> path = {};
    if (!unit.IsValid()) {
        return path;
    }

    path = unit.GetWaypoints();
    if (!path.empty()) {
        return path;
    }

    return path;
}

inline float PathLength(const std::vector<Vec3>& path) {
    float total = 0.0f;
    for (size_t i = 1; i < path.size(); ++i) {
        total += path[i - 1].Distance2D(path[i]);
    }
    return total;
}

inline float PathLength(const AIBaseClient& unit) {
    return PathLength(GetCurrentPath(unit));
}

inline Vec3 PositionAfter(const AIBaseClient& unit, float timeSeconds, float moveSpeedOverride = -1.0f) {
    if (!unit.IsValid()) {
        return {};
    }

    const Vec3 start = unit.ServerPosition().IsZero() ? unit.Position() : unit.ServerPosition();
    const Vec3 end = unit.PathEnd();
    const float moveSpeed = moveSpeedOverride > 0.0f ? moveSpeedOverride : std::max(unit.MoveSpeed(), 0.0f);

    if (!end.IsZero() && end.Distance2D(start) > 1.0f) {
        const float distance = std::min(start.Distance2D(end), moveSpeed * std::max(timeSeconds, 0.0f));
        return start.Extend(end, distance);
    }

    const Vec3 velocity = unit.Velocity();
    if (!velocity.IsZero() && velocity.Length2D() > 1.0f) {
        return start + (velocity * std::max(timeSeconds, 0.0f));
    }

    return start;
}

inline float TimeToReach(const AIBaseClient& unit, const Vec3& point, float moveSpeedOverride = -1.0f) {
    if (!unit.IsValid()) {
        return FLT_MAX;
    }

    const Vec3 start = unit.ServerPosition().IsZero() ? unit.Position() : unit.ServerPosition();
    const float speed = moveSpeedOverride > 0.0f ? moveSpeedOverride : std::max(unit.MoveSpeed(), 0.0f);
    if (speed <= 0.0f) {
        return FLT_MAX;
    }

    return start.Distance2D(point) / speed;
}

inline std::vector<StoredPath> GetStoredPaths(const AIBaseClient& unit, double maxT) {
    Initialize();
    const auto* paths = detail::PathStore();
    if (!paths || !unit.IsValid()) {
        return {};
    }

    const auto it = paths->find(unit.NetworkId());
    if (it == paths->end()) {
        return {};
    }

    std::vector<StoredPath> out = {};
    out.reserve(it->second.size());
    for (const auto& entry : it->second) {
        if (entry.Time() < maxT) {
            out.push_back(entry);
        }
    }
    return out;
}

inline float GetMeanSpeed(const AIBaseClient& unit, double maxT) {
    const auto paths = GetStoredPaths(unit, maxT);
    if (paths.empty()) {
        return unit.MoveSpeed();
    }

    double distance = 0.0;

    distance += (maxT - paths.front().Time()) * unit.MoveSpeed();

    for (size_t i = 0; i + 1 < paths.size(); ++i) {
        const auto& currentPath = paths[i];
        const auto& nextPath = paths[i + 1];
        if (currentPath.WaypointCount() <= 0) {
            continue;
        }

        double currentLength = 0.0;
        for (size_t j = 1; j < currentPath.Path.size(); ++j) {
            currentLength += currentPath.Path[j - 1].Distance(currentPath.Path[j]);
        }

        distance += std::min((currentPath.Time() - nextPath.Time()) * unit.MoveSpeed(), currentLength);
    }

    const auto& lastPath = paths.back();
    if (lastPath.WaypointCount() > 0) {
        double lastLength = 0.0;
        for (size_t i = 1; i < lastPath.Path.size(); ++i) {
            lastLength += lastPath.Path[i - 1].Distance(lastPath.Path[i]);
        }
        distance += std::min(lastPath.Time() * unit.MoveSpeed(), lastLength);
    }

    return maxT > 0.0 ? static_cast<float>(distance / maxT) : unit.MoveSpeed();
}

} // namespace SDK::Prediction::GamePath

namespace SDK::GamePath {

using StoredPath = Prediction::GamePath::StoredPath;

namespace detail {
    inline StoredPath BuildCurrentStoredPath(const AIBaseClient& unit) {
        StoredPath stored = {};
        stored.Tick = Game::TickCount();
        for (const auto& point : Prediction::GamePath::GetCurrentPath(unit)) {
            stored.Path.push_back(point.To2D());
        }
        return stored;
    }
}

namespace PathTracker {

inline void Initialize() {
    Prediction::GamePath::Initialize();
}

inline void Update() {
    Prediction::GamePath::Update();
}

inline void Reset() {
    Prediction::GamePath::Reset();
}

inline StoredPath GetCurrentPath(const AIBaseClient& unit) {
    const auto paths = Prediction::GamePath::GetStoredPaths(unit, std::numeric_limits<double>::max());
    if (!paths.empty()) {
        return paths.back();
    }
    return detail::BuildCurrentStoredPath(unit);
}

inline std::vector<StoredPath> GetStoredPaths(const AIBaseClient& unit, double maxT) {
    return Prediction::GamePath::GetStoredPaths(unit, maxT);
}

inline double GetMeanSpeed(const AIBaseClient& unit, double maxT) {
    return static_cast<double>(Prediction::GamePath::GetMeanSpeed(unit, maxT));
}

} // namespace PathTracker

} // namespace SDK::GamePath
