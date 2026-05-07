#pragma once

// ============================================================================
// SpellCastTracker — push-driven cast event surface
// ============================================================================
// Subscribes to the three CoreEventHook ids that cover the cast lifecycle:
//
//   OnProcessSpell (1)  — Shadow-VMT hook on the message-factory dispatcher.
//                         Fires the moment a hero begins a cast (windup
//                         start) for EVERY hero on the map. Auto-attacks
//                         surface here too with `intParam == 64`.
//   OnDoCast (53)       — Inline detour piggy-back from `HkOnFinishCast`.
//                         Fires when a cast resolves (projectile released
//                         or instant hit landed) — paired one-to-one with
//                         OnFinishCast on every successful cast.
//   OnStopCast (2)      — Inline detour on the cancel path. Fires when an
//                         active cast disappears BEFORE its end time
//                         (interrupt, manual cancel, death during windup).
//
// All three hooks pass `context = SpellCastInfo*` and `intParam = SpellSlot`
// (0-7 for Q/W/E/R/D/F + items, 64 for auto-attacks). We rebuild the SDK
// `ProcessSpellCastEventArgs` / `StopCastEventArgs` from the SpellCastInfo
// using `CoreSpellCastInfo::CastRef`, so the public API and the event-arg
// shape are identical to the previous poll-based implementation.
//
// The previous tick loop (per-hero `GetActiveSpellCast()` diff with manual
// windup-elapsed bookkeeping for OnDoCast inference) is gone — Update() is
// a no-op kept only so `Events::Update()` does not need a special case.
// ============================================================================

#include "../../core/CoreEventHook.h"
#include "../../core/CoreSpellCastInfo.h"
#include "../../core/offset.h"
#include "../../menu/MenuUI.h"
#include "../GameObjects/AIBaseClient.h"

#include <string>

namespace SDK::Events::SpellCast {

// ── Args ──────────────────────────────────────────────────────────────
struct ProcessSpellCastEventArgs {
    Vector3 Start = {};
    Vector3 End = {};            // "To" in EnsoulSharp
    Vector3 CastPosition = {};
    SpellSlot Slot = SpellSlot::Unknown;
    std::string SpellName = {};
    bool IsAutoAttack = false;
    bool IsSpecialAttack = false;
    float CastDelay = 0.0f;
    float MissileSpeed = 0.0f;
    int TargetNetworkId = 0;

    bool IsValid() const {
        return !SpellName.empty() || Slot != SpellSlot::Unknown;
    }
};

struct StopCastEventArgs {
    SpellSlot Slot = SpellSlot::Unknown;
    std::string SpellName = {};
    bool SuccessfullyCasted = false;
    bool ForceStop = false;
    bool MissileDestroyed = false;
};

// ── Handlers ──────────────────────────────────────────────────────────
using ProcessSpellCastHandler = void(*)(const AIBaseClient& sender, const ProcessSpellCastEventArgs& args);
using DoCastHandler           = void(*)(const AIBaseClient& sender, const ProcessSpellCastEventArgs& args);
using StopCastHandler         = void(*)(const AIBaseClient& sender, const StopCastEventArgs& args);

namespace detail {
    inline MenuUI::FixedList<ProcessSpellCastHandler, 64> g_onProcess = {};
    inline MenuUI::FixedList<DoCastHandler, 64>           g_onDoCast  = {};
    inline MenuUI::FixedList<StopCastHandler, 64>         g_onStop    = {};
    inline bool g_registered = false;

