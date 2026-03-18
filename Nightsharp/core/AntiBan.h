#pragma once
/*
 * ============================================================================
 *  AntiBan.h — STEALTH MODULE for NightSharp
 *  
 *  Goal: Make our DLL completely invisible to Packman (stub.dll)
 *  
 *  ❌ DO NOT overwrite ntdll hooks (scanner detects)
 *  ✅ HIDE ourselves: unlink PEB, erase PE header, wipe strings
 *  ✅ Direct syscalls for when we need to bypass hooks
 *  ✅ stub.dll patching handled by PackmanPatcher.h (separate module)
 *
 *  STEALTH LAYERS:
 *  Layer 1: Clean PEB debug flags (anti Cases 0, 1, 4)
 *  Layer 2: Unlink from PEB module list (anti Case 10 scan)
 *  Layer 3: Erase PE header via direct syscall (anti signature scan)
 *  Layer 4: Wipe identifying strings from memory (anti string scan)
 *  Layer 5: Halo's Gate SSN + direct syscall wrappers
 * ============================================================================
 */

#include <Windows.h>
#include <cstdint>

// ============================================================================
// PEB/LDR structures (winternl.h has incomplete definitions)
// ============================================================================
typedef struct _AB_UNICODE_STRING {
    USHORT Length;
    USHORT MaximumLength;
    PWSTR  Buffer;
} AB_UNICODE_STRING;

typedef struct _AB_LDR_ENTRY {
    LIST_ENTRY InLoadOrderLinks;
    LIST_ENTRY InMemoryOrderLinks;
    LIST_ENTRY InInitializationOrderLinks;
    PVOID      DllBase;
    PVOID      EntryPoint;
    ULONG      SizeOfImage;
    AB_UNICODE_STRING FullDllName;
    AB_UNICODE_STRING BaseDllName;
} AB_LDR_ENTRY;

typedef struct _AB_PEB_LDR {
    ULONG      Length;
    BOOLEAN    Initialized;
    HANDLE     SsHandle;
    LIST_ENTRY InLoadOrderModuleList;
    LIST_ENTRY InMemoryOrderModuleList;
    LIST_ENTRY InInitializationOrderModuleList;
} AB_PEB_LDR;

typedef struct _AB_PEB {
    BYTE Reserved1[2];
    BYTE BeingDebugged;
    BYTE Reserved2[1];
    PVOID Reserved3[2];
    AB_PEB_LDR* Ldr;
} AB_PEB;

namespace AntiBan {

// ============================================================================
// LAYER 5: HALO'S GATE SSN RESOLVER
// ============================================================================
// Resolves syscall numbers from in-memory ntdll.
// If target function is hooked, scans neighboring stubs to calculate SSN.
// No disk I/O, no PE parsing, safe in DllMain.
// ============================================================================

namespace Syscall {

    inline uint32_t GetSSN(const char* functionName) {
        HMODULE ntdll = GetModuleHandleA("ntdll.dll");
        if (!ntdll) return 0;
        auto fn = (uint8_t*)GetProcAddress(ntdll, functionName);
        if (!fn) return 0;

        // Clean stub: 4C 8B D1 B8 <SSN 4 bytes>
        if (fn[0] == 0x4C && fn[1] == 0x8B && fn[2] == 0xD1 && fn[3] == 0xB8)
            return *(uint32_t*)(fn + 4);

        // Hooked — Halo's Gate: scan neighbors at ±0x20
        for (uint32_t i = 1; i < 500; i++) {
            uint8_t* up = fn - (i * 0x20);
            if (up[0] == 0x4C && up[1] == 0x8B && up[2] == 0xD1 && up[3] == 0xB8)
                return *(uint32_t*)(up + 4) + i;
            uint8_t* down = fn + (i * 0x20);
            if (down[0] == 0x4C && down[1] == 0x8B && down[2] == 0xD1 && down[3] == 0xB8)
                return *(uint32_t*)(down + 4) - i;
        }
        return 0;
    }

    struct Table {
        uint32_t NtProtectVirtualMemory  = 0;
        uint32_t NtQueryVirtualMemory    = 0;
        uint32_t NtCreateThreadEx        = 0;
        bool ok = false;
    };

    inline Table& Get() { static Table t; return t; }

