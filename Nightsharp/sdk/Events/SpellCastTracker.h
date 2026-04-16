#pragma once

// ============================================================================
// SpellCastTracker — Poll-based OnProcessSpellCast / OnDoCast / OnStopCast
//
// EnsoulSharp equivalents:
//   AIBaseClient.OnProcessSpellCast  → SpellCast::AddOnProcessSpellCast()
//   AIBaseClient.OnDoCast            → SpellCast::AddOnDoCast()
//   Spellbook.OnStopCast             → SpellCast::AddOnStopCast()
//
// How it works (manual-map safe, no hooks):
//   Each tick we poll GetActiveSpellCast() for every hero.
//   - New CastRef.address appears       → fire OnProcessSpellCast
//   - CastRef disappears after windup   → fire OnDoCast
//   - CastRef disappears before windup  → fire OnStopCast
//
// DEPENDENCY: CoreSpellCastInfo::CastRef (core/CoreSpellCastInfo.h)
//   Provides: address, GetSlot, GetStartPos, GetEndPos, GetCastPos,
//             GetCastDelay, IsAutoAttack, IsSpecialAttack, ReadSpellName,
//             GetTargetIndex
// DEPENDENCY: CoreObjects::ObjectRef::GetActiveSpellCast (core/CoreObjects.h)
// DEPENDENCY: SDK ObjectManager::Heroes() (sdk/Core/Objects.h)
// ============================================================================

#include "../../core/CoreEventHook.h"
#include "../../menu/MenuUI.h"
#include "../Core/Game.h"
#include "../Core/Objects.h"

#include <algorithm>
#include <new>
#include <string>
#include <unordered_map>

namespace SDK::Events::SpellCast {

// ---------------------------------------------------------------------------
// Event Args
// ---------------------------------------------------------------------------

/// Matches EnsoulSharp: AIBaseClientProcessSpellCastEventArgs
struct ProcessSpellCastEventArgs {
    Vector3 Start = {};
    Vector3 End = {};            // "To" in EnsoulSharp
    Vector3 CastPosition = {};
    SpellSlot Slot = SpellSlot::Unknown;
    std::string SpellName = {};
    bool IsAutoAttack = false;
    bool IsSpecialAttack = false;
    float CastDelay = 0.0f;
    float MissileSpeed = 0.0f;   // from SData.MissileSpeed
    int TargetNetworkId = 0;

    bool IsValid() const {
        return !SpellName.empty() || Slot != SpellSlot::Unknown;
    }
};

/// Matches EnsoulSharp: SpellbookStopCastEventArgs
struct StopCastEventArgs {
    SpellSlot Slot = SpellSlot::Unknown;
    std::string SpellName = {};
    bool SuccessfullyCasted = false;
    bool ForceStop = false;
    bool MissileDestroyed = false;
};

// ---------------------------------------------------------------------------
// Handler types
// ---------------------------------------------------------------------------

/// Fires when cast begins (windup starts). EnsoulSharp: AIBaseClient.OnProcessSpellCast
using ProcessSpellCastHandler = void(*)(const AIBaseClient& sender, const ProcessSpellCastEventArgs& args);

/// Fires when cast completes (projectile releases). EnsoulSharp: AIBaseClient.OnDoCast
using DoCastHandler = void(*)(const AIBaseClient& sender, const ProcessSpellCastEventArgs& args);

/// Fires when cast is interrupted. EnsoulSharp: Spellbook.OnStopCast
using StopCastHandler = void(*)(const AIBaseClient& sender, const StopCastEventArgs& args);

// ---------------------------------------------------------------------------
// Internal
// ---------------------------------------------------------------------------
namespace detail {

    struct CastState {
        uintptr_t CastAddress = 0;
        ProcessSpellCastEventArgs Args = {};
        int StartTick = 0;
        float CastDelayMs = 0.0f;
        bool Active = false;
        bool DoCastFired = false;
    };

    inline std::unordered_map<int, CastState>* g_states = nullptr;
    inline MenuUI::FixedList<ProcessSpellCastHandler, 64> g_onProcess = {};
    inline MenuUI::FixedList<DoCastHandler, 64>           g_onDoCast  = {};
    inline MenuUI::FixedList<StopCastHandler, 64>         g_onStop    = {};

    inline bool EnsureStorage() {
        if (!g_states) {
            g_states = new(std::nothrow) std::unordered_map<int, CastState>();
        }
        return g_states != nullptr;
    }