    // Build a ProcessSpellCastEventArgs from the dispatcher's SpellCastInfo*.
    // Slot is taken from the explicit `intParam` (more reliable than reading
    // the layout byte, since the OnProcessSpell VMT path injects the slot
    // index directly into the call frame).
    inline ProcessSpellCastEventArgs BuildArgs(uintptr_t castInfoAddr, int slotParam) {
        ProcessSpellCastEventArgs args = {};
        CoreSpellCastInfo::CastRef cast{ castInfoAddr };
        if (!cast.IsValid()) {
            // Slot may still be valid even with no SpellCastInfo; keep it.
            if (slotParam >= 0 && slotParam <= static_cast<int>(SpellSlot::R)) {
                args.Slot = static_cast<SpellSlot>(slotParam);
            }
            return args;
        }

        args.Start           = cast.GetStartPos();
        args.End             = cast.GetEndPos();
        args.CastPosition    = cast.GetCastPos();
        args.CastDelay       = cast.GetCastDelay();
        args.IsAutoAttack    = cast.IsAutoAttack() || slotParam == 64;
        args.IsSpecialAttack = cast.IsSpecialAttack();
        args.TargetNetworkId = cast.GetTargetIndex();
        args.MissileSpeed    = cast.GetMissileSpeed();

        // Prefer the explicit slot param; fall back to the layout read.
        const int slot = (slotParam >= 0 && slotParam != 64)
            ? slotParam
            : cast.GetSlot();
        args.Slot = (slot >= 0 && slot <= static_cast<int>(SpellSlot::R))
            ? static_cast<SpellSlot>(slot)
            : SpellSlot::Unknown;

        char buf[128] = {};
        if (cast.ReadSpellName(buf, static_cast<int>(sizeof(buf)))) {
            args.SpellName = buf;
        }
        return args;
    }

    // ── CoreEventHook trampolines ───────────────────────────────────
    inline void OnProcessThunk(uintptr_t sender, uintptr_t context, int intParam) {
        if (g_onProcess.empty()) return;
        AIBaseClient hero(sender);
        if (!hero.IsValid()) return;
        ProcessSpellCastEventArgs args = BuildArgs(context, intParam);
        for (const auto& h : g_onProcess) { if (h) h(hero, args); }
    }

    inline void OnDoCastThunk(uintptr_t sender, uintptr_t context, int intParam) {
        if (g_onDoCast.empty()) return;
        AIBaseClient hero(sender);
        if (!hero.IsValid()) return;
        ProcessSpellCastEventArgs args = BuildArgs(context, intParam);
        for (const auto& h : g_onDoCast) { if (h) h(hero, args); }
    }

    inline void OnStopThunk(uintptr_t sender, uintptr_t context, int intParam) {
        if (g_onStop.empty()) return;
        AIBaseClient hero(sender);
        if (!hero.IsValid()) return;

        StopCastEventArgs sa = {};
        CoreSpellCastInfo::CastRef cast{ context };
        if (cast.IsValid()) {
            const int slot = (intParam >= 0 && intParam != 64) ? intParam : cast.GetSlot();
            sa.Slot = (slot >= 0 && slot <= static_cast<int>(SpellSlot::R))
                ? static_cast<SpellSlot>(slot)
                : SpellSlot::Unknown;
            char buf[128] = {};
            if (cast.ReadSpellName(buf, static_cast<int>(sizeof(buf)))) {
                sa.SpellName = buf;
            }
        } else if (intParam >= 0 && intParam <= static_cast<int>(SpellSlot::R)) {
            sa.Slot = static_cast<SpellSlot>(intParam);
        }
        sa.SuccessfullyCasted = false;
        sa.ForceStop          = true;

        for (const auto& h : g_onStop) { if (h) h(hero, sa); }
    }

} // namespace detail

// ── Lifecycle ─────────────────────────────────────────────────────────
inline void Initialize() {
    if (detail::g_registered) return;
    CoreEventHook::SetCallback(Offset::Events::OnProcessSpell, detail::OnProcessThunk);
    CoreEventHook::SetCallback(Offset::Events::OnDoCast,       detail::OnDoCastThunk);
    CoreEventHook::SetCallback(Offset::Events::OnStopCast,     detail::OnStopThunk);
    detail::g_registered = true;
}

inline bool AddOnProcessSpellCast(ProcessSpellCastHandler h) {
    Initialize();
    return h && detail::g_onProcess.push_back(h);
}
inline bool OnProcessSpellCast(ProcessSpellCastHandler h) { return AddOnProcessSpellCast(h); }

inline bool AddOnDoCast(DoCastHandler h) {
    Initialize();
    return h && detail::g_onDoCast.push_back(h);
}
inline bool OnDoCast(DoCastHandler h) { return AddOnDoCast(h); }

inline bool AddOnStopCast(StopCastHandler h) {
    Initialize();
    return h && detail::g_onStop.push_back(h);
}
inline bool OnStopCast(StopCastHandler h) { return AddOnStopCast(h); }

inline void Update() {
    // Push-driven — nothing to poll.
}

inline void Reset() {
    detail::g_onProcess.clear();
    detail::g_onDoCast.clear();
    detail::g_onStop.clear();
}

} // namespace SDK::Events::SpellCast
