#pragma once
// ============================================================================
// CoreAPI.h — umbrella header for the ported `core/` modules.
// ============================================================================
// Downstream consumers (the SDK layer under `sdk/`, plugin shims such as
// `NightSharpAPIBinding.h`, and any future `Core*` wrapper) include THIS ONE
// file instead of each individual `CoreX.h`. It also exposes a short
// `CoreAPI::` prefix on top of the real namespaces so call-sites read as
// `CoreAPI::Control::CastSpell(...)` rather than the verbose `::CoreControl::CastSpell(...)`.
//
// The umbrella deliberately includes EVERY ported module — even the ones a
// given SDK file doesn't touch — because:
//   1. Header-only inclusion is cheap (each module is `#pragma once` gated).
//   2. It guarantees the sub-namespace aliases below always resolve, so
//      downstream code never has to worry about forgetting a specific
//      `#include "core/CoreXxx.h"` alongside this umbrella.
//   3. The build cost is paid by `dllmain.cpp` / `overlay/Overlay.cpp` only;
//      the SDK TUs (once they come online in Phase 2) pull the same set
//      anyway.
//
// NOT included here:
//   - `CoreEventHook.h` — downstream SDK code that only needs data accessors
//     (Objects / Buffs / Spells / …) shouldn't drag in the 2.4 k-LOC hook
//     machinery. SDKDiagnostics + overlay include it directly.
//   - `RuntimeAPI.h`    — REMOVED from the port. Legacy SDK code that still
//     references `CoreAPI::RuntimeAPI::…` must either inline the accessor it
//     needed or re-route through one of the sub-namespaces below.
// ============================================================================

// ── Low-level building blocks (offsets / vectors / globals) ─────────────────
#include "offset.h"
#include "Vector.h"
#include "Globals.h"
#include "LeagueObfuscation.h"

// ── Runtime / lifecycle ──
#include "CoreRuntime.h"

// ── Data-layer accessors (no hooks, no trampolines) ─────────────────────────
#include "CoreGame.h"
#include "CoreView.h"
#include "CoreAi.h"
#include "CoreBuffs.h"
#include "CoreClassification.h"
#include "CoreItem.h"
#include "CoreNavGrid.h"
#include "CoreObjects.h"
#include "CoreSpellBook.h"
#include "CoreSpellCastInfo.h"

// ── Write-path helpers (spoof call / validation / issue-order) ──────────────
#include "CoreBypass.h"
#include "CoreControl.h"
#include "CoreValidation.h"

// ============================================================================
// `CoreAPI::` prefix — thin namespace aliases plus a handful of facade helpers
// ============================================================================
// Each `namespace X = ::CoreX;` below makes `CoreAPI::X::Symbol` resolve to
// exactly `::CoreX::Symbol` (same ADL, same types, no copies). The aliases
// are pure name-routing — they don't introduce new overloads or trigger any
// additional translation-unit work.
//
// The `State` namespace is the one deliberate wrapper: the SDK layer expects
// `CoreAPI::State::IsRuntimeReady()` but the real readiness check lives under
// `CoreRuntime::IsReady()`. We wrap rather than alias so a future readiness
// refactor (e.g. folding in SpoofCall / plugin lifecycle) can happen without
// touching every SDK call-site.
namespace CoreAPI {

    // Raw data-layer accessors — no state, no hooks.
    namespace Game     = ::CoreGame;
    namespace View     = ::CoreView;
    namespace Ai       = ::CoreAi;
    namespace Buffs    = ::CoreBuffs;
    namespace Items    = ::CoreItem;
    namespace NavGrid  = ::CoreNavGrid;
    namespace Objects  = ::CoreObjects;
    namespace Spells   = ::CoreSpellBook;
    namespace SpellBook = ::CoreSpellBook;  // legacy alias used by sdk/GameObjects/SpellDataInstClient.h + SpellbookClient.h
    namespace Cast     = ::CoreSpellCastInfo;
    namespace Classify = ::CoreClassification;

    // Write-path helpers.
    namespace Bypass   = ::CoreBypass;
    namespace Control  = ::CoreControl;
    namespace Validate = ::CoreValidation;

    // ── State facade — thin wrapper over `CoreRuntime` so SDK call-sites
    //    don't bind to the raw runtime module. Only the ready/init/tick/
    //    shutdown surface is forwarded; anything more specific (phase
    //    introspection, status masks, …) stays on `::CoreRuntime` where it
    //    belongs. ──
    namespace State {
        inline bool IsRuntimeReady() { return ::CoreRuntime::IsReady(); }
        inline bool IsInitReady()    { return ::CoreRuntime::HasInitReady(); }
        inline bool Initialize()     { return ::CoreRuntime::Initialize(); }
        inline bool TickRead()       { return ::CoreRuntime::TickRead(); }
        inline void Shutdown()       { ::CoreRuntime::Shutdown(); }
    } // namespace State

} // namespace CoreAPI
