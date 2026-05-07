#pragma once
// ============================================================================
// CoreEventHook — Multi-event hook manager.
// ============================================================================
// TWO mechanisms are implemented side-by-side:
//
//   A. Shadow-VMT swap  — the ONLY reliable hook on the current Riot Packman /
//      Vanguard build (no code page writes, no inline detours). Used for
//      OnProcessSpell because the game has a dedicated vtable slot whose
//      entry is ALWAYS executed when a spell-cast packet arrives from server.
//      Confirmed working on LoL 26.6.
//
//   B. State polling    — everything else. Packet handlers for stop-cast,
//      buff-add, buff-remove, auto-attack, death, level-up, … do NOT have
//      their own dedicated (hookable) vtable slot in this build, and any
//      inline detour on a static helper (`IssueOrder` @ 0x2AFE10, `CastSpell`
//      @ 0xBDD6B0, …) is immediately flagged by the packer's code-integrity
//      scan. The only tamper-proof alternative is to diff game state every
//      SDK tick and fire a synthetic event whenever a transition matches.
//
// Public API (all ‘inline’ so this header is drop-in):
//
//   Registration:
//       SetCallback(Offset::Events::Id, Callback);
//       GetCallback(Offset::Events::Id);       // for debugging
//       ClearCallback(Offset::Events::Id);
//
//   Lifecycle (call once after Globals::base / hero-manager become valid):
//       InstallAllHooks();
//       PollAllEvents();                       // every SDK tick
//       UninstallAll();                        // on unload
//
//   Legacy helpers (preserved so existing SDK glue keeps working):
//       SetOnProcessSpellCallback / InstallProcessSpellHook /
//       IsProcessSpellHooked / PollVmtSpellEvents / UninstallAll
//
// Re-verification notes live inline in offset.h (SpellEventVMT block).
// ============================================================================

#include "offset.h"
#include "StealthBuffCatalog.h"
// spoof/spoofcall.h include removed — `spoof_call` was only used from the
// (now-deleted) dephook fallback branches. Re-add this include if the
// spoof-call path is ever re-enabled alongside a new carrier.

#include <Windows.h>
#include <TlHelp32.h>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>

// ----------------------------------------------------------------------------
// Lightweight shims — the legacy core/*.h tree is gone, so any helpers the
// hook used to borrow are reproduced locally here to keep this header
// self-contained. These are deliberately minimal.
// ----------------------------------------------------------------------------
namespace CoreEventHook {

namespace shim {

    // LoL.exe base address (injected DLL shares the main-exe module handle).
    inline uintptr_t GetGameBase() {
        static uintptr_t cached = 0;
        if (!cached) {
            cached = reinterpret_cast<uintptr_t>(GetModuleHandleA(nullptr));
        }
        return cached;
    }

    // SEH-guarded pointer sanity check (page must be committed + readable).
    inline bool IsValidPtr(uintptr_t p) {
        if (p < 0x10000 || p > 0x7FFFFFFFFFFF) return false;
        MEMORY_BASIC_INFORMATION mbi{};
        if (!VirtualQuery(reinterpret_cast<void*>(p), &mbi, sizeof(mbi))) return false;
        if (mbi.State != MEM_COMMIT) return false;
        constexpr DWORD kReadable =
            PAGE_READONLY | PAGE_READWRITE |
            PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE |
            PAGE_WRITECOPY | PAGE_EXECUTE_WRITECOPY;
        return (mbi.Protect & kReadable) != 0;
    }

    template <typename T>
    inline T SafeRead(uintptr_t p, T fallback = {}) {
        if (!IsValidPtr(p)) return fallback;
        __try { return *reinterpret_cast<T*>(p); } __except (1) { return fallback; }
    }

    inline uintptr_t ReadPtr(uintptr_t p) { return SafeRead<uintptr_t>(p, 0); }
    inline int       ReadInt(uintptr_t p) { return SafeRead<int>(p, 0); }
    inline float     ReadFloat(uintptr_t p) { return SafeRead<float>(p, 0.0f); }
    inline uint8_t   ReadByte(uintptr_t p) { return SafeRead<uint8_t>(p, 0); }

    // ------------------------------------------------------------------------
    // DecodeAiMgr — decrypt the AiManager wrapper pointer from a hero
    // ------------------------------------------------------------------------
    // Riot stores a small obfuscation table at `hero + 0x4228`. Game code
    // calls `sub_2A9F10` (`GetAiManager`) / `sub_2AB530` (`GetAiManagerInner`)
    // to decode it. We reimplement the decode inline so we don't have to
    // call a game function from an arbitrary thread.
    //
    // Decode procedure (matches IDA decompile of sub_29CE70):
    //   table   = hero + 0x4228
    //   len     = table[41]                      // scalar-loop count
    //   flag    = table[42]                      // byte-fixup length
    //   seedIdx = table[43]                      // selects initial seed
    //   result  = *(qword*)&table[8 + 8*seedIdx] // starting seed
    //   if (len > 0) result ^= ~*(qword*)&table[0]   // only v17[0] matters
    //   if (flag)    for (i = 8-flag .. 7) result_bytes[i] ^= ~table[i]
    //   return result           // this is the WRAPPER pointer
    //
    // InnerManager (what callers actually use) is at wrapper + 0x10.
    inline uintptr_t DecodeAiMgr(uintptr_t hero) {
        if (!IsValidPtr(hero)) return 0;
        const auto table = hero + Offset::AiManagerInnerCompatLayout::Offset;
        if (!IsValidPtr(table)) return 0;

        const uint8_t len     = ReadByte(table + 41);
        const uint8_t flag    = ReadByte(table + 42);
        const uint8_t seedIdx = ReadByte(table + 43);

        uint64_t result = SafeRead<uint64_t>(table + 8 + 8 * seedIdx, 0);
        if (!result) return 0;

        if (len > 0) {
            const uint64_t q0 = SafeRead<uint64_t>(table, 0);
            result ^= ~q0;
        }
        if (flag) {
            uint8_t* bytes = reinterpret_cast<uint8_t*>(&result);
            for (int v14 = 8 - flag; v14 < 8; ++v14) {
                const uint8_t b = ReadByte(table + v14);
                bytes[v14] ^= static_cast<uint8_t>(~b);
            }
        }

        const auto wrapper = static_cast<uintptr_t>(result);
        if (!IsValidPtr(wrapper)) return 0;

        // Dereference +0x10 → real inner manager (what GetAiManagerInner does).
        const auto inner = ReadPtr(wrapper + Offset::AiManagerInnerCompatLayout::InnerManager);
        return IsValidPtr(inner) ? inner : 0;
    }

    inline uintptr_t ResolveAiMgrNavBase(uintptr_t hero) {
        const auto inner = DecodeAiMgr(hero);
        if (!IsValidPtr(inner)) return 0;

        const auto typePtr = ReadPtr(inner + Offset::AiManagerNavBaseLayout::InnerTypePtr);
        if (!IsValidPtr(typePtr)) return 0;

        const int adjust = ReadInt(typePtr + Offset::AiManagerNavBaseLayout::InnerTypeAdjust);
        const auto navBase = static_cast<uintptr_t>(
            static_cast<intptr_t>(inner) + static_cast<intptr_t>(adjust) +
            static_cast<intptr_t>(Offset::AiManagerNavBaseLayout::FinalBaseAdd));
        return IsValidPtr(navBase) ? navBase : 0;
    }

} // namespace shim

// ----------------------------------------------------------------------------
// Crash-resistant debug logger — writes to `C:\Users\Public\nightsharp_dephook.txt`.
// ----------------------------------------------------------------------------
// PURE Win32 (CreateFile/WriteFile/CloseHandle). No C stdio anywhere. Why:
//   * `fopen`/`fflush`/`_commit` depend on the CRT being fully initialised
//     and on thread-local storage. Calling them from DllMain or from a VEH
//     handler can hit a half-initialised CRT state and crash.
//   * Closing the file after every line is "slow" but every line is fsync'd
//     — a hard crash cannot lose log output (CloseHandle flushes to OS).
//
// Same pattern `ns_stage.txt` / `ns_orbwalker_debug.txt` already use.
// ----------------------------------------------------------------------------
namespace dbglog {

    inline constexpr const char* kLogPath =
        "C:\\Users\\Public\\nightsharp_dephook.txt";

    // ── RUNTIME PERFORMANCE GATE (May/2026) ─────────────────────────────────
    // Every Log() call opens + writes + closes the log file (CloseHandle ==
    // flush for crash safety). That costs ~1-3 ms per call, and the VMT-poll
    // hot path fires 20-30 logs/second during combat (per-buff, per-cast,
    // per-hero), producing the 47-78 ms EventHookPoll spikes the user saw
    // in overlay profiling. Turn it OFF by default — flip to `true` only
    // when actively debugging OnProcessSpell / OnBuffUpdate plumbing.
    inline constexpr bool kLogEnabled = false;

    inline CRITICAL_SECTION g_cs{};
    inline bool             g_csInit = false;

    inline void WriteRaw(const char* bytes, int len) {
        if (len <= 0) return;
        HANDLE h = CreateFileA(
            kLogPath,
            FILE_APPEND_DATA,
            FILE_SHARE_READ,
            nullptr,
            OPEN_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);
        if (h == INVALID_HANDLE_VALUE) return;
        DWORD written = 0;
        WriteFile(h, bytes, (DWORD)len, &written, nullptr);
        CloseHandle(h);  // Close == flush to OS cache; crash-safe.
    }

    inline void Log(const char* fmt, ...) {
        if constexpr (!kLogEnabled) {
            // Compile-time elided when logging is disabled — zero cost in
            // the poll hot path. Consumers that still need the formatted
            // string (none on this build) would have to branch separately.
            (void)fmt;
            return;
        }

        char buf[768];

        SYSTEMTIME st{};
        GetLocalTime(&st);
        int n = _snprintf_s(buf, sizeof(buf), _TRUNCATE,
            "[%02d:%02d:%02d.%03d T%u] ",
            st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
            (unsigned)GetCurrentThreadId());
        if (n < 0) n = 0;

        va_list args;
        va_start(args, fmt);
        int m = _vsnprintf_s(buf + n, sizeof(buf) - n, _TRUNCATE, fmt, args);
        va_end(args);
        if (m < 0) m = 0;

        int total = n + m;
        if (total > (int)sizeof(buf) - 3) total = (int)sizeof(buf) - 3;
        buf[total++] = '\r';
        buf[total++] = '\n';

        if (!g_csInit) {
            InitializeCriticalSection(&g_cs);
            g_csInit = true;
        }
        EnterCriticalSection(&g_cs);
        WriteRaw(buf, total);
        LeaveCriticalSection(&g_cs);
    }

} // namespace dbglog

// ----------------------------------------------------------------------------
// Direct-syscall bypass for Riot Packman (stub.dll).
// ----------------------------------------------------------------------------
// Packman's userland layer hooks `NtProtectVirtualMemory` in ntdll.dll so any
// attempt to flip a LoL.exe code page to RWX (which we need to install inline
// detours) returns STATUS_ACCESS_DENIED. We sidestep that by:
//
//   1. Reading the syscall number (SSN) from a PRISTINE ntdll.dll loaded
//      straight off disk — Packman can't patch a file mapping we open fresh.
//   2. Building a 11-byte shellcode stub that issues the syscall directly:
//            mov r10, rcx
//            mov eax, <ssn>
//            syscall
//            ret
//      and calling it in place of VirtualProtect.
//
// This is the same trick game-hacking "direct-syscall" loaders use to evade
// EDR hooks. It doesn't emulate Packman — it simply avoids the entry point
// where Packman installed its interceptor.
// ----------------------------------------------------------------------------
namespace stealth {

    using NtProtectFn = LONG(NTAPI*)(HANDLE, PVOID*, PSIZE_T, ULONG, PULONG);
    // NtWriteVirtualMemory(HANDLE, PVOID, PVOID, SIZE_T, PSIZE_T)
    using NtWriteFn   = LONG(NTAPI*)(HANDLE, PVOID, PVOID, SIZE_T, PSIZE_T);

    // Parse PE in `buf` (raw file image), find export by name, and return
    // the syscall number hard-coded in the stub's `mov eax, imm32`. -1 on fail.
    // Generalised from a hard-coded "NtProtectVirtualMemory" string so we can
    // resolve any ntdll syscall (NtWriteVirtualMemory etc.).
    inline int ExtractSSNFromImageByName(const uint8_t* buf, const char* funcName) {
        const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(buf);
        if (dos->e_magic != IMAGE_DOS_SIGNATURE) return -1;
        const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(buf + dos->e_lfanew);
        if (nt->Signature != IMAGE_NT_SIGNATURE) return -1;

        const auto& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
        if (!dir.VirtualAddress) return -1;

        auto rvaToOff = [&](DWORD rva) -> DWORD {
            const auto* sec = IMAGE_FIRST_SECTION(nt);
            for (int i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++sec) {
                const DWORD vEnd = sec->VirtualAddress + sec->Misc.VirtualSize;
                if (rva >= sec->VirtualAddress && rva < vEnd)
                    return rva - sec->VirtualAddress + sec->PointerToRawData;
            }
            return 0;
        };

        const auto* exp = reinterpret_cast<const IMAGE_EXPORT_DIRECTORY*>(
            buf + rvaToOff(dir.VirtualAddress));
        const auto* names = reinterpret_cast<const DWORD*>(buf + rvaToOff(exp->AddressOfNames));
        const auto* ords  = reinterpret_cast<const WORD*>(buf + rvaToOff(exp->AddressOfNameOrdinals));
        const auto* funcs = reinterpret_cast<const DWORD*>(buf + rvaToOff(exp->AddressOfFunctions));

        for (DWORD i = 0; i < exp->NumberOfNames; ++i) {
            const auto* s = reinterpret_cast<const char*>(buf + rvaToOff(names[i]));
            if (std::strcmp(s, funcName) == 0) {
                const auto* stub = buf + rvaToOff(funcs[ords[i]]);
                // Canonical Win10/11 stub: 4C 8B D1 B8 <SSN:u32>
                if (stub[0] == 0x4C && stub[1] == 0x8B && stub[2] == 0xD1 && stub[3] == 0xB8)
                    return (int)*reinterpret_cast<const uint32_t*>(stub + 4);
                return -1;
            }
        }
        return -1;
    }

    // Convenience: keep the old name for Protect resolution
    inline int ExtractSSNFromImage(const uint8_t* buf) {
        return ExtractSSNFromImageByName(buf, "NtProtectVirtualMemory");
    }

    // Fallback: if Packman didn't hook the in-memory stub, just read it live.
    inline int ResolveSSNInMemory() {
        HMODULE h = GetModuleHandleW(L"ntdll.dll");
        if (!h) return -1;
        auto* p = reinterpret_cast<const uint8_t*>(
            GetProcAddress(h, "NtProtectVirtualMemory"));
        if (!p) return -1;
        if (p[0] == 0x4C && p[1] == 0x8B && p[2] == 0xD1 && p[3] == 0xB8)
            return (int)*reinterpret_cast<const uint32_t*>(p + 4);
        return -1;  // hooked
    }

