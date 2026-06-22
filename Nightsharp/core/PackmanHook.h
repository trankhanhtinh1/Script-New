#pragma once

// ============================================================================
// PackmanHook.h - Packman (Riot anti-cheat) hook detection & direct syscall bypass
// ----------------------------------------------------------------------------
// MUST BE INITIALIZED FIRST after injection (before any other Core module).
//
// Packman hooks ntdll syscall stubs to intercept debugging/memory APIs. The game
// itself uses these same syscalls internally ("hidden syscalls"). Because Packman
// redirects the ntdll exports via jmp [rip+0] trampolines, we cannot call them
// normally. This module:
//
//   1. Detects all known Packman hook sites in ntdll
//   2. Builds clean direct-syscall stubs (mov r10,rcx; mov eax,SSN; syscall; ret)
//      using SSNs extracted from disk or memory — bypassing Packman entirely
//   3. Exposes callable function pointers for each syscall the game uses internally
//
// Hook patterns observed:
//   - 14-byte jmp: FF 25 00 00 00 00 [8-byte absolute address]
//   - Syscall stub redirect: original `4C 8B D1 B8 xx` replaced with jmp
//   - Single-byte patches: 0x4C->0xCC (int3), 0xC3->0xCC, 0xCC->0x90 (nop)
//   - Function kill: 0x48->0xC3 (immediate ret)
//
// Known game-internal syscalls (SSNs from current ntdll):
//   NtContinue               SSN 0x43
//   NtDelayExecution          SSN 0x34
//   NtProtectVirtualMemory    SSN 0x50
//   NtQueryVirtualMemory      SSN 0x23
//   NtSuspendThread           SSN 0x1BE
//   NtContinueEx              SSN 0xA1
//   NtSetContextThread        SSN 0x18D
//   NtGetContextThread        SSN 0xF3
//
// Usage (call IMMEDIATELY after DLL injection, before anything else):
//   PackmanHook::Install();   // detect + build syscall stubs
//   // Now safe to use:
//   PackmanHook::Syscall::NtProtectVirtualMemory(...)
//   PackmanHook::Syscall::NtQueryVirtualMemory(...)
//   // Or check status:
//   bool active = PackmanHook::IsPackmanActive();
// ============================================================================

#include <Windows.h>
#include <cstdint>
#include <cstring>

