#pragma once
// ============================================================================
// GamePath.h — Port of EnsoulSharp.SDK/Core/Math/Prediction/GamePath.cs
// ============================================================================
// Path Tracker: records hero movement history for better prediction.
//
// Usage:
//   SDK::GamePath::PathTracker::Init();   // Once (registers event)
//   SDK::GamePath::PathTracker::Update(); // Each frame
//
//   auto path = PathTracker::GetCurrentPath(hero);
//   double speed = PathTracker::GetMeanSpeed(hero, 1.5);
//   auto paths = PathTracker::GetStoredPaths(hero, 1.5);
// ============================================================================

#include "AiManager.h"
#include "GameObjects.h"
#include "Game.h"
#include "core/Vector.h"

#include <vector>
#include <unordered_map>
#include <algorithm>

namespace SDK {
namespace GamePath {

    // ========================================================================
    // StoredPath — Container for a recorded path
    // ========================================================================
    struct StoredPath {
        std::vector<Vec2> Path;
        float Tick = 0.0f;          // Game time (seconds) when path was recorded

        Vec2 StartPoint() const {
            return Path.empty() ? Vec2() : Path.front();
        }

        Vec2 EndPoint() const {
            return Path.empty() ? Vec2() : Path.back();
        }

        // Time in seconds since this path was recorded
        float Time(float now) const {
            return now - Tick;
        }

        int WaypointCount() const {
            return (int)Path.size();
        }

        // Total path length
        float PathLength() const {
            float len = 0.0f;
            for (size_t i = 1; i < Path.size(); i++) {
                len += Path[i].Distance(Path[i - 1]);
            }
            return len;
        }
    };

    // ========================================================================
    // PathTracker — Records hero path history
    // ========================================================================
    class PathTracker {
    public:
        // Max time window for path storage (seconds)
        static constexpr float MaxTime = 1.5f;

        // Max paths stored per hero before cleanup
        static constexpr int MaxStoredPaths = 50;
        static constexpr int CleanupThreshold = 40;

        // ---- Initialize ----
        static void Init() {
            if (s_initialized) return;
            s_initialized = true;
        }

        // ---- Update each frame — detect new paths from AiManager ----
        static void Update() {
            float now = Game::GetTime();

            for (auto& hero : GameObjects::AllHeroes) {
                if (!hero.IsValid()) continue;

                unsigned int netId = (unsigned int)hero.GetNetId();
                if (netId == 0) continue;

                AiManager ai(hero.address);
                if (!ai.IsValid()) continue;

                // Get current waypoints
                auto waypoints = ai.GetRemainingPath();
                if (waypoints.empty()) continue;

                // Convert to Vec2
                std::vector<Vec2> path2d;
                path2d.reserve(waypoints.size());
                for (auto& wp : waypoints) {
                    path2d.push_back(wp.To2D());
                }

                // Check if this is a new path (different from last recorded)
                auto& heroState = s_heroStates[netId];
                bool isNewPath = false;

                if (heroState.lastPath.empty()) {
                    isNewPath = true;
                } else if (path2d.size() != heroState.lastPath.size()) {
                    isNewPath = true;
                } else {
                    // Check if endpoint changed significantly
                    Vec2 lastEnd = heroState.lastPath.back();
                    Vec2 newEnd = path2d.back();
                    if (lastEnd.Distance(newEnd) > 50.0f) {
                        isNewPath = true;
                    }
                }

                if (isNewPath) {
                    heroState.lastPath = path2d;

                    StoredPath sp;
                    sp.Path = path2d;
                    sp.Tick = now;

                    auto& stored = s_storedPaths[netId];
                    stored.push_back(sp);

                    // Cleanup old entries
                    if ((int)stored.size() > MaxStoredPaths) {
                        stored.erase(stored.begin(),
                                     stored.begin() + CleanupThreshold);
                    }
                }
            }
        }

        // ---- Get current path of a hero ----
        static StoredPath GetCurrentPath(const GameObject& unit) {
            unsigned int netId = (unsigned int)unit.GetNetId();
            auto it = s_storedPaths.find(netId);
            if (it != s_storedPaths.end() && !it->second.empty()) {
                return it->second.back();
            }
            return StoredPath();
        }

