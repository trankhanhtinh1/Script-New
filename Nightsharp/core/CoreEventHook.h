#pragma once

// ============================================================================
// CoreEventHook — Minimal detour for OnProcessSpell / OnStopCast / OnFinishCast
//
// Reversed from IDA (binary dump):
//   OnProcessSpell @ 0x9362A0:
//     rcx = SpellBook* (AIBaseClient + 0x30E8)
//     rdx = SpellCastInfo*
//     Reads: SpellData[0x0], IsAuto[0x141], Slot[0x14C], SpellData->0x60
//     Returns: int (0 = success, 2 = no SpellData)
//
//   Callers (verified):
//     sub_30CB30 @ 30CE4D: lea rcx,[rdi+30E8h]; mov rdx,rbx; call OnProcessSpell
//     sub_2DC8E0 @ 2DCB88: lea rcx,[rbx+30E8h]; mov rdx,rbp; call OnProcessSpell
//
// Usage:
//   CoreEventHook::Install();   // after SDK::Bootstrap::Init
//   CoreEventHook::Uninstall(); // on unload
// ============================================================================

#include "CoreRuntime.h"
#include "CoreSpellCastInfo.h"
#include "Offsets.h"

#include <cstdint>
#include <cstring>

namespace CoreEventHook {

// ---------------------------------------------------------------------------
// Function typedefs (from IDA reverse)
// ---------------------------------------------------------------------------

// OnProcessSpell(SpellBook*, SpellCastInfo*) → int
using OnProcessSpellFn = int(__fastcall*)(uintptr_t spellBook, uintptr_t castInfo);

// OnStopCast(???, ???, byte, SpellCastInfo*, ...) → void
using OnStopCastFn = void(__fastcall*)(uintptr_t a1, uintptr_t a2, uint8_t a3, uintptr_t castInfo, uintptr_t a5);

// ---------------------------------------------------------------------------
// Callback types — SDK layer registers these
// ---------------------------------------------------------------------------

// Callback: (senderAddress, castInfoAddress) — raw pointers, SDK wraps them
using RawProcessSpellCallback = void(*)(uintptr_t senderObj, uintptr_t castInfo);
using RawStopCastCallback     = void(*)(uintptr_t senderObj, uintptr_t castInfo);

// ---------------------------------------------------------------------------
// Minimal x64 Detour (14-byte absolute jmp)
// ---------------------------------------------------------------------------
namespace detail {

    struct Detour {
        uintptr_t targetAddr   = 0;           // address to hook
        uint8_t   original[16] = {};           // saved original bytes
        uint8_t   trampoline[32] = {};         // original bytes + jmp back
        uintptr_t trampolineAddr = 0;          // executable trampoline page
        bool      installed = false;