namespace PackmanHook {

// ── Hook site descriptor ─────────────────────────────────────────────────────

struct HookSite {
    const char* functionName;
    uint32_t    rvaFromNtdll;       // RVA within ntdll (0 = resolve by export name)
    int         offset;             // byte offset from function start
    int         patchSize;          // number of bytes patched
    uint8_t     originalBytes[16];  // expected original bytes (clean ntdll)
    uint8_t     hookedBytes[16];    // expected hooked bytes (Packman active)
    int         originalSize;
    int         hookedSize;
};

// ── Hook identifiers ─────────────────────────────────────────────────────────

enum HookId : int {
    LdrInitializeThunk = 0,
    RtlpAddVectoredHandler,
    NtQueryVirtualMemory,
    NtQueryVirtualMemory_Plus6,
    NtContinue,
    NtContinue_Plus6,
    NtCreateThread,
    NtCreateThread_Plus14,
    NtProtectVirtualMemory,
    NtProtectVirtualMemory_Plus6,
    NtContinueEx,
    NtContinueEx_Plus6,
    NtCreateThreadEx,
    NtCreateThreadEx_Plus14,
    NtGetContextThread,
    NtGetContextThread_Plus7,
    NtSetContextThread,
    NtSuspendThread,
    DbgBreakPoint,
    DbgUserBreakPoint,
    KiUserExceptionDispatch,
    DbgUiRemoteBreakin,
    RtlpQueryProcessDebugInformationRemote,
    HookCount
};

// ── Hook site table ──────────────────────────────────────────────────────────

inline constexpr HookSite kHookSites[HookCount] = {
    // LdrInitializeThunk: 12 bytes
    { "LdrInitializeThunk", 0, 0, 12,
      { 0x40, 0x53, 0x48, 0x83, 0xEC, 0x20, 0x48, 0x8B },
      { 0xFF, 0x25, 0x00, 0x00, 0x00, 0x00, 0x10, 0xFE }, 8, 8 },
    // RtlpAddVectoredHandler: 14 bytes
    { "RtlpAddVectoredHandler", 0, 0, 14,
      { 0x48, 0x89, 0x5C, 0x24, 0x08, 0x48, 0x89, 0x6C },
      { 0xFF, 0x25, 0x00, 0x00, 0x00, 0x00, 0x30, 0xCF }, 8, 8 },
    // NtQueryVirtualMemory: 5 bytes (syscall stub)
    { "NtQueryVirtualMemory", 0, 0, 5,
      { 0x4C, 0x8B, 0xD1, 0xB8, 0x23 },
      { 0xFF, 0x25, 0x00, 0x00, 0x00 }, 5, 5 },
    // NtQueryVirtualMemory+6: 8 bytes
    { "NtQueryVirtualMemory", 0, 6, 8,
      { 0x00, 0x00, 0xF6, 0x04, 0x25, 0x08, 0x03, 0xFE },
      { 0xC0, 0xF9, 0x3A, 0x70, 0xFE, 0x7F, 0x00, 0x00 }, 8, 8 },
    // NtContinue: 5 bytes
    { "NtContinue", 0, 0, 5,
      { 0x4C, 0x8B, 0xD1, 0xB8, 0x43 },
      { 0xFF, 0x25, 0x00, 0x00, 0x00 }, 5, 5 },
    // NtContinue+6: 8 bytes
    { "NtContinue", 0, 6, 8,
      { 0x00, 0x00, 0xF6, 0x04, 0x25, 0x08, 0x03, 0xFE },
      { 0xC0, 0xCD, 0x37, 0x70, 0xFE, 0x7F, 0x00, 0x00 }, 8, 8 },
    // NtCreateThread: 1 byte (0x4C -> 0xCC)
    { "NtCreateThread", 0, 0, 1, { 0x4C }, { 0xCC }, 1, 1 },
    // NtCreateThread+14: 1 byte (0xC3 -> 0xCC)
    { "NtCreateThread", 0, 0x14, 1, { 0xC3 }, { 0xCC }, 1, 1 },
    // NtProtectVirtualMemory: 5 bytes
    { "NtProtectVirtualMemory", 0, 0, 5,
      { 0x4C, 0x8B, 0xD1, 0xB8, 0x50 },
      { 0xFF, 0x25, 0x00, 0x00, 0x00 }, 5, 5 },
    // NtProtectVirtualMemory+6: 8 bytes
    { "NtProtectVirtualMemory", 0, 6, 8,
      { 0x00, 0x00, 0xF6, 0x04, 0x25, 0x08, 0x03, 0xFE },
      { 0xA0, 0x83, 0x46, 0x70, 0xFE, 0x7F, 0x00, 0x00 }, 8, 8 },
    // NtContinueEx: 5 bytes
    { "NtContinueEx", 0, 0, 5,
      { 0x4C, 0x8B, 0xD1, 0xB8, 0xA1 },
      { 0xFF, 0x25, 0x00, 0x00, 0x00 }, 5, 5 },
    // NtContinueEx+6: 8 bytes
    { "NtContinueEx", 0, 6, 8,
      { 0x00, 0x00, 0xF6, 0x04, 0x25, 0x08, 0x03, 0xFE },
      { 0x70, 0xD1, 0x37, 0x70, 0xFE, 0x7F, 0x00, 0x00 }, 8, 8 },
    // NtCreateThreadEx: 1 byte (0x4C -> 0xCC)
    { "NtCreateThreadEx", 0, 0, 1, { 0x4C }, { 0xCC }, 1, 1 },
    // NtCreateThreadEx+14: 1 byte (0xC3 -> 0xCC)
    { "NtCreateThreadEx", 0, 0x14, 1, { 0xC3 }, { 0xCC }, 1, 1 },
    // NtGetContextThread: 5 bytes
    { "NtGetContextThread", 0, 0, 5,
      { 0x4C, 0x8B, 0xD1, 0xB8, 0xF3 },
      { 0xFF, 0x25, 0x00, 0x00, 0x00 }, 5, 5 },
    // NtGetContextThread+7: 7 bytes
    { "NtGetContextThread", 0, 7, 7,
      { 0x00, 0xF6, 0x04, 0x25, 0x08, 0x03, 0xFE },
      { 0xEB, 0x37, 0x70, 0xFE, 0x7F, 0x00, 0x00 }, 7, 7 },
    // NtSetContextThread: 14 bytes
    { "NtSetContextThread", 0, 0, 14,
      { 0x4C, 0x8B, 0xD1, 0xB8, 0x8D, 0x01, 0x00, 0x00 },
      { 0xFF, 0x25, 0x00, 0x00, 0x00, 0x00, 0x20, 0xD5 }, 8, 8 },
    // NtSuspendThread: 14 bytes
    { "NtSuspendThread", 0, 0, 14,
      { 0x4C, 0x8B, 0xD1, 0xB8, 0xBE, 0x01, 0x00, 0x00 },
      { 0xFF, 0x25, 0x00, 0x00, 0x00, 0x00, 0x80, 0xC1 }, 8, 8 },
    // DbgBreakPoint: 1 byte (0xCC -> 0x90)
    { "DbgBreakPoint", 0, 0, 1, { 0xCC }, { 0x90 }, 1, 1 },
    // DbgUserBreakPoint: 1 byte (0xCC -> 0x90)
    { "DbgUserBreakPoint", 0, 0, 1, { 0xCC }, { 0x90 }, 1, 1 },
    // KiUserExceptionDispatcher: 14 bytes (exported as ...Dispatcher, NOT ...Dispatch)
    { "KiUserExceptionDispatcher", 0, 0, 14,
      { 0xFC, 0x48, 0x8B, 0x05, 0xD8, 0xFD, 0x0D, 0x00 },
      { 0xFF, 0x25, 0x00, 0x00, 0x00, 0x00, 0xDB, 0xD4 }, 8, 8 },
    // DbgUiRemoteBreakin: 1 byte (0x48 -> 0xC3)
    { "DbgUiRemoteBreakin", 0, 0, 1, { 0x48 }, { 0xC3 }, 1, 1 },
    // RtlpQueryProcessDebugInformationRemote: 1 byte (0x48 -> 0xC3)
    { "RtlpQueryProcessDebugInformationRemote", 0, 0, 1, { 0x48 }, { 0xC3 }, 1, 1 },
};

// ── Runtime state ────────────────────────────────────────────────────────────

enum HookState : uint8_t {
    State_Unknown   = 0,
    State_Clean     = 1,
    State_Hooked    = 2,
    State_Modified  = 3,
    State_NotFound  = 4,
};

struct HookStatus {
    HookState state;
    uint8_t   liveBytes[16];
    uintptr_t resolvedAddr;
};

inline uintptr_t  g_ntdllBase = 0;
inline HookStatus g_status[HookCount] = {};
inline volatile int g_initialized = 0;
inline volatile int g_installed = 0;

// ═══════════════════════════════════════════════════════════════════════════════
// DIRECT SYSCALL STUB INFRASTRUCTURE
// ═══════════════════════════════════════════════════════════════════════════════
// The game uses many hidden syscalls internally. Packman hooks the ntdll exports
// so we build our own clean stubs:
//   mov r10, rcx       ; 4C 8B D1
//   mov eax, <SSN>     ; B8 xx xx xx xx
//   syscall            ; 0F 05
//   ret                ; C3
// Total: 12 bytes per stub. We allocate a single RWX page for all stubs.

namespace SyscallStubs {

