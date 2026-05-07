#pragma once

// ============================================================================
// BuffTracker — push-driven buff event surface
// ============================================================================
// Subscribes to `CoreEventHook::Events::OnBuffUpdate` (id 12). CoreEventHook
// supplies this from the live BuffManager path, with a poll fallback for add,
// refresh, and removal edges.
// manager every tick (12 heroes × up to 64 buffs); that snapshot loop is
// gone — `Update()` is now a no-op kept only for API compatibility with
//
// Public API preserved:
//
//     BuffTracker::OnBuffGain(cb)   // alias of OnBuffUpdate
//     BuffTracker::OnBuffLose(cb)   // see deprecation note below
//     BuffTracker::OnBuffUpdate(cb) // canonical
//     BuffTracker::Initialize()     // wires the CoreEventHook callback
//     BuffTracker::Update()         // no-op
//     BuffTracker::Reset()          // clears handler list
//
// Semantics:
//   * `OnBuffUpdate` fires exactly when the game's BuffManager dispatcher
//     processes a buff. The dispatched event has the buff in its **current**
//     state — newly added, refreshed duration, or new stack count. Consumers
//     that previously distinguished "gain" vs "refresh" by diffing must now
//     read `args.Stacks` / `args.EndTime` and decide for themselves.
//   * `OnBuffLose` is a compatibility filter over OnBuffUpdate removals.
// ============================================================================

#include "../../core/CoreBuffs.h"
#include "../../core/CoreEventHook.h"
#include "../../core/offset.h"
#include "../../menu/MenuUI.h"
#include "../GameObjects/AIBaseClient.h"

#include <Windows.h>
#include <functional>
#include <string>

namespace SDK::Events {

struct BuffEventArgs {
    std::string Name;          // buff identifier (cosmetic; hash-stable)
    int         Type      = 0; // BuffData.Type byte (Internal / Aura / …)
    int         Stacks    = 0; // current stack count
    float       StartTime = 0.0f;
    float       EndTime   = 0.0f;
    uintptr_t   Address   = 0; // raw BuffData* for advanced reads
};

namespace BuffTracker {

using BuffCallback = std::function<void(const AIBaseClient&, const BuffEventArgs&)>;

namespace detail {

    // Storage — fixed slots so the handler list does not allocate inside
    // the SEH-guarded callback path.
    static constexpr int kMaxCallbacks = 32;
    inline BuffCallback s_handlers[kMaxCallbacks] = {};
    inline int          s_count = 0;
    inline bool         s_registered = false;

    // CoreEventHook trampoline. `sender` = hero object*, `context` =
    // BuffData*, `intParam` = stacks (0 if unavailable). We rebuild the
    // BuffEventArgs via `CoreBuffs::BuffRef` so the consumer sees the same
    // shape it always did.
    inline void OnBuffUpdateThunk(uintptr_t sender, uintptr_t context, int intParam) {
        if (s_count == 0) return;

        AIBaseClient hero(sender);
        if (!hero.IsValid()) return;

        BuffEventArgs args = {};
        args.Address = context;
        args.Stacks  = intParam;

        // If the dispatcher gave us a BuffData* we can read the canonical
        // fields. Otherwise fall back to whatever `intParam` carried.
        CoreBuffs::BuffRef ref{ context };
        if (ref.IsValid()) {
            char nameBuf[96] = {};
            if (ref.ReadName(nameBuf, sizeof(nameBuf))) {
                args.Name = nameBuf;
            }
            args.Type      = ref.GetType();
            const int s    = ref.GetStacks();
            if (intParam != 0 && s > 0) args.Stacks = s;
            args.StartTime = ref.GetStartTime();
            args.EndTime   = ref.GetEndTime();
        }

        for (int i = 0; i < s_count; ++i) {
            if (s_handlers[i]) s_handlers[i](hero, args);
        }
    }

} // namespace detail

// ── Lifecycle ──────────────────────────────────────────────────────────
inline void Initialize() {
    if (detail::s_registered) return;
    CoreEventHook::SetCallback(Offset::Events::OnBuffUpdate, detail::OnBuffUpdateThunk);
    detail::s_registered = true;
}

inline void Update() {
    // Push-driven now — nothing to poll. Kept so the central
    // `Events::Update()` dispatcher does not need a special case.
}

inline void Reset() {
    for (int i = 0; i < detail::s_count; ++i) detail::s_handlers[i] = nullptr;
    detail::s_count = 0;
    // Note: we intentionally leave the CoreEventHook callback installed so
    // a subsequent Initialize()/subscribe pair re-uses the same trampoline.
}

// ── Subscription ───────────────────────────────────────────────────────
inline void OnBuffUpdate(BuffCallback cb) {
    Initialize();
    if (detail::s_count < detail::kMaxCallbacks) {
        detail::s_handlers[detail::s_count++] = std::move(cb);
    }
}

// Legacy aliases are filtered over the canonical OnBuffUpdate stream.
// — see file header for the rationale.
inline void OnBuffGain(BuffCallback cb) {
    OnBuffUpdate([cb = std::move(cb)](const AIBaseClient& hero, const BuffEventArgs& args) {
        if (args.Stacks > 0) cb(hero, args);
    });
}

inline void OnBuffLose(BuffCallback cb) {
    OnBuffUpdate([cb = std::move(cb)](const AIBaseClient& hero, const BuffEventArgs& args) {
        if (args.Stacks == 0) cb(hero, args);
    });
}

inline void Clear() { Reset(); }

} // namespace BuffTracker
} // namespace SDK::Events