        bool Install(uintptr_t target, uintptr_t detourFn) {
            if (installed) return true;
            targetAddr = target;

            // Allocate executable page for trampoline
            trampolineAddr = reinterpret_cast<uintptr_t>(
                VirtualAlloc(nullptr, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
            if (!trampolineAddr) return false;

            // Save original 14 bytes
            memcpy(original, reinterpret_cast<void*>(targetAddr), 14);

            // Build trampoline: original bytes + jmp back to (target + 14)
            memcpy(reinterpret_cast<void*>(trampolineAddr), original, 14);
            auto* tramJmp = reinterpret_cast<uint8_t*>(trampolineAddr + 14);
            tramJmp[0] = 0xFF; tramJmp[1] = 0x25; // jmp [rip+0]
            *reinterpret_cast<int32_t*>(tramJmp + 2) = 0;
            *reinterpret_cast<uintptr_t*>(tramJmp + 6) = targetAddr + 14;

            // Patch target: mov rax, detourFn; jmp rax
            DWORD oldProtect = 0;
            VirtualProtect(reinterpret_cast<void*>(targetAddr), 14, PAGE_EXECUTE_READWRITE, &oldProtect);

            auto* patch = reinterpret_cast<uint8_t*>(targetAddr);
            patch[0] = 0xFF; patch[1] = 0x25; // jmp [rip+0]
            *reinterpret_cast<int32_t*>(patch + 2) = 0;
            *reinterpret_cast<uintptr_t*>(patch + 6) = detourFn;

            VirtualProtect(reinterpret_cast<void*>(targetAddr), 14, oldProtect, &oldProtect);

            installed = true;
            return true;
        }

        void Uninstall() {
            if (!installed) return;

            // Restore original bytes
            DWORD oldProtect = 0;
            VirtualProtect(reinterpret_cast<void*>(targetAddr), 14, PAGE_EXECUTE_READWRITE, &oldProtect);
            memcpy(reinterpret_cast<void*>(targetAddr), original, 14);
            VirtualProtect(reinterpret_cast<void*>(targetAddr), 14, oldProtect, &oldProtect);

            // Free trampoline
            if (trampolineAddr) {
                VirtualFree(reinterpret_cast<void*>(trampolineAddr), 0, MEM_RELEASE);
                trampolineAddr = 0;
            }

            installed = false;
        }

        template<typename T>
        T GetOriginal() const {
            return reinterpret_cast<T>(trampolineAddr);
        }
    };

    // ── State ──
    inline Detour g_onProcessSpellDetour = {};
    inline Detour g_onStopCastDetour     = {};

    inline RawProcessSpellCallback g_processSpellCb = nullptr;
    inline RawStopCastCallback     g_stopCastCb     = nullptr;

    // ── Recover AIBaseClient address from SpellBook pointer ──
    // Game calls: lea rcx, [obj + 0x30E8]  → rcx = SpellBook
    // So sender = SpellBook - 0x30E8
    inline uintptr_t SpellBookToSender(uintptr_t spellBook) {
        return spellBook - Offset::SpellBook::Offset;
    }

    // ── Hook body: OnProcessSpell ──
    int __fastcall HkOnProcessSpell(uintptr_t spellBook, uintptr_t castInfo) {
        // Fire our callback BEFORE the original (so scripts see the event first)
        if (g_processSpellCb && castInfo) {
            const uintptr_t sender = SpellBookToSender(spellBook);
            if (Globals::IsValidPtr(sender) && Globals::IsValidPtr(castInfo)) {
                g_processSpellCb(sender, castInfo);
            }
        }

        // Call original game function
        auto original = g_onProcessSpellDetour.GetOriginal<OnProcessSpellFn>();
        return original(spellBook, castInfo);
    }

    // ── Hook body: OnStopCast ──
    void __fastcall HkOnStopCast(uintptr_t a1, uintptr_t a2, uint8_t a3, uintptr_t castInfo, uintptr_t a5) {
        if (g_stopCastCb && castInfo) {
            // a1 in OnStopCast context — need to figure out sender
            // For now pass castInfo; SDK layer resolves SrcIndex from it
            if (Globals::IsValidPtr(castInfo)) {
                g_stopCastCb(0, castInfo);
            }
        }

        auto original = g_onStopCastDetour.GetOriginal<OnStopCastFn>();
        original(a1, a2, a3, castInfo, a5);
    }

} // namespace detail

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

inline void SetOnProcessSpellCallback(RawProcessSpellCallback cb) {
    detail::g_processSpellCb = cb;
}

inline void SetOnStopCastCallback(RawStopCastCallback cb) {
    detail::g_stopCastCb = cb;
}

inline bool InstallProcessSpellHook() {
    const uintptr_t base = Globals::base;
    if (!base) return false;

    const uintptr_t target = base + Offset::Function::OnProcessSpell;
    return detail::g_onProcessSpellDetour.Install(
        target,
        reinterpret_cast<uintptr_t>(&detail::HkOnProcessSpell));
}

inline bool InstallStopCastHook() {
    const uintptr_t base = Globals::base;
    if (!base) return false;

    const uintptr_t target = base + Offset::Function::OnStopCast;
    return detail::g_onStopCastDetour.Install(
        target,
        reinterpret_cast<uintptr_t>(&detail::HkOnStopCast));
}

inline void UninstallAll() {
    detail::g_onProcessSpellDetour.Uninstall();
    detail::g_onStopCastDetour.Uninstall();
}

inline bool IsProcessSpellHooked() { return detail::g_onProcessSpellDetour.installed; }
inline bool IsStopCastHooked()     { return detail::g_onStopCastDetour.installed; }

} // namespace CoreEventHook