    // Known SSNs (System Service Numbers) for current Windows build.
    // These match the game's internal usage. If Windows updates change SSNs,
    // we resolve dynamically from disk.
    struct SyscallInfo {
        const char* name;
        int         knownSSN;       // known SSN (fallback if resolution fails)
        int         resolvedSSN;    // runtime-resolved SSN (-1 = not yet resolved)
        void*       stubPtr;        // pointer to our clean stub
    };

    enum SyscallId : int {
        SC_NtContinue = 0,
        SC_NtDelayExecution,
        SC_NtProtectVirtualMemory,
        SC_NtQueryVirtualMemory,
        SC_NtSuspendThread,
        SC_NtContinueEx,
        SC_NtSetContextThread,
        SC_NtGetContextThread,
        SC_NtCreateThreadEx,
        SC_NtWriteVirtualMemory,
        SC_NtReadVirtualMemory,
        SC_NtAllocateVirtualMemory,
        SC_NtFreeVirtualMemory,
        SC_Count
    };

    inline SyscallInfo g_syscalls[SC_Count] = {
        { "NtContinue",              0x43,  -1, nullptr },
        { "NtDelayExecution",        0x34,  -1, nullptr },
        { "NtProtectVirtualMemory",  0x50,  -1, nullptr },
        { "NtQueryVirtualMemory",    0x23,  -1, nullptr },
        { "NtSuspendThread",         0x1BE, -1, nullptr },
        { "NtContinueEx",            0xA1,  -1, nullptr },
        { "NtSetContextThread",      0x18D, -1, nullptr },
        { "NtGetContextThread",      0xF3,  -1, nullptr },
        { "NtCreateThreadEx",        0xC7,  -1, nullptr },
        { "NtWriteVirtualMemory",    0x3A,  -1, nullptr },
        { "NtReadVirtualMemory",     0x3F,  -1, nullptr },
        { "NtAllocateVirtualMemory", 0x18,  -1, nullptr },
        { "NtFreeVirtualMemory",     0x1E,  -1, nullptr },
    };

    inline void* g_stubPage = nullptr;   // single RWX allocation for all stubs
    inline volatile int g_stubsBuilt = 0;

    // ── SSN resolution from on-disk ntdll (bypasses Packman hooks completely) ──

    inline DWORD RvaToFileOffset(const IMAGE_NT_HEADERS* nt, DWORD rva) {
        const IMAGE_SECTION_HEADER* sec = IMAGE_FIRST_SECTION(nt);
        for (int i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++sec) {
            if (rva >= sec->VirtualAddress &&
                rva < sec->VirtualAddress + sec->Misc.VirtualSize) {
                return rva - sec->VirtualAddress + sec->PointerToRawData;
            }
        }
        return 0;
    }

    inline int ExtractSSNFromDiskImage(const uint8_t* buf, const char* funcName) {
        if (!buf || !funcName) return -1;
        const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(buf);
        if (dos->e_magic != IMAGE_DOS_SIGNATURE) return -1;
        const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(buf + dos->e_lfanew);
        if (nt->Signature != IMAGE_NT_SIGNATURE) return -1;

        const auto& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
        if (!dir.VirtualAddress) return -1;

        const auto* exp = reinterpret_cast<const IMAGE_EXPORT_DIRECTORY*>(
            buf + RvaToFileOffset(nt, dir.VirtualAddress));
        const auto* names = reinterpret_cast<const DWORD*>(
            buf + RvaToFileOffset(nt, exp->AddressOfNames));
        const auto* ords = reinterpret_cast<const WORD*>(
            buf + RvaToFileOffset(nt, exp->AddressOfNameOrdinals));
        const auto* funcs = reinterpret_cast<const DWORD*>(
            buf + RvaToFileOffset(nt, exp->AddressOfFunctions));

        for (DWORD i = 0; i < exp->NumberOfNames; ++i) {
            const auto* name = reinterpret_cast<const char*>(
                buf + RvaToFileOffset(nt, names[i]));
            if (std::strcmp(name, funcName) != 0) continue;

            const auto* stub = buf + RvaToFileOffset(nt, funcs[ords[i]]);
            // Standard syscall stub pattern: 4C 8B D1 B8 [SSN as DWORD]
            if (stub[0] == 0x4C && stub[1] == 0x8B && stub[2] == 0xD1 && stub[3] == 0xB8) {
                return static_cast<int>(*reinterpret_cast<const uint32_t*>(stub + 4));
            }
            return -1;
        }
        return -1;
    }

    // Try to resolve SSN from in-memory ntdll (works if Packman hasn't
    // patched THIS specific function yet, or if the SSN bytes survived).
    inline int ResolveSSNFromMemory(const char* funcName) {
        HMODULE h = GetModuleHandleW(L"ntdll.dll");
        if (!h) return -1;
        const auto* p = reinterpret_cast<const uint8_t*>(GetProcAddress(h, funcName));
        if (!p) return -1;
        // Check if prologue is intact: mov r10, rcx; mov eax, SSN
        if (p[0] == 0x4C && p[1] == 0x8B && p[2] == 0xD1 && p[3] == 0xB8) {
            return static_cast<int>(*reinterpret_cast<const uint32_t*>(p + 4));
        }
        return -1; // hooked, can't read SSN from memory
    }

