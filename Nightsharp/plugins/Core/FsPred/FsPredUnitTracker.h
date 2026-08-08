#pragma once

#include "../../../sdk/GameObjects/GameObjects.h"
#include "../../../sdk/GameObjects/ObjectManager.h"
#include "../../../sdk/Math/Prediction/Movement.h"
#include "../../../sdk/Utils/MathUtils.h"
#include "../../../core/CoreBuffs.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <deque>
#include <string>
#include <unordered_map>
#include <vector>

namespace Plugins::FsPred {

struct PathInfo {
    SDK::Vector2 Position{};
    SDK::Vector2 Direction{};
    float Time = 0.0f;
};

struct UnitTrackerInfo {
    std::uint32_t NetworkId = 0;
    int AaTick = 0;
    int NewPathTick = 0;
    int StopMoveTick = 0;
    int LastInvisibleTick = 0;
    int SpecialSpellFinishTick = 0;
    std::vector<PathInfo> PathBank;
    std::deque<PathInfo> RecentPathChanges;
    SDK::Vector3 LastPosition{};
    std::vector<SDK::Vector3> LastWaypoints{};
    bool WasMoving = false;
    bool WasWindingUp = false;
};

struct SpecialSpellDef {
    std::string Name;
    double Duration = 0.0;
};

class UnitTracker {
public:
    static void Initialize() {
        if (initialized_) return;
        initialized_ = true;

        specialSpells_ = {
            { "katarinar", 1.0 },
            { "drain", 1.0 },
            { "crowstorm", 1.0 },
            { "consume", 0.5 },
            { "absolutezero", 1.0 },
            { "staticfield", 0.5 },
            { "cassiopeiapetrifyinggaze", 0.5 },
            { "ezrealtrueshotbarrage", 1.0 },
            { "galioidolofdurand", 1.0 },
            { "luxmalicecannon", 1.0 },
            { "reapthewhirlwind", 1.0 },
            { "jinxw", 0.6 },
            { "jinxr", 0.6 },
            { "missfortunebullettime", 1.0 },
            { "shenstandunited", 1.0 },
            { "threshe", 0.4 },
            { "threshrpenta", 0.75 },
            { "threshq", 0.75 },
            { "infiniteduress", 1.0 },
            { "meditate", 1.0 },
            { "alzaharnethergrasp", 1.0 },
            { "lucianq", 0.5 },
            { "caitlynpiltoverpeacemaker", 0.5 },
            { "velkozr", 0.5 },
            { "jhinr", 2.0 }
        };

        const int tick = SDK::Variables::TickCount();
        trackerMap_.clear();
        for (const auto& hero : SDK::GameObjects::EnemyHeroes()) {
            if (!hero.IsValid()) continue;
            UnitTrackerInfo info{};
            info.NetworkId = hero.NetworkId();
            info.AaTick = tick - 10000;
            info.NewPathTick = tick;
            info.StopMoveTick = hero.IsMoving() ? (tick - 10000) : tick;
            info.LastInvisibleTick = hero.IsVisible() ? (tick - 10000) : tick;
            info.SpecialSpellFinishTick = tick - 10000;
            info.LastPosition = hero.Position();
            info.LastWaypoints = hero.CachedWaypoints();
            info.WasMoving = hero.IsMoving();
            trackerMap_[hero.NetworkId()] = info;
        }
    }

    static void Update() {
        if (!initialized_) Initialize();
        const int tick = SDK::Variables::TickCount();

        // GetPrediction can be called dozens of times per game frame. The tracker
        // state only needs to be refreshed once per tick — every additional call
        // in the same tick returns immediately.
        if (lastUpdateTick_ == tick) return;
        lastUpdateTick_ = tick;

        // Zero-allocation per-frame snapshot (SDK::GameObjects frame buffer).
        const auto& enemies = SDK::GameObjects::EnemyHeroesFrame();
        for (const auto& hero : enemies) {
            if (!hero.IsValid()) continue;
            const std::uint32_t netId = hero.NetworkId();
            auto& info = trackerMap_[netId];
            info.NetworkId = netId;

            const auto& currentWaypoints = hero.CachedWaypoints();
            const bool isMoving = hero.IsMoving();
            const SDK::Vector3 currentPos = hero.Position();

            // Track visibility
            if (!hero.IsVisible()) {
                info.LastInvisibleTick = tick;
            }

            // Check if path or movement changed
            bool pathChanged = false;
            if (currentWaypoints.size() != info.LastWaypoints.size()) {
                pathChanged = true;
            } else {
                for (std::size_t i = 0; i < currentWaypoints.size(); ++i) {
                    if (currentWaypoints[i].DistanceSquared(info.LastWaypoints[i]) > 1.0f) {
                        pathChanged = true;
                        break;
                    }
                }
            }

            if (pathChanged) {
                info.NewPathTick = tick;
                if (currentWaypoints.size() <= 1) {
                    info.StopMoveTick = tick;
                }
                if (!currentWaypoints.empty()) {
                    const SDK::Vector2 endPos = currentWaypoints.back().To2D();
                    const SDK::Vector2 toEnd = endPos - currentPos.To2D();
                    const SDK::Vector2 heading = toEnd.LengthSqr() > 1.0f
                        ? toEnd.Normalized()
                        : SDK::Vector2{};
                    const PathInfo entry{ endPos, heading, static_cast<float>(tick) };
                    info.PathBank.push_back(entry);
                    if (info.PathBank.size() > 3) {
                        info.PathBank.erase(info.PathBank.begin());
                    }
                    info.RecentPathChanges.push_back(entry);
                    while (!info.RecentPathChanges.empty() &&
                           static_cast<float>(tick) - info.RecentPathChanges.front().Time > 800.0f) {
                        info.RecentPathChanges.pop_front();
                    }
                    while (info.RecentPathChanges.size() > 6) {
                        info.RecentPathChanges.pop_front();
                    }
                }
            }

            // Real stop detection: unit was moving last tick and is not moving now.
            if (info.WasMoving && !isMoving) {
                info.StopMoveTick = tick;
            }

            if (hero.Spellbook().IsWindingUp()) {
                info.WasWindingUp = true;
            } else {
                info.WasWindingUp = false;
            }

            info.LastPosition = currentPos;
            info.LastWaypoints = currentWaypoints;
            info.WasMoving = isMoving;
        }
    }

