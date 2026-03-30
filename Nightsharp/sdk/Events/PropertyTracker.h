#pragma once

// ============================================================================
// PropertyTracker — Poll-based OnIntegerPropertyChange
//
// EnsoulSharp equivalent: GameObject.OnIntegerPropertyChange
//
// How it works (manual-map safe):
//   Each tick we poll GetActionState() for every hero.
//   If the value changed from last tick → fire OnIntegerPropertyChange
//   with both old and new values.
//
// This is the primary source for Stealth detection in EnsoulSharp.
// NightSharp's Stealth.h already works via visibility polling, but this
// event provides the raw ActionState change for any subscriber.
//
// DEPENDENCY: CoreObjects::ObjectRef::GetActionState() (core/CoreObjects.h)
// DEPENDENCY: Offset::ActionState flags (core/Offsets.h)
// DEPENDENCY: SDK ObjectManager::Heroes() (sdk/Core/Objects.h)
//
// NOTE: Core đã có GetActionState() + GetActionState2(). Không cần bổ sung.
//
// LIMITATION: EnsoulSharp's OnIntegerPropertyChange tracks ANY integer
// property by name ("ActionState", etc.). This implementation only tracks
// ActionState, which is the only property used in practice.
// ============================================================================

#include "../../menu/MenuUI.h"
#include "../Core/Game.h"
#include "../Core/Objects.h"

#include <new>
#include <unordered_map>

namespace SDK::Events::PropertyTracker {

// ---------------------------------------------------------------------------
// Event Args — matches EnsoulSharp: GameObjectIntegerPropertyChangeEventArgs
// ---------------------------------------------------------------------------

struct IntegerPropertyChangeEventArgs {
    const char* Property = "ActionState";   // property name (always ActionState for now)
    int OldValue = 0;
    int NewValue = 0;

    bool IsValid() const { return OldValue != NewValue; }
};

using PropertyChangeHandler = void(*)(const GameObject& sender, const IntegerPropertyChangeEventArgs& args);

// ---------------------------------------------------------------------------
// Internal
// ---------------------------------------------------------------------------
namespace detail {

    struct PropertyState {
        int LastActionState = 0;
        bool Initialized = false;
    };

    inline std::unordered_map<int, PropertyState>* g_states = nullptr;
    inline MenuUI::FixedList<PropertyChangeHandler, 64> g_handlers = {};

    inline bool EnsureStorage() {
        if (!g_states) {
            g_states = new(std::nothrow) std::unordered_map<int, PropertyState>();
        }
        return g_states != nullptr;
    }

} // namespace detail

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

inline void Initialize() { detail::EnsureStorage(); }

inline bool AddOnIntegerPropertyChange(PropertyChangeHandler h) {
    return h && detail::g_handlers.push_back(h);
}
inline bool OnIntegerPropertyChange(PropertyChangeHandler h) {
    return AddOnIntegerPropertyChange(h);
}

inline void Update() {
    if (!detail::EnsureStorage()) return;
    if (detail::g_handlers.empty()) return;   // skip work if no subscribers

    for (const auto& hero : ObjectManager::Heroes()) {
        const int netId = hero.NetworkId();
        if (!hero.IsValid() || netId == 0 || hero.IsDead()) {
            if (netId != 0) detail::g_states->erase(netId);
            continue;
        }

        const int currentState = hero.Ref().GetActionState();
        auto& state = (*detail::g_states)[netId];

        if (state.Initialized && state.LastActionState != currentState) {
            IntegerPropertyChangeEventArgs args = {};
            args.Property = "ActionState";
            args.OldValue = state.LastActionState;
            args.NewValue = currentState;

            for (const auto& h : detail::g_handlers) {
                if (h) h(hero, args);
            }
        }

        state.LastActionState = currentState;
        state.Initialized = true;
    }
}

inline void Reset() {
    if (detail::g_states) detail::g_states->clear();
    detail::g_handlers.clear();
}

} // namespace SDK::Events::PropertyTracker