    // Read clean ntdll from disk and extract SSN.
    inline int ResolveSSNFromDisk(const char* funcName) {
        HANDLE hFile = CreateFileW(
            L"\\\\?\\C:\\Windows\\System32\\ntdll.dll",
            GENERIC_READ, FILE_SHARE_READ, nullptr,
            OPEN_EXISTING, 0, nullptr);
        if (hFile == INVALID_HANDLE_VALUE) return -1;

        LARGE_INTEGER sz{};
        if (!GetFileSizeEx(hFile, &sz) || sz.QuadPart <= 0 || sz.QuadPart > (64LL << 20)) {
            CloseHandle(hFile);
            return -1;
        }

        auto* buf = reinterpret_cast<uint8_t*>(VirtualAlloc(
            nullptr, static_cast<SIZE_T>(sz.QuadPart),
            MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
        if (!buf) { CloseHandle(hFile); return -1; }

        DWORD total = 0;
        while (total < static_cast<DWORD>(sz.QuadPart)) {
            DWORD chunk = 0;
            if (!ReadFile(hFile, buf + total,
                          static_cast<DWORD>(sz.QuadPart) - total, &chunk, nullptr) || chunk == 0)
                break;
            total += chunk;
        }
        CloseHandle(hFile);

        const int ssn = (total == static_cast<DWORD>(sz.QuadPart))
            ? ExtractSSNFromDiskImage(buf, funcName)
            : -1;
        VirtualFree(buf, 0, MEM_RELEASE);
        return ssn;
    }

    // Resolve SSN: try memory first (fast), then disk (bypasses hooks).
    inline int ResolveSSN(const char* funcName, int knownFallback) {
        int ssn = ResolveSSNFromMemory(funcName);
        if (ssn >= 0) return ssn;

        ssn = ResolveSSNFromDisk(funcName);
        if (ssn >= 0) return ssn;

        // Last resort: use the known SSN from our table
        return knownFallback;
    }

    // ── Build a single 12-byte syscall stub ──────────────────────────────────
    //   mov r10, rcx    ; 4C 8B D1
    //   mov eax, SSN    ; B8 xx xx xx xx
    //   syscall         ; 0F 05
    //   ret             ; C3
    inline void BuildStub(uint8_t* dest, int ssn) {
        dest[0] = 0x4C; dest[1] = 0x8B; dest[2] = 0xD1;          // mov r10, rcx
        dest[3] = 0xB8;                                            // mov eax, ...
        *reinterpret_cast<uint32_t*>(dest + 4) = static_cast<uint32_t>(ssn);
        dest[8] = 0x0F; dest[9] = 0x05;                           // syscall
        dest[10] = 0xC3;                                           // ret
        dest[11] = 0x90;                                           // nop (alignment)
    }

    // ── Build all syscall stubs ──────────────────────────────────────────────
    inline bool BuildAllStubs() {
        if (g_stubsBuilt) return g_stubPage != nullptr;

        // Allocate single page for all stubs (12 bytes each, 16-byte aligned)
        constexpr int kStubSize = 16;  // 12 bytes + 4 padding for alignment
        const SIZE_T pageSize = static_cast<SIZE_T>(SC_Count * kStubSize);

        g_stubPage = VirtualAlloc(nullptr, pageSize,
                                  MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
        if (!g_stubPage) {
            g_stubsBuilt = 1;
            return false;
        }

        auto* base = reinterpret_cast<uint8_t*>(g_stubPage);
        std::memset(base, 0xCC, pageSize); // fill with int3 for safety

        for (int i = 0; i < SC_Count; ++i) {
            auto& sc = g_syscalls[i];

            // Resolve the real SSN
            sc.resolvedSSN = ResolveSSN(sc.name, sc.knownSSN);

            if (sc.resolvedSSN >= 0) {
                uint8_t* stubDest = base + (i * kStubSize);
                BuildStub(stubDest, sc.resolvedSSN);
                sc.stubPtr = stubDest;
            }
        }

        FlushInstructionCache(GetCurrentProcess(), g_stubPage, pageSize);
        g_stubsBuilt = 1;
        return true;
    }

    // ── Get stub pointer by ID ───────────────────────────────────────────────
    inline void* GetStub(SyscallId id) {
        if (id < 0 || id >= SC_Count) return nullptr;
        return g_syscalls[id].stubPtr;
    }

    inline int GetSSN(SyscallId id) {
        if (id < 0 || id >= SC_Count) return -1;
        return g_syscalls[id].resolvedSSN;
    }

    inline const char* GetName(SyscallId id) {
        if (id < 0 || id >= SC_Count) return "?";
        return g_syscalls[id].name;
    }

    inline void Shutdown() {
        for (auto& syscall : g_syscalls) {
            syscall.resolvedSSN = -1;
            syscall.stubPtr = nullptr;
        }
        if (g_stubPage) {
            VirtualFree(g_stubPage, 0, MEM_RELEASE);
            g_stubPage = nullptr;
        }
        g_stubsBuilt = 0;
    }

} // namespace SyscallStubs

// ═══════════════════════════════════════════════════════════════════════════════
// TYPED SYSCALL WRAPPERS
// ═══════════════════════════════════════════════════════════════════════════════
// Direct-callable wrappers using our clean stubs. These bypass Packman entirely.

namespace Syscall {

    using NtProtectFn = LONG(NTAPI*)(HANDLE ProcessHandle, PVOID* BaseAddress,
                                     PSIZE_T RegionSize, ULONG NewProtect, PULONG OldProtect);

    using NtQueryVirtualMemoryFn = LONG(NTAPI*)(HANDLE ProcessHandle, PVOID BaseAddress,
                                                int MemoryInformationClass, PVOID MemoryInformation,
                                                SIZE_T MemoryInformationLength, PSIZE_T ReturnLength);

    using NtContinueFn = LONG(NTAPI*)(PCONTEXT ThreadContext, BOOLEAN RaiseAlert);

    using NtContinueExFn = LONG(NTAPI*)(PCONTEXT ThreadContext, BOOLEAN RaiseAlert);

    using NtDelayExecutionFn = LONG(NTAPI*)(BOOLEAN Alertable, PLARGE_INTEGER DelayInterval);

    using NtSuspendThreadFn = LONG(NTAPI*)(HANDLE ThreadHandle, PULONG PreviousSuspendCount);

    using NtSetContextThreadFn = LONG(NTAPI*)(HANDLE ThreadHandle, PCONTEXT ThreadContext);

    using NtGetContextThreadFn = LONG(NTAPI*)(HANDLE ThreadHandle, PCONTEXT ThreadContext);

    using NtCreateThreadExFn = LONG(NTAPI*)(PHANDLE ThreadHandle, ACCESS_MASK DesiredAccess,
                                            PVOID ObjectAttributes, HANDLE ProcessHandle,
                                            PVOID StartRoutine, PVOID Argument,
                                            ULONG CreateFlags, SIZE_T ZeroBits,
                                            SIZE_T StackSize, SIZE_T MaxStackSize,
                                            PVOID AttributeList);

    using NtWriteVirtualMemoryFn = LONG(NTAPI*)(HANDLE ProcessHandle, PVOID BaseAddress,
                                                PVOID Buffer, SIZE_T NumberOfBytesToWrite,
                                                PSIZE_T NumberOfBytesWritten);

    using NtReadVirtualMemoryFn = LONG(NTAPI*)(HANDLE ProcessHandle, PVOID BaseAddress,
                                               PVOID Buffer, SIZE_T NumberOfBytesToRead,
                                               PSIZE_T NumberOfBytesRead);

    using NtAllocateVirtualMemoryFn = LONG(NTAPI*)(HANDLE ProcessHandle, PVOID* BaseAddress,
                                                   ULONG_PTR ZeroBits, PSIZE_T RegionSize,
                                                   ULONG AllocationType, ULONG Protect);

    using NtFreeVirtualMemoryFn = LONG(NTAPI*)(HANDLE ProcessHandle, PVOID* BaseAddress,
                                               PSIZE_T RegionSize, ULONG FreeType);

    // ── Inline accessors (call through our clean stubs) ──────────────────────

    inline NtProtectFn NtProtectVirtualMemory() {
        return reinterpret_cast<NtProtectFn>(
            SyscallStubs::GetStub(SyscallStubs::SC_NtProtectVirtualMemory));
    }

    inline NtQueryVirtualMemoryFn NtQueryVirtualMemory() {
        return reinterpret_cast<NtQueryVirtualMemoryFn>(
            SyscallStubs::GetStub(SyscallStubs::SC_NtQueryVirtualMemory));
    }

    inline NtContinueFn NtContinue() {
        return reinterpret_cast<NtContinueFn>(
            SyscallStubs::GetStub(SyscallStubs::SC_NtContinue));
    }

    inline NtContinueExFn NtContinueEx() {
        return reinterpret_cast<NtContinueExFn>(
            SyscallStubs::GetStub(SyscallStubs::SC_NtContinueEx));
    }

    inline NtDelayExecutionFn NtDelayExecution() {
        return reinterpret_cast<NtDelayExecutionFn>(
            SyscallStubs::GetStub(SyscallStubs::SC_NtDelayExecution));
    }

    inline NtSuspendThreadFn NtSuspendThread() {
        return reinterpret_cast<NtSuspendThreadFn>(
            SyscallStubs::GetStub(SyscallStubs::SC_NtSuspendThread));
    }

    inline NtSetContextThreadFn NtSetContextThread() {
        return reinterpret_cast<NtSetContextThreadFn>(
            SyscallStubs::GetStub(SyscallStubs::SC_NtSetContextThread));
    }

    inline NtGetContextThreadFn NtGetContextThread() {
        return reinterpret_cast<NtGetContextThreadFn>(
            SyscallStubs::GetStub(SyscallStubs::SC_NtGetContextThread));
    }

    inline NtCreateThreadExFn NtCreateThreadEx() {
        return reinterpret_cast<NtCreateThreadExFn>(
            SyscallStubs::GetStub(SyscallStubs::SC_NtCreateThreadEx));
    }

    inline NtWriteVirtualMemoryFn NtWriteVirtualMemory() {
        return reinterpret_cast<NtWriteVirtualMemoryFn>(
            SyscallStubs::GetStub(SyscallStubs::SC_NtWriteVirtualMemory));
    }

    inline NtReadVirtualMemoryFn NtReadVirtualMemory() {
        return reinterpret_cast<NtReadVirtualMemoryFn>(
            SyscallStubs::GetStub(SyscallStubs::SC_NtReadVirtualMemory));
    }

    inline NtAllocateVirtualMemoryFn NtAllocateVirtualMemory() {
        return reinterpret_cast<NtAllocateVirtualMemoryFn>(
            SyscallStubs::GetStub(SyscallStubs::SC_NtAllocateVirtualMemory));
    }

    inline NtFreeVirtualMemoryFn NtFreeVirtualMemory() {
        return reinterpret_cast<NtFreeVirtualMemoryFn>(
            SyscallStubs::GetStub(SyscallStubs::SC_NtFreeVirtualMemory));
    }

    // ── Convenience wrappers ─────────────────────────────────────────────────

    // VirtualProtect bypass (uses our syscall stub, not ntdll export)
    inline BOOL ProtectMemory(void* addr, SIZE_T size, DWORD newProt, DWORD* oldProt) {
        auto fn = NtProtectVirtualMemory();
        if (!fn) return FALSE;

        PVOID base = addr;
        SIZE_T region = size;
        ULONG old = 0;
        LONG status = fn(GetCurrentProcess(), &base, &region, newProt, &old);
        if (oldProt) *oldProt = static_cast<DWORD>(old);
        return status >= 0;
    }

    // WriteProcessMemory bypass
    inline BOOL WriteMemory(void* dst, const void* src, SIZE_T size) {
        auto fn = NtWriteVirtualMemory();
        if (!fn) return FALSE;

        SIZE_T written = 0;
        LONG status = fn(GetCurrentProcess(), dst, const_cast<PVOID>(src), size, &written);
        return status >= 0 && written == size;
    }

    // ReadProcessMemory bypass
    inline BOOL ReadMemory(void* src, void* dst, SIZE_T size) {
        auto fn = NtReadVirtualMemory();
        if (!fn) return FALSE;

        SIZE_T read = 0;
        LONG status = fn(GetCurrentProcess(), src, dst, size, &read);
        return status >= 0 && read == size;
    }

    // VirtualAlloc bypass
    inline void* AllocMemory(SIZE_T size, ULONG protect = PAGE_EXECUTE_READWRITE) {
        auto fn = NtAllocateVirtualMemory();
        if (!fn) return nullptr;

        PVOID base = nullptr;
        SIZE_T region = size;
        LONG status = fn(GetCurrentProcess(), &base, 0, &region,
                         MEM_COMMIT | MEM_RESERVE, protect);
        return (status >= 0) ? base : nullptr;
    }

    // VirtualFree bypass
    inline BOOL FreeMemory(void* addr) {
        auto fn = NtFreeVirtualMemory();
        if (!fn) return FALSE;

        PVOID base = addr;
        SIZE_T region = 0;
        LONG status = fn(GetCurrentProcess(), &base, &region, MEM_RELEASE);
        return status >= 0;
    }

    // Sleep bypass (NtDelayExecution)
    inline void SleepDirect(DWORD milliseconds) {
        auto fn = NtDelayExecution();
        if (!fn) { Sleep(milliseconds); return; }

        LARGE_INTEGER interval;
        interval.QuadPart = -static_cast<LONGLONG>(milliseconds) * 10000LL;
        fn(FALSE, &interval);
    }

} // namespace Syscall

// ═══════════════════════════════════════════════════════════════════════════════
// HOOK DETECTION
// ═══════════════════════════════════════════════════════════════════════════════

inline uintptr_t GetNtdllBase() {
    if (g_ntdllBase) return g_ntdllBase;
    HMODULE h = GetModuleHandleW(L"ntdll.dll");
    if (h) g_ntdllBase = reinterpret_cast<uintptr_t>(h);
    return g_ntdllBase;
}

inline uintptr_t ResolveExport(const char* name) {
    HMODULE h = GetModuleHandleW(L"ntdll.dll");
    if (!h) return 0;
    return reinterpret_cast<uintptr_t>(GetProcAddress(h, name));
}

inline bool MemCompare(const uint8_t* mem, const uint8_t* pattern, int size) {
    for (int i = 0; i < size; ++i) {
        if (mem[i] != pattern[i]) return false;
    }
    return true;
}

inline bool SafeReadBytes(uintptr_t addr, uint8_t* out, int size) {
    __try {
        for (int i = 0; i < size; ++i) {
            out[i] = *reinterpret_cast<const uint8_t*>(addr + i);
        }
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

// ── Internal-function resolution via pattern scanning ────────────────────────
// Some Packman targets are NOT exported by ntdll (e.g. RtlpAddVectoredHandler,
// RtlpQueryProcessDebugInformationRemote). These are internal functions called
// by exported wrappers. We resolve them by:
//   1. Resolve the exported wrapper (e.g. RtlAddVectoredExceptionHandler)
//   2. Scan the wrapper's body for the first E8 (call rel32) or E9 (jmp rel32)
//      that matches the expected target function shape
//   3. Follow the rel32 to get the internal function address
//
// The Nth-call parameter lets us skip past calls to common helpers
// (RtlpRunOnceInitialize, etc.) and reach the real target.

// Follow a `call rel32` (E8) or `jmp rel32` (E9) at `at`. Returns 0 on failure.
inline uintptr_t FollowRelCall(uintptr_t at) {
    uint8_t bytes[5] = {};
    if (!SafeReadBytes(at, bytes, 5)) return 0;
    if (bytes[0] != 0xE8 && bytes[0] != 0xE9) return 0;
    int32_t rel = *reinterpret_cast<int32_t*>(&bytes[1]);
    return at + 5 + static_cast<uintptr_t>(rel);
}

// Scan up to `scanLimit` bytes starting at `wrapperStart` for the Nth E8/E9
// instruction and return its target. `nthCall` is 1-based.
inline uintptr_t FindNthRelCallTarget(uintptr_t wrapperStart, int nthCall, int scanLimit = 0x200) {
    if (!wrapperStart || nthCall <= 0) return 0;

    int found = 0;
    for (int i = 0; i < scanLimit; ++i) {
        uint8_t b = 0;
        if (!SafeReadBytes(wrapperStart + i, &b, 1)) return 0;

        // Stop scanning if we hit a function epilogue (0xC3 ret, 0xCC int3 padding)
        // BUT only if we're past the first few bytes (some prologues use ret-like
        // encodings as part of larger instructions).
        if (i > 0x10 && (b == 0xC3 || (b == 0xCC && i > 0x10))) {
            // Check if next byte is also CC (likely padding between functions)
            uint8_t b2 = 0;
            SafeReadBytes(wrapperStart + i + 1, &b2, 1);
            if (b == 0xC3 && b2 == 0xCC) return 0; // end of function
        }

        if (b == 0xE8 || b == 0xE9) {
            ++found;
            if (found == nthCall) {
                uintptr_t target = FollowRelCall(wrapperStart + i);
                // Sanity check: target must be inside ntdll
                const uintptr_t base = GetNtdllBase();
                if (base && target >= base && target < base + (64u << 20)) {
                    return target;
                }
                return 0;
            }
            i += 4; // skip the rel32 operand
        }
    }
    return 0;
}

// Resolve an internal ntdll function by scanning an exported wrapper's body.
// Returns 0 if resolution fails.
inline uintptr_t ResolveInternalFunction(const char* internalName) {
    if (!internalName) return 0;

    // RtlpAddVectoredHandler is reached from RtlAddVectoredExceptionHandler.
    // The wrapper is small; the first E8/call is to RtlpAddVectoredHandler.
    if (std::strcmp(internalName, "RtlpAddVectoredHandler") == 0) {
        uintptr_t wrapper = ResolveExport("RtlAddVectoredExceptionHandler");
        if (!wrapper) return 0;
        // The wrapper does:
        //   xor r8d, r8d (or similar) ; mov edx, ...; mov ecx, 1
        //   jmp/call RtlpAddVectoredHandler
        // The first E8 or E9 is what we want.
        uintptr_t target = FindNthRelCallTarget(wrapper, 1, 0x80);
        if (target) return target;

        // Fallback: try RtlAddVectoredContinueHandler (same internal callee)
        wrapper = ResolveExport("RtlAddVectoredContinueHandler");
        if (wrapper) {
            target = FindNthRelCallTarget(wrapper, 1, 0x80);
            if (target) return target;
        }
        return 0;
    }

    // RtlpQueryProcessDebugInformationRemote is reached from
    // RtlQueryProcessDebugInformation. The wrapper validates args then calls
    // the internal function. The exact call index varies; we try the first
    // few rel32 calls and pick the one inside ntdll.
    if (std::strcmp(internalName, "RtlpQueryProcessDebugInformationRemote") == 0) {
        uintptr_t wrapper = ResolveExport("RtlQueryProcessDebugInformation");
        if (!wrapper) return 0;
        // Scan for up to 5 calls; return the first one that lives in ntdll.
        for (int n = 1; n <= 5; ++n) {
            uintptr_t target = FindNthRelCallTarget(wrapper, n, 0x300);
            if (target) {
                // Heuristic: the target should NOT be another exported wrapper.
                // Internal "Rtlp*" helpers usually start with `48 ...` not `4C 8B D1`.
                uint8_t prologue[4] = {};
                if (SafeReadBytes(target, prologue, 4) &&
                    !(prologue[0] == 0x4C && prologue[1] == 0x8B && prologue[2] == 0xD1)) {
                    return target;
                }
            }
        }
        return 0;
    }

    return 0;
}

// Returns true if the given function name refers to an internal (non-exported)
// ntdll function that requires pattern-scan resolution.
inline bool IsInternalFunction(const char* name) {
    if (!name) return false;
    return std::strcmp(name, "RtlpAddVectoredHandler") == 0 ||
           std::strcmp(name, "RtlpQueryProcessDebugInformationRemote") == 0;
}

inline HookState CheckSite(int id) {
    if (id < 0 || id >= HookCount) return State_Unknown;

    const auto& site = kHookSites[id];
    auto& status = g_status[id];

    // Try export first; for internal functions, fall back to pattern scan
    uintptr_t funcAddr = IsInternalFunction(site.functionName)
        ? ResolveInternalFunction(site.functionName)
        : ResolveExport(site.functionName);

    if (!funcAddr) {
        status.state = State_NotFound;
        status.resolvedAddr = 0;
        return State_NotFound;
    }


    uintptr_t checkAddr = funcAddr + site.offset;
    status.resolvedAddr = checkAddr;

    std::memset(status.liveBytes, 0, sizeof(status.liveBytes));
    int readSize = site.hookedSize > site.originalSize ? site.hookedSize : site.originalSize;
    if (readSize > 16) readSize = 16;

    if (!SafeReadBytes(checkAddr, status.liveBytes, readSize)) {
        status.state = State_NotFound;
        return State_NotFound;
    }

    if (MemCompare(status.liveBytes, site.hookedBytes, site.hookedSize)) {
        status.state = State_Hooked;
        return State_Hooked;
    }

    if (MemCompare(status.liveBytes, site.originalBytes, site.originalSize)) {
        status.state = State_Clean;
        return State_Clean;
    }

    // Generic jmp [rip+0] detection
    if (site.patchSize >= 6 && status.liveBytes[0] == 0xFF && status.liveBytes[1] == 0x25) {
        status.state = State_Hooked;
        return State_Hooked;
    }

    status.state = State_Modified;
    return State_Modified;
}

// ═══════════════════════════════════════════════════════════════════════════════
// PUBLIC API
// ═══════════════════════════════════════════════════════════════════════════════

inline bool Init() {
    if (g_initialized) return g_ntdllBase != 0;
    g_ntdllBase = GetNtdllBase();
    g_initialized = 1;
    return g_ntdllBase != 0;
}

// ── PRIMARY ENTRY POINT ──────────────────────────────────────────────────────
// Call this FIRST after injection, before any other Core module initializes.
// It detects Packman hooks and builds direct syscall stubs.
inline bool Install() {
    if (g_installed) return true;

    // Step 1: Resolve ntdll base
    Init();

    // Step 2: Build all direct syscall stubs (from disk-read SSNs)
    // This MUST happen before detection because detection reads memory at
    // hook sites, and some security checks may fire if we read too slowly.
    bool stubsOk = SyscallStubs::BuildAllStubs();

    // Step 3: Detect all Packman hook sites
    for (int i = 0; i < HookCount; ++i) {
        CheckSite(i);
    }

    g_installed = 1;
    return stubsOk;
}

// Check all hook sites, returns count of active (hooked) sites.
inline int DetectAll() {
    Init();
    int hooked = 0;
    for (int i = 0; i < HookCount; ++i) {
        if (CheckSite(i) == State_Hooked) ++hooked;
    }
    return hooked;
}

inline bool IsHooked(HookId id) {
    if (g_status[id].state == State_Unknown) CheckSite(id);
    return g_status[id].state == State_Hooked;
}

inline bool IsClean(HookId id) {
    if (g_status[id].state == State_Unknown) CheckSite(id);
    return g_status[id].state == State_Clean;
}

inline const HookStatus& GetStatus(HookId id) {
    if (id >= 0 && id < HookCount && g_status[id].state == State_Unknown) CheckSite(id);
    return g_status[id];
}

inline bool IsPackmanActive() {
    if (!g_installed) Install();
    int hooked = 0;
    const HookId critical[] = {
        NtProtectVirtualMemory, NtQueryVirtualMemory, NtContinue,
        DbgBreakPoint, DbgUserBreakPoint, DbgUiRemoteBreakin, KiUserExceptionDispatch,
    };
    for (auto id : critical) {
        if (g_status[id].state == State_Hooked) ++hooked;
    }
    return hooked >= 4;
}

inline bool NeedDirectSyscall() {
    return IsHooked(NtProtectVirtualMemory);
}

inline bool IsThreadCreationBlocked() {
    return IsHooked(NtCreateThread) && IsHooked(NtCreateThreadEx);
}

inline bool IsDebugAttachBlocked() {
    return IsHooked(DbgUiRemoteBreakin);
}

inline uintptr_t GetTrampolineTarget(HookId id) {
    if (id < 0 || id >= HookCount) return 0;
    const auto& st = GetStatus(id);
    if (st.state != State_Hooked) return 0;
    if (st.liveBytes[0] == 0xFF && st.liveBytes[1] == 0x25 &&
        st.liveBytes[2] == 0x00 && st.liveBytes[3] == 0x00 &&
        st.liveBytes[4] == 0x00 && st.liveBytes[5] == 0x00) {
        uintptr_t target = 0;
        if (SafeReadBytes(st.resolvedAddr + 6, reinterpret_cast<uint8_t*>(&target), 8))
            return target;
    }
    return 0;
}

inline int Rescan() {
    for (int i = 0; i < HookCount; ++i) g_status[i].state = State_Unknown;
    return DetectAll();
}

// ── Status helpers ───────────────────────────────────────────────────────────

inline const char* StateLabel(HookState s) {
    switch (s) {
        case State_Clean:    return "clean";
        case State_Hooked:   return "HOOKED";
        case State_Modified: return "modified";
        case State_NotFound: return "not-found";
        default:             return "unknown";
    }
}

inline const char* GetHookName(HookId id) {
    return (id >= 0 && id < HookCount) ? kHookSites[id].functionName : "?";
}

// ── Logging ──────────────────────────────────────────────────────────────────

inline constexpr const char* kLogPath =
    "C:\\Users\\Public\\nightsharp_packman.txt";

inline void WriteLog(const char* text) {
    if (!text || !*text) return;
    HANDLE h = CreateFileA(kLogPath, FILE_APPEND_DATA, FILE_SHARE_READ,
                           nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return;
    DWORD w = 0;
    WriteFile(h, text, static_cast<DWORD>(lstrlenA(text)), &w, nullptr);
    CloseHandle(h);
}

inline void LogAllStatus() {
    char buf[512] = {};

    wsprintfA(buf, "[PackmanHook] === Install Report ===\r\n");
    WriteLog(buf);
    wsprintfA(buf, "[PackmanHook] ntdll base = 0x%p\r\n",
              reinterpret_cast<void*>(g_ntdllBase));
    WriteLog(buf);

    // Log hook detection results
    wsprintfA(buf, "\r\n[PackmanHook] --- Hook Sites ---\r\n");
    WriteLog(buf);
    for (int i = 0; i < HookCount; ++i) {
        const auto& site = kHookSites[i];
        const auto& st = g_status[i];
        wsprintfA(buf,
            "  [%2d] %-40s +0x%02X  %-8s  addr=0x%p  [%02X %02X %02X %02X %02X]\r\n",
            i, site.functionName, site.offset, StateLabel(st.state),
            reinterpret_cast<void*>(st.resolvedAddr),
            st.liveBytes[0], st.liveBytes[1], st.liveBytes[2],
            st.liveBytes[3], st.liveBytes[4]);
        WriteLog(buf);
    }

    // Log syscall stub status
    wsprintfA(buf, "\r\n[PackmanHook] --- Syscall Stubs ---\r\n");
    WriteLog(buf);
    for (int i = 0; i < SyscallStubs::SC_Count; ++i) {
        const auto& sc = SyscallStubs::g_syscalls[i];
        wsprintfA(buf,
            "  [%2d] %-28s SSN=0x%03X (known=0x%03X)  stub=%p  %s\r\n",
            i, sc.name,
            sc.resolvedSSN >= 0 ? sc.resolvedSSN : 0,
            sc.knownSSN,
            sc.stubPtr,
            sc.stubPtr ? "OK" : "FAILED");
        WriteLog(buf);
    }

    // Summary
    int hookedCount = 0;
    for (int i = 0; i < HookCount; ++i) {
        if (g_status[i].state == State_Hooked) ++hookedCount;
    }
    int stubCount = 0;
    for (int i = 0; i < SyscallStubs::SC_Count; ++i) {
        if (SyscallStubs::g_syscalls[i].stubPtr) ++stubCount;
    }

    wsprintfA(buf,
        "\r\n[PackmanHook] Summary: hooks=%d/%d active, stubs=%d/%d built, Packman=%s\r\n\r\n",
        hookedCount, HookCount, stubCount, SyscallStubs::SC_Count,
        IsPackmanActive() ? "ACTIVE" : "inactive");
    WriteLog(buf);
}

// Full install + detect + log in one call. Use as the FIRST init step.
inline bool InstallAndLog() {
    bool ok = Install();
    LogAllStatus();
    return ok;
}

inline void Shutdown() {
    SyscallStubs::Shutdown();
    for (auto& status : g_status) {
        status = {};
    }
    g_ntdllBase = 0;
    g_initialized = 0;
    g_installed = 0;
}

} // namespace PackmanHook
