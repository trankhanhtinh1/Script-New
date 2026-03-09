#pragma once
// ============================================================================
// MathUtils.h — Path utilities, waypoint tracking, position after time
// Ported from EnsoulSharp.SDK/Core/Utils/MathUtils.cs
// ============================================================================

#include <vector>
#include <unordered_map>
#include <algorithm>
#include <cmath>
#include "core/Vector.h"

namespace SDK {

    // ========================================================================
    // WaypointTracker — stores last-known paths for units entering fog of war
    // ========================================================================
    class WaypointTracker {
    public:
        struct Entry {
            std::vector<Vec2> path;
            DWORD             tick = 0;     // GetTickCount64() when stored
        };

        // Store path for a given networkId
        static void StorePath(uint32_t networkId, const std::vector<Vec2>& path) {
            auto& e = s_data[networkId];
            e.path = path;
            e.tick = static_cast<DWORD>(GetTickCount64());
        }

        // Try to get a stored path
        static bool TryGetPath(uint32_t networkId, Entry& out) {
            auto it = s_data.find(networkId);
            if (it == s_data.end()) return false;
            out = it->second;
            return true;
        }

        // Clear all stored paths (call on game end / reset)
        static void Clear() { s_data.clear(); }

    private:
        static inline std::unordered_map<uint32_t, Entry> s_data;
    };

    // ========================================================================
    // MathUtils — geometry / path helper functions
    // ========================================================================
    namespace MathUtils {

        // --------------------------------------------------------------------
        // PathLength — total length of a polyline
        // --------------------------------------------------------------------
        inline float PathLength(const std::vector<Vec2>& path) {
            float length = 0.f;
            for (size_t i = 0; i + 1 < path.size(); ++i) {
                float dx = path[i + 1].x - path[i].x;
                float dy = path[i + 1].y - path[i].y;
                length += std::sqrtf(dx * dx + dy * dy);
            }
            return length;
        }

        inline float PathLength(const std::vector<Vec3>& path) {
            float length = 0.f;
            for (size_t i = 0; i + 1 < path.size(); ++i) {
                float dx = path[i + 1].x - path[i].x;
                float dz = path[i + 1].z - path[i].z;
                length += std::sqrtf(dx * dx + dz * dz);
            }
            return length;
        }

        // --------------------------------------------------------------------
        // CutPath — removes waypoints before 'distance' along the path
        //   Returns the remaining path starting from the point at 'distance'
        // --------------------------------------------------------------------
        inline std::vector<Vec2> CutPath(const std::vector<Vec2>& path, float distance) {
            std::vector<Vec2> result;
            if (path.empty()) return result;

            for (size_t i = 0; i + 1 < path.size(); ++i) {
                float dx = path[i + 1].x - path[i].x;
                float dy = path[i + 1].y - path[i].y;
                float dist = std::sqrtf(dx * dx + dy * dy);

                if (dist > distance) {
                    // Interpolate the cutting point
                    float t = distance / dist;
                    result.push_back({
                        path[i].x + t * dx,
                        path[i].y + t * dy
                    });
                    // Add remaining waypoints
                    for (size_t j = i + 1; j < path.size(); ++j) {
                        result.push_back(path[j]);
                    }
                    return result;
                }
                distance -= dist;
            }

            // Distance exceeds path length — return last point
            result.push_back(path.back());
            return result;
        }

        // --------------------------------------------------------------------
        // PositionAfter — position along path after 'timeMs' at 'speed'
        //   speed is in units/sec, timeMs and delayMs in milliseconds
        // --------------------------------------------------------------------
        inline Vec2 PositionAfter(const std::vector<Vec2>& path, float timeMs, float speed, float delayMs = 0.f) {
            if (path.empty()) return { 0, 0 };
            if (path.size() == 1) return path[0];

            float distance = (std::max)(0.f, timeMs - delayMs) * speed / 1000.f;

            for (size_t i = 0; i + 1 < path.size(); ++i) {
                float dx = path[i + 1].x - path[i].x;
                float dy = path[i + 1].y - path[i].y;
                float d = std::sqrtf(dx * dx + dy * dy);

                if (d > distance && d > 0.0001f) {
                    float t = distance / d;
                    return {
                        path[i].x + t * dx,
                        path[i].y + t * dy
                    };
                }
                distance -= d;
            }
            return path.back();
        }