        static StoredPath GetCurrentPath(unsigned int netId) {
            auto it = s_storedPaths.find(netId);
            if (it != s_storedPaths.end() && !it->second.empty()) {
                return it->second.back();
            }
            return StoredPath();
        }

        // ---- Get stored paths within a time window (seconds) ----
        static std::vector<StoredPath> GetStoredPaths(const GameObject& unit,
                                                       float maxT) {
            std::vector<StoredPath> result;
            unsigned int netId = (unsigned int)unit.GetNetId();

            auto it = s_storedPaths.find(netId);
            if (it == s_storedPaths.end()) return result;

            float now = Game::GetTime();
            for (auto& sp : it->second) {
                if (sp.Time(now) <= maxT) {
                    result.push_back(sp);
                }
            }
            return result;
        }

        // ---- Get Root-Mean-Squared speed of a hero ----
        // Estimates effective movement speed based on path changes over time
        static float GetMeanSpeed(const GameObject& unit, float maxT) {
            auto paths = GetStoredPaths(unit, maxT);
            float moveSpeed = unit.GetMoveSpeed();

            if (paths.empty()) return moveSpeed;

            float distance = 0.0f;
            float now = Game::GetTime();

            // First path: assume unit was moving before
            distance += (maxT - paths[0].Time(now)) * moveSpeed;

            for (size_t i = 0; i + 1 < paths.size(); i++) {
                auto& current = paths[i];
                auto& next = paths[i + 1];

                if (current.WaypointCount() > 0) {
                    float timeDiff = current.Time(now) - next.Time(now);
                    float pathLen = current.PathLength();
                    float moveDist = timeDiff * moveSpeed;
                    distance += (moveDist < pathLen) ? moveDist : pathLen;
                }
            }

            // Last path
            auto& lastPath = paths.back();
            if (lastPath.WaypointCount() > 0) {
                float timeDist = lastPath.Time(now) * moveSpeed;
                float pathLen = lastPath.PathLength();
                distance += (timeDist < pathLen) ? timeDist : pathLen;
            }

            return (maxT > 0.0f) ? (distance / maxT) : moveSpeed;
        }

        // ---- Get average path angle change (radians) ----
        // Higher value = more juking, lower prediction confidence
        static float GetPathAngleVariance(const GameObject& unit, float maxT = 1.5f) {
            auto paths = GetStoredPaths(unit, maxT);
            if (paths.size() < 2) return 0.0f;

            float totalAngle = 0.0f;
            int count = 0;

            for (size_t i = 0; i + 1 < paths.size(); i++) {
                Vec2 end1 = paths[i].EndPoint();
                Vec2 end2 = paths[i + 1].EndPoint();
                Vec2 start = paths[i].StartPoint();

                if (start.IsZero() || end1.IsZero() || end2.IsZero()) continue;

                Vec2 dir1 = (end1 - start).Normalized();
                Vec2 dir2 = (end2 - start).Normalized();
                float angle = fabsf(dir1.AngleBetween(dir2));
                totalAngle += angle;
                count++;
            }

            return (count > 0) ? (totalAngle / (float)count) : 0.0f;
        }

        // ---- Is the hero likely juking (changing direction frequently)? ----
        static bool IsJuking(const GameObject& unit, float maxT = 1.0f,
                              float angleThreshold = 0.5f) {
            return GetPathAngleVariance(unit, maxT) > angleThreshold;
        }

        // ---- Get the number of path changes in the last N seconds ----
        static int GetPathChangeCount(const GameObject& unit, float maxT = 1.0f) {
            return (int)GetStoredPaths(unit, maxT).size();
        }

        // ---- Clear all stored paths ----
        static void Clear() {
            s_storedPaths.clear();
            s_heroStates.clear();
        }

    private:
        struct HeroState {
            std::vector<Vec2> lastPath;
        };

        static inline std::unordered_map<unsigned int, std::vector<StoredPath>> s_storedPaths;
        static inline std::unordered_map<unsigned int, HeroState> s_heroStates;
        static inline bool s_initialized = false;
    };

} // namespace GamePath
} // namespace SDK