    // Pristine read from disk — Packman can't touch this mapping.
    inline int ResolveSSNFromDisk() {
        HANDLE hFile = CreateFileW(L"\\\\?\\C:\\Windows\\System32\\ntdll.dll",
            GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
        if (hFile == INVALID_HANDLE_VALUE) return -1;

        LARGE_INTEGER sz{};
        if (!GetFileSizeEx(hFile, &sz) || sz.QuadPart <= 0 || sz.QuadPart > (64 << 20)) {
            CloseHandle(hFile); return -1;
        }
        auto* buf = reinterpret_cast<uint8_t*>(VirtualAlloc(nullptr, (SIZE_T)sz.QuadPart,
            MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
        if (!buf) { CloseHandle(hFile); return -1; }

        DWORD total = 0, chunk = 0;
        while (total < sz.QuadPart &&
               ReadFile(hFile, buf + total, (DWORD)(sz.QuadPart - total), &chunk, nullptr) &&
               chunk > 0) {
            total += chunk;
        }
        CloseHandle(hFile);

        const int ssn = (total == sz.QuadPart) ? ExtractSSNFromImage(buf) : -1;
        VirtualFree(buf, 0, MEM_RELEASE);
        return ssn;
    }

    // Cached syscall stub. Built on first use.
    inline void*       g_stubPtr       = nullptr;
    inline NtProtectFn g_ntProtect     = nullptr;
    inline int         g_ssnCached     = -1;
    // Last NTSTATUS returned by the syscall stub (for diagnostics).
    inline volatile LONG g_lastStatus  = 0;
    // Source of the resolved SSN: 0=not resolved, 1=in-memory, 2=disk.
    inline volatile int  g_ssnSource   = 0;
    // Self-test status:
    //   0 = not run, 1 = PAGE_EXECUTE_READWRITE ok,
    //   2 = only PAGE_READONLY ok (Packman blocks +W), 3 = both blocked.
    inline volatile int  g_selfTest    = 0;
    // NTSTATUS from the two self-test calls (for diagnostics display).
    inline volatile LONG g_selfTestStatusRWX = 0;
    inline volatile LONG g_selfTestStatusRO  = 0;

    // ── NtWriteVirtualMemory direct-syscall stub ──
    //
    // `NtWriteVirtualMemory` takes the `MmCopyVirtualMemory` code path in the
    // kernel, which does NOT enforce the `STATUS_SECTION_PROTECTION` check that
    // `NtProtectVirtualMemory` hits when we try to flip an image section to
    // RWX. So we can write directly to LoL.exe code pages without ever touching
    // protection — exactly what an inline detour needs, minus the flag check.
    //
    // If that bypass holds on this OS build, `InlineDetour::Install` can skip
    // the VirtualProtect dance entirely and succeed on all 8 events.
    inline void*     g_writeStubPtr  = nullptr;
    inline NtWriteFn g_ntWrite       = nullptr;
    inline int       g_writeSsn      = -1;
    inline volatile LONG g_writeLastStatus    = 0;
    // Self-test for NtWriteVirtualMemory: 0 = not tested, 1 = works, 2 = fails.
    inline volatile int  g_writeSelfTest      = 0;
    inline volatile LONG g_writeSelfTestStatus = 0;

    inline int ResolveSSN() {
        int ssn = ResolveSSNInMemory();
        if (ssn >= 0) { g_ssnSource = 1; return ssn; }
        ssn = ResolveSSNFromDisk();
        if (ssn >= 0) { g_ssnSource = 2; return ssn; }
        g_ssnSource = 0;
        return -1;
    }

    inline bool Init() {
        if (g_ntProtect) return true;
        const int ssn = ResolveSSN();
        if (ssn < 0) return false;
        g_ssnCached = ssn;
        auto* stub = reinterpret_cast<uint8_t*>(VirtualAlloc(nullptr, 32,
            MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
        if (!stub) return false;
        // mov r10, rcx        ; 4C 8B D1
        // mov eax, <ssn>      ; B8 XX XX XX XX
        // syscall             ; 0F 05
        // ret                 ; C3
        stub[0] = 0x4C; stub[1] = 0x8B; stub[2] = 0xD1;
        stub[3] = 0xB8;
        *reinterpret_cast<uint32_t*>(stub + 4) = (uint32_t)ssn;
        stub[8] = 0x0F; stub[9] = 0x05;
        stub[10] = 0xC3;
        g_stubPtr   = stub;
        g_ntProtect = reinterpret_cast<NtProtectFn>(stub);
        return true;
    }

    // Resolve NtWriteVirtualMemory SSN and build its direct-syscall stub.
    inline bool InitWrite() {
        if (g_ntWrite) return true;

        // Prefer in-memory stub (Packman usually doesn't hook this one).
        int ssn = -1;
        HMODULE h = GetModuleHandleW(L"ntdll.dll");
        if (h) {
            auto* p = reinterpret_cast<const uint8_t*>(
                GetProcAddress(h, "NtWriteVirtualMemory"));
            if (p && p[0] == 0x4C && p[1] == 0x8B && p[2] == 0xD1 && p[3] == 0xB8) {
                ssn = (int)*reinterpret_cast<const uint32_t*>(p + 4);
            }
        }

        // Fallback: read SSN from pristine disk ntdll.
        if (ssn < 0) {
            HANDLE hFile = CreateFileW(L"\\\\?\\C:\\Windows\\System32\\ntdll.dll",
                GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
            if (hFile != INVALID_HANDLE_VALUE) {
                LARGE_INTEGER sz{};
                if (GetFileSizeEx(hFile, &sz) && sz.QuadPart > 0 &&
                    sz.QuadPart < (64LL << 20)) {
                    auto* buf = reinterpret_cast<uint8_t*>(VirtualAlloc(nullptr,
                        (SIZE_T)sz.QuadPart, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
                    if (buf) {
                        DWORD total = 0, chunk = 0;
                        while (total < sz.QuadPart &&
                               ReadFile(hFile, buf + total,
                                        (DWORD)(sz.QuadPart - total), &chunk, nullptr) &&
                               chunk > 0) {
                            total += chunk;
                        }
                        if (total == (DWORD)sz.QuadPart) {
                            ssn = ExtractSSNFromImageByName(buf, "NtWriteVirtualMemory");
                        }
                        VirtualFree(buf, 0, MEM_RELEASE);
                    }
                }
                CloseHandle(hFile);
            }
        }

        if (ssn < 0) return false;
        g_writeSsn = ssn;

        auto* stub = reinterpret_cast<uint8_t*>(VirtualAlloc(nullptr, 32,
            MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
        if (!stub) return false;
        stub[0] = 0x4C; stub[1] = 0x8B; stub[2] = 0xD1;     // mov r10, rcx
        stub[3] = 0xB8;                                     // mov eax, imm32
        *reinterpret_cast<uint32_t*>(stub + 4) = (uint32_t)ssn;
        stub[8] = 0x0F; stub[9] = 0x05;                     // syscall
        stub[10] = 0xC3;                                    // ret
        g_writeStubPtr = stub;
        g_ntWrite      = reinterpret_cast<NtWriteFn>(stub);
        return true;
    }

    // Write `size` bytes from `src` into the game's address space at `dst`,
    // BYPASSING section-protection enforcement (which normally rejects writes
    // to a PE-image code page). Uses the direct-syscall stub so stub.dll's
    // userland hook doesn't see the call. Returns TRUE on success.
    inline BOOL WriteVirtualMemoryDirect(void* dst, const void* src, SIZE_T size) {
        if (!InitWrite()) return FALSE;
        SIZE_T written = 0;
        const LONG status = g_ntWrite(GetCurrentProcess(), dst,
                                      const_cast<void*>(src), size, &written);
        g_writeLastStatus = status;
        return status >= 0 && written == size;
    }

    // Drop-in replacement for `VirtualProtect`. Returns TRUE on success.
    inline BOOL VirtualProtectDirect(void* addr, SIZE_T size,
                                     DWORD newProt, DWORD* oldProt) {
        if (!Init()) {
            dbglog::Log("  [VPD] Init() failed — SSN unresolved");
            return FALSE;
        }
        PVOID   pBase    = addr;
        SIZE_T  region   = size;
        ULONG   oldUlong = 0;
        dbglog::Log("  [VPD] syscall NtProtectVirtualMemory addr=0x%llX size=%llu newProt=0x%X SSN=0x%X",
                    (unsigned long long)addr,
                    (unsigned long long)size,
                    (unsigned)newProt,
                    (unsigned)g_ssnCached);
        const LONG status = g_ntProtect(GetCurrentProcess(), &pBase, &region,
                                        (ULONG)newProt, &oldUlong);
        g_lastStatus = status;
        dbglog::Log("  [VPD] returned status=0x%08X oldProt=0x%X",
                    (unsigned)status, (unsigned)oldUlong);
        if (oldProt) *oldProt = (DWORD)oldUlong;
        return status >= 0;  // NT_SUCCESS
    }

    // Probe the game's code page with both RWX (what inline-detour needs) and
    // PAGE_READONLY (what DEP-hook needs). Packman typically blocks the first
    // but allows the second — the result decides which strategy to use.
    //
    // Must be called after Init() + after game base is known. The target is
    // a known-executable byte deep inside LoL.exe code (we pick OnStopCast's
    // page because it's what we'll hook anyway).
    inline int SelfTest(uintptr_t anyCodePageAddr) {
        if (g_selfTest != 0) return g_selfTest;  // memoised
        if (!Init())         { g_selfTest = 3; return 3; }

        DWORD oldProt = 0;
        // Attempt 1: RWX flip (inline-detour pattern).
        const BOOL okRWX = VirtualProtectDirect(
            reinterpret_cast<void*>(anyCodePageAddr & ~0xFFFULL), 0x1000,
            PAGE_EXECUTE_READWRITE, &oldProt);
        g_selfTestStatusRWX = g_lastStatus;
        if (okRWX) {
            // Restore original protection immediately.
            DWORD dummy = 0;
            VirtualProtectDirect(
                reinterpret_cast<void*>(anyCodePageAddr & ~0xFFFULL), 0x1000,
                oldProt, &dummy);
            g_selfTest = 1;  // RWX works, no need for DEP trick
            return 1;
        }

        // Attempt 2: PAGE_READONLY flip (DEP-hook pattern).
        const BOOL okRO = VirtualProtectDirect(
            reinterpret_cast<void*>(anyCodePageAddr & ~0xFFFULL), 0x1000,
            PAGE_READONLY, &oldProt);
        g_selfTestStatusRO = g_lastStatus;
        if (okRO) {
            // Restore back to executable immediately so the game keeps running.
            DWORD dummy = 0;
            VirtualProtectDirect(
                reinterpret_cast<void*>(anyCodePageAddr & ~0xFFFULL), 0x1000,
                oldProt, &dummy);
            g_selfTest = 2;  // Only PAGE_READONLY → must use DEP hook
            return 2;
        }

        g_selfTest = 3;  // Both blocked — only DBVM/kernel-side tricks left
        return 3;
    }

    // ── Copy-on-Write probe ─────────────────────────────────────────────
    //
    // Image sections mapped into a process with `SEC_IMAGE` semantics can
    // normally be flipped to `PAGE_EXECUTE_WRITECOPY` (or `PAGE_WRITECOPY`)
    // without the kernel raising `STATUS_SECTION_PROTECTION`. Writing into a
    // WRITECOPY page triggers a copy-on-write fault, which promotes the
    // 4K page to a PRIVATE page backed by the pagefile — identical bytes
    // at first, but writable, and CPU execution uses the private copy.
    //
    // This is the classical "detour on image code without RWX" trick: the
    // file-backed mapping (what Packman's CRC scanner normally reads) stays
    // pristine while the process keeps executing modified bytes.
    //
    //   g_cowSelfTest   1 = EXECUTE_WRITECOPY flip OK (writecopy detour OK)
    //                   2 = only plain WRITECOPY OK (need temp EXEC flip back)
    //                   3 = both blocked (kernel forbids COW on image here)
    inline volatile int  g_cowSelfTest         = 0;
    inline volatile LONG g_cowSelfTestStatusEWC = 0;
    inline volatile LONG g_cowSelfTestStatusWC  = 0;

    inline int SelfTestCoW(uintptr_t anyCodePageAddr) {
        if (g_cowSelfTest != 0) return g_cowSelfTest;
        if (!Init())            { g_cowSelfTest = 3; return 3; }

        void* pg = reinterpret_cast<void*>(anyCodePageAddr & ~0xFFFULL);
        DWORD oldProt = 0;

        // Attempt 1: PAGE_EXECUTE_WRITECOPY — page stays executable AND
        // writable via CoW. Ideal for inline detour.
        const BOOL okEWC = VirtualProtectDirect(pg, 0x1000,
                                                PAGE_EXECUTE_WRITECOPY, &oldProt);
        g_cowSelfTestStatusEWC = g_lastStatus;
        if (okEWC) {
            DWORD dummy = 0;
            VirtualProtectDirect(pg, 0x1000, oldProt, &dummy);
            g_cowSelfTest = 1;
            return 1;
        }

        // Attempt 2: PAGE_WRITECOPY — data only; we would need to flip back
        // to executable before letting the CPU resume.
        const BOOL okWC = VirtualProtectDirect(pg, 0x1000,
                                               PAGE_WRITECOPY, &oldProt);
        g_cowSelfTestStatusWC = g_lastStatus;
        if (okWC) {
            DWORD dummy = 0;
            VirtualProtectDirect(pg, 0x1000, oldProt, &dummy);
            g_cowSelfTest = 2;
            return 2;
        }

        g_cowSelfTest = 3;
        return 3;
    }

    // Separate probe: can `NtWriteVirtualMemory` write to an image code page
    // even though `NtProtectVirtualMemory` refuses to grant write access?
    //
    //   1 = write succeeds and bytes are verifiable (inline detour feasible)
    //   2 = write returns NT_SUCCESS but bytes didn't land
    //   3 = write syscall fails (reports g_writeLastStatus)
    //
    // We write the byte `0xCC` (int3) then restore the original. The page is
    // cloaked externally by `cloak_events.lua`, so reads through the cloak
    // return the pre-write snapshot; to verify the write actually landed we
    // compare with the byte we last wrote (which is kept in `src`).
    inline int SelfTestWrite(uintptr_t anyCodePageAddr) {
        if (g_writeSelfTest != 0) return g_writeSelfTest;
        if (!InitWrite()) { g_writeSelfTest = 3; return 3; }

        uint8_t  orig   = 0;
        uint8_t  probe  = 0xCC;
        SIZE_T   rd     = 0;
        // Read current byte (safe — reads always succeed on PAGE_EXECUTE_READ).
        const auto* p = reinterpret_cast<const uint8_t*>(anyCodePageAddr);
        __try { orig = *p; } __except (1) { g_writeSelfTest = 3; return 3; }

        // Probe-write CC.
        const BOOL ok = WriteVirtualMemoryDirect(
            reinterpret_cast<void*>(anyCodePageAddr), &probe, 1);
        g_writeSelfTestStatus = g_writeLastStatus;
        if (!ok) { g_writeSelfTest = 3; return 3; }

        // Read back. (Cloaked reads return the snapshot — so without cloak we
        // would see CC; with cloak we'd see `orig`. The VERIFICATION of whether
        // the write actually landed requires CPU execution to differ, which
        // we can't do from here without risking a crash. We at least confirm
        // the syscall returned success with `written == 1`.)
        uint8_t after = 0;
        __try { after = *p; } __except (1) {}
        (void)after; // intentionally unused in the simple probe

        // Restore original byte immediately.
        WriteVirtualMemoryDirect(
            reinterpret_cast<void*>(anyCodePageAddr), &orig, 1);

        g_writeSelfTest = 1;   // syscall claims success
        return 1;
    }

    // ── Return-address spoofer gadget finder ──
    //
    // When the hook body calls back into the original game function, the CPU
    // pushes OUR hook body's return address onto the stack. If Packman walks
    // the thread stack mid-call, that pointer lives inside NightSharp.dll
    // (manual-mapped, not in the PEB loader list) — a dead giveaway for any
    // anti-cheat doing module-membership checks on stack frames.
    //
    // Classical fix (Vault7 etc.): substitute the return address with a small
    // gadget in game.exe (`jmp qword ptr [rbx]` = `FF 23`). The gadget hands
    // execution back to our "fixup" label via `rbx`, which the MASM stub in
    // `spoof.asm` pre-loaded with the shell-param struct. From the stack
    // walker's point of view, every frame's return address lives inside
    // game.exe — no anomaly.
    //
    // We cache the first gadget we find on first use.
    inline uintptr_t FindSpoofGadget() {
        static uintptr_t cached = 0;
        if (cached) return cached;

        HMODULE hGame = GetModuleHandleA(nullptr);
        if (!hGame) return 0;

        auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(hGame);
        if (dos->e_magic != IMAGE_DOS_SIGNATURE) return 0;
        auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(
            reinterpret_cast<uint8_t*>(hGame) + dos->e_lfanew);
        if (nt->Signature != IMAGE_NT_SIGNATURE) return 0;

        auto* sec = IMAGE_FIRST_SECTION(nt);
        for (int i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++sec) {
            if (!(sec->Characteristics & IMAGE_SCN_MEM_EXECUTE)) continue;
            const auto* start = reinterpret_cast<uint8_t*>(hGame) + sec->VirtualAddress;
            const size_t sz   = sec->Misc.VirtualSize;
            for (size_t j = 0; j + 1 < sz; ++j) {
                if (start[j] == 0xFF && start[j + 1] == 0x23) {
                    cached = reinterpret_cast<uintptr_t>(start + j);
                    return cached;
                }
            }
        }
        return 0;
    }

} // namespace stealth

// ----------------------------------------------------------------------------
// DEP Hook (Vault7-style) -- REMOVED.
//
// This block used to flip LoL.exe code pages to PAGE_READONLY and let a
// VEH + single-step trampoline redirect RIP into our hook bodies without
// writing any bytes to the page. In practice, the first click on "Arm
// DEP Hooks" in the diagnostics panel always crashed the game -- Packman
// tracks MBI.Protect flips on its own code pages and detonates the process.
//
// The codebase now relies exclusively on:
//   * Shadow-VMT hooks   (virtual-table swap; never touches code)
//   * Inline+EPT detours (CoW-backed; page flip stays in private WS)
//
// Removed together: namespace dephook { ... }, InstallAllDepHooks,
// InstallDepHooksNow, UninstallDepHooksNow, plus the dephook fallback
// branches that each Hk* body used to carry.
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// Types
// ----------------------------------------------------------------------------

// Generic callback — every event reports (sender, context, intParam). The
// meaning of ‘context’/‘intParam’ depends on which event fired:
//
//   OnProcessSpell / OnStopCast / OnFinishCast / OnChannel*
//       sender  = hero object*          context = SpellCastInfo*
//       intParam= SpellSlot (0-7)       // -1 if unavailable
//
//   OnBuffGain / OnBuffLose / OnBuffUpdate
//       sender  = hero object*          context = BuffData*
//       intParam= stacks                // 0 if unavailable
//
//   (OnAutoAttack was REMOVED — auto-attacks now surface through
//    OnProcessSpell with intParam==64; consumers filter by slot.)
//
//   OnDeath / OnNewPath / OnPlayAnimation
//       sender  = hero object*          context = 0
//       intParam= new value (anim-id / waypoint count / …)
//
using Callback = void(*)(uintptr_t sender, uintptr_t context, int intParam);

// Legacy alias preserved for older SDK glue code.
using RawProcessSpellCallback = void(*)(uintptr_t senderObj, uintptr_t castInfo);

// ----------------------------------------------------------------------------
// Callback registry
// ----------------------------------------------------------------------------
namespace detail {

    // Max event id currently in use is 53 (OnDoCast) — keep slack to 64.
    inline constexpr int kMaxEventId = 64;

    // Multi-subscriber callback registry. `SetCallback(id, cb)` appends to
    // the per-event list (with dedupe) so several SDK trackers can fan out
    // the same engine event — e.g. SpellCastTracker AND AnimationTracker
    // both want OnProcessSpell. `Fire()` walks every installed slot.
    // `kMaxCallbacksPerEvent` is sized for the worst case (currently 3-4
    // SDK consumers + room for user-script direct hooks).
    inline constexpr int kMaxCallbacksPerEvent = 8;
    inline Callback     g_callbacks  [kMaxEventId][kMaxCallbacksPerEvent] = {};
    inline uint64_t     g_fireCount  [kMaxEventId] = {};   // cumulative fire counter per event
    inline uint64_t     g_totalFires = 0;                  // grand total across all events

    inline void Fire(int id, uintptr_t sender, uintptr_t ctx, int param) {
        if (id <= 0 || id >= kMaxEventId) return;
        ++g_fireCount[id];
        ++g_totalFires;
        for (int i = 0; i < kMaxCallbacksPerEvent; ++i) {
            if (auto cb = g_callbacks[id][i]) cb(sender, ctx, param);
        }
    }

} // namespace detail

// ----------------------------------------------------------------------------
// Diagnostics API — consumed by the menu's "SDK Diagnostics" tab.
// Pure-offset/pure-callback-registry helpers live here. Accessors that need
// `detail::g_vmtHook` are defined further down (after PART A, where that
// variable is declared) to avoid forward-ref errors.
// ----------------------------------------------------------------------------
namespace diagnostics {
    // Name for an event id (for UI labels). Returns nullptr for unknown ids.
    inline const char* NameOf(int id) {
        using namespace Offset::Events;
        switch (id) {
            case OnProcessSpell:   return "OnProcessSpell";
            case OnStopCast:       return "OnStopCast";
            case OnFinishCast:     return "OnFinishCast";
            case OnChannelEnd:     return "OnChannelEnd";
            case OnBuffUpdate:     return "OnBuffUpdate";
            case OnDeath:          return "OnDeath";
            case OnNewPath:        return "OnNewPath";
            case OnIssueOrder:     return "OnIssueOrder";
            case OnDash:           return "OnDash";
            case OnStealth:        return "OnStealth";
            case OnTurretAttack:   return "OnTurretAttack";
            case OnDoCast:         return "OnDoCast";
            case OnIntegerPropertyChange: return "OnIntegerPropertyChange";
            default:               return nullptr;
        }
    }

    // How this event is implemented. After the switch to "inline-only"
    // dispatch, every event is driven by either the Shadow-VMT hook or
    // a raw inline detour — no pollers remain. `Unknown` means "the ID
    // has no active carrier; consumers should treat it as disabled".
    enum class Method { VmtHook, InlineHook, Unknown };

    // Forward decls — defined further down once hooks are visible.
    bool HookInstalled();                 // OnProcessSpell VMT
    bool InlineHookInstalled(int eventId); // Inline detour (OnStopCast, …)

    inline Method MethodOf(int id) {
        using namespace Offset::Events;
        switch (id) {
            // OnProcessSpell → Shadow-VMT dispatch slot. The poll fallback
            // that used to back this has been deleted.
            case OnProcessSpell:
                return HookInstalled() ? Method::VmtHook : Method::Unknown;

            // Events whose game function we splice with an inline detour.
            case OnStopCast: case OnFinishCast:
                return InlineHookInstalled(id) ? Method::InlineHook : Method::Unknown;
            // OnChannelEnd is DERIVED from the stop/finish inline detours —
            // they fire it when the halted cast was channeled.
            case OnChannelEnd:
                return (InlineHookInstalled(OnStopCast) ||
                        InlineHookInstalled(OnFinishCast))
                       ? Method::InlineHook : Method::Unknown;
            // OnBuffUpdate — unified buff lifecycle event fired from inside
            // the OnBuffAdd detour body (HkOnBuffAdd). OnBuffGain /
            // OnBuffLose have been removed from the enum entirely.
            case OnBuffUpdate:
                return InlineHookInstalled(id) ? Method::InlineHook : Method::Unknown;
            case OnNewPath:
                return InlineHookInstalled(id) ? Method::InlineHook : Method::Unknown;
            // OnDeath piggy-backs on TWO inline detours — HkOnBuffAdd and
            // HkOnHeroActionState — via CheckDeathForHero. Either being
            // installed is sufficient to call it "Inline+EPT".
            case OnDeath:
                return (InlineHookInstalled(OnBuffUpdate) ||
                        InlineHookInstalled(OnNewPath))
                       ? Method::InlineHook : Method::Unknown;
            // OnIssueOrder: raw-asm inline detour on ControlRuntime::IssueOrder.
            case OnIssueOrder:
                return InlineHookInstalled(id) ? Method::InlineHook : Method::Unknown;
            // OnDash piggy-backs on HkOnHeroActionState.
            case OnDash:
                return InlineHookInstalled(OnNewPath)
                       ? Method::InlineHook : Method::Unknown;
            // OnStealth piggy-backs on HkOnBuffAdd (same carrier as
            // OnBuffUpdate).
            case OnStealth:
                return InlineHookInstalled(OnBuffUpdate)
                       ? Method::InlineHook : Method::Unknown;
            // OnTurretAttack uses the Shadow-VMT counter (same signal as
            // OnProcessSpell, but scans the turret manager separately).
            case OnTurretAttack:
                return HookInstalled() ? Method::VmtHook : Method::Unknown;
            // OnDoCast piggy-backs on HkOnFinishCast.
            case OnDoCast:
                return InlineHookInstalled(OnFinishCast)
                       ? Method::InlineHook : Method::Unknown;
            // OnIntegerPropertyChange now polled in the VMT poll loop
            // (HkOnHeroActionState inline detour is dead on the current
            // build). Carrier readiness == VMT hook installed.
            case OnIntegerPropertyChange:
                return HookInstalled() ? Method::VmtHook : Method::Unknown;
            default:
                return Method::Unknown;
        }
    }
    inline const char* MethodLabel(Method m) {
        switch (m) {
            case Method::VmtHook:    return "Shadow-VMT";
            case Method::InlineHook: return "Inline+EPT";
            default:                 return "(disabled)";
        }
    }
    // Is the underlying hook even installed? With the polling paths gone,
    // readiness is simply "does the hook carrying this event live?". The
    // UI uses this to surface deactivated events as "DISABLED".
    inline bool IsEventReady(int id) {
        const auto m = MethodOf(id);
        return m == Method::VmtHook || m == Method::InlineHook;
    }

    inline uint64_t FireCountOf(int id) {
        return (id > 0 && id < detail::kMaxEventId) ? detail::g_fireCount[id] : 0;
    }
    inline uint64_t TotalFires() { return detail::g_totalFires; }

    // These accessors touch `detail::g_vmtHook` and are defined AFTER PART A.
    bool       HookInstalled();
    bool       HookInstalling();
    int        DispatchSlotCount();
    uint64_t   VmtEventCounter();
    uintptr_t  DispatchSlotAddr();
    uintptr_t  TrampolineAddr();
    uintptr_t  OriginalFnAddr();

    // Small set of event ids the menu iterates over to build its table.
    inline constexpr int kAllEventIds[] = {
        Offset::Events::OnProcessSpell,
        Offset::Events::OnStopCast,
        Offset::Events::OnFinishCast,
        Offset::Events::OnChannelEnd,
        Offset::Events::OnBuffUpdate,
        Offset::Events::OnDeath,
        Offset::Events::OnNewPath,
        Offset::Events::OnIssueOrder,
        Offset::Events::OnDash,
        Offset::Events::OnStealth,
        Offset::Events::OnTurretAttack,
        Offset::Events::OnDoCast,
        Offset::Events::OnIntegerPropertyChange,
    };
    inline constexpr int kAllEventCount = (int)(sizeof(kAllEventIds) / sizeof(kAllEventIds[0]));
} // namespace diagnostics

// Append `cb` to `eventId`'s callback list. Idempotent: registering the
// same function pointer twice is a no-op. Passing `nullptr` is treated as
// `ClearCallback(eventId)` for backwards compat with the old single-slot
// API.
inline void SetCallback(int eventId, Callback cb) {
    if (eventId <= 0 || eventId >= detail::kMaxEventId) return;
    if (!cb) {
        for (int i = 0; i < detail::kMaxCallbacksPerEvent; ++i) {
            detail::g_callbacks[eventId][i] = nullptr;
        }
        return;
    }
    // Dedupe: skip if already installed.
    for (int i = 0; i < detail::kMaxCallbacksPerEvent; ++i) {
        if (detail::g_callbacks[eventId][i] == cb) return;
    }
    // Append into the first empty slot.
    for (int i = 0; i < detail::kMaxCallbacksPerEvent; ++i) {
        if (!detail::g_callbacks[eventId][i]) {
            detail::g_callbacks[eventId][i] = cb;
            return;
        }
    }
    // Silently drop if the per-event slot table is full — raise
    // `kMaxCallbacksPerEvent` if this becomes a real constraint.
}

// Returns the first installed callback (legacy single-slot semantic).
// New code should iterate via Fire() rather than reading slots directly.
inline Callback GetCallback(int eventId) {
    if (eventId <= 0 || eventId >= detail::kMaxEventId) return nullptr;
    for (int i = 0; i < detail::kMaxCallbacksPerEvent; ++i) {
        if (auto cb = detail::g_callbacks[eventId][i]) return cb;
    }
    return nullptr;
}

// Wipe every callback installed on this event id.
inline void ClearCallback(int eventId) { SetCallback(eventId, nullptr); }

// ============================================================================
// PART A — Shadow-VMT hook for OnProcessSpell  (unchanged from working build)
// ============================================================================
namespace detail {

    struct ShadowVMT {
        static constexpr int kMaxTrackedHeroes = 20;

        // Shellcode template (20 bytes) — see the diagram in the old header.
        //   push rax ; mov rax,<counter> ; lock inc qword[rax] ; pop rax ;
        //   jmp [rip+0] ; dq <origFn>
        static constexpr uint8_t kTrampTemplate[] = {
            0x50,
            0x48, 0xB8, 0,0,0,0, 0,0,0,0,
            0xF0, 0x48, 0xFF, 0x00,
            0x58,
            0xFF, 0x25, 0x00, 0x00, 0x00, 0x00,
        };
        static constexpr int kCounterPatchOff = 3;
        static constexpr int kOrigPatchOff    = sizeof(kTrampTemplate);
        static constexpr int kTrampTotalSize  = sizeof(kTrampTemplate) + 8;

        alignas(64) uintptr_t shadowTable[Offset::SpellEventVMT::VTableEntryCount] = {};
        uintptr_t          originalFn         = 0;
        uintptr_t*         dispatchSlot       = nullptr;   // single writable heap slot (legacy backup pattern)
        uintptr_t          originalVtable     = 0;
        uintptr_t          trampolineAddr     = 0;
        uintptr_t          prevCasts[kMaxTrackedHeroes] = {};
        volatile long long eventCounter       = 0;
        long long          lastPolledCount    = 0;
        bool               installed          = false;

        // Find ONE writable qword in committed memory that currently stores
        // `vtableAbsAddr` — that is the dispatch slot we will swap.
        //
        // Restored from `CoreEventHookbk.h::FindDispatchSlot` (the legacy
        // build that was verified working). The Phase-2 SDK refactor that
        // tried to swap *every* copy of the vtable pointer broke
        // OnProcessSpell on the current build because it filled the slot
        // array with non-dispatch heap copies, leaving the real dispatch
        // slot un-swapped on some game-state transitions.
        static uintptr_t* FindDispatchSlot(uintptr_t vtableAbsAddr) {
            MEMORY_BASIC_INFORMATION mbi{};
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
            if (installed) { dbglog::Log("[VMT] Install() already installed - skip"); return true; }

            const auto base = shim::GetGameBase();
            if (!base) { dbglog::Log("[VMT] Install FAIL: GetGameBase()==0"); return false; }

            const auto vtableAbsAddr = base + Offset::SpellEventVMT::VTableRVA;
            const auto* origTable = reinterpret_cast<const uintptr_t*>(vtableAbsAddr);
            dbglog::Log("[VMT] Install start  base=%p  vtableAbs=%p  HandlerIdx=%d  WrapperRVA=0x%llx",
                        (void*)base, (void*)vtableAbsAddr,
                        (int)Offset::SpellEventVMT::HandlerIndex,
                        (unsigned long long)Offset::SpellEventVMT::WrapperRVA);

            __try {
                const uintptr_t got = origTable[Offset::SpellEventVMT::HandlerIndex];
                const uintptr_t expected = base + Offset::SpellEventVMT::WrapperRVA;
                dbglog::Log("[VMT]  vtable[%d] = %p  expected=%p  match=%d",
                            (int)Offset::SpellEventVMT::HandlerIndex,
                            (void*)got, (void*)expected, (int)(got == expected));
                if (got != expected) {
                    dbglog::Log("[VMT] Install FAIL: vtable handler mismatch");
                    return false;
                }
            } __except (1) {
                dbglog::Log("[VMT] Install FAIL: SEH reading vtable[HandlerIndex]");
                return false;
            }

            // Locate the writable dispatch slot on the heap.
            auto* slot = FindDispatchSlot(vtableAbsAddr);
            dbglog::Log("[VMT]  FindDispatchSlot -> %p", (void*)slot);
            if (!slot) {
                dbglog::Log("[VMT] Install FAIL: no writable heap slot stores vtableAbsAddr (game not ready?)");
                return false;
            }
            dispatchSlot = slot;

            __try {
                memcpy(shadowTable, origTable,
                       sizeof(uintptr_t) * Offset::SpellEventVMT::VTableEntryCount);
            } __except (1) {
                dbglog::Log("[VMT] Install FAIL: SEH copying vtable to shadow");
                dispatchSlot = nullptr; return false;
            }

            originalFn     = shadowTable[Offset::SpellEventVMT::HandlerIndex];
            originalVtable = vtableAbsAddr;

            trampolineAddr = reinterpret_cast<uintptr_t>(
                VirtualAlloc(nullptr, 64, MEM_COMMIT | MEM_RESERVE,
                             PAGE_EXECUTE_READWRITE));
            if (!trampolineAddr) {
                dbglog::Log("[VMT] Install FAIL: VirtualAlloc trampoline RWX returned 0");
                dispatchSlot = nullptr; return false;
            }

            memcpy(reinterpret_cast<void*>(trampolineAddr),
                   kTrampTemplate, sizeof(kTrampTemplate));
            *reinterpret_cast<uintptr_t*>(trampolineAddr + kCounterPatchOff) =
                reinterpret_cast<uintptr_t>(&eventCounter);
            *reinterpret_cast<uintptr_t*>(trampolineAddr + kOrigPatchOff) =
                originalFn;
            FlushInstructionCache(GetCurrentProcess(),
                reinterpret_cast<void*>(trampolineAddr), kTrampTotalSize);

            shadowTable[Offset::SpellEventVMT::HandlerIndex] = trampolineAddr;

            // ----------------------------------------------------------------
            // Shadow-VMT swap — atomically replace the dispatch slot with
            // our shadow vtable. Aligned qword writes are atomic on x64 so
            // the game never sees a torn pointer.
            //
            // NOTE: we deliberately do NOT VirtualProtect+patch the original
            // .rdata vtable in place — Vanguard's code-integrity scan flags
            // that immediately and crashes the process. Shadow-VMT works
            // because we never write to any page marked EXECUTE_READ.
            // ----------------------------------------------------------------
            *dispatchSlot = reinterpret_cast<uintptr_t>(shadowTable);

            eventCounter    = 0;
            lastPolledCount = 0;
            memset(prevCasts, 0, sizeof(prevCasts));
            installed = true;
            dbglog::Log("[VMT] Install SUCCESS  slot=%p  shadow=%p  origFn=%p  tramp=%p",
                        (void*)dispatchSlot, (void*)shadowTable,
                        (void*)originalFn, (void*)trampolineAddr);
            return true;
        }

        void Uninstall() {
            if (!installed || !dispatchSlot) return;
            __try { *dispatchSlot = originalVtable; } __except (1) {}
            dispatchSlot = nullptr;
            if (trampolineAddr) {
                __try { VirtualFree(reinterpret_cast<void*>(trampolineAddr), 0, MEM_RELEASE); } __except (1) {}
                trampolineAddr = 0;
            }
            installed = false;
        }
    };

    inline ShadowVMT g_vmtHook{};

} // namespace detail

// Definitions for the diagnostics accessors that need `detail::g_vmtHook`.
namespace diagnostics {
    inline bool      HookInstalled()      { return detail::g_vmtHook.installed; }
    inline bool      HookInstalling()     { return false; } // legacy: no async install state
    inline int       DispatchSlotCount()  { return detail::g_vmtHook.dispatchSlot ? 1 : 0; }
    inline uint64_t  VmtEventCounter()    { return (uint64_t)detail::g_vmtHook.eventCounter; }
    inline uintptr_t DispatchSlotAddr()   {
        return reinterpret_cast<uintptr_t>(detail::g_vmtHook.dispatchSlot);
    }
    inline uintptr_t TrampolineAddr()     { return detail::g_vmtHook.trampolineAddr; }
    inline uintptr_t OriginalFnAddr()     { return detail::g_vmtHook.originalFn; }
} // namespace diagnostics

namespace detail {

    // Legacy raw OnProcessSpell sink (used by old SDK glue). Modern code should
    // register via SetCallback(Events::OnProcessSpell, …) instead.
    inline RawProcessSpellCallback g_rawProcessSpellCb = nullptr;

} // namespace detail

// ============================================================================
// PART A2 — Inline detour hooks (DBVM EPT-cloaked)
// ============================================================================
// For events that are NOT dispatched through a vtable (OnStopCast,
// OnFinishCast, OnBuffAdd, OnSpellImpact, OnCreateObject, OnGameUpdate,
// OnHeroActionStateChange, OnMinionFollowChange) Shadow-VMT doesn't apply.
// We splice a 14-byte `jmp [rip+0]; dq target` at the function prologue
// instead.
//
// Packman's code-integrity scan would normally detect the modified bytes.
// We rely on a DBVM EPT cloak (see `cloak_events.lua`, activated from
// Cheat Engine before DLL injection) so the page has a dual view:
//
//     READ access → scanner sees ORIGINAL (cloaked) bytes
//     EXEC access → CPU fetches our JMP, hook fires
//
// When DBVM cloak is not active the install still succeeds, but Packman
// will eventually flag the patched page. The Lua sidecar reads the
// exported `g_CloakRVAs` array and calls `dbvm_cloak_activate` per entry.
// ============================================================================
namespace detail {

    // Tiny x64 instruction-length decoder — enough to step over the common
    // prologue ops emitted by MSVC (push/mov/sub/lea/test). Returns 1 on
    // anything unrecognised so we never spin forever, but then caller will
    // abort the install because `StolenSize` may land mid-instruction and
    // the trampoline would fault.
    inline int InstrLength(const uint8_t* code) {
        const uint8_t* p = code;
        while (*p == 0x66 || *p == 0x67 || *p == 0xF2 || *p == 0xF3 ||
               (*p >= 0x40 && *p <= 0x4F)) ++p;
        uint8_t op = *p++;
        if (op == 0x90 || op == 0xC3 || op == 0xCC || op == 0xCB) return (int)(p - code);
        if (op >= 0x50 && op <= 0x5F) return (int)(p - code);
        if (op == 0xC2) return (int)(p - code) + 2;
        if ((op >= 0x70 && op <= 0x7F) || op == 0xEB || op == 0xE3) return (int)(p - code) + 1;
        if (op == 0xE8 || op == 0xE9) return (int)(p - code) + 4;
        bool hasModRM = false;
        int imm = 0;
        // ALU r/m8, imm8 (opcodes 80 / 82) and r/m16|32|64, imm8 (83)
        if (op == 0x80 || op == 0x82 || op == 0x83) { hasModRM = true; imm = 1; }
        else if (op == 0x81 || op == 0xC7 || op == 0xF7) { hasModRM = true; imm = 4; }
        else if (op == 0xC6) { hasModRM = true; imm = 1; }
        else if (op == 0x0F) {
            uint8_t op2 = *p++;
            if (op2 >= 0x80 && op2 <= 0x8F) return (int)(p - code) + 4;
            hasModRM = true;
        }
        else if ((op >= 0x00 && op <= 0x3F) || (op >= 0x84 && op <= 0x8F) ||
                 op == 0x63 || op == 0x69 || op == 0x6B || op == 0xFF || op == 0xF6 ||
                 op == 0x8B || op == 0x89 || op == 0x8D ||
                 op == 0x03 || op == 0x0B || op == 0x33 || op == 0x3B ||
                 op == 0x23 || op == 0x85 || op == 0x87) {
            hasModRM = true;
            if (op == 0x69) imm = 4;
            if (op == 0x6B) imm = 1;
        }
        if (hasModRM) {
            uint8_t modrm = *p++;
            uint8_t mod = (modrm >> 6) & 3;
            uint8_t rm  = modrm & 7;
            if (mod != 3 && rm == 4) ++p;          // SIB
            if (mod == 0 && rm == 5) p += 4;       // [rip+disp32]
            else if (mod == 1) p += 1;
            else if (mod == 2) p += 4;
            p += imm;
        }
        int len = (int)(p - code);
        return len > 0 ? len : 1;
    }

    inline int StolenSize(uintptr_t at, int minBytes) {
        int total = 0;
        while (total < minBytes) total += InstrLength((const uint8_t*)(at + total));
        return total;
    }

    constexpr int kJmpPatchSize = 14;
    constexpr int kMaxStolen    = 32;

    // Machine-readable install-status codes so diagnostics can distinguish
    // a decoder abort from a VirtualProtect failure etc.
    enum InstallStatus : uint8_t {
        kInst_NotAttempted = 0,
        kInst_OK           = 1,
        kInst_BadArgs      = 2,  // null target / hookFn
        kInst_DecodeFail   = 3,  // stolenSize out of range
        kInst_VAllocFail   = 4,  // trampoline allocation failed
        kInst_VProtectFail = 5,  // could not flip page to RWX
        kInst_SehFault     = 6,  // __except path (page not readable, etc.)
    };

    struct InlineDetour {
        uintptr_t targetAddr    = 0;
        int       stolenSize    = 0;
        uint8_t   originalBytes[kMaxStolen] = {};
        uintptr_t trampolineAddr = 0;
        volatile long long hitCounter = 0;
        bool      installed     = false;
        uint8_t   lastStatus    = kInst_NotAttempted;

        bool Install(uintptr_t target, uintptr_t hookFn) {
            if (installed) return true;
            if (!target || !hookFn) { lastStatus = kInst_BadArgs; return false; }
            targetAddr = target;
            __try {
                stolenSize = StolenSize(target, kJmpPatchSize);
                if (stolenSize <= 0 || stolenSize > kMaxStolen) {
                    lastStatus = kInst_DecodeFail;
                    return false;
                }

                trampolineAddr = (uintptr_t)VirtualAlloc(nullptr, 64,
                    MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
                if (!trampolineAddr) { lastStatus = kInst_VAllocFail; return false; }

                memcpy(originalBytes, (void*)target, stolenSize);
                memcpy((void*)trampolineAddr, originalBytes, stolenSize);
                auto* tJ = (uint8_t*)(trampolineAddr + stolenSize);
                tJ[0] = 0xFF; tJ[1] = 0x25;
                *(int32_t*)(tJ + 2) = 0;
                *(uintptr_t*)(tJ + 6) = target + stolenSize;

                DWORD oldProt = 0;
                // ── Copy-on-Write detour ──
                //
                // Kernel refuses +W/+RWX on LoL.exe image sections
                // (STATUS_SECTION_PROTECTION = 0xC000004E), but it *does*
                // allow `PAGE_EXECUTE_WRITECOPY`. Writing into a WRITECOPY
                // image page triggers a copy-on-write fault: the kernel
                // allocates a private 4K page, copies the original bytes,
                // maps the new page in place of the shared file-backed one,
                // and completes our write there.
                //
                // Net effect: our JMP bytes land in a private page that only
                // this process sees. Packman's CRC scanner reads the original
                // file-backed mapping and sees UNMODIFIED bytes — no detection.
                // The CPU executes the private page and sees our detour.
                if (!stealth::VirtualProtectDirect((void*)target, stolenSize,
                                                   PAGE_EXECUTE_WRITECOPY, &oldProt)) {
                    VirtualFree((void*)trampolineAddr, 0, MEM_RELEASE);
                    trampolineAddr = 0;
                    lastStatus = kInst_VProtectFail;
                    return false;
                }
                memset((void*)target, 0x90, stolenSize);
                auto* p = (uint8_t*)target;
                p[0] = 0xFF; p[1] = 0x25;
                *(int32_t*)(p + 2) = 0;
                *(uintptr_t*)(p + 6) = hookFn;
                // Flip back to the original protection (typically
                // PAGE_EXECUTE_READ) — the CoW promotion has already
                // happened; this just restores the MBI-visible flag so
                // a scanner sees a normal RX image page.
                stealth::VirtualProtectDirect((void*)target, stolenSize, oldProt, &oldProt);
                FlushInstructionCache(GetCurrentProcess(), (void*)target, stolenSize);

                installed  = true;
                lastStatus = kInst_OK;
                return true;
            } __except (1) {
                if (trampolineAddr) {
                    VirtualFree((void*)trampolineAddr, 0, MEM_RELEASE);
                    trampolineAddr = 0;
                }
                lastStatus = kInst_SehFault;
                return false;
            }
        }

        void Uninstall() {
            if (!installed) return;
            __try {
                DWORD oldProt = 0;
                // Same CoW trick on the way out: the page is already
                // private-CoW from Install, so writing the original bytes
                // back just touches the private page.
                stealth::VirtualProtectDirect((void*)targetAddr, stolenSize,
                                              PAGE_EXECUTE_WRITECOPY, &oldProt);
                memcpy((void*)targetAddr, originalBytes, stolenSize);
                stealth::VirtualProtectDirect((void*)targetAddr, stolenSize,
                                              oldProt, &oldProt);
                FlushInstructionCache(GetCurrentProcess(),
                                      (void*)targetAddr, stolenSize);
            } __except (1) {}
            if (trampolineAddr) {
                VirtualFree((void*)trampolineAddr, 0, MEM_RELEASE);
                trampolineAddr = 0;
            }
            installed = false;
        }

        template<typename T>
        T GetOriginal() const { return (T)trampolineAddr; }
    };

    // ------------------------------------------------------------------------
    // RawAsmDetour — inline detour for functions we CANNOT forward through a
    // typed C++ prototype. IssueOrder (sub_2AFE10) takes 21 arguments including
    // 6 doubles in XMM0-XMM3 + 2 on the stack; spilling through a 4-arg C
    // wrapper would clobber the XMM regs and drop the stack args.
    //
    // Instead we emit a hand-written trampoline that:
    //     1. sub rsp, 0x88              — reserve 136 B aligned scratch frame
    //     2. save RCX/RDX/R8/R9 + XMM0-3 on our scratch frame
    //     3. reload RCX/RDX/R8/R9 from scratch (they weren't touched — defensive)
    //     4. mov rax, <logger>; call rax — fire the event (C++ logger)
    //     5. restore XMM0-3 + RCX/RDX/R8/R9 from scratch
    //     6. add rsp, 0x88              — unwind
    //     7. execute the stolen prologue bytes in the caller's correct context
    //     8. jmp [rip+0]; qword target+stolenSize — resume the real function
    //
    // The caller's stack (args 5+ on the stack) is NEVER touched, XMM regs
    // are preserved bit-exact, and RCX/RDX/R8/R9 pass through unmodified.
    // ------------------------------------------------------------------------
    struct RawAsmDetour {
        uintptr_t targetAddr    = 0;
        int       stolenSize    = 0;
        uint8_t   originalBytes[kMaxStolen] = {};
        uintptr_t trampolineAddr = 0;          // our raw asm page
        volatile long long hitCounter = 0;     // bumped by the logger
        bool      installed     = false;
        uint8_t   lastStatus    = kInst_NotAttempted;

        bool Install(uintptr_t target, uintptr_t loggerFn) {
            if (installed) return true;
            if (!target || !loggerFn) { lastStatus = kInst_BadArgs; return false; }
            targetAddr = target;
            __try {
                stolenSize = StolenSize(target, kJmpPatchSize);
                if (stolenSize <= 0 || stolenSize > kMaxStolen) {
                    lastStatus = kInst_DecodeFail;
                    return false;
                }

                // 256 B is more than enough for our ~160-byte trampoline.
                trampolineAddr = (uintptr_t)VirtualAlloc(nullptr, 256,
                    MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
                if (!trampolineAddr) { lastStatus = kInst_VAllocFail; return false; }

                memcpy(originalBytes, (void*)target, stolenSize);

                uint8_t* p = (uint8_t*)trampolineAddr;
                size_t off = 0;
                auto emit = [&](const uint8_t* bytes, size_t n) {
                    memcpy(p + off, bytes, n); off += n;
                };

                // ── sub rsp, 0x88 (imm32 — 0x88 doesn't fit signed imm8) ──
                const uint8_t sub_rsp[]   = { 0x48, 0x81, 0xEC, 0x88, 0x00, 0x00, 0x00 };
                emit(sub_rsp, sizeof(sub_rsp));
                // ── save int args at +0x20..+0x38 ──
                const uint8_t save_rcx[]  = { 0x48, 0x89, 0x4C, 0x24, 0x20 };
                const uint8_t save_rdx[]  = { 0x48, 0x89, 0x54, 0x24, 0x28 };
                const uint8_t save_r8[]   = { 0x4C, 0x89, 0x44, 0x24, 0x30 };
                const uint8_t save_r9[]   = { 0x4C, 0x89, 0x4C, 0x24, 0x38 };
                emit(save_rcx, 5); emit(save_rdx, 5); emit(save_r8, 5); emit(save_r9, 5);
                // ── save xmm0-3 at +0x40..+0x70 (aligned-16 — RSP is mul-16 here) ──
                const uint8_t save_xm0[]  = { 0x0F, 0x29, 0x44, 0x24, 0x40 };
                const uint8_t save_xm1[]  = { 0x0F, 0x29, 0x4C, 0x24, 0x50 };
                const uint8_t save_xm2[]  = { 0x0F, 0x29, 0x54, 0x24, 0x60 };
                const uint8_t save_xm3[]  = { 0x0F, 0x29, 0x5C, 0x24, 0x70 };
                emit(save_xm0, 5); emit(save_xm1, 5); emit(save_xm2, 5); emit(save_xm3, 5);
                // ── reload int args into RCX/RDX/R8/R9 for logger call ──
                const uint8_t ld_rcx[]    = { 0x48, 0x8B, 0x4C, 0x24, 0x20 };
                const uint8_t ld_rdx[]    = { 0x48, 0x8B, 0x54, 0x24, 0x28 };
                const uint8_t ld_r8[]     = { 0x4C, 0x8B, 0x44, 0x24, 0x30 };
                const uint8_t ld_r9[]     = { 0x4C, 0x8B, 0x4C, 0x24, 0x38 };
                emit(ld_rcx, 5); emit(ld_rdx, 5); emit(ld_r8, 5); emit(ld_r9, 5);
                // ── mov rax, <logger>; call rax ──
                p[off++] = 0x48; p[off++] = 0xB8;
                *(uintptr_t*)(p + off) = loggerFn; off += 8;
                p[off++] = 0xFF; p[off++] = 0xD0;
                // ── restore xmm0-3 ──
                const uint8_t ld_xm0[]    = { 0x0F, 0x28, 0x44, 0x24, 0x40 };
                const uint8_t ld_xm1[]    = { 0x0F, 0x28, 0x4C, 0x24, 0x50 };
                const uint8_t ld_xm2[]    = { 0x0F, 0x28, 0x54, 0x24, 0x60 };
                const uint8_t ld_xm3[]    = { 0x0F, 0x28, 0x5C, 0x24, 0x70 };
                emit(ld_xm0, 5); emit(ld_xm1, 5); emit(ld_xm2, 5); emit(ld_xm3, 5);
                // ── restore int args ──
                emit(ld_rcx, 5); emit(ld_rdx, 5); emit(ld_r8, 5); emit(ld_r9, 5);
                // ── add rsp, 0x88 ──
                const uint8_t add_rsp[]   = { 0x48, 0x81, 0xC4, 0x88, 0x00, 0x00, 0x00 };
                emit(add_rsp, sizeof(add_rsp));
                // ── execute stolen prologue bytes in correct RSP context ──
                emit(originalBytes, stolenSize);
                // ── jmp [rip+0] + qword target+stolenSize ──
                p[off++] = 0xFF; p[off++] = 0x25;
                *(int32_t*)(p + off) = 0; off += 4;
                *(uintptr_t*)(p + off) = target + stolenSize; off += 8;

                FlushInstructionCache(GetCurrentProcess(), (void*)trampolineAddr, off);

                // ── Patch target with JMP to trampoline (same CoW bypass) ──
                DWORD oldProt = 0;
                if (!stealth::VirtualProtectDirect((void*)target, stolenSize,
                                                    PAGE_EXECUTE_WRITECOPY, &oldProt)) {
                    VirtualFree((void*)trampolineAddr, 0, MEM_RELEASE);
                    trampolineAddr = 0;
                    lastStatus = kInst_VProtectFail;
                    return false;
                }
                memset((void*)target, 0x90, stolenSize);
                uint8_t* t = (uint8_t*)target;
                t[0] = 0xFF; t[1] = 0x25;
                *(int32_t*)(t + 2) = 0;
                *(uintptr_t*)(t + 6) = trampolineAddr;
                stealth::VirtualProtectDirect((void*)target, stolenSize, oldProt, &oldProt);
                FlushInstructionCache(GetCurrentProcess(), (void*)target, stolenSize);

                installed = true;
                lastStatus = kInst_OK;
                return true;
            } __except (1) {
                if (trampolineAddr) {
                    VirtualFree((void*)trampolineAddr, 0, MEM_RELEASE);
                    trampolineAddr = 0;
                }
                lastStatus = kInst_SehFault;
                return false;
            }
        }

        void Uninstall() {
            if (!installed) return;
            __try {
                DWORD oldProt = 0;
                stealth::VirtualProtectDirect((void*)targetAddr, stolenSize,
                                              PAGE_EXECUTE_WRITECOPY, &oldProt);
                memcpy((void*)targetAddr, originalBytes, stolenSize);
                stealth::VirtualProtectDirect((void*)targetAddr, stolenSize,
                                              oldProt, &oldProt);
                FlushInstructionCache(GetCurrentProcess(),
                                      (void*)targetAddr, stolenSize);
            } __except (1) {}
            if (trampolineAddr) {
                VirtualFree((void*)trampolineAddr, 0, MEM_RELEASE);
                trampolineAddr = 0;
            }
            installed = false;
        }
    };

    // One detour slot per hooked event.
    inline InlineDetour g_detOnStopCast{};
    inline InlineDetour g_detOnFinishCast{};
    inline InlineDetour g_detOnBuffAdd{};
    inline InlineDetour g_detOnSpellImpact{};
    inline InlineDetour g_detOnCreateObject{};
    inline InlineDetour g_detOnGameUpdate{};
    inline InlineDetour g_detOnHeroActionState{};
    inline InlineDetour g_detOnMinionFollow{};
    inline InlineDetour g_detOnProcessSpell{};   // sub_1FD080 (SpellEvent::Wrapper)
    // Runtime toggle — set false to skip arming the OnProcessSpell inline
    // detour during InstallAllInlineDetours. Default OFF until we verify
    // it doesn't regress sibling detours (OnStopCast/OnChannelEnd).
    inline bool g_processSpellInlineEnabled = false;
    inline RawAsmDetour g_detOnIssueOrder{};

    // ── Hook-body signatures (matched to IDA reverse in Offset::Function). ──
    using OnStopCastFn      = void (__fastcall*)(uintptr_t, uintptr_t, uint8_t, uintptr_t, uintptr_t);
    using OnFinishCastFn    = void*(__fastcall*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t);
    using OnBuffAddFn       = void*(__fastcall*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, int, uint64_t);
    using OnSpellImpactFn   = void*(__fastcall*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t);
    using OnCreateObjectFn  = void (__fastcall*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, int, uint64_t);
    using OnGameUpdateFn    = void*(__fastcall*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, int);
    using OnHeroActionFn    = void (__fastcall*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t);
    using OnMinionFollowFn  = void (__fastcall*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t);
    // sub_1FD080: int __fastcall(SpellBook* a1, SpellCastInfo* a2). Used to be hooked
    // via Shadow-VMT swap but on the current build the spell-event vtable pointer
    // is no longer stored persistently on the heap (FindDispatchSlot finds a stale
    // copy that the game zeroes ~1 s after init). Inline+EPT detour is reliable.
    using OnProcessSpellInlineFn = int (__fastcall*)(uintptr_t, uintptr_t);

    // ── Forward decls for piggy-back helpers (defined further down, once
    //    the per-hero state arrays are visible). Hook bodies call these
    //    to turn OnDeath / OnDash / OnStealth into Inline+EPT events
    //    without owning dedicated game-function offsets. Each helper is
    //    a no-op when the hook fires outside a tracked hero. ──
    inline void CheckDeathForHero(uintptr_t hero);
    inline void CheckDashForHero(uintptr_t hero);
    inline void CheckStealthFromBuffAdd(uintptr_t hero, uintptr_t buffPtr);

    // ── Hook bodies. Each fires the matching Events:: id then forwards to
    //    the original through the trampoline (which executes stolen bytes
    //    and JMPs back to `target + stolenSize`). ──

    inline void __fastcall HkOnStopCast(uintptr_t a1, uintptr_t a2,
                                        uint8_t a3, uintptr_t castInfo,
                                        uintptr_t a5) {
        __try {
            if (shim::IsValidPtr(castInfo)) {
                Fire(Offset::Events::OnStopCast, a1, castInfo, -1);
                // Derived: OnChannelEnd fires inline for channeled casts.
                const float cEnd = shim::ReadFloat(
                    castInfo + Offset::SpellCastInfoLayout::ChannelEnd);
                if (cEnd > 0.0f)
                    Fire(Offset::Events::OnChannelEnd, a1, castInfo, -1);
            }
        } __except (1) {}
        InterlockedIncrement64(
            const_cast<long long*>(&g_detOnStopCast.hitCounter));
        if (auto orig = g_detOnStopCast.GetOriginal<OnStopCastFn>()) {
            orig(a1, a2, a3, castInfo, a5);
        }
    }

    // sub_1FD080 — the spell-event wrapper that used to be reached via
    // Shadow-VMT swap. Inline+EPT detour replaces that approach because the
    // dispatch-slot heap copy is no longer stable on the current build.
    //
    // We bump `g_vmtHook.eventCounter` from here so the existing
    // `PollVmtSpellEvents` scan path (which reads that counter and walks
    // hero ActiveSpellCast pointers) keeps driving OnProcessSpell exactly
    // the way the legacy Shadow-VMT trampoline did.
    inline int __fastcall HkOnProcessSpell(uintptr_t a1, uintptr_t a2) {
        InterlockedIncrement64(
            const_cast<long long*>(&g_detOnProcessSpell.hitCounter));
        // Drive PollVmtSpellEvents — it reads g_vmtHook.eventCounter to
        // detect dispatch and then scans heroes for new ActiveSpellCast.
        InterlockedIncrement64(
            reinterpret_cast<long long*>(
                const_cast<long long*>(&g_vmtHook.eventCounter)));
        if (auto orig = g_detOnProcessSpell.GetOriginal<OnProcessSpellInlineFn>()) {
            return orig(a1, a2);
        }
        return 0;
    }

    inline void* __fastcall HkOnFinishCast(uintptr_t a1, uintptr_t castInfo,
                                           uintptr_t a3, uintptr_t a4) {
        __try {
            if (shim::IsValidPtr(castInfo)) {
                // Read slot once for both events below.
                int slot = -1;
                __try {
                    slot = shim::ReadInt(
                        castInfo + Offset::SpellCastInfoLayout::SpellSlot) & 0xFF;
                } __except (1) { slot = -1; }

                Fire(Offset::Events::OnFinishCast, a1, castInfo, slot);

                // OnDoCast piggy-back — fires at the same moment as
                // OnFinishCast. For instant spells this IS the spell-effect
                // moment; for projectile spells the projectile normally
                // spawns slightly earlier (at windup end), but we don't
                // track missile creation directly, so this is the best
                // approximation available from the spell-cast path.
                Fire(Offset::Events::OnDoCast, a1, castInfo, slot);

                // Derived: OnChannelEnd for channeled finishes too.
                const float cEnd = shim::ReadFloat(
                    castInfo + Offset::SpellCastInfoLayout::ChannelEnd);
                if (cEnd > 0.0f)
                    Fire(Offset::Events::OnChannelEnd, a1, castInfo, slot);
            }
        } __except (1) {}
        InterlockedIncrement64(
            const_cast<long long*>(&g_detOnFinishCast.hitCounter));
        if (auto orig = g_detOnFinishCast.GetOriginal<OnFinishCastFn>()) {
            return orig(a1, castInfo, a3, a4);
        }
        return nullptr;
    }

    inline void* __fastcall HkOnBuffAdd(uintptr_t buffMgr, uintptr_t a2,
                                        uintptr_t a3, uintptr_t a4,
                                        int a5, uint64_t a6) {
        __try {
            if (shim::IsValidPtr(buffMgr)) {
                // Game function sub_BF59E0 receives the BuffManager sub-object
                // as its `this` pointer. BuffManagerRuntime::BuffManagerOffset
                // locates it inside the hero; subtracting recovers the hero.
                const auto hero =
                    buffMgr - Offset::BuffManagerRuntime::BuffManagerOffset;

                if (shim::IsValidPtr(hero)) {
                    // Unified buff-state-change event. The BuffManager
                    // dispatcher fires for add, stack-refresh, and
                    // duration-refresh all through this same call path —
                    // treating them as a single OnBuffUpdate avoids the
                    // unreliable add-vs-lose distinction and matches the
                    // behaviour SDK consumers actually rely on.
                    Fire(Offset::Events::OnBuffUpdate, hero, a2, a5);

                    // OnDeath piggy-back — buff dispatches fire regularly
                    // for every hero (poisons, DOTs, passive stacks) so
                    // this is the cheapest way to catch the alive→dead
                    // edge within a frame, with zero polling overhead.
                    CheckDeathForHero(hero);

                    // OnStealth piggy-back — if the just-added buff is a
                    // known stealth/invisible/camouflage buff, fire the
                    // derived event once. `a2` is the BuffData* passed
                    // straight through so the helper can read the name.
                    CheckStealthFromBuffAdd(hero, a2);
                }

                // OnRecall / OnTeleport were removed intentionally — use
                // `AIHeroClient::HasBuff("Recall")` / `IsRecalling()` on
                // the consumer side; that's strictly more reliable than
                // a derived transition event.
            }
        } __except (1) {}
        InterlockedIncrement64(
            const_cast<long long*>(&g_detOnBuffAdd.hitCounter));
        if (auto orig = g_detOnBuffAdd.GetOriginal<OnBuffAddFn>()) {
            return orig(buffMgr, a2, a3, a4, a5, a6);
        }
        return nullptr;
    }

    inline void* __fastcall HkOnSpellImpact(uintptr_t a1, uintptr_t a2,
                                            uintptr_t a3, uintptr_t a4) {
        // No dedicated Events:: id; track via hit counter only for now.
        InterlockedIncrement64(
            const_cast<long long*>(&g_detOnSpellImpact.hitCounter));
        if (auto orig = g_detOnSpellImpact.GetOriginal<OnSpellImpactFn>()) {
            return orig(a1, a2, a3, a4);
        }
        return nullptr;
    }

    inline void __fastcall HkOnCreateObject(uintptr_t a1, uintptr_t a2,
                                            uintptr_t a3, uintptr_t objPtr,
                                            int a5, uint64_t a6) {
        InterlockedIncrement64(
            const_cast<long long*>(&g_detOnCreateObject.hitCounter));
        if (auto orig = g_detOnCreateObject.GetOriginal<OnCreateObjectFn>()) {
            orig(a1, a2, a3, objPtr, a5, a6);
        }
    }

    inline void* __fastcall HkOnGameUpdate(uintptr_t a1, uintptr_t a2,
                                           uintptr_t a3, uintptr_t a4, int a5) {
        // VERY hot path — skip Fire, just bump counter.
        InterlockedIncrement64(
            const_cast<long long*>(&g_detOnGameUpdate.hitCounter));
        if (auto orig = g_detOnGameUpdate.GetOriginal<OnGameUpdateFn>()) {
            return orig(a1, a2, a3, a4, a5);
        }
        return nullptr;
    }

    inline void __fastcall HkOnHeroActionState(uintptr_t a1, uintptr_t a2,
                                               uintptr_t a3, uintptr_t a4) {
        __try {
            if (shim::IsValidPtr(a1)) {
                Fire(Offset::Events::OnNewPath, a1, a2, 0);
                // Second OnDeath piggy-back site. Action-state changes
                // (move orders, stops, path rebuilds) happen on essentially
                // every interaction with a hero — catching the IsDead edge
                // here gives us sub-frame latency even before the next
                // buff tick runs.
                CheckDeathForHero(a1);
                // OnDash piggy-back. Dashes show up on this hook because
                // they ARE an action-state change (the hero's path is
                // rebuilt as a dash trajectory). Edge-detect the AiMgr
                // `IsDashing` byte.
                CheckDashForHero(a1);
                // OnIntegerPropertyChange piggy-back. The current ActionState
                // int is forwarded raw — the SDK PropertyTracker dedupes by
                // comparing against its cached previous value per-hero.
                __try {
                    const int actionState = shim::ReadInt(
                        a1 + Offset::AttackableUnit::ActionState1);
                    Fire(Offset::Events::OnIntegerPropertyChange, a1, 0, actionState);
                } __except (1) {}
            }
        } __except (1) {}
        InterlockedIncrement64(
            const_cast<long long*>(&g_detOnHeroActionState.hitCounter));
        if (auto orig = g_detOnHeroActionState.GetOriginal<OnHeroActionFn>()) {
            orig(a1, a2, a3, a4);
        }
    }

    inline void __fastcall HkOnMinionFollow(uintptr_t a1, uintptr_t a2,
                                            uintptr_t a3, uintptr_t a4) {
        InterlockedIncrement64(
            const_cast<long long*>(&g_detOnMinionFollow.hitCounter));
        if (auto orig = g_detOnMinionFollow.GetOriginal<OnMinionFollowFn>()) {
            orig(a1, a2, a3, a4);
        }
    }

    // ── IssueOrder logger ──
    //
    // Called from our raw-asm trampoline installed at `ControlRuntime::IssueOrder`
    // (sub_2AFE10). The trampoline has already saved and restored XMM0-3 and
    // RCX/RDX/R8/R9 around this call, so we can run regular C++ here without
    // fear of clobbering the caller's float position args.
    //
    // Real IssueOrder prologue decoded from dump:
    //     mov [rsp+0x20], r9    ; save R9 (pos) to shadow slot 3
    //     mov [rsp+0x18], r8    ; save R8 (target) to shadow slot 2
    //     mov [rsp+0x10], dl    ; save DL (order byte) to shadow slot 1
    //
    // So the real signature of the integer args is:
    //     (void* self, uint8_t orderType, void* target, Vector3* position)
    //
    // The full `rdx` we receive may have non-zero upper bytes from prior code
    // (depending on how the caller loaded it) — mask to DL only.
    inline void __fastcall HkOnIssueOrder_Logger(uintptr_t self,
                                                  uint64_t orderQW,
                                                  uintptr_t target,
                                                  uintptr_t position) {
        InterlockedIncrement64(
            const_cast<long long*>(&g_detOnIssueOrder.hitCounter));
        __try {
            if (shim::IsValidPtr(self)) {
                const int order = (int)(orderQW & 0xFF);
                Fire(Offset::Events::OnIssueOrder, self, target, order);
                // `position` (R9) is intentionally not forwarded in the
                // callback signature — consumers that need the Vector3
                // can read it directly off the hero/control struct.
                (void)position;
            }
        } __except (1) {}
    }

} // namespace detail — close early so we can declare the exported arrays
  // (they need to live at CoreEventHook:: scope, not inside `detail`), then
  // reopen the namespace for the install helpers that touch them.

// ── Inline-hook install telemetry (defined here so `InstallAllInlineDetours`
//    below can see it). One byte per event (0=not attempted, 1=OK,
//    2..6=fail reason). Scannable from CE via the "NSHK" marker. ──
extern "C" __declspec(dllexport) volatile uint8_t g_InlineStatus[12] = {
    0x4E, 0x53, 0x48, 0x4B, // "NSHK" marker start
    0, 0, 0, 0, 0, 0, 0, 0, // 8 status bytes
};

namespace detail {

    // ── Bulk install / uninstall. Returns number of detours successfully
    //    installed (so the caller can log "n/8 hooks live"). Each per-detour
    //    result code is mirrored into `g_InlineStatus[4..11]` for external
    //    inspection (SDK Diagnostics panel + CE scan). ──
    inline int InstallAllInlineDetours() {
        // Reset status bytes.
        for (int i = 0; i < 8; ++i) g_InlineStatus[4 + i] = kInst_NotAttempted;

        const auto base = shim::GetGameBase();
        if (!base) return 0;

        // Probe page-protection bypass strategy. The result is read by the
        // diagnostics panel so the operator can see whether Packman blocks
        // RWX flips only or every flip.
        (void)stealth::SelfTest(base + Offset::Function::OnStopCast);
        // Separately probe the NtWriteVirtualMemory bypass — on Win10/11 the
        // write syscall can succeed even when Protect refuses RWX.
        (void)stealth::SelfTestWrite(base + Offset::Function::OnStopCast);
        // CoW probe: can we flip to PAGE_EXECUTE_WRITECOPY so writes create
        // a private page that still executes?
        (void)stealth::SelfTestCoW(base + Offset::Function::OnStopCast);

        // Helper: install + mirror into status slot.
        auto doOne = [&](InlineDetour& d, int rva, uintptr_t hookFn, int slot) -> int {
            const bool ok = d.Install(base + rva, hookFn);
            g_InlineStatus[4 + slot] = d.lastStatus;
            return ok ? 1 : 0;
        };

        int n = 0;
        n += doOne(g_detOnStopCast,        Offset::Function::OnStopCast,              (uintptr_t)&HkOnStopCast,       0);
        n += doOne(g_detOnFinishCast,      Offset::Function::OnFinishCast,            (uintptr_t)&HkOnFinishCast,     1);
        n += doOne(g_detOnBuffAdd,         Offset::Function::OnBuffAdd,               (uintptr_t)&HkOnBuffAdd,        2);
        n += doOne(g_detOnSpellImpact,     Offset::Function::OnSpellImpact,           (uintptr_t)&HkOnSpellImpact,    3);
        n += doOne(g_detOnCreateObject,    Offset::Function::OnCreateObject,          (uintptr_t)&HkOnCreateObject,   4);
        n += doOne(g_detOnGameUpdate,      Offset::Function::OnGameUpdate,            (uintptr_t)&HkOnGameUpdate,     5);
        n += doOne(g_detOnHeroActionState, Offset::Function::OnHeroActionStateChange, (uintptr_t)&HkOnHeroActionState,6);
        n += doOne(g_detOnMinionFollow,    Offset::Function::OnMinionFollowChange,    (uintptr_t)&HkOnMinionFollow,   7);

        // OnProcessSpell — inline detour on the spell-event wrapper
        // (sub_1FD080). Disabled by default while we confirm whether
        // arming this detour regresses other hooks (user report:
        // adding it dropped OnStopCast/OnChannelEnd from ACTIVE).
        // Toggle the flag via `EnableProcessSpellInline(true)` from
        // anywhere (e.g. the diagnostics panel) to rearm.
        if (g_processSpellInlineEnabled) {
            (void)g_detOnProcessSpell.Install(
                base + Offset::SpellEventVMT::WrapperRVA,
                (uintptr_t)&HkOnProcessSpell);
            if (g_detOnProcessSpell.installed) ++n;
        }

        // OnIssueOrder uses a dedicated RawAsmDetour — not tracked via
        // `g_InlineStatus` (only 8 slots there for CE-scan compatibility).
        // Its state lives in `g_detOnIssueOrder.{installed, lastStatus}`.
        if (g_detOnIssueOrder.Install(base + Offset::ControlRuntime::IssueOrder,
                                       (uintptr_t)&HkOnIssueOrder_Logger)) {
            ++n;
        }
        return n;
    }

    inline void UninstallAllInlineDetours() {
        g_detOnProcessSpell   .Uninstall();
        g_detOnStopCast       .Uninstall();
        g_detOnFinishCast     .Uninstall();
        g_detOnBuffAdd        .Uninstall();
        g_detOnSpellImpact    .Uninstall();
        g_detOnCreateObject   .Uninstall();
        g_detOnGameUpdate     .Uninstall();
        g_detOnHeroActionState.Uninstall();
        g_detOnMinionFollow   .Uninstall();
        g_detOnIssueOrder     .Uninstall();
    }

    // InstallAllDepHooks was REMOVED together with namespace dephook.
    // See the REMOVED comment block near the top of this file for why.

} // namespace detail

// ── DBVM cloak manifest ──
// `cloak_events.lua` (run from Cheat Engine **before** DLL injection) uses
// the same hard-coded RVAs; the array below is kept as a machine-readable
// copy so an external tool could scan for it by pattern if needed.
// A magic sentinel bracket makes that scan trivial.
extern "C" __declspec(dllexport) const uintptr_t g_CloakRVAs[] = {
    0x4E49474854534841ULL, // "NIGHTSHA" - signature byte 1
    Offset::Function::OnStopCast,
    Offset::Function::OnFinishCast,
    Offset::Function::OnBuffAdd,
    Offset::Function::OnSpellImpact,
    Offset::Function::OnCreateObject,
    Offset::Function::OnGameUpdate,
    Offset::Function::OnHeroActionStateChange,
    Offset::Function::OnMinionFollowChange,
    0x525020434C4F414BULL, // "RP CLOAK" - signature byte 2
};
extern "C" __declspec(dllexport) const int g_CloakRVACount = 8;

// Names matched 1:1 with g_InlineStatus[4..11].
inline const char* InlineStatusName(int slot) {
    static const char* kNames[8] = {
        "OnStopCast", "OnFinishCast", "OnBuffAdd", "OnSpellImpact",
        "OnCreateObject", "OnGameUpdate", "OnHeroActionStateChange",
        "OnMinionFollowChange",
    };
    return (slot >= 0 && slot < 8) ? kNames[slot] : "?";
}
inline const char* InlineStatusLabel(uint8_t code) {
    switch (code) {
        case detail::kInst_NotAttempted: return "not attempted";
        case detail::kInst_OK:           return "OK";
        case detail::kInst_BadArgs:      return "bad args (null target/fn)";
        case detail::kInst_DecodeFail:   return "decoder fail";
        case detail::kInst_VAllocFail:   return "VirtualAlloc fail";
        case detail::kInst_VProtectFail: return "VirtualProtect fail";
        case detail::kInst_SehFault:     return "SEH fault (page unreadable)";
        default:                         return "?";
    }
}

// ── Diagnostics accessors for the inline-detour hooks ──
namespace diagnostics {
    inline bool InlineHookInstalled(int eventId) {
        using namespace Offset::Events;
        switch (eventId) {
            case OnStopCast:   return detail::g_detOnStopCast.installed;
            case OnFinishCast: return detail::g_detOnFinishCast.installed;
            // OnBuffUpdate rides the OnBuffAdd detour (HkOnBuffAdd fires it).
            case OnBuffUpdate: return detail::g_detOnBuffAdd.installed;
            case OnNewPath:    return detail::g_detOnHeroActionState.installed;
            // OnDeath is reachable from EITHER the OnBuffAdd or the
            // OnHeroActionState detour (both call CheckDeathForHero).
            case OnDeath:      return detail::g_detOnBuffAdd.installed ||
                                      detail::g_detOnHeroActionState.installed;
            case OnIssueOrder: return detail::g_detOnIssueOrder.installed;
            // OnDash piggy-backs on HkOnHeroActionState (CheckDashForHero).
            case OnDash:       return detail::g_detOnHeroActionState.installed;
            // OnStealth piggy-backs on HkOnBuffAdd (CheckStealthFromBuffAdd).
            case OnStealth:    return detail::g_detOnBuffAdd.installed;
            // OnDoCast fires inside HkOnFinishCast body.
            case OnDoCast:     return detail::g_detOnFinishCast.installed;
            default:           return false;
        }
    }
    inline uint64_t InlineHookHits(int eventId) {
        using namespace Offset::Events;
        switch (eventId) {
            case OnStopCast:   return (uint64_t)detail::g_detOnStopCast.hitCounter;
            case OnFinishCast: return (uint64_t)detail::g_detOnFinishCast.hitCounter;
            // Counter = OnBuffAdd detour hits (the carrier for OnBuffUpdate).
            case OnBuffUpdate: return (uint64_t)detail::g_detOnBuffAdd.hitCounter;
            case OnNewPath:    return (uint64_t)detail::g_detOnHeroActionState.hitCounter;
            // OnDeath counter = sum of its two carrier detours' hits; this
            // reflects how often the piggy-back code ran, not how many
            // deaths actually fired (that's FireCountOf(OnDeath)).
            case OnDeath:      return (uint64_t)detail::g_detOnBuffAdd.hitCounter +
                                      (uint64_t)detail::g_detOnHeroActionState.hitCounter;
            case OnIssueOrder: return (uint64_t)detail::g_detOnIssueOrder.hitCounter;
            // Shared carrier counters for the piggy-back events.
            case OnDash:       return (uint64_t)detail::g_detOnHeroActionState.hitCounter;
            case OnStealth:    return (uint64_t)detail::g_detOnBuffAdd.hitCounter;
            case OnDoCast:     return (uint64_t)detail::g_detOnFinishCast.hitCounter;
            default:           return 0;
        }
    }
} // namespace diagnostics

// ============================================================================
// PART B — Per-hero state trackers (drive every polling-based event)
// ============================================================================
namespace detail {

    // Slim per-hero state. After the switch to "inline hooks only" (no
    // fallback polling), fields are limited to edge-detect caches used by
    // inline-hook piggy-back helpers (CheckDeath / CheckDash / …). The
    // old fields (prevCast / prevBuffs / prevAnim / prevWaypointCount /
    // prevAutoAttack) all belonged to the deleted PollSpellCast /
    // PollBuffs / PollLife / PollAutoAttack routines.
    struct HeroState {
        uintptr_t hero           = 0;          // AIHeroClient* this slot maps to
        uint8_t   prevIsDead     = 0;          // last observed IsDead byte
        uint8_t   prevIsDashing  = 0;          // last observed AiMgr.IsDashing
        uint8_t   prevIsStealth  = 0;          // last observed stealth-buff state
        uint8_t   actionStateInit = 0;         // 1 once prevActionState has been seeded
        int       prevActionState = 0;         // last observed AttackableUnit::ActionState1

        // ── Buff-poll fallback (OnBuffUpdate carrier) ──────────────────────
        // The inline detour on `sub_BF59E0` doesn't fire on this build
        // (IDA shows zero direct callers — function is dispatched via
        // some other path we haven't recovered). Instead we walk the
        // BuffManager entry array each tick and fire OnBuffUpdate on any
        // (ptr / stacks / endTime) edge. Capacity matches the maximum
        // number of concurrent buffs the engine keeps per hero.
        static constexpr int kMaxBuffs = 96;
        struct BuffSlot {
            uintptr_t ptr      = 0;
            uint16_t  stacks   = 0;
            uint32_t  endTime  = 0;   // float bits — exact compare is fine
        };
        BuffSlot prevBuffs[kMaxBuffs] = {};
        int      prevBuffCount        = 0;

        // ── OnNewPath poll-fallback fingerprint ────────────────────────────
        // sub_EBA680 (Offset::Function::OnHeroActionStateChange) has zero
        // direct callers in IDA on this build — exact same situation as
        // the buff-add detour. We instead fingerprint the AiMgr Inner
        // path state (PathEnd Vec3 + SegmentsCount) and fire OnNewPath
        // whenever the fingerprint changes.
        uint32_t prevPathEndX  = 0;
        uint32_t prevPathEndY  = 0;
        uint32_t prevPathEndZ  = 0;
        uint32_t prevSegCount  = 0;
        uint8_t  havePathSig   = 0;
    };

    inline HeroState g_heroStates[ShadowVMT::kMaxTrackedHeroes] = {};

    // ------------------------------------------------------------------------
    // Iterate all heroes via the HeroManager ManagerList and invoke `fn(hero, slot)`.
    // `slot` is the index into g_heroStates (0..kMaxTrackedHeroes-1).
    // ------------------------------------------------------------------------
    // Resolve the local-player pointer directly from its global slot. Used
    // by probe-log filters so that diagnostic output targets only the
    // user's own hero (regardless of where in the hero ObjectManager list
    // they happen to land — slot 0 is NOT guaranteed to be self).
    inline uintptr_t GetLocalHero() {
        const auto base = shim::GetGameBase();
        if (!base) return 0;
        const auto playerSlot = base + Offset::GameObjectsRuntime::Player;
        return shim::ReadPtr(playerSlot);
    }

    template <typename Fn>
    inline void ForEachHero(Fn fn) {
        const auto base = shim::GetGameBase();
        if (!base) return;

        const auto managerPtrSlot = base + Offset::GameObjectsRuntime::Heroes;
        const auto heroManager    = shim::ReadPtr(managerPtrSlot);
        if (!shim::IsValidPtr(heroManager)) return;

        __try {
            const auto items = shim::ReadPtr(heroManager + Offset::ObjectManagerRuntime::ManagerListItems);
            const auto size  = shim::ReadInt(heroManager + Offset::ObjectManagerRuntime::ManagerListSize);
            if (!shim::IsValidPtr(items) || size <= 0) return;

            const int cap = (int)ShadowVMT::kMaxTrackedHeroes;
            const int n   = size < cap ? size : cap;
            for (int i = 0; i < n; ++i) {
                const auto hero = shim::ReadPtr(items + i * (int)sizeof(uintptr_t));
                if (shim::IsValidPtr(hero)) fn(hero, i);
            }
        } __except (1) {}
    }

    // ------------------------------------------------------------------------
    // Same as ForEachHero but reads from the Turret ObjectManager instead.
    // Turrets share the AIBaseClient layout so their ActiveSpellCast slot
    // is at the same offset — this is what lets PollVmtSpellEvents reuse
    // the same scan for OnTurretAttack.
    //
    // `slot` is a per-turret index (0..kMaxTrackedTurrets-1); it is NOT
    // an index into g_heroStates.
    // ------------------------------------------------------------------------
    constexpr int kMaxTrackedTurrets = 32;

    // Parallel prev-cast cache for turrets (mirrors
    // `g_vmtHook.prevCasts` but keyed by turret slot).
    inline uintptr_t g_prevTurretCasts[kMaxTrackedTurrets] = {};

    template <typename Fn>
    inline void ForEachTurret(Fn fn) {
        const auto base = shim::GetGameBase();
        if (!base) return;

        const auto managerPtrSlot = base + Offset::GameObjectsRuntime::Turrets;
        const auto turretMgr      = shim::ReadPtr(managerPtrSlot);
        if (!shim::IsValidPtr(turretMgr)) return;

        __try {
            const auto items = shim::ReadPtr(turretMgr + Offset::ObjectManagerRuntime::ManagerListItems);
            const auto size  = shim::ReadInt(turretMgr + Offset::ObjectManagerRuntime::ManagerListSize);
            if (!shim::IsValidPtr(items) || size <= 0) return;

            const int cap = kMaxTrackedTurrets;
            const int n   = size < cap ? size : cap;
            for (int i = 0; i < n; ++i) {
                const auto turret = shim::ReadPtr(items + i * (int)sizeof(uintptr_t));
                if (shim::IsValidPtr(turret)) fn(turret, i);
            }
        } __except (1) {}
    }

    // ------------------------------------------------------------------------
    // Consumer-side buff helper. Kept independent of the (now-deleted) poll
    // machinery because script code still calls it to probe buff presence
    // without waiting for a state-change event — e.g. "am I recalling?",
    // "does the enemy still have Zhonya?".
    // ------------------------------------------------------------------------

    // Forward decl — defined later in this header.
    inline bool ReadBuffName(uintptr_t buffPtr, char* out, int maxOut);

    // Walk the hero's BuffManager entries and return true if any live buff
    // (buffPtr valid + not expired) matches `name` exactly.
    //
    // This replaces the old "name-match on Add" approach — that only fired
    // when the inline OnBuffAdd hook saw the buff being inserted, which on
    // current builds can miss the event entirely. Polling the list instead
    // makes detection robust: if the buff is present at any tick, we see it.
    //
    // Matches the game's internal GetBuffByName (sub_8D0090) behaviour, but
    // doesn't touch refcounts, so it's safe to call every tick.
    inline bool HasBuffByName(uintptr_t hero, const char* name, float gameTime) {
        const auto mgr      = hero + Offset::BuffManagerRuntime::BuffManagerOffset;
        const auto arrBegin = shim::ReadPtr(mgr + Offset::BuffManagerLayout::EntriesStart);
        const auto arrEnd   = shim::ReadPtr(mgr + Offset::BuffManagerLayout::EntriesEnd);
        if (!shim::IsValidPtr(arrBegin) || arrEnd <= arrBegin) return false;

        const size_t stride = Offset::BuffEntryLayout::EntryStride;
        const size_t total  = (arrEnd - arrBegin) / stride;
        const size_t n      = total > 64 ? 64 : total;
        for (size_t i = 0; i < n; ++i) {
            const auto entry   = arrBegin + i * stride;
            const auto buffPtr = shim::ReadPtr(entry + Offset::BuffEntryLayout::EntryBuff);
            if (!shim::IsValidPtr(buffPtr)) continue;
            // Skip expired entries — on some builds the manager keeps them
            // in the array briefly before compacting.
            if (gameTime > 0.0f) {
                const float endT = shim::ReadFloat(buffPtr + Offset::BuffDataLayout::BuffEndTime);
                if (endT > 0.0f && endT < gameTime) continue;
            }
            char buf[64]{};
            if (!ReadBuffName(buffPtr, buf, (int)sizeof(buf))) continue;

            size_t k = 0;
            while (buf[k] && name[k] && buf[k] == name[k]) ++k;
            if (buf[k] == 0 && name[k] == 0) return true;
        }
        return false;
    }

    // ------------------------------------------------------------------------
    // Inline hook helpers — no polling.
    //
    // `FindHeroSlot` is a linear scan of `g_heroStates` by `.hero`. It's how
    // inline hook bodies (HkOnBuffAdd / HkOnHeroActionState) map an incoming
    // hero pointer to our per-hero death-edge cache. Returns -1 when the
    // hero isn't seeded yet (the per-frame `SyncHeroSlots` pass seeds all
    // slots; a -1 result just means "first frame after spawn" and the
    // caller no-ops safely).
    // ------------------------------------------------------------------------
    inline int FindHeroSlot(uintptr_t hero) {
        for (int i = 0; i < (int)ShadowVMT::kMaxTrackedHeroes; ++i) {
            if (g_heroStates[i].hero == hero) return i;
        }
        return -1;
    }

    // Alive→dead edge detector. The naive `hero + 0x250` byte read does
    // NOT work on the current build — that field is obfuscated through
    // the generic XOR-decoder at sub_2AB1B0 (table-driven; reading the
    // raw byte yields a scrambled value that's typically nonzero even
    // for living heroes, so the false→true edge fires once at first
    // observation and then never again).
    //
    // Instead we drive death detection from `AttackableUnit::HP`
    // (offset 0x1080, IEEE-754 float). The game writes the actual
    // current-HP value here in plaintext, so:
    //
    //     HP <= 0   → dead
    //     HP  > 0   → alive
    //
    // This mirrors what the SDK's PropertyTracker would observe and
    // correctly handles respawn (HP jumps from 0 back to fullHP) as a
    // dead→alive edge. We log both edges to make manual verification
    // easy; only the alive→dead edge fires `OnDeath`.
    inline void CheckDeathForHero(uintptr_t hero) {
        if (!shim::IsValidPtr(hero)) return;
        const int slot = FindHeroSlot(hero);
        if (slot < 0) return;
        auto& st = g_heroStates[slot];
        __try {
            const float hp = shim::ReadFloat(
                hero + Offset::AttackableUnit::HP);
            // Reject obviously bogus reads (NaN, negative-huge from
            // garbage memory) so we don't flap the edge on transient
            // corruption.
            const uint8_t dead = (hp > 0.0f) ? 0 : (hp >= -1.0f ? 1 : 0xFF);
            if (dead == 0xFF) return;   // bogus; ignore this tick
            if (dead != st.prevIsDead) {
                if (dead) {
                    Fire(Offset::Events::OnDeath, hero, 0, 0);
                    dbglog::Log("[Death] hero=%p slot=%d alive->dead hp=%.1f",
                                (void*)hero, slot, hp);
                } else {
                    dbglog::Log("[Death] hero=%p slot=%d respawn dead->alive hp=%.1f",
                                (void*)hero, slot, hp);
                }
                st.prevIsDead = dead;
            }
        } __except (1) {}
    }

    // OnIntegerPropertyChange edge detector. Reads
    // `hero + AttackableUnit::ActionState1` (offset 0x1470, plaintext
    // int — verified against `CoreObjects::ObjectRef::GetActionState`
    // and the in-game `sub_EBA680` hook which compares
    // `*(_DWORD*)(sub_object + 0xC8)`). Fires
    // `Offset::Events::OnIntegerPropertyChange` on every change with
    // the new value as `intParam`.
    //
    // The first observation per hero seeds `prevActionState` without
    // firing — otherwise every newly-tracked hero would trigger a fake
    // change event on its first poll tick.
    //
    // PropertyTracker (the SDK-side consumer) additionally dedupes per
    // (hero, property) pair, so even if this function were called
    // unguarded, no double-dispatch would happen — but firing only on
    // edge here keeps Fire() / dispatch overhead minimal when 10
    // heroes are being scanned every tick.
    // ── Wide-range ActionState offset scanner ──
    // Scans hero+0xE00..hero+0x1800 (DWORD-aligned) looking for fields
    // whose value changes when the hero moves / attacks / casts.
    // Only runs on the LOCAL hero to limit overhead.
    static constexpr uint32_t kProbeStart  = 0xE00;
    static constexpr uint32_t kProbeEnd    = 0x1800;
    static constexpr uint32_t kProbeStep   = 4;       // DWORD aligned
    static constexpr int      kProbeSlots  = (kProbeEnd - kProbeStart) / kProbeStep;

    inline uint32_t g_probePrev[kProbeSlots] = {};
    inline uint8_t  g_probeInited = 0;

    inline void CheckActionStateForHero(uintptr_t hero) {
        if (!shim::IsValidPtr(hero)) return;
        const int slot = FindHeroSlot(hero);
        if (slot < 0) return;
        auto& st = g_heroStates[slot];
        __try {
            // ── Wide-range ActionState offset scanner (DISABLED) ──
            // This reverse-engineering probe iterated 640 DWORD slots on the
            // local hero every single tick, SEH-guarded each read, and on any
            // change issued a synchronous CreateFile+WriteFile+CloseHandle to
            // C:\Users\Public\nightsharp_dephook.txt behind a CRITICAL_SECTION.
            // With normal gameplay dozens of fields mutate per frame → dozens
            // of disk-synchronous log writes per frame → overlay drops to
            // ~8-15 FPS. The probe's data-collection purpose is long done
            // (ActionState1 offset is known) so we gate it off at compile
            // time. Re-enable by defining NS_ENABLE_ACTIONSTATE_PROBE when
            // hunting for a new offset.
            #if defined(NS_ENABLE_ACTIONSTATE_PROBE)
            if (hero == GetLocalHero()) {
                if (!g_probeInited) {
                    int candidates = 0;
                    for (int i = 0; i < kProbeSlots; ++i) {
                        uint32_t off = kProbeStart + i * kProbeStep;
                        g_probePrev[i] = (uint32_t)shim::ReadInt(hero + off);
                        if (g_probePrev[i] > 0 && g_probePrev[i] <= 0xFFFF) {
                            dbglog::Log("[ASProbe] seed +0x%X = 0x%08X (%u)",
                                        off, g_probePrev[i], g_probePrev[i]);
                            ++candidates;
                        }
                    }
                    dbglog::Log("[ASProbe] init hero=%p range=0x%X..0x%X candidates=%d",
                                (void*)hero, kProbeStart, kProbeEnd, candidates);
                    g_probeInited = 1;
                } else {
                    for (int i = 0; i < kProbeSlots; ++i) {
                        uint32_t off = kProbeStart + i * kProbeStep;
                        uint32_t cur = (uint32_t)shim::ReadInt(hero + off);
                        if (cur != g_probePrev[i]) {
                            dbglog::Log("[ASProbe] CHG +0x%X: 0x%08X -> 0x%08X",
                                        off, g_probePrev[i], cur);
                            g_probePrev[i] = cur;
                        }
                    }
                }
            }
            #endif

            // ── Normal ActionState event (still uses old offset for now) ──
            const int curState = (int)shim::ReadInt(
                hero + Offset::AttackableUnit::ActionState1);
            if (!st.actionStateInit) {
                st.prevActionState = curState;
                st.actionStateInit = 1;
                return;
            }
            if (curState == st.prevActionState) return;
            Fire(Offset::Events::OnIntegerPropertyChange,
                 hero, 0, curState);
            st.prevActionState = curState;
        } __except (1) {}
    }

    // OnDash edge detector. Reads the decoded inner AiManager dash byte
    // (inner+0x384, matching the 13338 old build) and fires OnDash on the
    // false->true transition. `context` = decoded nav pointer when available.
    inline void CheckDashForHero(uintptr_t hero) {
        if (!shim::IsValidPtr(hero)) return;
        const int slot = FindHeroSlot(hero);
        if (slot < 0) return;
        auto& st = g_heroStates[slot];
        __try {
            const auto inner = shim::DecodeAiMgr(hero);
            if (!shim::IsValidPtr(inner)) return;
            const uint8_t dashState = shim::ReadByte(inner + Offset::AiManagerInnerCompatLayout::IsDashing);
            const uint8_t isDashing = dashState != 0 ? 1 : 0;

            if (isDashing && !st.prevIsDashing) {
                const auto navBase = shim::ResolveAiMgrNavBase(hero);
                Fire(Offset::Events::OnDash, hero, navBase ? navBase : inner, 1);
                if (hero == GetLocalHero()) {
                    dbglog::Log("[Dash] hero=%p slot=%d inner=%p navBase=%p state384=%u",
                                (void*)hero, slot, (void*)inner, (void*)navBase,
                                static_cast<unsigned>(dashState));
                }
            }
            st.prevIsDashing = isDashing;
        } __except (1) {}
    }

    // ── BuffData → name resolver ──
    //
    // The buff name is NOT stored at `BuffData + 0x8` directly. The
    // correct chain (verified against `core/CoreBuffs.h::ReadName`
    // and confirmed by hex-dumping live BuffData on the current build):
    //
    //     BuffData  + 0x10 (BuffScriptPtr) → ScriptBaseBuff*
    //     ScriptBaseBuff + 0x8             → char*  (raw C-string, NUL-terminated)
    //
    // `Offset::BuffDataLayout::BuffName = 0x8` is therefore the offset
    // of the char* INSIDE ScriptBaseBuff (not BuffData) — keeping it
    // there for symmetry with CoreBuffs but the call sites must do the
    // 2-hop manually. SEH-guarded — on any fault returns `false`.
    inline bool ReadBuffName(uintptr_t buffPtr, char* out, int maxOut) {
        if (!out || maxOut <= 0) return false;
        out[0] = 0;
        if (!shim::IsValidPtr(buffPtr)) return false;
        __try {
            const auto scriptBase = shim::ReadPtr(
                buffPtr + Offset::BuffDataLayout::BuffScriptPtr);   // +0x10
            if (!shim::IsValidPtr(scriptBase)) return false;
            const auto charPtr = shim::ReadPtr(
                scriptBase + Offset::BuffDataLayout::BuffName);     // +0x8
            if (!shim::IsValidPtr(charPtr)) return false;
            const char* src = reinterpret_cast<const char*>(charPtr);
            int i = 0;
            for (; i < maxOut - 1; ++i) {
                const char c = src[i];
                if (!c) break;
                out[i] = c;
            }
            out[i] = 0;
            return i > 0;
        } __except (1) { out[0] = 0; return false; }
    }

    // ASCII-only prefix / contains matcher (case-insensitive). The
    // BuffName table inside LoL is plain ASCII so a tiny hand-rolled
    // matcher is cheaper and safer than pulling in <string>.
    inline bool IcContains(const char* haystack, const char* needle) {
        if (!haystack || !needle) return false;
        for (size_t i = 0; haystack[i]; ++i) {
            size_t j = 0;
            while (needle[j] && haystack[i + j]) {
                char a = haystack[i + j];
                char b = needle[j];
                if (a >= 'A' && a <= 'Z') a = char(a - 'A' + 'a');
                if (b >= 'A' && b <= 'Z') b = char(b - 'A' + 'a');
                if (a != b) break;
                ++j;
            }
            if (!needle[j]) return true;
        }
        return false;
    }

    // Match the just-added buff's name against the static stealth-buff
    // catalog (`StealthBuffCatalog::IsStealthBuff`) and fire OnStealth
    // on the false→true edge of the hero's stealth state. The catalog
    // lives in `core/StealthBuffCatalog.h` and follows the same
    // data-driven pattern as `sdk/Data/DamageData.h` /
    // `sdk/Data/GapcloserData.h`. Adding a new champion's stealth
    // ability is a one-row append in the catalog; this function never
    // needs to change.
    inline void CheckStealthFromBuffAdd(uintptr_t hero, uintptr_t buffPtr) {
        if (!shim::IsValidPtr(hero) || !shim::IsValidPtr(buffPtr)) return;
        const int slot = FindHeroSlot(hero);
        if (slot < 0) return;
        auto& st = g_heroStates[slot];

        char buf[96] = {};
        if (!ReadBuffName(buffPtr, buf, (int)sizeof(buf))) return;

        const auto* entry = StealthBuffCatalog::Find(buf);
        if (!entry) return;
        if (st.prevIsStealth) return;   // already in stealth — no re-fire

        Fire(Offset::Events::OnStealth, hero, buffPtr, 1);
        st.prevIsStealth = 1;
        dbglog::Log("[Stealth] hero=%p slot=%d buff='%s' champ=%s kind=%u",
                    (void*)hero, slot, buf,
                    entry->Champion, (unsigned)entry->Kind);
        // Note: we DON'T clear prevIsStealth here — stealth-out is
        // better detected via a buff-lose signal or explicit
        // `HasBuff(...)` poll on the consumer side. Leaving the
        // edge sticky prevents spurious re-fires when stacking
        // stealth-providing buffs (Eve passive + Eve W).
    }

    // ------------------------------------------------------------------------
    // OnBuffUpdate poll-fallback.
    //
    // The inline detour at `Offset::Function::OnBuffAdd` (sub_BF59E0) never
    // fires on the current build — the function has no direct callers in
    // the IDA database, only data references, so the dispatch is going
    // through some indirect path we haven't recovered. To restore the
    // OnBuffUpdate event surface (which a lot of SDK code depends on)
    // we walk the BuffManager entry list each tick and fire OnBuffUpdate
    // on any (ptr / stacks / endTime) edge.
    //
    // Layout walked here is the legacy contiguous-array layout described
    // in `Offset::BuffManagerLayout` / `Offset::BuffEntryLayout`:
    //   BuffManager + EntriesStart        → first BuffEntry*
    //   BuffManager + EntriesEnd          → one-past-last BuffEntry*
    //   each entry stride = EntryStride   → +0x10
    //   entry + EntryBuff                 → BuffData*
    //
    // Cap reads at `HeroState::kMaxBuffs` so a corrupted Entries pair
    // never spirals into a million-iteration loop.
    inline void PollBuffsForHero(uintptr_t hero, int slot) {
        if (slot < 0 || slot >= (int)ShadowVMT::kMaxTrackedHeroes) return;
        if (!shim::IsValidPtr(hero)) return;
        auto& st = g_heroStates[slot];

        const auto buffMgr = hero + Offset::BuffManagerRuntime::BuffManagerOffset;

        // Self-detection + probe-tick throttle. Computed BEFORE the buff
        // pointer reads so a fault in the read path still produces a
        // probe entry (with a 'fault' tag) — otherwise we'd silently
        // return on every tick and never emit a diagnostic.
        const bool isSelf = (hero == GetLocalHero());
        bool probeNow = false;
        if (isSelf) {
            static int s_probeTick = 0;
            if (++s_probeTick >= 120) {  // ~6-10 s real time (verbose dump)
                s_probeTick = 0;
                probeNow = true;
            }
        }

        uintptr_t entriesStart = 0, entriesEnd = 0;
        bool readOK = false;
        __try {
            entriesStart = shim::ReadPtr(
                buffMgr + Offset::BuffManagerLayout::EntriesStart);
            entriesEnd = shim::ReadPtr(
                buffMgr + Offset::BuffManagerLayout::EntriesEnd);
            readOK = true;
        } __except (1) { readOK = false; }

        // Verbose probe removed — buff-name 2-hop chain (BuffScriptPtr +
        // ScriptBaseBuff::Name) is now implemented via `ReadBuffName()`
        // and exercised on every newly-detected buff via the [Buff+]
        // log inside the diff loop below.
        (void)probeNow;
        if (!readOK) return;

        if (!shim::IsValidPtr(entriesStart) || entriesEnd < entriesStart) return;
        const size_t span = (size_t)(entriesEnd - entriesStart);
        const int    n    = (int)(span / Offset::BuffEntryLayout::EntryStride);
        if (n <= 0) {
            // No buffs anymore — clear cache so re-acquired buffs fire as new.
            if (st.prevBuffCount != 0) {
                for (int j = 0; j < st.prevBuffCount; ++j) {
                    const auto& prev = st.prevBuffs[j];
                    if (prev.ptr) {
                        Fire(Offset::Events::OnBuffUpdate, hero, prev.ptr, 0);
                        InterlockedIncrement64(
                            const_cast<long long*>(&g_detOnBuffAdd.hitCounter));
                    }
                }
                memset(st.prevBuffs, 0, sizeof(st.prevBuffs));
                st.prevBuffCount = 0;
            }
            return;
        }
        const int count = n < HeroState::kMaxBuffs ? n : HeroState::kMaxBuffs;

        // Build current snapshot (ptr/stacks/endTime) for each entry.
        HeroState::BuffSlot current[HeroState::kMaxBuffs] = {};
        int                 curCount = 0;
        for (int i = 0; i < count; ++i) {
            uintptr_t buffPtr = 0;
            __try {
                buffPtr = shim::ReadPtr(
                    entriesStart + i * Offset::BuffEntryLayout::EntryStride
                                 + Offset::BuffEntryLayout::EntryBuff);
            } __except (1) { buffPtr = 0; }
            if (!shim::IsValidPtr(buffPtr)) continue;

            HeroState::BuffSlot s;
            s.ptr = buffPtr;
            __try {
                s.stacks = (uint16_t)(
                    shim::ReadInt(buffPtr + Offset::BuffDataLayout::BuffStacks)
                    & 0xFFFF);
                const float t = shim::ReadFloat(
                    buffPtr + Offset::BuffDataLayout::BuffEndTime);
                memcpy(&s.endTime, &t, sizeof(uint32_t));
            } __except (1) { s.stacks = 0; s.endTime = 0; }
            current[curCount++] = s;
        }

        // Fire OnBuffUpdate for each entry whose (ptr,stacks,endTime) tuple
        // doesn't match anything in `prevBuffs`. This unifies add and
        // refresh into a single event — exactly what the SDK contract
        // promises (see offset.h Events::OnBuffUpdate doc-comment).
        for (int i = 0; i < curCount; ++i) {
            const auto& cur = current[i];
            bool seen = false;
            for (int j = 0; j < st.prevBuffCount; ++j) {
                const auto& prev = st.prevBuffs[j];
                if (prev.ptr == cur.ptr &&
                    prev.stacks == cur.stacks &&
                    prev.endTime == cur.endTime) {
                    seen = true;
                    break;
                }
            }
            if (!seen) {
                Fire(Offset::Events::OnBuffUpdate,
                     hero, cur.ptr, (int)cur.stacks);

                // Probe log: dump the new buff's name so we can identify
                // game-internal buff names that aren't yet in the
                // stealth catalog. Filter to own-hero only (compared
                // against `GetLocalHero()` — slot 0 is NOT necessarily
                // self in the HeroManager list).
                if (hero == GetLocalHero()) {
                    char nameBuf[96] = {};
                    if (ReadBuffName(cur.ptr, nameBuf, (int)sizeof(nameBuf))) {
                        dbglog::Log("[Buff+] slot=%d name='%s' stacks=%u",
                                    slot, nameBuf, (unsigned)cur.stacks);
                    }
                }

                // Drive the OnStealth piggy-back from poll path too, so
                // we don't miss it just because the inline detour is dead.
                CheckStealthFromBuffAdd(hero, cur.ptr);
                // Bump the carrier inline-detour hit counter so the
                // diagnostics panel still shows live activity for the
                // OnBuffUpdate row.
                InterlockedIncrement64(
                    const_cast<long long*>(&g_detOnBuffAdd.hitCounter));
            }
        }

        for (int j = 0; j < st.prevBuffCount; ++j) {
            const auto& prev = st.prevBuffs[j];
            bool stillPresent = false;
            for (int i = 0; i < curCount; ++i) {
                if (current[i].ptr == prev.ptr) {
                    stillPresent = true;
                    break;
                }
            }
            if (!stillPresent) {
                Fire(Offset::Events::OnBuffUpdate, hero, prev.ptr, 0);
                InterlockedIncrement64(
                    const_cast<long long*>(&g_detOnBuffAdd.hitCounter));
            }
        }

        // Update cache.
        memcpy(st.prevBuffs, current, sizeof(current));
        st.prevBuffCount = curCount;
    }

    // ------------------------------------------------------------------------
    // OnNewPath poll-fallback.
    //
    // Fingerprints the inner AiManager `PathEnd` Vec3 + `SegmentsCount`
    // each tick. Any change fires `OnNewPath(hero, nav, segCount)` which
    // matches the inline-detour contract: `intParam` carries the new
    // waypoint count so PathTracker can size its CopyWaypoints buffer.
    //
    // Also pumps `CheckDashForHero` so the OnDash piggy-back continues to
    // function even though the inline OnHeroActionState detour never
    // fires on this build (zero direct callers in IDA — same situation
    // as sub_BF59E0).
    inline void PollPathForHero(uintptr_t hero, int slot) {
        if (slot < 0 || slot >= (int)ShadowVMT::kMaxTrackedHeroes) return;
        if (!shim::IsValidPtr(hero)) return;
        auto& st = g_heroStates[slot];

        uint32_t curX = 0, curY = 0, curZ = 0, curSeg = 0;
        uintptr_t navBase = 0;
        __try {
            navBase = shim::ResolveAiMgrNavBase(hero);
            if (!shim::IsValidPtr(navBase)) return;
            const auto pathState = navBase + Offset::AiManagerNavBaseLayout::PathState;
            const float ex = shim::ReadFloat(
                pathState + Offset::AiManagerPathStateLayout::FallbackEnd + 0);
            const float ey = shim::ReadFloat(
                pathState + Offset::AiManagerPathStateLayout::FallbackEnd + 4);
            const float ez = shim::ReadFloat(
                pathState + Offset::AiManagerPathStateLayout::FallbackEnd + 8);
            memcpy(&curX, &ex, sizeof(uint32_t));
            memcpy(&curY, &ey, sizeof(uint32_t));
            memcpy(&curZ, &ez, sizeof(uint32_t));
            curSeg = (uint32_t)shim::ReadInt(
                pathState + Offset::AiManagerPathStateLayout::Count);
        } __except (1) { return; }

        // Cap segment count to avoid spamming consumers when the field
        // contains garbage (e.g. while a hero is being constructed).
        if (curSeg > 64) curSeg = 64;

        // Edge-detect on the (PathEnd, SegmentsCount) tuple. First sample
        // primes the cache without firing — otherwise every hero spawn
        // would generate a spurious OnNewPath event.
        if (!st.havePathSig) {
            st.prevPathEndX = curX;
            st.prevPathEndY = curY;
            st.prevPathEndZ = curZ;
            st.prevSegCount = curSeg;
            st.havePathSig  = 1;
            return;
        }

        const bool changed =
            curX   != st.prevPathEndX ||
            curY   != st.prevPathEndY ||
            curZ   != st.prevPathEndZ ||
            curSeg != st.prevSegCount;
        if (!changed) return;

        st.prevPathEndX = curX;
        st.prevPathEndY = curY;
        st.prevPathEndZ = curZ;
        st.prevSegCount = curSeg;

        Fire(Offset::Events::OnNewPath, hero, navBase, (int)curSeg);
        // Drive OnDash from the same poll tick so the dash edge is caught
        // when the inline detour is dead.
        CheckDashForHero(hero);
        // Bump the carrier inline-detour hit counter so the diagnostics
        // panel shows OnNewPath / OnDash as ACTIVE.
        InterlockedIncrement64(
            const_cast<long long*>(&g_detOnHeroActionState.hitCounter));
    }

    // ------------------------------------------------------------------------
    // Per-frame hero-slot sync. Mirrors the HeroManager's ManagerList into
    // `g_heroStates[i].hero` so inline hooks can map hero* → slot via
    // `FindHeroSlot`. This is NOT polling — it doesn't read game state or
    // fire events; it's purely a pointer-to-index cache refresh.
    //
    // Must run on the game thread (we piggy-back it on the existing
    // Shadow-VMT tick in `PollVmtSpellEvents`).
    // ------------------------------------------------------------------------
    inline void SyncHeroSlots() {
        ForEachHero([](uintptr_t hero, int slot) {
            if (slot < 0 || slot >= (int)ShadowVMT::kMaxTrackedHeroes) return;
            g_heroStates[slot].hero = hero;
        });
    }

} // namespace detail

// ============================================================================
// PART C — Public API
// ============================================================================

// ── Legacy raw OnProcessSpell sink (kept for the old SDK glue). ──
inline void SetOnProcessSpellCallback(RawProcessSpellCallback cb) {
    detail::g_rawProcessSpellCb = cb;
}

inline bool InstallProcessSpellHook() { return detail::g_vmtHook.Install(); }
inline bool IsProcessSpellHooked()    { return detail::g_vmtHook.installed; }

// ── Shadow-VMT hook path — drives OnProcessSpell when the hook is live. ──
//
// The trampoline shellcode bumps `g_vmtHook.eventCounter` every time the
// real dispatch runs through our shadow vtable. When we see the counter
// move we scan all heroes, find the one whose ActiveSpellCast pointer
// just changed, and fire OnProcessSpell.
//
// This is now the sole driver of OnProcessSpell — the `PollSpellCast` /
// `PollAutoAttack` fallback pollers that used to dedup against this path
// have been deleted.
//
// Auto-attacks are NOT dispatched as a separate event anymore. They flow
// through this same hook; consumers distinguish them from spells by the
// `intParam` argument, which carries `SpellCastInfo::SpellSlot` (64 for
// AA, 0..7 for spells/summoners).
inline void PollVmtSpellEvents() {
    auto& h = detail::g_vmtHook;

    // PERF (May/2026): The full per-hero scan below executes ~5 SEH-guarded
    // sub-checks (PollBuffs / PollPath / CheckDash / CheckDeath /
    // CheckActionState) for every hero every frame, plus the turret pass.
    // The real cost is not the scan itself — it's `Fire()` dispatching each
    // diff edge into the SDK subscribers (Prediction, SpellTracker,
    // BuffTracker, HealthPrediction…), many of which do non-trivial work
    // per callback. Overlay profiling showed 47–140 ms spikes on the
    // render thread with kPollEveryNFrames=6 (every ~100 ms).
    //
    // Bumped to 15 (≈250 ms @60fps) — still well under human reaction time
    // (~200-250 ms) and the downstream consumers only need eventual
    // consistency. This cuts amortized per-second poll/fire cost by ~2.5×
    // vs the previous 6-frame gate. If combat feels unresponsive in
    // extreme corner cases (super-fast cancel chains), flip this back to
    // 6–10; if the overlay thread still stalls, move the poll onto a
    // dedicated low-priority worker thread.
    static int s_pollSkip = 0;
    constexpr int kPollEveryNFrames = 15;
    if (++s_pollSkip < kPollEveryNFrames) return;
    s_pollSkip = 0;

    // Heartbeat: log VMT state every ~1s (60 frames @60fps) regardless of
    // counter movement so we can tell if the hook is installed but the
    // game is not dispatching through our shadow vtable. Also detect
    // anti-tamper: if `*dispatchSlot` reverted to the original vtable
    // (game restored it), automatically re-swap.
    static uint32_t hbFrame = 0;
    if ((++hbFrame % 60) == 0) {
        uintptr_t curSlotVal = 0;
        bool      slotReadable = false;
        if (h.dispatchSlot) {
            __try {
                curSlotVal   = *h.dispatchSlot;
                slotReadable = true;
            } __except (1) { slotReadable = false; }
        }
        const uintptr_t shadowAddr = reinterpret_cast<uintptr_t>(h.shadowTable);
        const bool reverted = slotReadable && curSlotVal == h.originalVtable;
        dbglog::Log("[VMT] hb  inst=%d  cnt=%lld  slot=%p  *slot=%p  shadow=%p  origVT=%p  reverted=%d",
                    (int)h.installed, (long long)h.eventCounter,
                    (void*)h.dispatchSlot,
                    (void*)curSlotVal,
                    (void*)shadowAddr,
                    (void*)h.originalVtable,
                    (int)reverted);
        // Auto re-swap if anti-tamper restored the original vtable.
        if (reverted && h.installed && h.dispatchSlot) {
            __try { *h.dispatchSlot = shadowAddr; } __except (1) {}
            dbglog::Log("[VMT]  re-swapped dispatch slot back to shadow");
        }
    }
    // PURE-POLL DRIVER (no hook required).
    //
    // Both Shadow-VMT (build-mới slot không stable) and Inline+EPT detour
    // on sub_1FD080 (game không call trực tiếp) đều fail trên LoL hiện tại.
    // OnProcessSpell signal được lấy từ heroes' ActiveSpellCast pointer:
    // mỗi tick scan, nếu pointer đổi thì fire event. Không cần counter gate.
    //
    // Periodic log (every ~1 s = 60 frames) so we can verify the scan is
    // running even if no spells are cast.
    static uint32_t scanLogFrame = 0;
    static int      heroesScannedAccum = 0;
    static int      castChangesAccum   = 0;
    int        heroesScannedThisTick = 0;
    int        castChangesThisTick   = 0;
    detail::ForEachHero([&](uintptr_t hero, int slot) {
        ++heroesScannedThisTick;
        if (slot < 0 || slot >= detail::ShadowVMT::kMaxTrackedHeroes) return;

        // OnBuffUpdate poll-fallback — the inline detour at sub_BF59E0 is
        // dead on this build; this restores the OnBuffUpdate event surface
        // by walking the BuffManager entry array directly.
        detail::PollBuffsForHero(hero, slot);

        // OnNewPath poll-fallback — the inline detour at sub_EBA680 is
        // dead on this build for the same reason; fingerprint the AiMgr
        // PathEnd/SegmentsCount tuple and fire OnNewPath on edge.
        detail::PollPathForHero(hero, slot);

        // OnDash poll — runs every tick (not gated on path change). Some
        // dashes (Tristana W onto a target, Riven 3rd Q upward) flip the
        // IsDashing byte without producing a fresh PathEnd/SegmentsCount
        // tuple, so the dash edge would be missed if we only checked it
        // from PollPathForHero.
        detail::CheckDashForHero(hero);

        // OnDeath poll — same reasoning as OnDash. The inline detours
        // that originally drove `CheckDeathForHero` (HkOnBuffAdd /
        // HkOnHeroActionState) are dead on this build, so we drive the
        // alive→dead edge detector from the per-tick poll loop instead.
        // Cheap: a single byte read on `hero + Offset::All::Dead`.
        detail::CheckDeathForHero(hero);

        // OnIntegerPropertyChange poll — `HkOnHeroActionState` (the
        // original carrier) is dead on this build (zero direct callers
        // in IDA, same situation as OnBuffAdd / OnHeroActionState). We
        // poll `AttackableUnit::ActionState1` per tick and fire the
        // event on every change. Single dword read; PropertyTracker
        // dedupes downstream so EnsoulSharp ports that subscribe to
        // OnIntegerPropertyChange behave identically to the legacy
        // push-driven path.
        detail::CheckActionStateForHero(hero);

        const auto activeCast = shim::ReadPtr(hero + Offset::SpellRuntime::ActiveSpellCast);
        if (!shim::IsValidPtr(activeCast)) return;
        if (activeCast == detail::g_vmtHook.prevCasts[slot]) return;

        detail::g_vmtHook.prevCasts[slot] = activeCast;
        ++castChangesThisTick;

        int   curSlot = -1;
        float curCEnd = 0.0f;
        __try {
            curSlot = shim::ReadInt(activeCast + Offset::SpellCastInfoLayout::SpellSlot) & 0xFF;
            curCEnd = shim::ReadFloat(activeCast + Offset::SpellCastInfoLayout::ChannelEnd);
        } __except (1) { curSlot = -1; curCEnd = 0.0f; }

        // Drive both the legacy raw callback AND the SDK Fire dispatch.
        // Also bump the VMT event counter so the diagnostics panel
        // still reports a live "fire count" (back-compat).
        InterlockedIncrement64(
            reinterpret_cast<long long*>(
                const_cast<long long*>(&detail::g_vmtHook.eventCounter)));
        dbglog::Log("[VMT-poll] hero=%p slot=%d cast=%p spellSlot=%d cEnd=%.3f",
                    (void*)hero, slot, (void*)activeCast, curSlot, curCEnd);
        detail::Fire(Offset::Events::OnProcessSpell, hero, activeCast, curSlot);
        if (detail::g_rawProcessSpellCb) {
            detail::g_rawProcessSpellCb(hero, activeCast);
        }

        // Basic attacks flow through the exact same dispatch as spells;
        // `curSlot == 64` identifies an AA. We no longer re-fire a
        // dedicated OnAutoAttack event — see offset.h (Events::Id
        // block) for the deprecation rationale and consumer pattern.

        // OnChannelStart was removed — see offset.h for rationale. The
        // consumer-side pattern is: subscribe to OnProcessSpell and examine
        // `activeCast->ChannelEnd` at callback time.
        (void)curCEnd;
    });

    heroesScannedAccum += heroesScannedThisTick;
    castChangesAccum   += castChangesThisTick;
    if ((++scanLogFrame % 300) == 0) {   // every ~5 s
        dbglog::Log("[VMT-poll] 5s summary: heroesScanned=%d castChanges=%d totalCount=%lld",
                    heroesScannedAccum, castChangesAccum,
                    (long long)detail::g_vmtHook.eventCounter);
        heroesScannedAccum = 0;
        castChangesAccum   = 0;
    }

    // ── Turret scan ─────────────────────────────────────────────────────
    //
    // Turrets share the AIBaseClient layout and dispatch their attacks
    // through the same spell-cast plumbing as heroes. We don't surface
    // them as OnProcessSpell (that would spam subscribers expecting hero
    // events); instead we emit a dedicated `OnTurretAttack` with the
    // turret as sender and SpellCastInfo as context.
    detail::ForEachTurret([](uintptr_t turret, int slot) {
        if (slot < 0 || slot >= detail::kMaxTrackedTurrets) return;
        const auto activeCast = shim::ReadPtr(
            turret + Offset::SpellRuntime::ActiveSpellCast);
        if (!shim::IsValidPtr(activeCast)) return;
        if (activeCast == detail::g_prevTurretCasts[slot]) return;

        detail::g_prevTurretCasts[slot] = activeCast;

        int curSlot = -1;
        __try {
            curSlot = shim::ReadInt(
                activeCast + Offset::SpellCastInfoLayout::SpellSlot) & 0xFF;
        } __except (1) { curSlot = -1; }

        detail::Fire(Offset::Events::OnTurretAttack, turret, activeCast, curSlot);
    });
}

// ── Install every hook we own (VMT + inline detours + polling state init). ──
//
// Return value is true if the flagship Shadow-VMT install succeeded; inline
// detours are best-effort (any that fail fall through to their polling
// backup) so their per-event status is inspected via `InlineHookInstalled`.
inline bool InstallAllHooks() {
    // Zero state trackers so the first poll tick doesn't spam false events.
    memset(detail::g_heroStates, 0, sizeof(detail::g_heroStates));
    memset(detail::g_prevTurretCasts, 0, sizeof(detail::g_prevTurretCasts));
    const bool vmtOk = detail::g_vmtHook.Install();

    // Try inline detours. On current LoL + Packman this fails with
    // STATUS_SECTION_PROTECTION for every slot — we leave them attempted so
    // the diagnostic panel surfaces the fact.
    (void)detail::InstallAllInlineDetours();

    // DEP hook fallback was removed — see the REMOVED comment block near
    // the top of this file. Only Shadow-VMT + Inline+EPT remain.
    return vmtOk;
}

// InstallDepHooksNow / UninstallDepHooksNow were REMOVED; the overlay
// panel that called them has been deleted as well.

// ── Self-healing install driver.
//
// The dispatch slot for the spell-event vtable is allocated well after
// the overlay starts running, so an `Install()` call from `Overlay::Run`
// may not yet find a writable slot. We retry once per second until the
// slot exists.
//
// NOTE: any "rescan if counter == 0 after N seconds" logic must NOT
// uninstall+reinstall — doing so resets `eventCounter` to 0 every time,
// which loops forever and makes OnProcessSpell appear permanently IDLE.
// (This was the broken Phase-2 SDK behaviour; restored to the legacy
// install-once-then-trust pattern that was verified working.)
inline void DriveHookInstall() {
    static uint32_t frame          = 0;
    static uint32_t lastInstallTry = 0;
    ++frame;

    auto& h = detail::g_vmtHook;
    if (h.installed) return;

    if (frame - lastInstallTry > 60) {   // ~1 retry/s while not installed
        lastInstallTry = frame;
        h.Install();
    }
}

// ── Call every SDK tick.
//
// Now VERY lightweight: only drives the self-healing hook install, walks
// the Shadow-VMT counter for OnProcessSpell (which also carries AA casts
// with intParam==64), and refreshes the hero-pointer-to-slot cache so
// inline hook bodies can resolve the death-edge state. No event polling
// happens here anymore — every event is fired directly from the detour
// that caught it on the game thread.
inline void PollAllEvents() {
    DriveHookInstall();
    PollVmtSpellEvents();

    // Hero-pointer-to-slot cache refresh. Inline detour bodies use this
    // mapping to resolve a hero pointer → state slot; a stale entry just
    // means a missed death/action-state edge for ~1 tick after the hero
    // pointer moves, which is imperceptible. Throttled to 200 ms so the
    // ForEachHero manager walk doesn't run 20× per second on the overlay
    // thread during combat — bumped from 50 ms in May/2026 alongside
    // kPollEveryNFrames 6→15 to reclaim overlay FPS.
    static DWORD s_lastHeroSlotSyncTick = 0;
    const DWORD now = GetTickCount();
    if (now - s_lastHeroSlotSyncTick >= 200) {
        s_lastHeroSlotSyncTick = now;
        detail::SyncHeroSlots();
    }
}

// ── Tear-down. Safe to call if never installed. ──
inline void UninstallAll() {
    detail::UninstallAllInlineDetours();
    detail::g_vmtHook.Uninstall();
    memset(detail::g_heroStates, 0, sizeof(detail::g_heroStates));
    memset(detail::g_callbacks, 0, sizeof(detail::g_callbacks));
    detail::g_rawProcessSpellCb = nullptr;
}

} // namespace CoreEventHook