        // Vec3 version (uses x,z plane)
        inline Vec3 PositionAfter(const std::vector<Vec3>& path, float timeMs, float speed, float delayMs = 0.f) {
            if (path.empty()) return { 0, 0, 0 };
            if (path.size() == 1) return path[0];

            float distance = (std::max)(0.f, timeMs - delayMs) * speed / 1000.f;

            for (size_t i = 0; i + 1 < path.size(); ++i) {
                float dx = path[i + 1].x - path[i].x;
                float dz = path[i + 1].z - path[i].z;
                float d = std::sqrtf(dx * dx + dz * dz);

                if (d > distance && d > 0.0001f) {
                    float t = distance / d;
                    return {
                        path[i].x + t * dx,
                        path[i].y,
                        path[i].z + t * dz
                    };
                }
                distance -= d;
            }
            return path.back();
        }

        // --------------------------------------------------------------------
        // GetWaypoints — returns current waypoints for a GameObject
        //   If visible: position + path
        //   If in fog: use stored path and cut based on elapsed time + speed
        // --------------------------------------------------------------------
        inline std::vector<Vec2> GetWaypoints(const class GameObject& unit);
        // Implementation in MathUtils_impl section below after GameObject is available
        // For now, provide a version that takes explicit parameters:

        inline std::vector<Vec2> GetWaypointsFromStored(
            uint32_t networkId,
            const Vec3& position,
            bool isVisible,
            float moveSpeed,
            const std::vector<Vec3>& currentPath)
        {
            std::vector<Vec2> result;

            if (isVisible) {
                result.push_back({ position.x, position.z });
                for (auto& p : currentPath) {
                    result.push_back({ p.x, p.z });
                }
            }
            else {
                WaypointTracker::Entry entry;
                if (WaypointTracker::TryGetPath(networkId, entry)) {
                    auto& storedPath = entry.path;
                    float timePassed = (GetTickCount64() - entry.tick) / 1000.f;
                    float pathLen = PathLength(storedPath);

                    if (pathLen >= moveSpeed * timePassed) {
                        result = CutPath(storedPath, moveSpeed * timePassed);
                    }
                }
            }

            return result;
        }

        // --------------------------------------------------------------------
        // DistanceToPath — shortest distance from a point to a polyline path
        // --------------------------------------------------------------------
        inline float DistanceToPath(const Vec2& point, const std::vector<Vec2>& path) {
            float minDist = FLT_MAX;
            for (size_t i = 0; i + 1 < path.size(); ++i) {
                // Project point onto segment path[i]..path[i+1]
                float dx = path[i + 1].x - path[i].x;
                float dy = path[i + 1].y - path[i].y;
                float len2 = dx * dx + dy * dy;
                if (len2 < 0.0001f) continue;

                float t = ((point.x - path[i].x) * dx + (point.y - path[i].y) * dy) / len2;
                t = (std::max)(0.f, (std::min)(1.f, t));

                Vec2 proj = {
                    path[i].x + t * dx,
                    path[i].y + t * dy
                };

                float dpx = point.x - proj.x;
                float dpy = point.y - proj.y;
                float dist = std::sqrtf(dpx * dpx + dpy * dpy);
                if (dist < minDist) minDist = dist;
            }
            return minDist;
        }

        // --------------------------------------------------------------------
        // IsPathIntersectingCircle — checks if any segment of path intersects circle
        // --------------------------------------------------------------------
        inline bool IsPathIntersectingCircle(const std::vector<Vec2>& path, const Vec2& center, float radius) {
            return DistanceToPath(center, path) <= radius;
        }

        // --------------------------------------------------------------------
        // Vec3 → Vec2 conversion helpers
        // --------------------------------------------------------------------
        inline Vec2 ToVec2(const Vec3& v) { return { v.x, v.z }; }
        inline Vec3 ToVec3(const Vec2& v, float y = 0.f) { return { v.x, y, v.y }; }

        // --------------------------------------------------------------------
        // Normalize2D — normalize a Vec2
        // --------------------------------------------------------------------
        inline Vec2 Normalize2D(const Vec2& v) {
            float len = std::sqrtf(v.x * v.x + v.y * v.y);
            if (len < 0.0001f) return { 0, 0 };
            return { v.x / len, v.y / len };
        }

        // --------------------------------------------------------------------
        // Distance2D — distance between two Vec2
        // --------------------------------------------------------------------
        inline float Distance2D(const Vec2& a, const Vec2& b) {
            float dx = a.x - b.x;
            float dy = a.y - b.y;
            return std::sqrtf(dx * dx + dy * dy);
        }

        // --------------------------------------------------------------------
        // Extend2D — extend from 'from' towards 'to' by 'distance'
        // --------------------------------------------------------------------
        inline Vec2 Extend2D(const Vec2& from, const Vec2& to, float distance) {
            float dx = to.x - from.x;
            float dy = to.y - from.y;
            float len = std::sqrtf(dx * dx + dy * dy);
            if (len < 0.0001f) return from;
            float t = distance / len;
            return { from.x + t * dx, from.y + t * dy };
        }

    } // namespace MathUtils

} // namespace SDK