    static bool SpamSamePlace(const SDK::AIBaseClient& unit) {
        if (!unit.IsValid()) return false;
        auto it = trackerMap_.find(unit.NetworkId());
        if (it == trackerMap_.end()) return false;

        const auto& tracker = it->second;
        if (tracker.PathBank.size() < 3) return false;

        const int tick = SDK::Variables::TickCount();
        const auto& p1 = tracker.PathBank[1];
        const auto& p2 = tracker.PathBank[2];

        if (p2.Time - p1.Time < 180.0f && static_cast<float>(tick) - p2.Time < 90.0f) {
            if (p1.Position.Distance(p2.Position) < 50.0f) {
                return true;
            }

            const SDK::Vector2 C = p1.Position;
            const SDK::Vector2 A = p2.Position;
            const SDK::Vector2 B = unit.Position().To2D();

            const float AB = (A.x - B.x) * (A.x - B.x) + (A.y - B.y) * (A.y - B.y);
            const float BC = (B.x - C.x) * (B.x - C.x) + (B.y - C.y) * (B.y - C.y);
            const float AC = (A.x - C.x) * (A.x - C.x) + (A.y - C.y) * (A.y - C.y);

            const float denom = 2.0f * std::sqrt(AB) * std::sqrt(BC);
            if (denom > 0.0001f) {
                const float cosVal = std::clamp((AB + BC - AC) / denom, -1.0f, 1.0f);
                const float angleDeg = std::acos(cosVal) * 180.0f / 3.14159265358979323846f;
                if (angleDeg < 31.0f) {
                    return true;
                }
            }
        }
        return false;
    }

    static bool IsReversing(const SDK::AIBaseClient& unit) {
        if (!unit.IsValid()) return false;
        auto it = trackerMap_.find(unit.NetworkId());
        if (it == trackerMap_.end()) return false;

        const auto& changes = it->second.RecentPathChanges;
        if (changes.size() < 2) return false;

        const int tick = SDK::Variables::TickCount();

        // Compare the intended heading captured at each re-path. One sharp reversal
        // is already enough to invalidate long linear extrapolation for a short time.
        for (std::size_t i = 1; i < changes.size(); ++i) {
            if (static_cast<float>(tick) - changes[i].Time > kReversalWindowMs) {
                continue;
            }

            const SDK::Vector2 dir1 = changes[i - 1].Direction;
            const SDK::Vector2 dir2 = changes[i].Direction;
            if (dir1.LengthSqr() < 0.25f || dir2.LengthSqr() < 0.25f) continue;

            if (dir1.AngleBetween(dir2) > kReversalAngleDeg) {
                return true;
            }
        }

        return false;
    }

    static double GetSpecialSpellEndTime(const SDK::AIBaseClient& unit) {
        if (!unit.IsValid()) return 0.0;
        auto it = trackerMap_.find(unit.NetworkId());
        if (it == trackerMap_.end()) return 0.0;
        return static_cast<double>(it->second.SpecialSpellFinishTick - SDK::Variables::TickCount());
    }

    static double GetLastAutoAttackTime(const SDK::AIBaseClient& unit) {
        if (!unit.IsValid()) return 0.0;
        auto it = trackerMap_.find(unit.NetworkId());
        if (it == trackerMap_.end()) return 0.0;
        return static_cast<double>(SDK::Variables::TickCount() - it->second.AaTick);
    }

    static double GetLastNewPathTime(const SDK::AIBaseClient& unit) {
        if (!unit.IsValid()) return 0.0;
        auto it = trackerMap_.find(unit.NetworkId());
        if (it == trackerMap_.end()) return 0.0;
        return static_cast<double>(SDK::Variables::TickCount() - it->second.NewPathTick);
    }

    static double GetLastVisibleTime(const SDK::AIBaseClient& unit) {
        if (!unit.IsValid()) return 0.0;
        auto it = trackerMap_.find(unit.NetworkId());
        if (it == trackerMap_.end()) return 0.0;
        return static_cast<double>(SDK::Variables::TickCount() - it->second.LastInvisibleTick);
    }

    static double GetLastStopMoveTime(const SDK::AIBaseClient& unit) {
        if (!unit.IsValid()) return 0.0;
        auto it = trackerMap_.find(unit.NetworkId());
        if (it == trackerMap_.end()) return 0.0;
        return static_cast<double>(SDK::Variables::TickCount() - it->second.StopMoveTick);
    }

private:
    static constexpr float kReversalWindowMs = 400.0f;
    static constexpr float kReversalAngleDeg = 100.0f;

    inline static bool initialized_ = false;
    inline static int lastUpdateTick_ = -1;
    inline static std::unordered_map<std::uint32_t, UnitTrackerInfo> trackerMap_;
    inline static std::vector<SpecialSpellDef> specialSpells_;
};

} // namespace Plugins::FsPred
