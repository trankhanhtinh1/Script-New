#pragma once

// ============================================================================
// AnimationTracker — hybrid push + throttled-poll OnPlayAnimation
// ============================================================================
// The dedicated `OnPlayAnimation` engine event remains REMOVED — the
// underlying AnimationId field offset is not recoverable via IDA (no RTTI
// string / reflection field name) and `AIBaseClient::PlayAnimation` has
// no direct code xrefs. So a single inline detour cannot drive every
// animation transition.
//
// This tracker covers the gap with two complementary surfaces:
//
//   1. **Push piggy-backs** for the 90% case scripts actually subscribe to:
//        - `OnProcessSpell` (id 1)  → fire "Attack{slot==64}" or
//                                     "Spell{slot}" the moment a cast or
//                                     auto-attack begins.
//        - `OnDeath`         (id 30) → fire "Death" on the death edge.
//      These two cover spell-cast, auto-attack, and death animations
//      with sub-frame latency.
//
//   2. **Throttled poll** for everything else (Run / Idle / Recall / …).
//      Runs once every `kPollEveryNTicks` instead of every tick, cutting
//      the read cost (4-deref pointer chain + string compare per hero) by
//      ~3× for animations the push hooks cannot capture.
//
// If no handler is registered, BOTH paths early-out — the tracker is
// zero-cost when unused.
// ============================================================================

#include "../../core/CoreEventHook.h"
#include "../../core/offset.h"
#include "../../menu/MenuUI.h"
#include "../Core/Game.h"
#include "../GameObjects/AIBaseClient.h"
#include "../GameObjects/ObjectManager.h"

#include <cstring>
#include <new>
#include <string>
#include <unordered_map>

namespace SDK::Events::AnimationTracker {

struct PlayAnimationEventArgs {
    std::string Animation = {};

    bool IsValid() const { return !Animation.empty(); }

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
    bool IsDeath() const { return Animation == "Death"; }
};

using PlayAnimationHandler = void(*)(const AIBaseClient& sender, const PlayAnimationEventArgs& args);

namespace detail {

    // Number of tick-loop calls between full poll passes. The push path
    // catches spell/AA/death; everything else is cosmetic and tolerates
    // a few-frame delay.
    static constexpr int kPollEveryNTicks = 3;

    struct AnimState {
        char LastAnimation[128] = {};
        bool Initialized = false;
    };

    inline std::unordered_map<int, AnimState>* g_states = nullptr;
    inline MenuUI::FixedList<PlayAnimationHandler, 64> g_handlers = {};
    inline bool g_registered = false;
    inline int  g_tickCounter = 0;

    inline bool EnsureStorage() {
        if (!g_states) {
            g_states = new(std::nothrow) std::unordered_map<int, AnimState>();
        }
        return g_states != nullptr;
    }

    // Mark the cached "last animation" to suppress a duplicate push from
    // the next poll pass.
    inline void NoteLastAnimation(int netId, const char* anim) {
        if (!EnsureStorage() || netId == 0 || !anim) return;
        auto& state = (*g_states)[netId];
        std::strncpy(state.LastAnimation, anim, sizeof(state.LastAnimation) - 1);
        state.LastAnimation[sizeof(state.LastAnimation) - 1] = 0;
        state.Initialized = true;
    }

    inline void Dispatch(const AIBaseClient& hero, const char* anim) {
        if (g_handlers.empty() || !anim || !*anim) return;
        PlayAnimationEventArgs args = {};
        args.Animation = anim;
        for (const auto& h : g_handlers) {
            if (h) h(hero, args);
        }
        NoteLastAnimation(hero.NetworkId(), anim);
    }

    // ── Push trampolines ────────────────────────────────────────────
    inline void OnProcessSpellThunk(uintptr_t sender, uintptr_t /*ctx*/, int slot) {
        if (g_handlers.empty()) return;
        AIBaseClient hero(sender);
        if (!hero.IsValid()) return;

        // Slot 64 is the auto-attack synthetic slot used by every cast
        // dispatcher path. Anything 0-7 is a real spell slot (Q/W/E/R + …).
        char buf[16] = {};
        if (slot == 64) {
            std::strcpy(buf, "Attack");
        } else if (slot >= 0 && slot <= 7) {
            buf[0] = 'S'; buf[1] = 'p'; buf[2] = 'e'; buf[3] = 'l'; buf[4] = 'l';
            buf[5] = static_cast<char>('1' + (slot & 7));
            buf[6] = 0;
        } else {
            return;
        }
        Dispatch(hero, buf);
    }

    inline void OnDeathThunk(uintptr_t sender, uintptr_t /*ctx*/, int /*p*/) {
        if (g_handlers.empty()) return;
        AIBaseClient hero(sender);
        if (!hero.IsValid()) return;
        Dispatch(hero, "Death");
    }

} // namespace detail

inline void Initialize() {
    detail::EnsureStorage();
    if (!detail::g_registered) {
        CoreEventHook::SetCallback(Offset::Events::OnProcessSpell, detail::OnProcessSpellThunk);
        CoreEventHook::SetCallback(Offset::Events::OnDeath,        detail::OnDeathThunk);
        detail::g_registered = true;
    }
}

inline bool AddOnPlayAnimation(PlayAnimationHandler h) {
    Initialize();
    return h && detail::g_handlers.push_back(h);
}
inline bool OnPlayAnimation(PlayAnimationHandler h) {
    return AddOnPlayAnimation(h);
}

// Throttled fallback poll for animations that the push path cannot reach
// (Run, Idle, Recall, channel transitions, …). One full Heroes() pass
// every `kPollEveryNTicks` ticks.
inline void Update() {
    if (!detail::EnsureStorage()) return;
    if (detail::g_handlers.empty()) return;

    if (++detail::g_tickCounter < detail::kPollEveryNTicks) return;
    detail::g_tickCounter = 0;

    for (const auto& hero : ObjectManager::Heroes()) {
        const int netId = hero.NetworkId();
        if (!hero.IsValid() || netId == 0 || hero.IsDead()) {
            if (netId != 0) detail::g_states->erase(netId);
            continue;
        }

        char currentAnim[128] = {};
        const bool hasAnim = hero.Ref().ReadCurrentAnimation(currentAnim, sizeof(currentAnim));
        if (!hasAnim || currentAnim[0] == 0) continue;

        auto& state = (*detail::g_states)[netId];
        if (state.Initialized && std::strcmp(state.LastAnimation, currentAnim) != 0) {
            PlayAnimationEventArgs args = {};
            args.Animation = currentAnim;
            for (const auto& h : detail::g_handlers) {
                if (h) h(hero, args);
            }
        }
        std::memcpy(state.LastAnimation, currentAnim, sizeof(currentAnim));
        state.Initialized = true;
    }
}

inline void Reset() {
    if (detail::g_states) detail::g_states->clear();
    detail::g_handlers.clear();
    detail::g_tickCounter = 0;
}

} // namespace SDK::Events::AnimationTracker
