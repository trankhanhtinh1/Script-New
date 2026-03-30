#pragma once

// ============================================================================
// PathTracker — Poll-based OnNewPath event
//
// EnsoulSharp equivalent: AIBaseClient.OnNewPath
//
// How it works (manual-map safe, no hooks):
//   Each tick we compare each hero's PathEnd / WaypointCount / IsDashing
//   against the previous tick. If any changed → fire OnNewPath.
//
// DEPENDENCY: CoreAi (core/CoreAi.h)
//   Provides: GetPathEnd, GetPathStart, GetWaypointCount, CopyWaypoints,
//             IsMoving, IsDashing, GetVelocity
// DEPENDENCY: SDK ObjectManager::Heroes() (sdk/Core/Objects.h)
// ============================================================================

#include "../../menu/MenuUI.h"
#include "../Core/Game.h"
#include "../Core/Objects.h"

#include <new>
#include <unordered_map>
#include <vector>

namespace SDK::Events::Path {

// ---------------------------------------------------------------------------
// Event Args — matches EnsoulSharp: AIBaseClientNewPathEventArgs
// ---------------------------------------------------------------------------

struct NewPathEventArgs {
    std::vector<Vector3> Path = {};
    bool IsDash = false;
    float Speed = 0.0f;

    bool IsValid() const { return !Path.empty(); }
};

using NewPathHandler = void(*)(const AIBaseClient& sender, const NewPathEventArgs& args);

// ---------------------------------------------------------------------------
// Internal
// ---------------------------------------------------------------------------
namespace detail {

    struct PathState {
        Vector3 LastPathEnd = {};
        int LastWaypointCount = 0;
        bool WasDashing = false;
        bool Initialized = false;
    };

    inline std::unordered_map<int, PathState>* g_states = nullptr;
    inline MenuUI::FixedList<NewPathHandler, 64> g_handlers = {};

    inline bool EnsureStorage() {
        if (!g_states) {
            g_states = new(std::nothrow) std::unordered_map<int, PathState>();
        }
        return g_states != nullptr;
    }

    inline bool HasChanged(const PathState& s, const Vector3& end, int wpCount, bool dashing) {
        if (!s.Initialized) return false;
        if (s.LastPathEnd.Distance2D(end) > 10.0f) return true;
        if (s.LastWaypointCount != wpCount) return true;
        if (s.WasDashing != dashing) return true;
        return false;
    }

    inline NewPathEventArgs BuildArgs(const AIBaseClient& hero, bool isDash) {
        NewPathEventArgs args = {};

        // Copy waypoints from core (Vec3 == Vector3)
        Vector3 wpBuf[32] = {};
        const int count = hero.Ref().CopyWaypoints(
            reinterpret_cast<Vec3*>(wpBuf), 32);

        args.Path.reserve(count > 0 ? count : 2);
        if (count > 0) {
            for (int i = 0; i < count; ++i) {
                args.Path.push_back(wpBuf[i]);
            }
        } else {
            args.Path.push_back(hero.Position());
            const auto end = hero.PathEnd();
            if (!end.IsZero() && end.Distance2D(hero.Position()) > 1.0f) {
                args.Path.push_back(end);
            }
        }

        args.IsDash = isDash;
        args.Speed  = isDash
            ? std::max(hero.Velocity().Length2D(), hero.MoveSpeed())
            : hero.MoveSpeed();
        if (args.Speed <= 0.0f) args.Speed = hero.MoveSpeed();

        return args;
    }

} // namespace detail

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

inline void Initialize() { detail::EnsureStorage(); }

inline bool AddOnNewPath(NewPathHandler h) { return h && detail::g_handlers.push_back(h); }
inline bool OnNewPath(NewPathHandler h)    { return AddOnNewPath(h); }

/// Call once per tick from Events::Update()
inline void Update() {
    if (!detail::EnsureStorage()) return;

    for (const auto& hero : ObjectManager::Heroes()) {
        const int netId = hero.NetworkId();
        if (!hero.IsValid() || netId == 0 || hero.IsDead()) {
            if (netId != 0) detail::g_states->erase(netId);
            continue;
        }

        const Vector3 pathEnd  = hero.PathEnd();
        const int wpCount      = hero.Ref().GetWaypointCount();
        const bool isDashing   = hero.IsDashing();

        auto& state = (*detail::g_states)[netId];

        if (detail::HasChanged(state, pathEnd, wpCount, isDashing)) {
            auto args = detail::BuildArgs(hero, isDashing);
            for (const auto& h : detail::g_handlers) {
                if (h) h(hero, args);
            }
        }

        state.LastPathEnd      = pathEnd;
        state.LastWaypointCount = wpCount;
        state.WasDashing       = isDashing;
        state.Initialized      = true;
    }
}

inline void Reset() {
    if (detail::g_states) detail::g_states->clear();
    detail::g_handlers.clear();
}

} // namespace SDK::Events::Path