    inline ProcessSpellCastEventArgs BuildArgs(const CoreSpellCastInfo::CastRef& cast) {
        ProcessSpellCastEventArgs args = {};
        args.Start        = cast.GetStartPos();
        args.End          = cast.GetEndPos();
        args.CastPosition = cast.GetCastPos();
        args.CastDelay    = cast.GetCastDelay();
        args.IsAutoAttack    = cast.IsAutoAttack();
        args.IsSpecialAttack = cast.IsSpecialAttack();
        args.TargetNetworkId = cast.GetTargetIndex();
        args.MissileSpeed    = cast.GetMissileSpeed();

        const int slot = cast.GetSlot();
        args.Slot = (slot >= 0 && slot <= static_cast<int>(SpellSlot::R))
            ? static_cast<SpellSlot>(slot) : SpellSlot::Unknown;

        char buf[128] = {};
        if (cast.ReadSpellName(buf, static_cast<int>(sizeof(buf)))) {
            args.SpellName = buf;
        }
        return args;
    }

    inline void FireProcess(const AIBaseClient& s, const ProcessSpellCastEventArgs& a) {
        for (const auto& h : g_onProcess) { if (h) h(s, a); }
    }
    inline void FireDoCast(const AIBaseClient& s, const ProcessSpellCastEventArgs& a) {
        for (const auto& h : g_onDoCast) { if (h) h(s, a); }
    }
    inline void FireStop(const AIBaseClient& s, const StopCastEventArgs& a) {
        for (const auto& h : g_onStop) { if (h) h(s, a); }
    }

    constexpr int kErrorBufferMs = 50;

} // namespace detail

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

inline void Initialize() { detail::EnsureStorage(); }

inline bool AddOnProcessSpellCast(ProcessSpellCastHandler h) { return h && detail::g_onProcess.push_back(h); }
inline bool OnProcessSpellCast(ProcessSpellCastHandler h)    { return AddOnProcessSpellCast(h); }

inline bool AddOnDoCast(DoCastHandler h) { return h && detail::g_onDoCast.push_back(h); }
inline bool OnDoCast(DoCastHandler h)    { return AddOnDoCast(h); }

inline bool AddOnStopCast(StopCastHandler h) { return h && detail::g_onStop.push_back(h); }
inline bool OnStopCast(StopCastHandler h)    { return AddOnStopCast(h); }

/// Poll spell cast state for a single unit
inline void PollUnit(const AIBaseClient& unit, int now) {
    const int netId = unit.NetworkId();
    if (!unit.IsValid() || netId == 0) {
        if (netId != 0) detail::g_states->erase(netId);
        return;
    }

    const auto cast = CoreSpellCastInfo::GetActive(unit.Address());
    auto& state = (*detail::g_states)[netId];

    if (cast.IsValid()) {
        if (!state.Active || state.CastAddress != cast.address) {
            // ── NEW CAST detected ──
            state.Active      = true;
            state.CastAddress = cast.address;
            state.Args        = detail::BuildArgs(cast);
            state.StartTick   = now;
            state.CastDelayMs = std::max(state.Args.CastDelay, 0.0f) * 1000.0f;
            state.DoCastFired = false;

            detail::FireProcess(unit, state.Args);
        }
        else if (state.Active && !state.DoCastFired) {
            // ── Same cast, check if windup elapsed → OnDoCast ──
            const int elapsed = now - state.StartTick;
            if (elapsed >= static_cast<int>(state.CastDelayMs) - detail::kErrorBufferMs) {
                state.DoCastFired = true;
                detail::FireDoCast(unit, state.Args);
            }
        }
    }
    else if (state.Active) {
        // ── Cast disappeared ──
        if (!state.DoCastFired) {
            const int elapsed = now - state.StartTick;
            if (elapsed >= static_cast<int>(state.CastDelayMs) - detail::kErrorBufferMs) {
                detail::FireDoCast(unit, state.Args);
            } else {
                StopCastEventArgs sa = {};
                sa.Slot              = state.Args.Slot;
                sa.SpellName         = state.Args.SpellName;
                sa.SuccessfullyCasted = false;
                sa.ForceStop         = true;
                detail::FireStop(unit, sa);
            }
        }
        state.Active      = false;
        state.CastAddress = 0;
        state.DoCastFired = false;
    }
}

/// Call once per tick from Events::Update()
inline void Update() {
    if (!detail::EnsureStorage()) return;

    const int now = Game::TickCount();

    // Poll heroes only — minion/turret AA tracking done via MissileTracker
    // in HealthPrediction::Update() using RuntimeAPI::ClassifyMissile
    for (const auto& hero : ObjectManager::Heroes()) {
        PollUnit(hero, now);
    }
}

inline void Reset() {
    if (detail::g_states) detail::g_states->clear();
    detail::g_onProcess.clear();
    detail::g_onDoCast.clear();
    detail::g_onStop.clear();
    CoreEventHook::UninstallAll();
}

// ---------------------------------------------------------------------------
// Hook-based mode (alternative to poll-based)
//
// IDA reverse of OnProcessSpell @ 0x9362A0:
//   rcx = SpellBook* = (AIBaseClient + 0x30E8)
//   rdx = SpellCastInfo*
//
// The hook fires our ProcessSpellCast callbacks with EXACT timing,
// covering ALL units (not just heroes like poll mode).
// ---------------------------------------------------------------------------
namespace hook {

