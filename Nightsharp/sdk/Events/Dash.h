#pragma once

// ============================================================================
// Dash — push-driven dash event surface
// ============================================================================
// Subscribes to `CoreEventHook::Events::OnDash` (id 50), the piggy-back
// edge fired from `HkOnHeroActionState` when an `AIManager.IsDashing` byte
// transitions false→true. The previous tick-based `IsDashing()` polling
// loop has been removed — this header is now zero-cost when no handlers
// are registered, and the per-hero state map is populated directly from
// the OnDash callback.
//
// `IsDashing()` and `GetDashInfo()` continue to work because the dash end
// tick is computed at start time from the start/end position + estimated
// speed, so we do not need to observe the false→true→false edge to know
// when a dash visually ends.
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

namespace SDK::Events::Dash {

struct DashArgs {
    int Duration = 0;
    Vector2 EndPos = {};
    int EndTick = 0;
    bool IsBlink = false;
    std::vector<Vector2> Path = {};
    float Speed = 0.0f;
    Vector2 StartPos = {};
    int StartTick = 0;
    AIBaseClient Unit = {};

    bool IsValid() const {
        return Unit.IsValid() && EndTick > StartTick;
    }
};

using DashHandler = void(*)(const DashArgs&);

namespace detail {
    struct DashState {
        DashArgs Args = {};
    };

    inline std::unordered_map<int, DashState>* g_dashStates = nullptr;
    inline MenuUI::FixedList<DashHandler, 64> g_dashHandlers = {};
    inline bool g_registered = false;

    inline bool EnsureStorage() {
        if (!g_dashStates) {
            g_dashStates = new(std::nothrow) std::unordered_map<int, DashState>();
        }
        return g_dashStates != nullptr;
    }

    inline Vector2 ResolveStart(const AIBaseClient& hero) {
        Vector3 start = hero.PreviousPosition();
        if (start.IsZero()) start = hero.Position();
        return start.To2D();
    }

    inline Vector2 ResolveEnd(const AIBaseClient& hero) {
        Vector3 end = hero.PathEnd();
        if (end.IsZero()) end = hero.OrderPosition();
        if (end.IsZero()) end = hero.Position();
        return end.To2D();
    }

    inline float ResolveSpeed(const AIBaseClient& hero) {
        const float v = hero.Velocity().Length2D();
        if (v > 50.0f) return v;
        const float ms = hero.MoveSpeed();
        return ms > 0.0f ? ms * 2.0f : 0.0f;
    }

    inline DashArgs BuildArgs(const AIBaseClient& hero) {
        DashArgs a = {};
        a.Unit      = hero;
        a.StartPos  = ResolveStart(hero);
        a.EndPos    = ResolveEnd(hero);
        a.Speed     = ResolveSpeed(hero);
        a.StartTick = Game::TickCount() - (Game::Ping() / 2);
        const float dist = a.StartPos.Distance(a.EndPos);
        a.Duration  = (a.Speed > 1.0f) ? static_cast<int>((dist / a.Speed) * 1000.0f) : 0;
        if (a.Duration <= 0) a.Duration = 50;
        a.EndTick   = a.StartTick + a.Duration;
        a.IsBlink   = a.Speed > 5000.0f || a.Duration <= 10;
        a.Path.clear();
        a.Path.push_back(a.StartPos);
        a.Path.push_back(a.EndPos);
        return a;
    }

    // CoreEventHook trampoline. Fires when a hero begins dashing.
    inline void OnDashThunk(uintptr_t sender, uintptr_t /*context*/, int /*intParam*/) {
        if (!EnsureStorage()) return;
        AIBaseClient hero(sender);
        if (!hero.IsValid() || hero.IsDead()) return;

        DashArgs args = BuildArgs(hero);
        (*g_dashStates)[hero.NetworkId()] = DashState{ args };

        for (const auto& h : g_dashHandlers) {
            if (h) h(args);
        }
    }
}

inline void Initialize() {
    detail::EnsureStorage();
    if (!detail::g_registered) {
        CoreEventHook::SetCallback(Offset::Events::OnDash, detail::OnDashThunk);
        detail::g_registered = true;
    }
}

inline bool AddOnDash(DashHandler handler) {
    Initialize();
    return handler && detail::g_dashHandlers.push_back(handler);
}

inline bool OnDash(DashHandler handler) {
    return AddOnDash(handler);
}

inline DashArgs GetDashInfo(const AIBaseClient& unit) {
    if (!detail::g_dashStates || !unit.IsValid()) return {};
    const auto it = detail::g_dashStates->find(unit.NetworkId());
    return (it != detail::g_dashStates->end()) ? it->second.Args : DashArgs{};
}

inline bool IsDashing(const AIBaseClient& unit) {
    const DashArgs info = GetDashInfo(unit);
    return info.IsValid() && Game::TickCount() <= info.EndTick;
}

inline void Update() {
    // Push-driven — nothing to poll. Kept so the central Events::Update()
    // dispatcher remains uniform.
}

inline void Reset() {
    if (detail::g_dashStates) detail::g_dashStates->clear();
    detail::g_dashHandlers.clear();
}

} // namespace SDK::Events::Dash
