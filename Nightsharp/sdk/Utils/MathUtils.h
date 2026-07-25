#pragma once

#include "../Core/Objects.h"
#include "../Events/Events.h"
#include "../Math/Polygons/Polygon.h"
#include "../Core/Variables.h"
#include "../../Third_Party/clipper/clipper.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <utility>
#include <vector>

namespace SDK::Core::Utils {

class MathUtils {
public:
    using IntPoint = SDK::Clipper::IntPoint;

    static std::vector<std::vector<IntPoint>> ClipPolygons(const std::vector<SDK::Polygon>& polygons) {
        ClipperLib::Paths subject;
        subject.reserve(polygons.size());

        for (const auto& polygon : polygons) {
            const auto sdkPath = polygon.ToClipperPath();
            if (sdkPath.size() < 3) {
                continue;
            }

            ClipperLib::Path path;
            path.reserve(sdkPath.size());
            for (const auto& point : sdkPath) {
                path.emplace_back(
                    static_cast<ClipperLib::cInt>(point.X),
                    static_cast<ClipperLib::cInt>(point.Y));
            }
            subject.push_back(std::move(path));
        }

        if (subject.empty()) {
            return {};
        }

        ClipperLib::Paths clip = subject;
        ClipperLib::Paths solution;
        ClipperLib::Clipper clipper;
        clipper.AddPaths(subject, ClipperLib::ptSubject, true);
        clipper.AddPaths(clip, ClipperLib::ptClip, true);
        clipper.Execute(
            ClipperLib::ctUnion,
            solution,
            ClipperLib::pftPositive,
            ClipperLib::pftEvenOdd);

        std::vector<std::vector<IntPoint>> result;
        result.reserve(solution.size());
        for (const auto& path : solution) {
            std::vector<IntPoint> sdkPath;
            sdkPath.reserve(path.size());
            for (const auto& point : path) {
                sdkPath.emplace_back(
                    static_cast<long long>(point.X),
                    static_cast<long long>(point.Y));
            }
            result.push_back(std::move(sdkPath));
        }
        return result;
    }

    static std::vector<Vec2> CutPath(const std::vector<Vec2>& path, float distance) {
        std::vector<Vec2> result;
        if (path.empty()) {
            return result;
        }

        for (std::size_t i = 0; i + 1 < path.size(); ++i) {
            const float dist = path[i].Distance(path[i + 1]);
            if (dist > distance) {
                result.push_back(path[i] + ((path[i + 1] - path[i]).Normalized() * distance));
                for (std::size_t j = i + 1; j < path.size(); ++j) {
                    result.push_back(path[j]);
                }
                break;
            }
            distance -= dist;
        }

        if (!result.empty()) {
            return result;
        }
        return { path.back() };
    }

    static std::vector<Vec2> GetWaypoints(const AIBaseClient& unit) {
        EnsureTracker();
        std::vector<Vec2> result;
        if (!unit.IsValid()) {
            return result;
        }

        if (unit.IsVisible()) {
            result.push_back(unit.Position().To2D());
            for (const auto& point : unit.Path()) {
                result.push_back(point.To2D());
            }
        } else {
            const auto pathIt = WaypointTracker::StoredPaths.find(static_cast<std::uint32_t>(unit.NetworkId()));
            const auto tickIt = WaypointTracker::StoredTick.find(static_cast<std::uint32_t>(unit.NetworkId()));
            if (pathIt != WaypointTracker::StoredPaths.end() && tickIt != WaypointTracker::StoredTick.end()) {
                const float timePassed = (SDK::Variables::TickCount() - tickIt->second) / 1000.0f;
                if (PathLength(pathIt->second) >= unit.MoveSpeed() * timePassed) {
                    result = CutPath(pathIt->second, unit.MoveSpeed() * timePassed);
                }
            }
        }
        return result;
    }

    static Vec2 PositionAfter(const std::vector<Vec2>& path, int time, int speed, int delay = 0) {
        if (path.empty()) {
            return {};
        }

        float distance = static_cast<float>(std::max(0, time - delay) * speed) / 1000.0f;
        for (std::size_t i = 0; i + 1 < path.size(); ++i) {
            const Vec2 from = path[i];
            const Vec2 to = path[i + 1];
            const float d = to.Distance(from);
            if (d > distance) {
                return from + ((to - from).Normalized() * distance);
            }
            distance -= d;
        }
        return path.back();
    }

    static SDK::Polygon ToPolygon(const std::vector<IntPoint>& list) {
        SDK::Polygon polygon;
        for (const auto& point : list) {
            polygon.Add(Vec2(static_cast<float>(point.X), static_cast<float>(point.Y)));
        }
        return polygon;
    }

    static std::vector<SDK::Polygon> ToPolygons(const std::vector<std::vector<IntPoint>>& paths) {
        std::vector<SDK::Polygon> polygons;
        polygons.reserve(paths.size());
        for (const auto& path : paths) {
            polygons.push_back(ToPolygon(path));
        }
        return polygons;
    }

    static float PathLength(const std::vector<Vec2>& path) {
        float result = 0.0f;
        for (std::size_t i = 0; i + 1 < path.size(); ++i) {
            result += path[i].Distance(path[i + 1]);
        }
        return result;
    }

    struct WaypointTracker {
        static inline std::unordered_map<std::uint32_t, std::vector<Vec2>> StoredPaths;
        static inline std::unordered_map<std::uint32_t, int> StoredTick;
        static inline bool Installed = false;
    };

    static void ResetWaypointTracker() {
        if (WaypointTracker::Installed) {
            SDK::Events::RemoveOnNewPath(&OnNewPath);
            WaypointTracker::Installed = false;
        }
        WaypointTracker::StoredPaths.clear();
        WaypointTracker::StoredTick.clear();
    }

private:
    static void EnsureTracker() {
        if (!WaypointTracker::Installed) {
            WaypointTracker::Installed = true;
            SDK::Events::AddOnNewPath(&OnNewPath);
            SDK::Events::AddOnDeleteObject(&OnObjectDelete);
        }
    }

    static void OnObjectDelete(const SDK::Events::ObjectEventArgs& args) {
        if (args.Sender.NetworkId) {
            WaypointTracker::StoredPaths.erase(args.Sender.NetworkId);
            WaypointTracker::StoredTick.erase(args.Sender.NetworkId);
        }
    }

    static void OnNewPath(const SDK::Events::NewPathEventArgs& args) {
        const std::uint32_t networkId = args.Sender.NetworkId;
        if (!networkId) {
            return;
        }

        const int now = SDK::Variables::TickCount();
        if (WaypointTracker::StoredTick.size() > 128) {
            for (auto it = WaypointTracker::StoredTick.begin(); it != WaypointTracker::StoredTick.end(); ) {
                if (now - it->second > 10000) {
                    WaypointTracker::StoredPaths.erase(it->first);
                    it = WaypointTracker::StoredTick.erase(it);
                } else {
                    ++it;
                }
            }
        }

        std::vector<Vec2> path;
        path.reserve(static_cast<std::size_t>(std::max(0, args.PathCount)));
        for (int i = 0; i < args.PathCount; ++i) {
            path.push_back(args.Path[i].To2D());
        }

        WaypointTracker::StoredPaths[networkId] = std::move(path);
        WaypointTracker::StoredTick[networkId] = now;
    }
};

} // namespace SDK::Core::Utils

namespace SDK::Utils {
    using MathUtils = ::SDK::Core::Utils::MathUtils;
} // namespace SDK::Utils