    inline bool Resolve() {
        auto& t = Get();
        if (t.ok) return true;
        t.NtProtectVirtualMemory = GetSSN("NtProtectVirtualMemory");
        t.NtQueryVirtualMemory   = GetSSN("NtQueryVirtualMemory");
        t.NtCreateThreadEx       = GetSSN("NtCreateThreadEx");
        t.ok = true;
        return true;
    }
}

// ============================================================================
// SYSCALL STUB GENERATOR
// ============================================================================

inline void* MakeStub(uint32_t ssn) {
    if (!ssn) return nullptr;
    uint8_t code[] = {
        0x4C, 0x8B, 0xD1,                      // mov r10, rcx
        0xB8, 0x00, 0x00, 0x00, 0x00,           // mov eax, <SSN>
        0x0F, 0x05,                              // syscall
        0xC3                                     // ret
    };
    *(uint32_t*)(code + 4) = ssn;
    void* mem = VirtualAlloc(nullptr, sizeof(code),
                              MEM_COMMIT | MEM_RESERVE,
                              PAGE_EXECUTE_READWRITE);
    if (!mem) return nullptr;
    memcpy(mem, code, sizeof(code));
    return mem;
}

// Direct NtProtectVirtualMemory (bypasses Packman hook)
inline NTSTATUS DirectProtect(HANDLE ph, PVOID* base, PSIZE_T size,
                               ULONG newProt, PULONG oldProt) {
    static auto fn = (NTSTATUS(NTAPI*)(HANDLE, PVOID*, PSIZE_T, ULONG, PULONG))
        MakeStub(Syscall::Get().NtProtectVirtualMemory);
    return fn ? fn(ph, base, size, newProt, oldProt) : (NTSTATUS)0xC0000001L;
}

// Stealth VirtualProtect wrapper
inline bool StealthProtect(void* addr, SIZE_T size, ULONG newProt) {
    PVOID base = addr;
    SIZE_T region = size;
    ULONG old = 0;
    return DirectProtect(GetCurrentProcess(), &base, &region, newProt, &old) >= 0;
}

// ============================================================================
// LAYER 1: PEB CLEANUP
// ============================================================================

__declspec(noinline) inline void CleanPEB() {
    __try {
        auto peb = (uint8_t*)__readgsqword(0x60);
        if (!peb) return;
        peb[2] = 0;                          // BeingDebugged = 0
        *(DWORD*)(peb + 0xBC) = 0;           // NtGlobalFlag = 0
        uintptr_t heap = *(uintptr_t*)(peb + 0x30);
        if (heap) {
            *(DWORD*)(heap + 0x70) = 2;      // Flags = HEAP_GROWABLE
            *(DWORD*)(heap + 0x74) = 0;      // ForceFlags = 0
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
}

// ============================================================================
// LAYER 2: UNLINK FROM PEB MODULE LIST
// ============================================================================
// Removes our DLL from all 3 PEB linked lists + wipes name strings.
// After this, EnumProcessModules and PEB walkers won't find us.
// ============================================================================

__declspec(noinline) inline void UnlinkModule(HMODULE hMod) {
    __try {
        auto peb = (AB_PEB*)__readgsqword(0x60);
        if (!peb || !peb->Ldr) return;

        PLIST_ENTRY head = &peb->Ldr->InLoadOrderModuleList;
        PLIST_ENTRY cur = head->Flink;

        while (cur != head) {
            auto entry = (AB_LDR_ENTRY*)cur;
            PLIST_ENTRY next = cur->Flink;

            if (entry->DllBase == (PVOID)hMod) {
                // Unlink from InLoadOrderModuleList
                cur->Blink->Flink = cur->Flink;
                cur->Flink->Blink = cur->Blink;

                // Unlink from InMemoryOrderModuleList
                PLIST_ENTRY mem = &entry->InMemoryOrderLinks;
                mem->Blink->Flink = mem->Flink;
                mem->Flink->Blink = mem->Blink;

                // Unlink from InInitializationOrderModuleList
                PLIST_ENTRY init = &entry->InInitializationOrderLinks;
                if (init->Flink && init->Blink) {
                    init->Blink->Flink = init->Flink;
                    init->Flink->Blink = init->Blink;
                }

                // Wipe DLL name strings from memory
                if (entry->FullDllName.Buffer && entry->FullDllName.Length > 0)
                    memset(entry->FullDllName.Buffer, 0, entry->FullDllName.Length);
                if (entry->BaseDllName.Buffer && entry->BaseDllName.Length > 0)
                    memset(entry->BaseDllName.Buffer, 0, entry->BaseDllName.Length);

                break;
            }
            cur = next;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
}

// ============================================================================
// LAYER 3: ERASE PE HEADER (via direct syscall)
// ============================================================================
// Wipes DOS header, PE signature, section table from our module's memory.
// Uses direct syscall for VirtualProtect (bypasses Packman hook).
// After this, memory scanners can't identify us as a PE/DLL.
// ============================================================================

__declspec(noinline) inline void ErasePEHeader(HMODULE hMod) {
    __try {
        auto dos = (IMAGE_DOS_HEADER*)hMod;
        if (dos->e_magic != 0x5A4D) return;

        auto nt = (IMAGE_NT_HEADERS*)((uint8_t*)hMod + dos->e_lfanew);
        DWORD headerSize = nt->OptionalHeader.SizeOfHeaders;

        // Use DIRECT SYSCALL — bypass Packman's NtProtectVirtualMemory hook!
        if (StealthProtect(hMod, headerSize, PAGE_READWRITE)) {
            memset(hMod, 0, headerSize);
            StealthProtect(hMod, headerSize, PAGE_READONLY);
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
}

// ============================================================================
// LAYER 4: WIPE IDENTIFYING STRINGS FROM MEMORY
// ============================================================================
// Scans our module's memory for known suspicious strings and zeroes them.
// This prevents Packman's string scanner from finding:
//   "MinHook", "Kiero", "NightSharp", "ImGui", etc.
//
// Note: Only wipes .rdata and .data sections (not .text code).
// Strings used by code must have already been consumed before wiping.
// ============================================================================

__declspec(noinline) inline void WipeStringsInRange(uint8_t* start, SIZE_T size) {
    // Strings to wipe (null-terminated, case-sensitive)
    static const char* targets[] = {
        "MinHook",
        "minhook",
        "MINHOOK",
        "Tsuda Kageyu",
        "kiero",
        "Kiero",
        "NightSharp",
        "nightsharp",
        "NIGHTSHARP",
        "ImGui",
        "imgui",
        "IMGUI",
        "Dear ImGui",
        "imgui.ini",
        "ImGuiCol",
        "omar cornut",
        "Cheat",
        "cheat",
        "CHEAT",
        "hack",
        "Hack",
        "HACK",
        "inject",
        "Inject",
    };

    for (const char* target : targets) {
        size_t tlen = strlen(target);
        if (tlen == 0 || tlen >= size) continue;

        for (SIZE_T i = 0; i <= size - tlen; i++) {
            if (memcmp(start + i, target, tlen) == 0) {
                memset(start + i, 0, tlen);
            }
        }
    }
}

__declspec(noinline) inline void WipeModuleStrings(HMODULE hMod) {
    __try {
        auto dos = (IMAGE_DOS_HEADER*)hMod;
        if (dos->e_magic != 0x5A4D) return;

        auto nt = (IMAGE_NT_HEADERS*)((uint8_t*)hMod + dos->e_lfanew);
        auto sec = IMAGE_FIRST_SECTION(nt);

        for (WORD i = 0; i < nt->FileHeader.NumberOfSections; i++) {
            // Only wipe .rdata and .data (strings live here)
            // Skip .text (would corrupt code!)
            bool isRdata = (memcmp(sec[i].Name, ".rdata", 6) == 0);
            bool isData  = (memcmp(sec[i].Name, ".data", 5) == 0);
            bool isRsrc  = (memcmp(sec[i].Name, ".rsrc", 5) == 0);

            if (!isRdata && !isData && !isRsrc) continue;

            uint8_t* secStart = (uint8_t*)hMod + sec[i].VirtualAddress;
            SIZE_T   secSize  = sec[i].Misc.VirtualSize;

            // Make writable via direct syscall
            if (StealthProtect(secStart, secSize, PAGE_READWRITE)) {
                WipeStringsInRange(secStart, secSize);
                // Restore original protection
                ULONG origProt = (sec[i].Characteristics & IMAGE_SCN_MEM_WRITE)
                    ? PAGE_READWRITE : PAGE_READONLY;
                StealthProtect(secStart, secSize, origProt);
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
}

// ============================================================================
// HOOK DETECTION UTILITY
// ============================================================================

enum class HookType { Clean, InlineJump, Int3Patch, Unknown };

inline HookType CheckHook(const char* funcName) {
    HMODULE ntdll = GetModuleHandleA("ntdll.dll");
    if (!ntdll) return HookType::Unknown;
    auto fn = (uint8_t*)GetProcAddress(ntdll, funcName);
    if (!fn) return HookType::Unknown;
    if (fn[0] == 0x4C && fn[1] == 0x8B && fn[2] == 0xD1) return HookType::Clean;
    if (fn[0] == 0xFF && fn[1] == 0x25)                   return HookType::InlineJump;
    if (fn[0] == 0xCC)                                     return HookType::Int3Patch;
    return HookType::Unknown;
}

// ============================================================================
// INITIALIZATION API
// ============================================================================

struct InitResult {
    bool syscallsOk;
    bool pebCleaned;
};

// Phase 1: Call from DLL_PROCESS_ATTACH (safe under loader lock)
// Only does pure memory reads/writes — no API calls that could be hooked
inline InitResult Initialize(HMODULE hSelf) {
    InitResult r = {};
    r.syscallsOk = Syscall::Resolve();
    CleanPEB();
    r.pebCleaned = true;
    return r;
}

// Phase 2: Call from MainThread AFTER DllMain returns
// Performs stealth operations that need direct syscalls
inline void DeferredInit(HMODULE hSelf) {
    // Unlink from PEB module list (hides from module enumeration)
    UnlinkModule(hSelf);

    // Erase PE header via direct syscall (hides PE/MZ signature)
    if (Syscall::Get().NtProtectVirtualMemory) {
        ErasePEHeader(hSelf);
    }
    // NOTE: String wiping is NOT done — it destroys ImGui/menu/script
    // strings that are actively used at runtime. The MinHook.rc metadata
    // fix + PE header erasure are sufficient for hiding DLL identity.
}

// ============================================================================
// FNV-1a HASH (Packman's algorithm — for research/testing)
// ============================================================================

inline uint64_t PackmanHash(const char* s) {
    uint64_t h = 0xCBF29CE484222325ULL;
    while (*s) { h = 0x100000001B3ULL * (h ^ ((uint8_t)*s | 0x20u)); s++; }
    return h;
}

} // namespace AntiBan
