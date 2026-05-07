#pragma once

// ============================================================================
// Stealth — push-driven stealth event surface
// ============================================================================
// Subscribes to `CoreEventHook::Events::OnStealth` (id 51), the piggy-back
// edge fired from `HkOnBuffAdd` when the added buff name matches a known
// stealth identifier (Invisible, Camouflage, *Stealth, *HideIn*, …).
//
// `sender` = hero, `context` = BuffData*, `intParam` = 1 (entering stealth).
//
// The previous visibility-poll loop (per-tick `IsVisible()` diff) is gone.
// One semantic shift: there is no push edge for "leaving stealth" because
// the BuffManager dispatcher does not surface buff removals. Consumers
// that need to react when a hero re-appears should poll `hero.IsVisible()`
// directly at the point of decision — this is rare in champion scripts
// (most logic only cares about the hide event).
//
// `GetLastVisiblePosition()` and `GetStealthDuration()` continue to work:
// the last visible position is captured at OnStealth-fire time from the
// hero's current `Position()` (which the game has not yet hidden when the
// dispatcher runs — buffs apply before vis is re-evaluated).
// ============================================================================

#include "../../core/CoreEventHook.h"
#include "../../core/offset.h"
#include "../../menu/MenuUI.h"
#include "../Core/Game.h"
#include "../GameObjects/AIHeroClient.h"
#include "../GameObjects/ObjectManager.h"

#include <new>
#include <unordered_map>

namespace SDK::Events::Stealth {

struct OnStealthEventArgs {
    bool IsStealthed = false;
    Vector3 LastPosition = {};
    AIHeroClient Sender = {};
    float Time = 0.0f;

    bool IsValid() const { return Sender.IsValid(); }
};

using StealthHandler = void(*)(const OnStealthEventArgs&);

namespace detail {
    struct StealthState {
        Vector3 LastVisiblePosition = {};
        float StealthStartTime = 0.0f;
        bool IsStealthed = false;
    };

    inline std::unordered_map<int, StealthState>* g_states = nullptr;
    inline MenuUI::FixedList<StealthHandler, 64> g_handlers = {};
    inline bool g_registered = false;

    inline bool EnsureStorage() {
        if (!g_states) {
            g_states = new(std::nothrow) std::unordered_map<int, StealthState>();
        }
        return g_states != nullptr;
    }

    // CoreEventHook trampoline.
    inline void OnStealthThunk(uintptr_t sender, uintptr_t /*context*/, int /*intParam*/) {
        if (!EnsureStorage()) return;
        AIHeroClient hero(sender);
        if (!hero.IsValid() || hero.IsDead()) return;

        const float now = Game::Time();
        auto& state = (*g_states)[hero.NetworkId()];
        state.IsStealthed         = true;
        state.StealthStartTime    = now;
        state.LastVisiblePosition = hero.Position();

        OnStealthEventArgs args = {};
        args.Sender       = hero;
        args.IsStealthed  = true;
        args.Time         = now;
        args.LastPosition = state.LastVisiblePosition;

        for (const auto& h : g_handlers) {
            if (h) h(args);
        }
    }
}

inline void Initialize() {
    detail::EnsureStorage();
    if (!detail::g_registered) {
        CoreEventHook::SetCallback(Offset::Events::OnStealth, detail::OnStealthThunk);
        detail::g_registered = true;
    }
}

inline bool AddOnStealth(StealthHandler handler) {
    Initialize();
    return handler && detail::g_handlers.push_back(handler);
}

inline bool OnStealth(StealthHandler handler) {
    return AddOnStealth(handler);
}

inline Vector3 GetLastVisiblePosition(const AIHeroClient& hero) {
    if (!detail::g_states || !hero.IsValid()) return {};
    const auto it = detail::g_states->find(hero.NetworkId());
    return (it != detail::g_states->end()) ? it->second.LastVisiblePosition : Vector3{};
}

inline float GetStealthDuration(const AIHeroClient& hero) {
    if (!detail::g_states || !hero.IsValid()) return 0.0f;
    const auto it = detail::g_states->find(hero.NetworkId());
    if (it == detail::g_states->end() || !it->second.IsStealthed) return 0.0f;
    // If the hero is now visible we treat the stealth state as ended even
    // though we never received a push edge for it. Keeps the duration
    // reading honest.
    if (hero.IsVisible()) return 0.0f;
    return Game::Time() - it->second.StealthStartTime;
}

inline void Update() {
    // Push-driven — nothing to poll.
}

inline void Reset() {
    if (detail::g_states) detail::g_states->clear();
    detail::g_handlers.clear();
}

} // namespace SDK::Events::Stealth
