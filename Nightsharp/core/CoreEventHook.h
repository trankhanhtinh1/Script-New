#pragma once

// ============================================================================
// CoreEventHook — Shadow-VMT OnProcessSpell hook (ported from legacy build)
//
// Everything else (OnStopCast / OnFinishCast / OnBuffAdd / …) used to sit in
// this file as shadow-module inline detours. Those all FAIL on the current
// build, so the only mechanism that is kept here is the one that was verified
// to work: the ShadowVMT swap around OnProcessSpell.
//
// How the ShadowVMT hook works:
//   1. The game stores a pointer to the spell-event vtable in a writable heap
//      slot (found via FindDispatchSlot).
//   2. We deep-copy the original vtable onto our own heap, overwrite entry
//      `HandlerIndex` with a shellcode trampoline, then atomically swap the
//      heap dispatch slot to our shadow copy.
//   3. The shellcode is 20 bytes: `push rax; mov rax,counter; lock inc qword
//      [rax]; pop rax; jmp [rip+0]; dq originalFn`. It preserves EVERY register
//      and flag except RAX/CF and hands control to the original function.
//      Crucially no code page is ever written, so CRC scanners stay quiet.
//   4. `PollVmtSpellEvents()` runs on the SDK tick; whenever it notices the
//      counter moved it scans hero ActiveSpellCast pointers, sees which unit
//      acquired a new cast, and fires the SDK `g_processSpellCb` callback.
//
// Public API kept small on purpose:
//   SetOnProcessSpellCallback / InstallProcessSpellHook / IsProcessSpellHooked
//   PollVmtSpellEvents        — MUST be called every SDK tick
//   UninstallAll              — restore the original vtable pointer on unload
//
// Re-verify in IDA when porting to a new build:
//   Offset::SpellEventVMT::WrapperRVA        (sub_1FD080 in legacy)
//   Offset::SpellEventVMT::VTableRVA         (.rdata xref to WrapperRVA − 35*8)
//   Offset::SpellEventVMT::HandlerIndex      (call [rax+??h] ⇒ ??h/8)
//   Offset::SpellEventVMT::VTableEntryCount  (count qwords until next vtable)
// ============================================================================

#include "CoreRuntime.h"
#include "CoreSpellCastInfo.h"
#include "Globals.h"
#include "Offsets.h"

#include <Windows.h>
#include <cstdint>
#include <cstring>

namespace CoreEventHook {

// ---------------------------------------------------------------------------
// Types
// ---------------------------------------------------------------------------

// OnProcessSpell(SpellBook*, SpellCastInfo*) → int
using OnProcessSpellFn = int(__fastcall*)(uintptr_t spellBook, uintptr_t castInfo);

// Callback: (senderAddress, castInfoAddress) — raw pointers, SDK wraps them
using RawProcessSpellCallback = void(*)(uintptr_t senderObj, uintptr_t castInfo);

// ---------------------------------------------------------------------------
// Shadow-VMT hook
// ---------------------------------------------------------------------------
namespace detail {

    struct ShadowVMT {
        static constexpr int kMaxTrackedHeroes = 20;

        // Shellcode template (20 bytes):
        //   push rax                                ; 1
        //   mov rax, imm64                          ; 10  ← counter address
        //   lock inc qword ptr [rax]                ; 4
        //   pop rax                                 ; 1
        //   jmp qword ptr [rip+0]                   ; 6
        //   dq <originalFn>                         ; 8  (data appended)
        static constexpr uint8_t kTrampTemplate[] = {
            0x50,                                     // push rax
            0x48, 0xB8, 0,0,0,0, 0,0,0,0,             // mov rax, imm64 (counter)
            0xF0, 0x48, 0xFF, 0x00,                   // lock inc qword [rax]
            0x58,                                     // pop rax
            0xFF, 0x25, 0x00, 0x00, 0x00, 0x00,       // jmp [rip+0]
        };
        static constexpr int kCounterPatchOff = 3;
        static constexpr int kOrigPatchOff    = sizeof(kTrampTemplate);
        static constexpr int kTrampTotalSize  = sizeof(kTrampTemplate) + 8;

        alignas(64) uintptr_t shadowTable[Offset::SpellEventVMT::VTableEntryCount] = {};
        uintptr_t          originalFn         = 0;
        uintptr_t*         dispatchSlot       = nullptr;   // writable heap slot
        uintptr_t          originalVtable     = 0;
        uintptr_t          trampolineAddr     = 0;
        uintptr_t          prevCasts[kMaxTrackedHeroes] = {};
        volatile long long eventCounter       = 0;         // bumped by shellcode
        long long          lastPolledCount    = 0;
        bool               installed          = false;

        // Find a writable qword in committed memory that currently stores
        // `vtableAbsAddr` — that is the dispatch slot we will swap.
        static uintptr_t* FindDispatchSlot(uintptr_t vtableAbsAddr) {
            MEMORY_BASIC_INFORMATION mbi = {};
            uintptr_t addr = 0x10000;

            while (VirtualQuery(reinterpret_cast<void*>(addr), &mbi, sizeof(mbi))) {
                if (mbi.State == MEM_COMMIT &&
                    (mbi.Protect == PAGE_READWRITE ||
                     mbi.Protect == PAGE_EXECUTE_READWRITE) &&
                    mbi.RegionSize >= sizeof(uintptr_t))
                {
                    const auto* p = static_cast<const uintptr_t*>(mbi.BaseAddress);
                    const size_t count = mbi.RegionSize / sizeof(uintptr_t);

                    __try {
                        for (size_t i = 0; i < count; ++i) {
                            if (p[i] == vtableAbsAddr) {
                                return const_cast<uintptr_t*>(&p[i]);
                            }
                        }
                    } __except (1) { /* skip region */ }
                }
                const auto next = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
                if (next <= addr) break;
                addr = next;
            }
            return nullptr;
        }

