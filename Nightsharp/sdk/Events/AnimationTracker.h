#pragma once

// ============================================================================
// AnimationTracker — Poll-based OnPlayAnimation
//
// EnsoulSharp equivalent: AIBaseClient.OnPlayAnimation
//
// How it works (manual-map safe):
//   Each tick we poll ReadCurrentAnimation() for every hero.
//   If the animation name changed from last tick → fire OnPlayAnimation.
//
// DEPENDENCY: CoreObjects::ObjectRef::ReadCurrentAnimation() (core/CoreObjects.h)
//   Uses AnimationLayout offsets: CharacterData → CharacterDataResource →
//   VariantEntries[SkinIndex] → VariantNamePtr (or FallbackNamePtr)
// DEPENDENCY: SDK ObjectManager::Heroes() (sdk/Core/Objects.h)
// ============================================================================

#include "../../menu/MenuUI.h"
#include "../Core/Game.h"
#include "../Core/Objects.h"

#include <cstring>
#include <new>
#include <string>
#include <unordered_map>

namespace SDK::Events::AnimationTracker {

// ---------------------------------------------------------------------------
// Event Args — matches EnsoulSharp: AIBaseClientPlayAnimationEventArgs
// ---------------------------------------------------------------------------

struct PlayAnimationEventArgs {
    std::string Animation = {};       // e.g. "Attack1", "Spell1", "Run", "Idle"

    bool IsValid() const { return !Animation.empty(); }

    // Convenience helpers
    bool IsAttack() const {
        return Animation.size() >= 6
            && Animation[0] == 'A' && Animation[1] == 't' && Animation[2] == 't'
            && Animation[3] == 'a' && Animation[4] == 'c' && Animation[5] == 'k';
    }
    bool IsSpell() const {
        return Animation.size() >= 5
            && Animation[0] == 'S' && Animation[1] == 'p' && Animation[2] == 'e'
            && Animation[3] == 'l' && Animation[4] == 'l';
    }
    bool IsChannel() const {
        return Animation.size() >= 7
            && Animation[0] == 'C' && Animation[1] == 'h' && Animation[2] == 'a'
            && Animation[3] == 'n' && Animation[4] == 'n' && Animation[5] == 'e'
            && Animation[6] == 'l';
    }
    bool IsDeath() const {
        return Animation == "Death";
    }
};

using PlayAnimationHandler = void(*)(const AIBaseClient& sender, const PlayAnimationEventArgs& args);

// ---------------------------------------------------------------------------
// Internal
// ---------------------------------------------------------------------------
namespace detail {

    struct AnimState {
        char LastAnimation[128] = {};
        bool Initialized = false;
    };

    inline std::unordered_map<int, AnimState>* g_states = nullptr;
    inline MenuUI::FixedList<PlayAnimationHandler, 64> g_handlers = {};

    inline bool EnsureStorage() {
        if (!g_states) {
            g_states = new(std::nothrow) std::unordered_map<int, AnimState>();
        }
        return g_states != nullptr;
    }

} // namespace detail

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

inline void Initialize() { detail::EnsureStorage(); }

inline bool AddOnPlayAnimation(PlayAnimationHandler h) {
    return h && detail::g_handlers.push_back(h);
}
inline bool OnPlayAnimation(PlayAnimationHandler h) {
    return AddOnPlayAnimation(h);
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

        char currentAnim[128] = {};
        const bool hasAnim = hero.Ref().ReadCurrentAnimation(currentAnim, sizeof(currentAnim));

        auto& state = (*detail::g_states)[netId];

        if (hasAnim && currentAnim[0] != 0) {
            if (state.Initialized && strcmp(state.LastAnimation, currentAnim) != 0) {
                // Animation changed → fire event
                PlayAnimationEventArgs args = {};
                args.Animation = currentAnim;

                for (const auto& h : detail::g_handlers) {
                    if (h) h(hero, args);
                }
            }
            memcpy(state.LastAnimation, currentAnim, sizeof(currentAnim));
        }

        state.Initialized = true;
    }
}

inline void Reset() {
    if (detail::g_states) detail::g_states->clear();
    detail::g_handlers.clear();
}

} // namespace SDK::Events::AnimationTracker
