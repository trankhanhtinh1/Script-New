#pragma once

// ============================================================================
// PathTracker — push-driven OnNewPath event surface (cache preserved)
// ============================================================================
// Subscribes to `CoreEventHook::Events::OnNewPath` (id 33), fired by the
// inline detour on the AIBaseClient path-replace path. `sender` = hero,
// `context` = 0, `intParam` = new waypoint count.
//
// Per the Phase 2.2 plan we KEEP the per-hero cache: consumers ask
// `GetCachedPath(hero)` to read the most recent waypoint list without
// re-issuing a `CopyWaypoints` syscall. The cache is populated from the
// callback (single dispatch per real path change) instead of being
// rebuilt on every tick by a poll loop.
// ============================================================================

#include "../../core/CoreEventHook.h"
#include "../../core/offset.h"
#include "../../menu/MenuUI.h"
#include "../Core/Game.h"
#include "../GameObjects/AIBaseClient.h"
#include "../GameObjects/ObjectManager.h"

#include <algorithm>
#include <new>
#include <unordered_map>
#include <vector>

namespace SDK::Events::Path {

struct NewPathEventArgs {
    std::vector<Vector3> Path = {};
    bool IsDash = false;
    float Speed = 0.0f;

    bool IsValid() const { return !Path.empty(); }
};

using NewPathHandler = void(*)(const AIBaseClient& sender, const NewPathEventArgs& args);

namespace detail {
    struct PathState {
        NewPathEventArgs LastArgs = {};
    };

    inline std::unordered_map<int, PathState>* g_states = nullptr;
    inline MenuUI::FixedList<NewPathHandler, 64> g_handlers = {};
    inline bool g_registered = false;

    inline bool EnsureStorage() {
        if (!g_states) {
            g_states = new(std::nothrow) std::unordered_map<int, PathState>();
        }
        return g_states != nullptr;
    }

    inline NewPathEventArgs BuildArgs(const AIBaseClient& hero, int wpCount) {
        NewPathEventArgs args = {};

        Vector3 wpBuf[32] = {};
        const int requested = (wpCount > 0 && wpCount <= 32) ? wpCount : 32;
        const int got = hero.Ref().CopyWaypoints(reinterpret_cast<Vec3*>(wpBuf), requested);

        args.Path.reserve(got > 0 ? got : 2);
        if (got > 0) {
            for (int i = 0; i < got; ++i) args.Path.push_back(wpBuf[i]);
        } else {
            args.Path.push_back(hero.Position());
            const auto end = hero.PathEnd();
            if (!end.IsZero() && end.Distance2D(hero.Position()) > 1.0f) {
                args.Path.push_back(end);
            }
        }

        args.IsDash = hero.IsDashing();
        args.Speed  = args.IsDash
            ? std::max(hero.Velocity().Length2D(), hero.MoveSpeed())
            : hero.MoveSpeed();
        if (args.Speed <= 0.0f) args.Speed = hero.MoveSpeed();
        return args;
    }

    // CoreEventHook trampoline.
    inline void OnNewPathThunk(uintptr_t sender, uintptr_t /*context*/, int intParam) {
        if (!EnsureStorage()) return;
        AIBaseClient hero(sender);
        if (!hero.IsValid() || hero.IsDead()) return;

        NewPathEventArgs args = BuildArgs(hero, intParam);
        (*g_states)[hero.NetworkId()].LastArgs = args;

        for (const auto& h : g_handlers) {
            if (h) h(hero, args);
        }
    }
}

inline void Initialize() {
    detail::EnsureStorage();
    if (!detail::g_registered) {
        CoreEventHook::SetCallback(Offset::Events::OnNewPath, detail::OnNewPathThunk);
        detail::g_registered = true;
    }
}

inline bool AddOnNewPath(NewPathHandler h) {
    Initialize();
    return h && detail::g_handlers.push_back(h);
}

inline bool OnNewPath(NewPathHandler h) { return AddOnNewPath(h); }

inline NewPathEventArgs GetCachedPath(const AIBaseClient& hero) {
    if (!detail::g_states || !hero.IsValid()) return {};
    const auto it = detail::g_states->find(hero.NetworkId());
    return (it != detail::g_states->end()) ? it->second.LastArgs : NewPathEventArgs{};
}

inline void Update() {
    // Push-driven — nothing to poll.
}

inline void Reset() {
    if (detail::g_states) detail::g_states->clear();
    detail::g_handlers.clear();
}

} // namespace SDK::Events::Path
