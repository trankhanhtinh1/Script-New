#pragma once

// ============================================================================
// CoreEventHook — Event detour system for all game hooks
// Hooks: OnProcessSpell, OnStopCast, OnFinishCast, OnBuffAdd, OnSpellImpact,
//        OnCreateObject, OnGameUpdate, OnHeroActionStateChange, OnMinionFollowChange
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
#include "spoof/spoofcall.h"

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
// OnFinishCast(obj, castInfo, ???, ???) → void*
using OnFinishCastFn = void*(__fastcall*)(uintptr_t a1, uintptr_t a2, uintptr_t a3, uintptr_t a4);
// OnBuffAdd(buffMgr, ???, flags, ???, int, int) → void*
using OnBuffAddFn = void*(__fastcall*)(uintptr_t a1, uintptr_t a2, uintptr_t a3, uintptr_t a4, int a5, int a6);
// OnSpellImpact(???, ???, ???, ???, double, double, float) → void*
using OnSpellImpactFn = void*(__fastcall*)(uintptr_t a1, uintptr_t a2, uintptr_t a3, uintptr_t a4);
// OnCreateObject(eventMgr, data, flags, objPtr, int, uint64) → void
using OnCreateObjectFn = void(__fastcall*)(uintptr_t a1, uintptr_t a2, uintptr_t a3, uintptr_t a4, int a5, uint64_t a6);
// OnGameUpdate(???, ???, ???, resultPair, flags) → void*
using OnGameUpdateFn = void*(__fastcall*)(uintptr_t a1, uintptr_t a2, uintptr_t a3, uintptr_t a4, int a5);
// OnHeroActionStateChange(???, ???, ???, ???) → void
using OnHeroActionStateChangeFn = void(__fastcall*)(uintptr_t a1, uintptr_t a2, uintptr_t a3, uintptr_t a4);
// OnMinionFollowTargetNetIdChange — same signature
using OnMinionFollowChangeFn = void(__fastcall*)(uintptr_t a1, uintptr_t a2, uintptr_t a3, uintptr_t a4);

// ---------------------------------------------------------------------------
// Callback types — SDK layer registers these
// ---------------------------------------------------------------------------

// Callbacks: raw pointers, SDK wraps them
using RawProcessSpellCallback   = void(*)(uintptr_t senderObj, uintptr_t castInfo);
using RawStopCastCallback       = void(*)(uintptr_t senderObj, uintptr_t castInfo);
using RawFinishCastCallback     = void(*)(uintptr_t senderObj, uintptr_t castInfo);
using RawBuffAddCallback        = void(*)(uintptr_t buffMgr, uintptr_t a2, uintptr_t a3);
using RawSpellImpactCallback    = void(*)(uintptr_t a1, uintptr_t a2, uintptr_t a3);
using RawCreateObjectCallback   = void(*)(uintptr_t objPtr);
using RawGameUpdateCallback     = void(*)();
using RawHeroActionStateCallback = void(*)(uintptr_t a1, uintptr_t a2);
using RawMinionFollowCallback   = void(*)(uintptr_t a1, uintptr_t a2);

// ---------------------------------------------------------------------------
// Minimal x64 Detour — instruction-boundary aware
//
// JMP patch = 14 bytes: FF 25 00 00 00 00 [8-byte addr]
// Must steal enough complete instructions to cover 14 bytes.
//
// Instruction boundaries for OnProcessSpell (0x9362A0):
//   +0  (2B)  push rbx
//   +2  (4B)  sub rsp, 30h
//   +6  (3B)  mov r8, [rdx]
//   +9  (3B)  mov rbx, rdx
//   +12 (7B)  mov rcx, [rcx+0AD8h]   ← 14 bytes falls MID-instruction!
//   +19 (3B)  test r8, r8             ← first clean boundary >= 14
//
// → Must steal 19 bytes, NOP the remaining 5 bytes at target.
// ---------------------------------------------------------------------------
namespace detail {