        bool Install() {
            if (installed) return true;

            const auto base = Globals::base;
            if (!base) return false;

            const auto vtableAbsAddr = base + Offset::SpellEventVMT::VTableRVA;
            const auto* origTable = reinterpret_cast<const uintptr_t*>(vtableAbsAddr);

            // Sanity: entry at HandlerIndex must be the known wrapper.
            __try {
                if (origTable[Offset::SpellEventVMT::HandlerIndex] !=
                    base + Offset::SpellEventVMT::WrapperRVA) {
                    return false;
                }
            } __except (1) { return false; }

            // Locate the writable dispatch slot on the heap.
            auto* slot = FindDispatchSlot(vtableAbsAddr);
            if (!slot) return false;
            dispatchSlot = slot;

            // Deep-copy the original vtable into our aligned shadow.
            __try {
                memcpy(shadowTable, origTable,
                       sizeof(uintptr_t) * Offset::SpellEventVMT::VTableEntryCount);
            } __except (1) { return false; }

            originalFn     = shadowTable[Offset::SpellEventVMT::HandlerIndex];
            originalVtable = vtableAbsAddr;

            // Build shellcode trampoline.
            trampolineAddr = reinterpret_cast<uintptr_t>(
                VirtualAlloc(nullptr, 64, MEM_COMMIT | MEM_RESERVE,
                             PAGE_EXECUTE_READWRITE));
            if (!trampolineAddr) return false;

            memcpy(reinterpret_cast<void*>(trampolineAddr),
                   kTrampTemplate, sizeof(kTrampTemplate));
            *reinterpret_cast<uintptr_t*>(trampolineAddr + kCounterPatchOff) =
                reinterpret_cast<uintptr_t>(&eventCounter);
            *reinterpret_cast<uintptr_t*>(trampolineAddr + kOrigPatchOff) =
                originalFn;
            FlushInstructionCache(GetCurrentProcess(),
                reinterpret_cast<void*>(trampolineAddr), kTrampTotalSize);

            // Redirect vtable[HandlerIndex] at our trampoline.
            shadowTable[Offset::SpellEventVMT::HandlerIndex] = trampolineAddr;

            // Swap the dispatch slot to our shadow (aligned qword = atomic on x64).
            *dispatchSlot = reinterpret_cast<uintptr_t>(shadowTable);

            // Reset state.
            eventCounter    = 0;
            lastPolledCount = 0;
            memset(prevCasts, 0, sizeof(prevCasts));

            installed = true;
            return true;
        }

        void Uninstall() {
            if (!installed || !dispatchSlot) return;
            *dispatchSlot = originalVtable;
            if (trampolineAddr) {
                VirtualFree(reinterpret_cast<void*>(trampolineAddr), 0, MEM_RELEASE);
                trampolineAddr = 0;
            }
            installed    = false;
            dispatchSlot = nullptr;
        }
    };

    // ── State ──
    inline ShadowVMT g_vmtHook = {};
    inline RawProcessSpellCallback g_processSpellCb = nullptr;

    // Scan heroes for a newly-appeared ActiveSpellCast and fire the callback.
    // Runs after the original handler completes (invoked via PollVmtSpellEvents).
    inline void VmtScanForNewCast() {
        const auto& ctx = CoreRuntime::GetContext();
        if (!Globals::IsValidPtr(ctx.heroManager)) return;

        __try {
            const auto items = Globals::Read<uintptr_t>(
                ctx.heroManager + Offset::ManagerList::Items);
            const auto size  = Globals::Read<int>(
                ctx.heroManager + Offset::ManagerList::Size);
            if (!Globals::IsValidPtr(items) || size <= 0 ||
                size > ShadowVMT::kMaxTrackedHeroes) return;

            for (int i = 0; i < size; ++i) {
                const auto hero = Globals::Read<uintptr_t>(
                    items + i * sizeof(uintptr_t));
                if (!Globals::IsValidPtr(hero)) continue;

                const auto activeCast = Globals::Read<uintptr_t>(
                    hero + Offset::SpellBook::ActiveSpellCast);
                if (!Globals::IsValidPtr(activeCast)) continue;

                if (activeCast != g_vmtHook.prevCasts[i]) {
                    g_vmtHook.prevCasts[i] = activeCast;
                    if (g_processSpellCb) {
                        g_processSpellCb(hero, activeCast);
                    }
                }
            }
        } __except (1) { /* safety */ }
    }

} // namespace detail

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

inline void SetOnProcessSpellCallback(RawProcessSpellCallback cb) {
    detail::g_processSpellCb = cb;
}

inline bool InstallProcessSpellHook() {
    return detail::g_vmtHook.Install();
}

inline bool IsProcessSpellHooked() {
    return detail::g_vmtHook.installed;
}

// Must be called every SDK tick — reads the shellcode-incremented counter
// and fires g_processSpellCb for every hero that just acquired a new cast.
inline void PollVmtSpellEvents() {
    auto& h = detail::g_vmtHook;
    if (!h.installed) return;
    const auto current = h.eventCounter;
    if (current == h.lastPolledCount) return;
    h.lastPolledCount = current;
    detail::VmtScanForNewCast();
}

inline void UninstallAll() {
    detail::g_vmtHook.Uninstall();
}

} // namespace CoreEventHook