    inline void OnRawProcessSpell(uintptr_t senderObj, uintptr_t castInfoAddr) {
        if (!senderObj || !castInfoAddr) return;

        CoreSpellCastInfo::CastRef cast = { castInfoAddr };
        if (!cast.IsValid()) return;

        AIBaseClient sender(senderObj);
        if (!sender.IsValid()) return;

        auto args = detail::BuildArgs(cast);
        detail::FireProcess(sender, args);

        // Also update poll state so DoCast/StopCast still work via polling
        if (detail::EnsureStorage()) {
            const int netId = sender.NetworkId();
            if (netId != 0) {
                auto& state = (*detail::g_states)[netId];
                state.Active      = true;
                state.CastAddress = castInfoAddr;
                state.Args        = args;
                state.StartTick   = Game::TickCount();
                state.CastDelayMs = std::max(args.CastDelay, 0.0f) * 1000.0f;
                state.DoCastFired = false;
            }
        }
    }

    inline bool Install() {
        CoreEventHook::SetOnProcessSpellCallback(OnRawProcessSpell);
        return CoreEventHook::InstallProcessSpellHook();
    }

    inline bool IsInstalled() {
        return CoreEventHook::IsProcessSpellHooked();
    }

} // namespace hook

// ---------------------------------------------------------------------------
// Hybrid Update: if hook installed, skip poll for OnProcessSpellCast
// but still poll for OnDoCast / OnStopCast transitions.
// ---------------------------------------------------------------------------
inline void UpdateHybrid() {
    if (!detail::EnsureStorage()) return;

    const int now = Game::TickCount();

    for (const auto& hero : ObjectManager::Heroes()) {
        const int netId = hero.NetworkId();
        if (!hero.IsValid() || netId == 0) continue;

        auto it = detail::g_states->find(netId);
        if (it == detail::g_states->end()) continue;

        auto& state = it->second;
        if (!state.Active) continue;

        const auto cast = CoreSpellCastInfo::GetActive(hero.Address());

        if (cast.IsValid() && cast.address == state.CastAddress) {
            // Same cast still active — check DoCast timing
            if (!state.DoCastFired) {
                const int elapsed = now - state.StartTick;
                if (elapsed >= static_cast<int>(state.CastDelayMs) - detail::kErrorBufferMs) {
                    state.DoCastFired = true;
                    detail::FireDoCast(hero, state.Args);
                }
            }
        }
        else if (!cast.IsValid() || cast.address != state.CastAddress) {
            // Cast disappeared or changed
            if (!state.DoCastFired) {
                const int elapsed = now - state.StartTick;
                if (elapsed >= static_cast<int>(state.CastDelayMs) - detail::kErrorBufferMs) {
                    detail::FireDoCast(hero, state.Args);
                } else {
                    StopCastEventArgs sa = {};
                    sa.Slot              = state.Args.Slot;
                    sa.SpellName         = state.Args.SpellName;
                    sa.SuccessfullyCasted = false;
                    sa.ForceStop         = true;
                    detail::FireStop(hero, sa);
                }
            }
            state.Active      = false;
            state.CastAddress = 0;
            state.DoCastFired = false;
        }
    }
}

} // namespace SDK::Events::SpellCast