    // Simple x64 instruction length decoder (covers common prefixes)
    inline int GetInstructionLength(const uint8_t* code) {
        // This is a minimal length-disassembler for x64.
        // Handles REX prefixes, ModRM, SIB, and common opcodes.
        const uint8_t* p = code;

        // Skip prefixes (REX, 66, 67, F2, F3, 40-4F)
        while (*p == 0x66 || *p == 0x67 || *p == 0xF2 || *p == 0xF3 ||
               (*p >= 0x40 && *p <= 0x4F)) {
            ++p;
        }

        uint8_t op = *p++;

        // 1-byte opcodes with no operands
        if (op == 0x90 || op == 0xC3 || op == 0xCC || op == 0xCB) return (int)(p - code);
        // push/pop reg (50-5F)
        if (op >= 0x50 && op <= 0x5F) return (int)(p - code);
        // ret imm16
        if (op == 0xC2) return (int)(p - code) + 2;

        // Jcc short (70-7F), JMP short (EB), LOOP etc
        if ((op >= 0x70 && op <= 0x7F) || op == 0xEB || op == 0xE3) return (int)(p - code) + 1;
        // CALL/JMP near (E8/E9)
        if (op == 0xE8 || op == 0xE9) return (int)(p - code) + 4;

        // Opcodes with ModRM byte
        bool hasModRM = false;
        int immSize = 0;

        // ALU reg,imm8 (83 /r ib)
        if (op == 0x83) { hasModRM = true; immSize = 1; }
        // ALU reg,imm32 (81 /r id)
        else if (op == 0x81) { hasModRM = true; immSize = 4; }
        // MOV reg,imm32 (C7 /0 id)
        else if (op == 0xC7) { hasModRM = true; immSize = 4; }
        // TEST r/m, imm (F7 /0)
        else if (op == 0xF7) { hasModRM = true; immSize = 4; }
        // MOV r/m8,imm8 (C6)
        else if (op == 0xC6) { hasModRM = true; immSize = 1; }
        // Common 2-operand with ModRM, no immediate
        else if ((op >= 0x00 && op <= 0x3F) || (op >= 0x84 && op <= 0x8F) ||
                 (op >= 0x88 && op <= 0x8E) || op == 0xFF || op == 0xF6 ||
                 op == 0x63 || op == 0x69 || op == 0x6B) {
            hasModRM = true;
            if (op == 0x69) immSize = 4;
            if (op == 0x6B) immSize = 1;
        }
        // 0F xx (two-byte opcodes)
        else if (op == 0x0F) {
            uint8_t op2 = *p++;
            // Jcc near (0F 80-8F)
            if (op2 >= 0x80 && op2 <= 0x8F) return (int)(p - code) + 4;
            // MOVZX, MOVSX, TEST, CMOVcc etc — all have ModRM
            hasModRM = true;
        }
        // LEA, MOV r64/r/m64
        else if (op == 0x8D || op == 0x8B || op == 0x89 || op == 0x0B ||
                 op == 0x03 || op == 0x33 || op == 0x3B || op == 0x23 ||
                 op == 0x85 || op == 0x87) {
            hasModRM = true;
        }

        if (hasModRM) {
            uint8_t modrm = *p++;
            uint8_t mod = (modrm >> 6) & 3;
            uint8_t rm  = modrm & 7;

            // SIB byte present?
            if (mod != 3 && rm == 4) ++p; // SIB

            // Displacement
            if (mod == 0 && rm == 5) p += 4;      // [rip+disp32]
            else if (mod == 1)       p += 1;       // [reg+disp8]
            else if (mod == 2)       p += 4;       // [reg+disp32]

            p += immSize;
        }

        int len = (int)(p - code);
        return len > 0 ? len : 1; // safety: never return 0
    }

    // Calculate minimum bytes to steal that covers at least minBytes
    inline int CalcStolenBytes(uintptr_t addr, int minBytes) {
        int total = 0;
        while (total < minBytes) {
            int len = GetInstructionLength(reinterpret_cast<const uint8_t*>(addr + total));
            total += len;
        }
        return total;
    }

    constexpr int kJmpPatchSize = 14; // FF 25 00 00 00 00 + 8-byte addr
    constexpr int kMaxStolen = 32;

    struct Detour {
        uintptr_t targetAddr      = 0;
        int       stolenSize      = 0;
        uint8_t   original[kMaxStolen] = {};
        uintptr_t trampolineAddr  = 0;
        bool      installed       = false;

