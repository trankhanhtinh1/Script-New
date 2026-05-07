#pragma once

// ============================================================================
// PropertyTracker — push-driven OnIntegerPropertyChange (ActionState)
// ============================================================================
// Subscribes to `CoreEventHook::Events::OnIntegerPropertyChange` (id 35),
// the piggy-back fired from `HkOnHeroActionState` after every action-state
// transition. The hook fires for essentially every hero interaction (move,
// stop, cast, dash, path rebuild) so this is a complete push surface for
// `ActionState` changes — no polling needed.
//
// `intParam` carries the raw current ActionState int. We dedupe per-hero
// by comparing against the cached previous value; user callbacks are only
// invoked when the value genuinely changes.
//
// EnsoulSharp's `OnIntegerPropertyChange` event tracks ANY integer
// property by name. NightSharp only surfaces `ActionState` because it is
// the only property scripts actually subscribe to in practice.
// ============================================================================

#include "../../core/CoreEventHook.h"
#include "../../core/offset.h"
#include "../../menu/MenuUI.h"
#include "../GameObjects/AIBaseClient.h"
#include "../GameObjects/GameObject.h"

#include <new>
#include <unordered_map>

namespace SDK::Events::PropertyTracker {

struct IntegerPropertyChangeEventArgs {
    const char* Property = "ActionState";
    int OldValue = 0;
    int NewValue = 0;

    bool IsValid() const { return OldValue != NewValue; }
};

using PropertyChangeHandler = void(*)(const GameObject& sender, const IntegerPropertyChangeEventArgs& args);

namespace detail {
    struct PropertyState {
        int LastActionState = 0;
        bool Initialized = false;
    };

    inline std::unordered_map<int, PropertyState>* g_states = nullptr;
    inline MenuUI::FixedList<PropertyChangeHandler, 64> g_handlers = {};
    inline bool g_registered = false;

    inline bool EnsureStorage() {
        if (!g_states) {
            g_states = new(std::nothrow) std::unordered_map<int, PropertyState>();
        }
        return g_states != nullptr;
    }

    // CoreEventHook trampoline. `intParam` = current ActionState int.
    inline void OnPropertyThunk(uintptr_t sender, uintptr_t /*context*/, int intParam) {
        if (g_handlers.empty()) return;
        if (!EnsureStorage()) return;

        // Resolve hero — keyed by network id so respawned heroes don't
        // alias each other in the cache.
        AIBaseClient hero(sender);
        if (!hero.IsValid()) return;
        const int netId = hero.NetworkId();
        if (netId == 0) return;

        auto& state = (*g_states)[netId];
        if (!state.Initialized) {
            state.LastActionState = intParam;
            state.Initialized = true;
            return;
        }
        if (state.LastActionState == intParam) return;

        IntegerPropertyChangeEventArgs args = {};
        args.Property = "ActionState";
        args.OldValue = state.LastActionState;
        args.NewValue = intParam;
        state.LastActionState = intParam;

        GameObject obj(sender);
        for (const auto& h : g_handlers) {
            if (h) h(obj, args);
        }
    }
}

inline void Initialize() {
    detail::EnsureStorage();
    if (!detail::g_registered) {
        CoreEventHook::SetCallback(Offset::Events::OnIntegerPropertyChange,
                                   detail::OnPropertyThunk);
        detail::g_registered = true;
    }
}

inline bool AddOnIntegerPropertyChange(PropertyChangeHandler h) {
    Initialize();
    return h && detail::g_handlers.push_back(h);
}
inline bool OnIntegerPropertyChange(PropertyChangeHandler h) {
    return AddOnIntegerPropertyChange(h);
}

inline void Update() {
    // Push-driven — nothing to poll.
}

inline void Reset() {
    if (detail::g_states) detail::g_states->clear();
    detail::g_handlers.clear();
}

} // namespace SDK::Events::PropertyTracker