        bool Install(uintptr_t target, uintptr_t detourFn) {
            if (installed) return true;
            targetAddr = target;

            __try {
                // Calculate how many bytes to steal (instruction-boundary aligned)
                stolenSize = CalcStolenBytes(target, kJmpPatchSize);
                if (stolenSize <= 0 || stolenSize > kMaxStolen) return false;

                // Allocate executable page for trampoline
                trampolineAddr = reinterpret_cast<uintptr_t>(
                    VirtualAlloc(nullptr, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
                if (!trampolineAddr) return false;

                // Save original bytes
                memcpy(original, reinterpret_cast<void*>(targetAddr), stolenSize);

                // Build trampoline: stolen bytes + jmp back to (target + stolenSize)
                memcpy(reinterpret_cast<void*>(trampolineAddr), original, stolenSize);
                auto* tramJmp = reinterpret_cast<uint8_t*>(trampolineAddr + stolenSize);
                tramJmp[0] = 0xFF; tramJmp[1] = 0x25;
                *reinterpret_cast<int32_t*>(tramJmp + 2) = 0;
                *reinterpret_cast<uintptr_t*>(tramJmp + 6) = targetAddr + stolenSize;

                // Patch target: jmp [rip+0] + addr + NOP padding
                DWORD oldProtect = 0;
                VirtualProtect(reinterpret_cast<void*>(targetAddr), stolenSize, PAGE_EXECUTE_READWRITE, &oldProtect);

                // NOP fill first, then write jmp
                memset(reinterpret_cast<void*>(targetAddr), 0x90, stolenSize);
                auto* patch = reinterpret_cast<uint8_t*>(targetAddr);
                patch[0] = 0xFF; patch[1] = 0x25;
                *reinterpret_cast<int32_t*>(patch + 2) = 0;
                *reinterpret_cast<uintptr_t*>(patch + 6) = detourFn;

                VirtualProtect(reinterpret_cast<void*>(targetAddr), stolenSize, oldProtect, &oldProtect);
                FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(targetAddr), stolenSize);

                installed = true;
                return true;
            } __except(1) {
                if (trampolineAddr) {
                    VirtualFree(reinterpret_cast<void*>(trampolineAddr), 0, MEM_RELEASE);
                    trampolineAddr = 0;
                }
                return false;
            }
        }

        void Uninstall() {
            if (!installed) return;

            DWORD oldProtect = 0;
            VirtualProtect(reinterpret_cast<void*>(targetAddr), stolenSize, PAGE_EXECUTE_READWRITE, &oldProtect);
            memcpy(reinterpret_cast<void*>(targetAddr), original, stolenSize);
            VirtualProtect(reinterpret_cast<void*>(targetAddr), stolenSize, oldProtect, &oldProtect);
            FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(targetAddr), stolenSize);

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
    inline Detour g_onProcessSpellDetour    = {};
    inline Detour g_onStopCastDetour        = {};
    inline Detour g_onFinishCastDetour      = {};
    inline Detour g_onBuffAddDetour         = {};
    inline Detour g_onSpellImpactDetour     = {};
    inline Detour g_onCreateObjectDetour    = {};
    inline Detour g_onGameUpdateDetour      = {};
    inline Detour g_onHeroActionStateDetour = {};
    inline Detour g_onMinionFollowDetour    = {};

    inline RawProcessSpellCallback   g_processSpellCb   = nullptr;
    inline RawStopCastCallback       g_stopCastCb       = nullptr;
    inline RawFinishCastCallback     g_finishCastCb     = nullptr;
    inline RawBuffAddCallback        g_buffAddCb        = nullptr;
    inline RawSpellImpactCallback    g_spellImpactCb    = nullptr;
    inline RawCreateObjectCallback   g_createObjectCb   = nullptr;
    inline RawGameUpdateCallback     g_gameUpdateCb     = nullptr;
    inline RawHeroActionStateCallback g_heroActionStateCb = nullptr;
    inline RawMinionFollowCallback   g_minionFollowCb   = nullptr;

    // ── Recover AIBaseClient address from SpellBook pointer ──
    // Game calls: lea rcx, [obj + 0x30E8]  → rcx = SpellBook
    // So sender = SpellBook - 0x30E8
    inline uintptr_t SpellBookToSender(uintptr_t spellBook) {
        return spellBook - Offset::SpellBook::Offset;
    }

    // ── Hook body: OnProcessSpell ──
    // ⚠️ Return address spoofing: call original via spoof_call so Packman
    //    sees a game-module return address instead of our DLL address.
    int __fastcall HkOnProcessSpell(uintptr_t spellBook, uintptr_t castInfo) {
        // Fire our callback BEFORE the original (so scripts see the event first)
        if (g_processSpellCb && castInfo) {
            const uintptr_t sender = SpellBookToSender(spellBook);
            if (Globals::IsValidPtr(sender) && Globals::IsValidPtr(castInfo)) {
                g_processSpellCb(sender, castInfo);
            }
        }

        // Call original game function with return address spoofing
        auto original = g_onProcessSpellDetour.GetOriginal<OnProcessSpellFn>();
        const auto trampoline = CoreRuntime::GetContext().spoofTrampoline;
        if (Globals::IsValidPtr(trampoline)) {
            return spoof_call(
                reinterpret_cast<void*>(trampoline),
                original,
                spellBook, castInfo);
        }
        // Fallback: direct call if spoof trampoline not resolved yet
        return original(spellBook, castInfo);
    }

    // ── Hook body: OnStopCast ──
    // ⚠️ Return address spoofing applied here too.
    void __fastcall HkOnStopCast(uintptr_t a1, uintptr_t a2, uint8_t a3, uintptr_t castInfo, uintptr_t a5) {
        if (g_stopCastCb && castInfo) {
            // a1 in OnStopCast context — need to figure out sender
            // For now pass castInfo; SDK layer resolves SrcIndex from it
            if (Globals::IsValidPtr(castInfo)) {
                g_stopCastCb(0, castInfo);
            }
        }

        // Call original game function with return address spoofing
        auto original = g_onStopCastDetour.GetOriginal<OnStopCastFn>();
        const auto trampoline = CoreRuntime::GetContext().spoofTrampoline;
        if (Globals::IsValidPtr(trampoline)) {
            spoof_call(
                reinterpret_cast<void*>(trampoline),
                original,
                a1, a2, a3, castInfo, a5);
            return;
        }
        // Fallback: direct call if spoof trampoline not resolved yet
        original(a1, a2, a3, castInfo, a5);
    }

    // ── Hook body: OnFinishCast ──
    void* __fastcall HkOnFinishCast(uintptr_t a1, uintptr_t a2, uintptr_t a3, uintptr_t a4) {
        if (g_finishCastCb && Globals::IsValidPtr(a1) && Globals::IsValidPtr(a2)) {
            g_finishCastCb(a1, a2);
        }
        auto original = g_onFinishCastDetour.GetOriginal<OnFinishCastFn>();
        const auto trampoline = CoreRuntime::GetContext().spoofTrampoline;
        if (Globals::IsValidPtr(trampoline)) {
            return spoof_call(reinterpret_cast<void*>(trampoline), original, a1, a2, a3, a4);
        }
        return original(a1, a2, a3, a4);
    }

    // ── Hook body: OnBuffAdd ──
    void* __fastcall HkOnBuffAdd(uintptr_t a1, uintptr_t a2, uintptr_t a3, uintptr_t a4, int a5, int a6) {
        if (g_buffAddCb && Globals::IsValidPtr(a1)) {
            g_buffAddCb(a1, a2, a3);
        }
        auto original = g_onBuffAddDetour.GetOriginal<OnBuffAddFn>();
        const auto trampoline = CoreRuntime::GetContext().spoofTrampoline;
        if (Globals::IsValidPtr(trampoline)) {
            return spoof_call(reinterpret_cast<void*>(trampoline), original, a1, a2, a3, a4, a5, a6);
        }
        return original(a1, a2, a3, a4, a5, a6);
    }

    // ── Hook body: OnSpellImpact ──
    void* __fastcall HkOnSpellImpact(uintptr_t a1, uintptr_t a2, uintptr_t a3, uintptr_t a4) {
        if (g_spellImpactCb && Globals::IsValidPtr(a1)) {
            g_spellImpactCb(a1, a2, a3);
        }
        auto original = g_onSpellImpactDetour.GetOriginal<OnSpellImpactFn>();
        const auto trampoline = CoreRuntime::GetContext().spoofTrampoline;
        if (Globals::IsValidPtr(trampoline)) {
            return spoof_call(reinterpret_cast<void*>(trampoline), original, a1, a2, a3, a4);
        }
        return original(a1, a2, a3, a4);
    }

    // ── Hook body: OnCreateObject ──
    void __fastcall HkOnCreateObject(uintptr_t a1, uintptr_t a2, uintptr_t a3, uintptr_t a4, int a5, uint64_t a6) {
        auto original = g_onCreateObjectDetour.GetOriginal<OnCreateObjectFn>();
        const auto trampoline = CoreRuntime::GetContext().spoofTrampoline;
        if (Globals::IsValidPtr(trampoline)) {
            spoof_call(reinterpret_cast<void*>(trampoline), original, a1, a2, a3, a4, a5, a6);
        } else {
            original(a1, a2, a3, a4, a5, a6);
        }
        // Fire callback AFTER original so the object is fully initialized
        if (g_createObjectCb && Globals::IsValidPtr(a4)) {
            g_createObjectCb(a4);
        }
    }

    // ── Hook body: OnGameUpdate ──
    void* __fastcall HkOnGameUpdate(uintptr_t a1, uintptr_t a2, uintptr_t a3, uintptr_t a4, int a5) {
        if (g_gameUpdateCb) {
            g_gameUpdateCb();
        }
        auto original = g_onGameUpdateDetour.GetOriginal<OnGameUpdateFn>();
        const auto trampoline = CoreRuntime::GetContext().spoofTrampoline;
        if (Globals::IsValidPtr(trampoline)) {
            return spoof_call(reinterpret_cast<void*>(trampoline), original, a1, a2, a3, a4, a5);
        }
        return original(a1, a2, a3, a4, a5);
    }

    // ── Hook body: OnHeroActionStateChange ──
    void __fastcall HkOnHeroActionStateChange(uintptr_t a1, uintptr_t a2, uintptr_t a3, uintptr_t a4) {
        if (g_heroActionStateCb) {
            g_heroActionStateCb(a1, a2);
        }
        auto original = g_onHeroActionStateDetour.GetOriginal<OnHeroActionStateChangeFn>();
        const auto trampoline = CoreRuntime::GetContext().spoofTrampoline;
        if (Globals::IsValidPtr(trampoline)) {
            spoof_call(reinterpret_cast<void*>(trampoline), original, a1, a2, a3, a4);
            return;
        }
        original(a1, a2, a3, a4);
    }

    // ── Hook body: OnMinionFollowTargetNetIdChange ──
    void __fastcall HkOnMinionFollowChange(uintptr_t a1, uintptr_t a2, uintptr_t a3, uintptr_t a4) {
        if (g_minionFollowCb) {
            g_minionFollowCb(a1, a2);
        }
        auto original = g_onMinionFollowDetour.GetOriginal<OnMinionFollowChangeFn>();
        const auto trampoline = CoreRuntime::GetContext().spoofTrampoline;
        if (Globals::IsValidPtr(trampoline)) {
            spoof_call(reinterpret_cast<void*>(trampoline), original, a1, a2, a3, a4);
            return;
        }
        original(a1, a2, a3, a4);
    }

} // namespace detail

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

inline void SetOnProcessSpellCallback(RawProcessSpellCallback cb) { detail::g_processSpellCb = cb; }
inline void SetOnStopCastCallback(RawStopCastCallback cb)       { detail::g_stopCastCb = cb; }
inline void SetOnFinishCastCallback(RawFinishCastCallback cb)   { detail::g_finishCastCb = cb; }
inline void SetOnBuffAddCallback(RawBuffAddCallback cb)         { detail::g_buffAddCb = cb; }
inline void SetOnSpellImpactCallback(RawSpellImpactCallback cb) { detail::g_spellImpactCb = cb; }
inline void SetOnCreateObjectCallback(RawCreateObjectCallback cb) { detail::g_createObjectCb = cb; }
inline void SetOnGameUpdateCallback(RawGameUpdateCallback cb)   { detail::g_gameUpdateCb = cb; }
inline void SetOnHeroActionStateCallback(RawHeroActionStateCallback cb) { detail::g_heroActionStateCb = cb; }
inline void SetOnMinionFollowCallback(RawMinionFollowCallback cb) { detail::g_minionFollowCb = cb; }

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
    return detail::g_onStopCastDetour.Install(base + Offset::Function::OnStopCast,
        reinterpret_cast<uintptr_t>(&detail::HkOnStopCast));
}

inline bool InstallFinishCastHook() {
    const uintptr_t base = Globals::base;
    if (!base) return false;
    return detail::g_onFinishCastDetour.Install(base + Offset::Function::OnFinishCast,
        reinterpret_cast<uintptr_t>(&detail::HkOnFinishCast));
}

inline bool InstallBuffAddHook() {
    const uintptr_t base = Globals::base;
    if (!base) return false;
    return detail::g_onBuffAddDetour.Install(base + Offset::Function::OnBuffAdd,
        reinterpret_cast<uintptr_t>(&detail::HkOnBuffAdd));
}

inline bool InstallSpellImpactHook() {
    const uintptr_t base = Globals::base;
    if (!base) return false;
    return detail::g_onSpellImpactDetour.Install(base + Offset::Function::OnSpellImpact,
        reinterpret_cast<uintptr_t>(&detail::HkOnSpellImpact));
}

inline bool InstallCreateObjectHook() {
    const uintptr_t base = Globals::base;
    if (!base) return false;
    return detail::g_onCreateObjectDetour.Install(base + Offset::Function::OnCreateObject,
        reinterpret_cast<uintptr_t>(&detail::HkOnCreateObject));
}

inline bool InstallGameUpdateHook() {
    const uintptr_t base = Globals::base;
    if (!base) return false;
    return detail::g_onGameUpdateDetour.Install(base + Offset::Function::OnGameUpdate,
        reinterpret_cast<uintptr_t>(&detail::HkOnGameUpdate));
}

inline bool InstallHeroActionStateHook() {
    const uintptr_t base = Globals::base;
    if (!base) return false;
    return detail::g_onHeroActionStateDetour.Install(
        base + Offset::EventPropertyRuntime::OnHeroActionStateChange,
        reinterpret_cast<uintptr_t>(&detail::HkOnHeroActionStateChange));
}

inline bool InstallMinionFollowHook() {
    const uintptr_t base = Globals::base;
    if (!base) return false;
    return detail::g_onMinionFollowDetour.Install(
        base + Offset::EventPropertyRuntime::OnMinionFollowTargetNetIdChange,
        reinterpret_cast<uintptr_t>(&detail::HkOnMinionFollowChange));
}

inline void UninstallAll() {
    detail::g_onProcessSpellDetour.Uninstall();
    detail::g_onStopCastDetour.Uninstall();
    detail::g_onFinishCastDetour.Uninstall();
    detail::g_onBuffAddDetour.Uninstall();
    detail::g_onSpellImpactDetour.Uninstall();
    detail::g_onCreateObjectDetour.Uninstall();
    detail::g_onGameUpdateDetour.Uninstall();
    detail::g_onHeroActionStateDetour.Uninstall();
    detail::g_onMinionFollowDetour.Uninstall();
}

inline bool IsProcessSpellHooked()    { return detail::g_onProcessSpellDetour.installed; }
inline bool IsStopCastHooked()        { return detail::g_onStopCastDetour.installed; }
inline bool IsFinishCastHooked()      { return detail::g_onFinishCastDetour.installed; }
inline bool IsBuffAddHooked()         { return detail::g_onBuffAddDetour.installed; }
inline bool IsSpellImpactHooked()     { return detail::g_onSpellImpactDetour.installed; }
inline bool IsCreateObjectHooked()    { return detail::g_onCreateObjectDetour.installed; }
inline bool IsGameUpdateHooked()      { return detail::g_onGameUpdateDetour.installed; }
inline bool IsHeroActionStateHooked() { return detail::g_onHeroActionStateDetour.installed; }
inline bool IsMinionFollowHooked()    { return detail::g_onMinionFollowDetour.installed; }

} // namespace CoreEventHook
