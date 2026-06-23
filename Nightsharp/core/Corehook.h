#pragma once
// ============================================================================
// corehook_test - ad-hoc hook verifier for newly generated hook RVAs.
// ============================================================================
//
// CoreEvents can wire this into the bootstrap path, but automatic all-hook
// install is disabled by default so hooks can be installed one by one while
// debugging. The implementation embeds the minimal RawAsmDetour backend
// locally, so unknown function prototypes are not guessed in C++.
//
// RawAsmDetour preserves the volatile integer/XMM argument registers, calls a
// small logger, then executes the stolen prologue and resumes the original
// function. That makes this file suitable for hit-count validation.
// ============================================================================

#include <Windows.h>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <intrin.h>

#include "offset.h"

#ifndef NIGHTSHARP_COREHOOK_AUTO_INSTALL_ALL
#define NIGHTSHARP_COREHOOK_AUTO_INSTALL_ALL 1
#endif

#ifndef NIGHTSHARP_ENABLE_CLIENTMAINLOOP_HOOK
#define NIGHTSHARP_ENABLE_CLIENTMAINLOOP_HOOK 0
#endif

#ifndef NIGHTSHARP_ENABLE_ONMISSILEDELETE_HOOK
// Enabled for single-hook debug validation.
#define NIGHTSHARP_ENABLE_ONMISSILEDELETE_HOOK 1
#endif


#ifndef NIGHTSHARP_ENABLE_ONCREATE_HOOK
// Enabled: hooks AssignNetworkId (0x562610). RCX = object, RDX = netId.
// Call original first, then fire create event — object is fully initialized.
#define NIGHTSHARP_ENABLE_ONCREATE_HOOK 1
#endif

#ifndef NIGHTSHARP_ENABLE_ONDELETE_HOOK
// Enabled: hooks DestroyObject (0x54CC30). RCX = manager, RDX = object.
// Event fires BEFORE original runs — object is still valid at hook entry.
#define NIGHTSHARP_ENABLE_ONDELETE_HOOK 1
#endif
namespace CoreEventHook {

namespace shim {

inline uintptr_t GetGameBase() {
    static uintptr_t cached = 0;
    if (!cached) cached = reinterpret_cast<uintptr_t>(GetModuleHandleA(nullptr));
    return cached;
}

} // namespace shim

namespace stealth {

using NtProtectFn = LONG(NTAPI*)(HANDLE, PVOID*, PSIZE_T, ULONG, PULONG);
using NtWriteFn   = LONG(NTAPI*)(HANDLE, PVOID, PVOID, SIZE_T, PSIZE_T);

inline void*       g_stubPtr = nullptr;
inline NtProtectFn g_ntProtect = nullptr;
inline int         g_ssnCached = -1;
inline volatile LONG g_lastStatus = 0;
inline volatile int  g_ssnSource = 0;

inline volatile int  g_selfTest = 0;
inline volatile LONG g_selfTestStatusRWX = 0;
inline volatile LONG g_selfTestStatusRO = 0;

inline void*       g_writeStubPtr = nullptr;
inline NtWriteFn   g_ntWrite = nullptr;
inline int         g_writeSsn = -1;
inline volatile LONG g_writeLastStatus = 0;
inline volatile int  g_writeSelfTest = 0;
inline volatile LONG g_writeSelfTestStatus = 0;

inline volatile int  g_cowSelfTest = 0;
inline volatile LONG g_cowSelfTestStatusEWC = 0;
inline volatile LONG g_cowSelfTestStatusWC = 0;

inline DWORD RvaToFileOff(const IMAGE_NT_HEADERS* nt, DWORD rva) {
    const IMAGE_SECTION_HEADER* sec = IMAGE_FIRST_SECTION(nt);
    for (int i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++sec) {
        const DWORD vEnd = sec->VirtualAddress + sec->Misc.VirtualSize;
        if (rva >= sec->VirtualAddress && rva < vEnd) {
            return rva - sec->VirtualAddress + sec->PointerToRawData;
        }
    }
    return 0;
}

inline int ExtractSSNFromImageByName(const uint8_t* buf, const char* funcName) {
    if (!buf || !funcName) return -1;
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(buf);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return -1;
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(buf + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return -1;

    const auto& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    if (!dir.VirtualAddress) return -1;
    const auto* exp = reinterpret_cast<const IMAGE_EXPORT_DIRECTORY*>(
        buf + RvaToFileOff(nt, dir.VirtualAddress));
    const auto* names = reinterpret_cast<const DWORD*>(buf + RvaToFileOff(nt, exp->AddressOfNames));
    const auto* ords  = reinterpret_cast<const WORD*>(buf + RvaToFileOff(nt, exp->AddressOfNameOrdinals));
    const auto* funcs = reinterpret_cast<const DWORD*>(buf + RvaToFileOff(nt, exp->AddressOfFunctions));

    for (DWORD i = 0; i < exp->NumberOfNames; ++i) {
        const auto* name = reinterpret_cast<const char*>(buf + RvaToFileOff(nt, names[i]));
        if (std::strcmp(name, funcName) != 0) continue;
        const auto* stub = buf + RvaToFileOff(nt, funcs[ords[i]]);
        if (stub[0] == 0x4C && stub[1] == 0x8B && stub[2] == 0xD1 && stub[3] == 0xB8) {
            return static_cast<int>(*reinterpret_cast<const uint32_t*>(stub + 4));
        }
        return -1;
    }
    return -1;
}

inline int ResolveSSNInMemory(const char* funcName) {
    HMODULE h = GetModuleHandleW(L"ntdll.dll");
    if (!h) return -1;
    const auto* p = reinterpret_cast<const uint8_t*>(GetProcAddress(h, funcName));
    if (!p) return -1;
    if (p[0] == 0x4C && p[1] == 0x8B && p[2] == 0xD1 && p[3] == 0xB8) {
        return static_cast<int>(*reinterpret_cast<const uint32_t*>(p + 4));
    }
    return -1;
}

inline int ResolveSSNFromDisk(const char* funcName) {
    HANDLE hFile = CreateFileW(L"\\\\?\\C:\\Windows\\System32\\ntdll.dll",
        GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return -1;

    LARGE_INTEGER sz{};
    if (!GetFileSizeEx(hFile, &sz) || sz.QuadPart <= 0 || sz.QuadPart > (64LL << 20)) {
        CloseHandle(hFile);
        return -1;
    }

    auto* buf = reinterpret_cast<uint8_t*>(VirtualAlloc(nullptr, static_cast<SIZE_T>(sz.QuadPart),
        MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    if (!buf) {
        CloseHandle(hFile);
        return -1;
    }

    DWORD total = 0;
    while (total < static_cast<DWORD>(sz.QuadPart)) {
        DWORD chunk = 0;
        if (!ReadFile(hFile, buf + total, static_cast<DWORD>(sz.QuadPart) - total, &chunk, nullptr) ||
            chunk == 0) {
            break;
        }
        total += chunk;
    }
    CloseHandle(hFile);

    const int ssn = (total == static_cast<DWORD>(sz.QuadPart))
        ? ExtractSSNFromImageByName(buf, funcName)
        : -1;
    VirtualFree(buf, 0, MEM_RELEASE);
    return ssn;
}

inline bool BuildSyscallStub(int ssn, void** outStub) {
    if (!outStub || ssn < 0) return false;
    auto* stub = reinterpret_cast<uint8_t*>(VirtualAlloc(nullptr, 32,
        MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    if (!stub) return false;
    stub[0] = 0x4C; stub[1] = 0x8B; stub[2] = 0xD1;
    stub[3] = 0xB8;
    *reinterpret_cast<uint32_t*>(stub + 4) = static_cast<uint32_t>(ssn);
    stub[8] = 0x0F; stub[9] = 0x05;
    stub[10] = 0xC3;
    *outStub = stub;
    return true;
}

inline bool Init() {
    if (g_ntProtect) return true;
    int ssn = ResolveSSNInMemory("NtProtectVirtualMemory");
    if (ssn >= 0) g_ssnSource = 1;
    if (ssn < 0) {
        ssn = ResolveSSNFromDisk("NtProtectVirtualMemory");
        if (ssn >= 0) g_ssnSource = 2;
    }
    if (ssn < 0) {
        g_ssnSource = 0;
        return false;
    }
    if (!BuildSyscallStub(ssn, &g_stubPtr)) return false;
    g_ssnCached = ssn;
    g_ntProtect = reinterpret_cast<NtProtectFn>(g_stubPtr);
    return true;
}

inline bool InitWrite() {
    if (g_ntWrite) return true;
    int ssn = ResolveSSNInMemory("NtWriteVirtualMemory");
    if (ssn < 0) ssn = ResolveSSNFromDisk("NtWriteVirtualMemory");
    if (ssn < 0) return false;
    if (!BuildSyscallStub(ssn, &g_writeStubPtr)) return false;
    g_writeSsn = ssn;
    g_ntWrite = reinterpret_cast<NtWriteFn>(g_writeStubPtr);
    return true;
}

inline BOOL VirtualProtectDirect(void* addr, SIZE_T size, DWORD newProt, DWORD* oldProt) {
    if (!addr || size == 0) return FALSE;
    if (!Init()) {
        return VirtualProtect(addr, size, newProt, oldProt);
    }
    PVOID base = addr;
    SIZE_T region = size;
    ULONG oldUlong = 0;
    const LONG status = g_ntProtect(GetCurrentProcess(), &base, &region, newProt, &oldUlong);
    g_lastStatus = status;
    if (oldProt) *oldProt = static_cast<DWORD>(oldUlong);
    return status >= 0;
}

inline BOOL WriteVirtualMemoryDirect(void* dst, const void* src, SIZE_T size) {
    if (!dst || !src || size == 0) return FALSE;
    if (!InitWrite()) return FALSE;
    SIZE_T written = 0;
    const LONG status = g_ntWrite(GetCurrentProcess(), dst, const_cast<void*>(src), size, &written);
    g_writeLastStatus = status;
    return status >= 0 && written == size;
}

inline int SelfTest(uintptr_t anyCodePageAddr) {
    if (g_selfTest) return g_selfTest;
    if (!anyCodePageAddr || !Init()) {
        g_selfTest = 3;
        return g_selfTest;
    }
    void* page = reinterpret_cast<void*>(anyCodePageAddr & ~0xFFFULL);
    DWORD oldProt = 0;
    const BOOL okRWX = VirtualProtectDirect(page, 0x1000, PAGE_EXECUTE_READWRITE, &oldProt);
    g_selfTestStatusRWX = g_lastStatus;
    if (okRWX) {
        DWORD dummy = 0;
        VirtualProtectDirect(page, 0x1000, oldProt, &dummy);
        g_selfTest = 1;
        return g_selfTest;
    }
    const BOOL okRO = VirtualProtectDirect(page, 0x1000, PAGE_READONLY, &oldProt);
    g_selfTestStatusRO = g_lastStatus;
    if (okRO) {
        DWORD dummy = 0;
        VirtualProtectDirect(page, 0x1000, oldProt, &dummy);
        g_selfTest = 2;
        return g_selfTest;
    }
    g_selfTest = 3;
    return g_selfTest;
}

inline int SelfTestWrite(uintptr_t anyCodePageAddr) {
    if (g_writeSelfTest) return g_writeSelfTest;
    if (!anyCodePageAddr || !InitWrite()) {
        g_writeSelfTest = 3;
        return g_writeSelfTest;
    }
    uint8_t orig = 0;
    __try { orig = *reinterpret_cast<uint8_t*>(anyCodePageAddr); }
    __except (1) {
        g_writeSelfTest = 3;
        return g_writeSelfTest;
    }
    const uint8_t probe = 0xCC;
    const BOOL ok = WriteVirtualMemoryDirect(reinterpret_cast<void*>(anyCodePageAddr), &probe, 1);
    g_writeSelfTestStatus = g_writeLastStatus;
    if (ok) {
        WriteVirtualMemoryDirect(reinterpret_cast<void*>(anyCodePageAddr), &orig, 1);
        g_writeSelfTest = 1;
    } else {
        g_writeSelfTest = 3;
    }
    return g_writeSelfTest;
}

inline int SelfTestCoW(uintptr_t anyCodePageAddr) {
    if (g_cowSelfTest) return g_cowSelfTest;
    if (!anyCodePageAddr || !Init()) {
        g_cowSelfTest = 3;
        return g_cowSelfTest;
    }
    void* page = reinterpret_cast<void*>(anyCodePageAddr & ~0xFFFULL);
    DWORD oldProt = 0;
    const BOOL okEwc = VirtualProtectDirect(page, 0x1000, PAGE_EXECUTE_WRITECOPY, &oldProt);
    g_cowSelfTestStatusEWC = g_lastStatus;
    if (okEwc) {
        DWORD dummy = 0;
        VirtualProtectDirect(page, 0x1000, oldProt, &dummy);
        g_cowSelfTest = 1;
        return g_cowSelfTest;
    }
    const BOOL okWc = VirtualProtectDirect(page, 0x1000, PAGE_WRITECOPY, &oldProt);
    g_cowSelfTestStatusWC = g_lastStatus;
    if (okWc) {
        DWORD dummy = 0;
        VirtualProtectDirect(page, 0x1000, oldProt, &dummy);
        g_cowSelfTest = 2;
    } else {
        g_cowSelfTest = 3;
    }
    return g_cowSelfTest;
}

inline void Shutdown() {
    g_ntProtect = nullptr;
    g_ntWrite = nullptr;

    if (g_stubPtr) {
        VirtualFree(g_stubPtr, 0, MEM_RELEASE);
        g_stubPtr = nullptr;
    }
    if (g_writeStubPtr) {
        VirtualFree(g_writeStubPtr, 0, MEM_RELEASE);
        g_writeStubPtr = nullptr;
    }

    g_ssnCached = -1;
    g_writeSsn = -1;
    g_lastStatus = 0;
    g_writeLastStatus = 0;
    g_ssnSource = 0;
    g_selfTest = 0;
    g_writeSelfTest = 0;
    g_cowSelfTest = 0;
    g_selfTestStatusRWX = 0;
    g_selfTestStatusRO = 0;
    g_writeSelfTestStatus = 0;
    g_cowSelfTestStatusEWC = 0;
    g_cowSelfTestStatusWC = 0;
}

} // namespace stealth

namespace detail {

inline int InstrLength(const uint8_t* code) {
    const uint8_t* p = code;
    while (*p == 0x66 || *p == 0x67 || *p == 0xF2 || *p == 0xF3 ||
           (*p >= 0x40 && *p <= 0x4F)) ++p;
    uint8_t op = *p++;
    if (op == 0x90 || op == 0xC3 || op == 0xCC || op == 0xCB) return static_cast<int>(p - code);
    if (op >= 0x50 && op <= 0x5F) return static_cast<int>(p - code);
    if (op == 0xC2) return static_cast<int>(p - code) + 2;
    if ((op >= 0x70 && op <= 0x7F) || op == 0xEB || op == 0xE3) return static_cast<int>(p - code) + 1;
    if (op == 0xE8 || op == 0xE9) return static_cast<int>(p - code) + 4;

    bool hasModRM = false;
    int imm = 0;
    if (op == 0x80 || op == 0x82 || op == 0x83) { hasModRM = true; imm = 1; }
    else if (op == 0x81 || op == 0xC7 || op == 0xF7) { hasModRM = true; imm = 4; }
    else if (op == 0xC6) { hasModRM = true; imm = 1; }
    else if (op == 0x0F) {
        uint8_t op2 = *p++;
        if (op2 >= 0x80 && op2 <= 0x8F) return static_cast<int>(p - code) + 4;
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
        uint8_t rm = modrm & 7;
        if (mod != 3 && rm == 4) ++p;
        if (mod == 0 && rm == 5) p += 4;
        else if (mod == 1) p += 1;
        else if (mod == 2) p += 4;
        p += imm;
    }
    const int len = static_cast<int>(p - code);
    return len > 0 ? len : 1;
}

inline int StolenSize(uintptr_t at, int minBytes) {
    int total = 0;
    while (total < minBytes) total += InstrLength(reinterpret_cast<const uint8_t*>(at + total));
    return total;
}

constexpr int kJmpPatchSize = 14;
constexpr int kMaxStolen = 32;

enum InstallStatus : uint8_t {
    kInst_NotAttempted = 0,
    kInst_OK           = 1,
    kInst_BadArgs      = 2,
    kInst_DecodeFail   = 3,
    kInst_VAllocFail   = 4,
    kInst_VProtectFail = 5,
    kInst_SehFault     = 6,
};

struct RawAsmDetour {
    uintptr_t targetAddr = 0;
    int stolenSize = 0;
    uint8_t originalBytes[kMaxStolen] = {};
    uintptr_t trampolineAddr = 0;
    volatile long long hitCounter = 0;
    bool installed = false;
    uint8_t lastStatus = kInst_NotAttempted;

    bool Install(uintptr_t target, uintptr_t loggerFn) {
        if (installed) return true;
        if (!target || !loggerFn) {
            lastStatus = kInst_BadArgs;
            return false;
        }
        targetAddr = target;
        __try {
            stolenSize = StolenSize(target, kJmpPatchSize);
            if (stolenSize <= 0 || stolenSize > kMaxStolen) {
                lastStatus = kInst_DecodeFail;
                return false;
            }

            trampolineAddr = reinterpret_cast<uintptr_t>(VirtualAlloc(
                nullptr, 256, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
            if (!trampolineAddr) {
                lastStatus = kInst_VAllocFail;
                return false;
            }

            std::memcpy(originalBytes, reinterpret_cast<void*>(target), stolenSize);
            uint8_t* p = reinterpret_cast<uint8_t*>(trampolineAddr);
            size_t off = 0;
            auto emit = [&](const uint8_t* bytes, size_t n) {
                std::memcpy(p + off, bytes, n);
                off += n;
            };

            const uint8_t subRsp[]  = { 0x48, 0x81, 0xEC, 0x88, 0x00, 0x00, 0x00 };
            const uint8_t addRsp[]  = { 0x48, 0x81, 0xC4, 0x88, 0x00, 0x00, 0x00 };
            const uint8_t saveRcx[] = { 0x48, 0x89, 0x4C, 0x24, 0x20 };
            const uint8_t saveRdx[] = { 0x48, 0x89, 0x54, 0x24, 0x28 };
            const uint8_t saveR8[]  = { 0x4C, 0x89, 0x44, 0x24, 0x30 };
            const uint8_t saveR9[]  = { 0x4C, 0x89, 0x4C, 0x24, 0x38 };
            const uint8_t loadRcx[] = { 0x48, 0x8B, 0x4C, 0x24, 0x20 };
            const uint8_t loadRdx[] = { 0x48, 0x8B, 0x54, 0x24, 0x28 };
            const uint8_t loadR8[]  = { 0x4C, 0x8B, 0x44, 0x24, 0x30 };
            const uint8_t loadR9[]  = { 0x4C, 0x8B, 0x4C, 0x24, 0x38 };
            const uint8_t loadSavedRegs[] = { 0x4C, 0x8D, 0x4C, 0x24, 0x38 };
            const uint8_t saveX0[]  = { 0x0F, 0x29, 0x44, 0x24, 0x40 };
            const uint8_t saveX1[]  = { 0x0F, 0x29, 0x4C, 0x24, 0x50 };
            const uint8_t saveX2[]  = { 0x0F, 0x29, 0x54, 0x24, 0x60 };
            const uint8_t saveX3[]  = { 0x0F, 0x29, 0x5C, 0x24, 0x70 };
            const uint8_t loadX0[]  = { 0x0F, 0x28, 0x44, 0x24, 0x40 };
            const uint8_t loadX1[]  = { 0x0F, 0x28, 0x4C, 0x24, 0x50 };
            const uint8_t loadX2[]  = { 0x0F, 0x28, 0x54, 0x24, 0x60 };
            const uint8_t loadX3[]  = { 0x0F, 0x28, 0x5C, 0x24, 0x70 };

            emit(subRsp, sizeof(subRsp));
            emit(saveRcx, sizeof(saveRcx)); emit(saveRdx, sizeof(saveRdx));
            emit(saveR8, sizeof(saveR8));   emit(saveR9, sizeof(saveR9));
            emit(saveX0, sizeof(saveX0));   emit(saveX1, sizeof(saveX1));
            emit(saveX2, sizeof(saveX2));   emit(saveX3, sizeof(saveX3));
            emit(loadRcx, sizeof(loadRcx)); emit(loadRdx, sizeof(loadRdx));
            emit(loadR8, sizeof(loadR8));   emit(loadSavedRegs, sizeof(loadSavedRegs));
            p[off++] = 0x48; p[off++] = 0xB8;
            *reinterpret_cast<uintptr_t*>(p + off) = loggerFn; off += 8;
            p[off++] = 0xFF; p[off++] = 0xD0;
            emit(loadX0, sizeof(loadX0));   emit(loadX1, sizeof(loadX1));
            emit(loadX2, sizeof(loadX2));   emit(loadX3, sizeof(loadX3));
            emit(loadRcx, sizeof(loadRcx)); emit(loadRdx, sizeof(loadRdx));
            emit(loadR8, sizeof(loadR8));   emit(loadR9, sizeof(loadR9));
            emit(addRsp, sizeof(addRsp));
            emit(originalBytes, stolenSize);
            p[off++] = 0xFF; p[off++] = 0x25;
            *reinterpret_cast<int32_t*>(p + off) = 0; off += 4;
            *reinterpret_cast<uintptr_t*>(p + off) = target + stolenSize; off += 8;
            FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(trampolineAddr), off);

            DWORD oldProt = 0;
            if (!stealth::VirtualProtectDirect(reinterpret_cast<void*>(target), stolenSize,
                                               PAGE_EXECUTE_WRITECOPY, &oldProt)) {
                VirtualFree(reinterpret_cast<void*>(trampolineAddr), 0, MEM_RELEASE);
                trampolineAddr = 0;
                lastStatus = kInst_VProtectFail;
                return false;
            }
            std::memset(reinterpret_cast<void*>(target), 0x90, stolenSize);
            auto* t = reinterpret_cast<uint8_t*>(target);
            t[0] = 0xFF; t[1] = 0x25;
            *reinterpret_cast<int32_t*>(t + 2) = 0;
            *reinterpret_cast<uintptr_t*>(t + 6) = trampolineAddr;
            stealth::VirtualProtectDirect(reinterpret_cast<void*>(target), stolenSize, oldProt, &oldProt);
            FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(target), stolenSize);

            installed = true;
            lastStatus = kInst_OK;
            return true;
        } __except (1) {
            if (trampolineAddr) {
                VirtualFree(reinterpret_cast<void*>(trampolineAddr), 0, MEM_RELEASE);
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
            stealth::VirtualProtectDirect(reinterpret_cast<void*>(targetAddr), stolenSize,
                                          PAGE_EXECUTE_WRITECOPY, &oldProt);
            std::memcpy(reinterpret_cast<void*>(targetAddr), originalBytes, stolenSize);
            stealth::VirtualProtectDirect(reinterpret_cast<void*>(targetAddr), stolenSize,
                                          oldProt, &oldProt);
            FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(targetAddr), stolenSize);
        } __except (1) {}

        if (trampolineAddr) {
            VirtualFree(reinterpret_cast<void*>(trampolineAddr), 0, MEM_RELEASE);
            trampolineAddr = 0;
        }
        installed = false;
    }

    template <typename T>
    T GetOriginal() const { return reinterpret_cast<T>(trampolineAddr); }
};

} // namespace detail

} // namespace CoreEventHook

namespace CoreHookTest {

namespace Hooks {
    constexpr uintptr_t OnIntegerPropertyChange = ::Offset::Hooks::OnIntegerPropertyChange;
    constexpr uintptr_t ClientMainLoop          = ::Offset::Hooks::ClientMainLoop;
    constexpr uintptr_t CreateClientEffect      = ::Offset::Hooks::CreateClientEffect;
    constexpr uintptr_t DispatchEvent           = ::Offset::Hooks::DispatchEvent;
    constexpr uintptr_t OnTeleport              = ::Offset::Hooks::OnTeleport;
    constexpr uintptr_t Hud_OnDisconnect        = ::Offset::Hooks::Hud_OnDisconnect;
    constexpr uintptr_t ProcessCastSpell        = ::Offset::Hooks::ProcessCastSpell;
    constexpr uintptr_t OnUpdateChargeableSpell = ::Offset::Hooks::OnUpdateChargeableSpell;
    constexpr uintptr_t OnBuffAdd               = ::Offset::Hooks::OnBuffAdd;
    constexpr uintptr_t OnBuffRemove            = ::Offset::Hooks::OnBuffRemove;
    constexpr uintptr_t OnBuffUpdate            = ::Offset::Hooks::OnBuffUpdate;
    constexpr uintptr_t OnBuffGain              = ::Offset::Hooks::OnBuffGain;
    constexpr uintptr_t OnCreate                = ::Offset::Hooks::OnCreate;
    constexpr uintptr_t OnDelete                = ::Offset::Hooks::OnDelete;
    constexpr uintptr_t OnMissileCreate         = ::Offset::Hooks::OnMissileCreate;
    constexpr uintptr_t OnMissileDelete         = ::Offset::Hooks::OnMissileDelete;
    constexpr uintptr_t OnDamage                = ::Offset::Hooks::OnDamage;
    constexpr uintptr_t OnDoCast                = ::Offset::Hooks::OnDoCast;
    constexpr uintptr_t OnFinishCast            = ::Offset::Hooks::OnFinishCast;
    constexpr uintptr_t OnGameUpdate            = ::Offset::Hooks::OnGameUpdate;
    constexpr uintptr_t OnLevelUp               = ::Offset::Hooks::OnLevelUp;
    constexpr uintptr_t OnPlayAnimation         = ::Offset::Hooks::OnPlayAnimation;
    constexpr uintptr_t OnPlayAnimationWrapper  = ::Offset::Hooks::OnPlayAnimationWrapper;
    constexpr uintptr_t OnProcessSpell          = ::Offset::Hooks::OnProcessSpell;
    constexpr uintptr_t OnSpellImpact           = ::Offset::Hooks::OnSpellImpact;
    constexpr uintptr_t OnStopCast              = ::Offset::Hooks::OnStopCast;
    constexpr uintptr_t OnStealth               = ::Offset::Hooks::OnStealth;
    constexpr uintptr_t OnSurrender             = ::Offset::Hooks::OnSurrender;
    constexpr uintptr_t ProcessWorldEvent       = ::Offset::Hooks::ProcessWorldEvent;
    constexpr uintptr_t OnNewPath               = ::Offset::Hooks::OnNewPath;
} // namespace Hooks

namespace Offsets = Hooks;

enum HookId : int {
    OnIntegerPropertyChange = 0,
    ClientMainLoop,
    CreateClientEffect,
    DispatchEvent,
    OnTeleport,
    Hud_OnDisconnect,
    OnBuffAdd,
    OnBuffRemove,
    OnBuffUpdate,
    OnDamage,
    OnDoCast,
    OnFinishCast,
    OnGameUpdate,
    OnLevelUp,
    OnPlayAnimation,
    OnPlayAnimationWrapper,
    OnProcessSpell,
    OnSpellImpact,
    OnStopCast,
    OnStealth,
    OnSurrender,
    ProcessWorldEvent,
    ProcessCastSpell,
    OnUpdateChargeableSpell,
    OnBuffGain,
    OnCreate,
    OnDelete,
    OnMissileCreate,
    OnMissileDelete,
    OnNewPath,
    HookCount
};

struct HookSpec {
    const char* name;
    uintptr_t   rva;
};

inline constexpr HookSpec kHookSpecs[HookCount] = {
    { "OnIntegerPropertyChange", Offsets::OnIntegerPropertyChange },
    { "ClientMainLoop",          Offsets::ClientMainLoop },
    { "CreateClientEffect",      Offsets::CreateClientEffect },
    { "DispatchEvent",           Offsets::DispatchEvent },
    { "OnTeleport",              Offsets::OnTeleport },
    { "Hud_OnDisconnect",        Offsets::Hud_OnDisconnect },
    { "OnBuffAdd",               Offsets::OnBuffAdd },
    { "OnBuffRemove",            Offsets::OnBuffRemove },
    { "OnBuffUpdate",            Offsets::OnBuffUpdate },
    { "OnDamage",                Offsets::OnDamage },
    { "OnDoCast",                Offsets::OnDoCast },
    { "OnFinishCast",            Offsets::OnFinishCast },
    { "OnGameUpdate",            Offsets::OnGameUpdate },
    { "OnLevelUp",               Offsets::OnLevelUp },
    { "OnPlayAnimation",         Offsets::OnPlayAnimation },
    { "OnPlayAnimWrapper",       Offsets::OnPlayAnimationWrapper },
    { "OnProcessSpell",          Offsets::OnProcessSpell },
    { "OnSpellImpact",           Offsets::OnSpellImpact },
    { "OnStopCast",              Offsets::OnStopCast },
    { "OnStealth",               Offsets::OnStealth },
    { "OnSurrender",             Offsets::OnSurrender },
    { "ProcessWorldEvent",       Offsets::ProcessWorldEvent },
    { "ProcessCastSpell",        Offsets::ProcessCastSpell },
    { "OnUpdateChargeableSpell", Offsets::OnUpdateChargeableSpell },
    { "OnBuffGain",              Offsets::OnBuffGain },
    { "OnCreate",                Offsets::OnCreate },
    { "OnDelete",                Offsets::OnDelete },
    { "OnMissileCreate",         Offsets::OnMissileCreate },
    { "OnMissileDelete",         Offsets::OnMissileDelete },
    { "OnNewPath",               Offsets::OnNewPath },
};

inline uintptr_t HookInstallRva(HookId id) {
    return (id >= 0 && id < HookCount) ? kHookSpecs[id].rva : 0;
}

inline CoreEventHook::detail::RawAsmDetour g_detours[HookCount] = {};
inline volatile long long g_hits[HookCount] = {};
inline volatile uint8_t g_status[HookCount] = {};

inline constexpr const char* kLogPath =
    "C:\\Users\\Public\\nightsharp_corehook_test.txt";

inline void WriteLog(const char* text) {
    if (!text || !*text) return;
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
    WriteFile(h, text, (DWORD)lstrlenA(text), &written, nullptr);
    CloseHandle(h);
}

inline void Logf(const char* fmt, ...) {
    char buf[512] = {};

    SYSTEMTIME st{};
    GetLocalTime(&st);
    int n = _snprintf_s(
        buf,
        sizeof(buf),
        _TRUNCATE,
        "[%02u:%02u:%02u.%03u T%u] ",
        (unsigned)st.wHour,
        (unsigned)st.wMinute,
        (unsigned)st.wSecond,
        (unsigned)st.wMilliseconds,
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
    buf[total] = 0;
    WriteLog(buf);
}

inline const char* StatusLabel(uint8_t status) {
    using namespace CoreEventHook::detail;
    switch (status) {
        case kInst_NotAttempted: return "not-attempted";
        case kInst_OK:           return "ok";
        case kInst_BadArgs:      return "bad-args";
        case kInst_DecodeFail:   return "decode-fail";
        case kInst_VAllocFail:   return "alloc-fail";
        case kInst_VProtectFail: return "protect-fail";
        case kInst_SehFault:     return "seh-fault";
        default:                 return "unknown";
    }
}

inline int FindHookByRva(uintptr_t absOrRva) {
    const uintptr_t base = CoreEventHook::shim::GetGameBase();
    const uintptr_t rva =
        (base && absOrRva >= base) ? (absOrRva - base) : absOrRva;

    for (int i = 0; i < HookCount; ++i) {
        if (kHookSpecs[i].rva && kHookSpecs[i].rva == rva) return i;
        const uintptr_t installRva = HookInstallRva(static_cast<HookId>(i));
        if (installRva && installRva == rva) return i;
    }
    return -1;
}

inline int FindInstalledHookByRva(HookId id) {
    if (id < 0 || id >= HookCount) return -1;
    const uintptr_t rva = HookInstallRva(id);
    if (!rva) return -1;
    for (int i = 0; i < HookCount; ++i) {
        if (i != id &&
            HookInstallRva(static_cast<HookId>(i)) == rva &&
            g_detours[i].installed) {
            return i;
        }
    }
    return -1;
}

inline bool IsInlineAllowed(HookId id) {
#if !NIGHTSHARP_ENABLE_CLIENTMAINLOOP_HOOK
    if (id == ClientMainLoop) {
        return false;
    }
#endif
#if !NIGHTSHARP_ENABLE_ONMISSILEDELETE_HOOK
    if (id == OnMissileDelete) {
        return false;
    }
#endif
#if !NIGHTSHARP_ENABLE_ONCREATE_HOOK
    if (id == OnCreate) {
        return false;
    }
#endif
#if !NIGHTSHARP_ENABLE_ONDELETE_HOOK
    if (id == OnDelete) {
        return false;
    }
#endif
    return id >= 0 && id < HookCount && HookInstallRva(id) != 0;
}

inline const char* InstallNote(HookId id) {
#if !NIGHTSHARP_ENABLE_CLIENTMAINLOOP_HOOK
    if (id == ClientMainLoop) {
        return "disabled by default; use OnGameUpdate for update/debug flow";
    }
#endif
#if !NIGHTSHARP_ENABLE_ONMISSILEDELETE_HOOK
    if (id == OnMissileDelete) {
        return "temporarily disabled; missile destructor/remove hook can run while missile memory is being destroyed";
    }
#endif
#if !NIGHTSHARP_ENABLE_ONCREATE_HOOK
    if (id == OnCreate) {
        return "disabled by default; old ObjectManager tree-insert hook was unstable";
    }
#endif
#if !NIGHTSHARP_ENABLE_ONDELETE_HOOK
    if (id == OnDelete) {
        return "disabled by default; DestroyObject hook was prone to vfunc crashes";
    }
#endif
    if (id >= 0 && id < HookCount && HookInstallRva(id) == 0) {
        return "RVA is 0; unresolved hook target";
    }
    if (id == ProcessCastSpell) {
        return "AIBaseClient PKT_NPC_CastSpellAns_s handler";
    }
    if (id == OnUpdateChargeableSpell) {
        return "charged spell update/release wrapper";
    }
    if (id == OnBuffAdd) {
        return "SpellBuffReceive event bridge; replaces wrong shop/UI 0xC07A30";
    }
    if (id == OnBuffRemove) {
        return "SpellBuffRemove event bridge";
    }
    if (id == OnBuffUpdate) {
        return "BuffCounterSet event bridge; stack/count refresh candidate";
    }
    if (id == OnBuffGain) {
        return "native AIBaseClient OnBuffGain/OnBuffAdd dispatcher";
    }
    if (id == OnCreate) {
        return "AssignNetworkId; RCX object, RDX networkId. Call original first";
    }
    if (id == OnDelete) {
        return "DestroyObject; RCX manager, RDX object. Fires before original";
    }
    if (id == OnMissileCreate) {
        return "MissileClient init path; inserts into MissileManager";
    }
    if (id == OnMissileDelete) {
        return "MissileClient destructor/remove path; erases from MissileManager";
    }
    if (id == OnNewPath) {
        return "common AIBaseClient path packet consumer; RDX path array, stack0 dash flag";
    }
    if (id == OnTeleport) {
        return "recall/teleport state handler; EnsoulSharp AttackableUnit.OnTeleport surface";
    }
    if (id == OnIntegerPropertyChange) {
        return "PKT_S2C_ReplicateFields_s handler; diff replicated integer fields in the consumer";
    }
    if (id == OnProcessSpell) {
        return "test offset 0x97A480; second 40 53 48 83 EC 30 4C 8B 02 match; has code callers";
    }
    if (id == OnDamage) {
        return "PKT_UnitApplyDamage_s handler; full source/target/damage decode, registered in packet table";
    }
    if (id == OnPlayAnimation) {
        return "packet animation callback (0x11D/0x1F1); RCX is AIBaseClient, name view is at RDX+0x18";
    }
    if (id == OnPlayAnimationWrapper) {
        return "central animation wrapper; owner is *(RCX+0x8), input name is the RDX string-view";
    }
    if (id == OnStealth) {
        return "PKT_StealthFlagsChanged_s handler; stealth buff catalog path is a separate derived fallback";
    }
    if (id == OnSurrender) {
        return "mid-function probe in PKT_S2C_SetCanSurrender_s handler; skips RIP-relative prologue";
    }
    return "";
}

inline bool HasInstallNote(HookId id) {
    const char* note = InstallNote(id);
    return note && *note;
}

inline bool IsEventGated(HookId id) {
    switch (id) {
        case OnIntegerPropertyChange:
        case OnTeleport:
        case OnBuffAdd:
        case OnBuffRemove:
        case OnBuffUpdate:
        case OnBuffGain:
        case OnCreate:
        case OnDelete:
        case OnMissileCreate:
        case OnMissileDelete:
        case OnNewPath:
        case OnDamage:
        case ProcessCastSpell:
        case OnFinishCast:
        case OnPlayAnimation:
        case OnPlayAnimationWrapper:
        case OnSpellImpact:
        case OnStopCast:
        case OnStealth:
        case OnSurrender:
            return true;
        default:
            return false;
    }
}

struct HookSavedRegisterBlock {
    uintptr_t r9 = 0;
    float xmm0[4] = {};
    float xmm1[4] = {};
    float xmm2[4] = {};
    float xmm3[4] = {};
};

using HookCallbackFn = void(__fastcall*)(int id,
                                         uintptr_t rcx,
                                         uint64_t rdx,
                                         uintptr_t r8,
                                         uintptr_t r9,
                                         float xmm0,
                                         float xmm1,
                                         float xmm2,
                                         float xmm3,
                                         uintptr_t stack0,
                                         uintptr_t stack1,
                                         uintptr_t stack2,
                                         uintptr_t stack3,
                                         uintptr_t stack4,
                                         uintptr_t stack5);
inline HookCallbackFn g_callback = nullptr;

inline void __fastcall Logger(uintptr_t rcx, uint64_t rdx, uintptr_t r8, const HookSavedRegisterBlock* saved) {
    uintptr_t ret = 0;
    __try {
        ret = reinterpret_cast<uintptr_t>(_ReturnAddress());
    } __except (1) {
        ret = 0;
    }

    int id = -1;
    for (int i = 0; i < HookCount; ++i) {
        const auto& d = g_detours[i];
        if (d.trampolineAddr && ret >= d.trampolineAddr &&
            ret < d.trampolineAddr + 256) {
            id = i;
            break;
        }
    }

    if (id < 0 || id >= HookCount) return;

    uintptr_t r9 = 0;
    float xmm0 = 0.0f;
    float xmm1 = 0.0f;
    float xmm2 = 0.0f;
    float xmm3 = 0.0f;
    uintptr_t stack0 = 0;
    uintptr_t stack1 = 0;
    uintptr_t stack2 = 0;
    uintptr_t stack3 = 0;
    uintptr_t stack4 = 0;
    uintptr_t stack5 = 0;
    __try {
        if (saved) {
            r9 = saved->r9;
            xmm0 = saved->xmm0[0];
            xmm1 = saved->xmm1[0];
            xmm2 = saved->xmm2[0];
            xmm3 = saved->xmm3[0];

            // The trampoline passes &savedR9 at originalRSP-0x50. Win64
            // stack arguments therefore begin at saved+0x78
            // (originalRSP+0x28). Capture them before the target runs.
            const auto stackArgs =
                reinterpret_cast<const uintptr_t*>(
                    reinterpret_cast<uintptr_t>(saved) + 0x78);
            stack0 = stackArgs[0];
            stack1 = stackArgs[1];
            stack2 = stackArgs[2];
            stack3 = stackArgs[3];
            stack4 = stackArgs[4];
            stack5 = stackArgs[5];
        }
    } __except (1) {
        r9 = 0;
        xmm0 = xmm1 = xmm2 = xmm3 = 0.0f;
        stack0 = stack1 = stack2 = stack3 = stack4 = stack5 = 0;
    }

    const long long hit = InterlockedIncrement64(
        const_cast<long long*>(&g_hits[id]));

    for (int i = 0; i < HookCount; ++i) {
        if (i != id &&
            HookInstallRva(static_cast<HookId>(i)) ==
                HookInstallRva(static_cast<HookId>(id))) {
            InterlockedExchange64(const_cast<long long*>(&g_hits[i]), hit);
            g_status[i] = CoreEventHook::detail::kInst_OK;
        }
    }

    if (g_callback) {
        __try {
            g_callback(
                id, rcx, rdx, r8, r9, xmm0, xmm1, xmm2, xmm3,
                stack0, stack1, stack2, stack3, stack4, stack5);
            for (int i = 0; i < HookCount; ++i) {
                if (i != id &&
                    HookInstallRva(static_cast<HookId>(i)) ==
                        HookInstallRva(static_cast<HookId>(id))) {
                    g_callback(
                        i, rcx, rdx, r8, r9, xmm0, xmm1, xmm2, xmm3,
                        stack0, stack1, stack2, stack3, stack4, stack5);
                }
            }
        } __except (1) {}
    }

    if (hit <= 8 || (hit % 1000) == 0) {
        Logf(
            "hit %-24s count=%lld rcx=%p rdx=%p r8=%p r9=%p",
            kHookSpecs[id].name,
            hit,
            (void*)rcx,
            (void*)rdx,
            (void*)r8,
            (void*)r9);
    }
}

inline bool InstallHook(HookId id);

inline int InstallInlineHooks() {
    const uintptr_t base = CoreEventHook::shim::GetGameBase();
    if (!base) {
        Logf("Inline+EPT install failed: game base is null");
        return 0;
    }

    // Run the local backend probes before patching function prologues.
    (void)CoreEventHook::stealth::SelfTest(base + Offsets::OnStopCast);
    (void)CoreEventHook::stealth::SelfTestWrite(base + Offsets::OnStopCast);
    (void)CoreEventHook::stealth::SelfTestCoW(base + Offsets::OnStopCast);

    int installed = 0;
    for (int i = 0; i < HookCount; ++i) {
        const auto id = static_cast<HookId>(i);
        if (!IsInlineAllowed(id)) {
            g_status[i] = CoreEventHook::detail::kInst_BadArgs;
            Logf("Inline+EPT %-24s skipped: %s",
                 kHookSpecs[i].name,
                 InstallNote(id));
            continue;
        }

        if (InstallHook(id)) {
            ++installed;
        }
    }
    return installed;
}

inline bool InstallHook(HookId id) {
    if (id < 0 || id >= HookCount) return false;
    if (!IsInlineAllowed(id)) {
        g_status[id] = CoreEventHook::detail::kInst_BadArgs;
        Logf("Inline+EPT single %-24s skipped: %s",
             kHookSpecs[id].name,
             InstallNote(id));
        return false;
    }

    const uintptr_t base = CoreEventHook::shim::GetGameBase();
    if (!base) return false;
    auto& d = g_detours[id];
    if (d.installed) {
        g_status[id] = CoreEventHook::detail::kInst_OK;
        Logf("Inline+EPT single %-24s already installed", kHookSpecs[id].name);
        return true;
    }

    const int carrier = FindInstalledHookByRva(id);
    if (carrier >= 0) {
        g_status[id] = CoreEventHook::detail::kInst_OK;
        Logf("Inline+EPT single %-24s aliased to installed %-24s rva=0x%llX%s%s",
             kHookSpecs[id].name,
             kHookSpecs[carrier].name,
             (unsigned long long)kHookSpecs[id].rva,
             HasInstallNote(id) ? " note=" : "",
             HasInstallNote(id) ? InstallNote(id) : "");
        return false;
    }

    const uintptr_t installRva = HookInstallRva(id);
    const bool ok = d.Install(base + installRva, reinterpret_cast<uintptr_t>(&Logger));
    g_status[id] = d.lastStatus;
    Logf(
        "Inline+EPT single %-24s rva=0x%llX installRva=0x%llX target=%p status=%s stolen=%d tramp=%p%s%s",
        kHookSpecs[id].name,
        (unsigned long long)kHookSpecs[id].rva,
        (unsigned long long)installRva,
        (void*)(base + installRva),
        StatusLabel(d.lastStatus),
        d.stolenSize,
        (void*)d.trampolineAddr,
        HasInstallNote(id) ? " note=" : "",
        HasInstallNote(id) ? InstallNote(id) : "");
    return ok;
}

inline bool UninstallHook(HookId id) {
    if (id < 0 || id >= HookCount) return false;

    int target = static_cast<int>(id);
    if (!g_detours[target].installed) {
        const int carrier = FindInstalledHookByRva(id);
        if (carrier < 0) {
            g_status[target] = CoreEventHook::detail::kInst_NotAttempted;
            return false;
        }
        target = carrier;
    }

    const uintptr_t rva = HookInstallRva(static_cast<HookId>(target));
    g_detours[target].Uninstall();
    for (int i = 0; i < HookCount; ++i) {
        if (HookInstallRva(static_cast<HookId>(i)) == rva) {
            g_status[i] = CoreEventHook::detail::kInst_NotAttempted;
        }
    }
    Logf("Inline+EPT single %-24s uninstalled", kHookSpecs[target].name);
    return true;
}

inline int InstallAllHooks(bool installInline = true) {
    Logf("InstallAllHooks begin");
    int count = 0;
    if (installInline) count += InstallInlineHooks();
    Logf("InstallAllHooks done installed=%d", count);
    return count;
}

inline int EnsureInstalled(bool installInline = true) {
    static volatile LONG s_installStarted = 0;
    static volatile LONG s_installFinished = 0;
    static volatile LONG s_installCount = 0;

    if (InterlockedCompareExchange(&s_installStarted, 1, 0) == 0) {
        const int count = InstallAllHooks(installInline);
        InterlockedExchange(&s_installCount, count);
        InterlockedExchange(&s_installFinished, 1);
        return count;
    }

    while (InterlockedCompareExchange(&s_installFinished, 0, 0) == 0) {
        Sleep(1);
    }
    return static_cast<int>(InterlockedCompareExchange(&s_installCount, 0, 0));
}

inline void UninstallInlineHooks() {
    for (int i = HookCount - 1; i >= 0; --i) {
        g_detours[i].Uninstall();
        g_status[i] = CoreEventHook::detail::kInst_NotAttempted;
    }
    Logf("Inline+EPT hooks uninstalled");
}

inline void UninstallAll() {
    UninstallInlineHooks();
    CoreEventHook::stealth::Shutdown();
}

inline long long HitCount(HookId id) {
    return (id >= 0 && id < HookCount) ? g_hits[id] : 0;
}

inline bool IsInstalled(HookId id) {
    return id >= 0 && id < HookCount &&
        (g_detours[id].installed || FindInstalledHookByRva(id) >= 0);
}

inline uint8_t Status(HookId id) {
    return (id >= 0 && id < HookCount)
        ? g_status[id]
        : CoreEventHook::detail::kInst_BadArgs;
}

inline uintptr_t TargetAddress(HookId id) {
    if (id < 0 || id >= HookCount) return 0;
    const uintptr_t base = CoreEventHook::shim::GetGameBase();
    const uintptr_t installRva = HookInstallRva(id);
    return (base && installRva) ? base + installRva : 0;
}

inline int InstalledCount() {
    int count = 0;
    for (int i = 0; i < HookCount; ++i) {
        if (g_detours[i].installed) ++count;
    }
    return count;
}

inline void DumpHookStatus(const char* reason) {
    Logf("HookStatus begin reason=%s installed=%d",
         reason ? reason : "?",
         InstalledCount());

    for (int i = 0; i < HookCount; ++i) {
        const auto id = static_cast<HookId>(i);
        const auto& detour = g_detours[i];
        Logf(
            "HookStatus %-24s rva=0x%llX target=%p installed=%d status=%s hits=%lld stolen=%d tramp=%p note=%s",
            kHookSpecs[i].name,
            static_cast<unsigned long long>(kHookSpecs[i].rva),
            reinterpret_cast<void*>(TargetAddress(id)),
            IsInstalled(id) ? 1 : 0,
            StatusLabel(Status(id)),
            HitCount(id),
            detour.stolenSize,
            reinterpret_cast<void*>(detour.trampolineAddr),
            HasInstallNote(id) ? InstallNote(id) : "");
    }

    Logf("HookStatus end reason=%s", reason ? reason : "?");
}

inline void ResetHits() {
    for (int i = 0; i < HookCount; ++i) {
        InterlockedExchange64(const_cast<long long*>(&g_hits[i]), 0);
    }
    Logf("hit counters reset");
}

} // namespace CoreHookTest

extern "C" __declspec(dllexport) inline const uintptr_t g_CoreHookTestRVAs[] = {
    0x4348544553543031ULL, // "CHTEST01"
    CoreHookTest::Offsets::OnIntegerPropertyChange,
    CoreHookTest::Offsets::ClientMainLoop,
    CoreHookTest::Offsets::CreateClientEffect,
    CoreHookTest::Offsets::DispatchEvent,
    CoreHookTest::Offsets::OnTeleport,
    CoreHookTest::Offsets::Hud_OnDisconnect,
    CoreHookTest::Offsets::OnBuffAdd,
    CoreHookTest::Offsets::OnBuffRemove,
    CoreHookTest::Offsets::OnBuffUpdate,
    CoreHookTest::Offsets::OnDamage,
    CoreHookTest::Offsets::OnDoCast,
    CoreHookTest::Offsets::OnFinishCast,
    CoreHookTest::Offsets::OnGameUpdate,
    CoreHookTest::Offsets::OnLevelUp,
    CoreHookTest::Offsets::OnPlayAnimation,
    CoreHookTest::Offsets::OnPlayAnimationWrapper,
    CoreHookTest::Offsets::OnProcessSpell,
    CoreHookTest::Offsets::OnSpellImpact,
    CoreHookTest::Offsets::OnStopCast,
    CoreHookTest::Offsets::OnStealth,
    CoreHookTest::Offsets::OnSurrender,
    CoreHookTest::Offsets::ProcessWorldEvent,
    CoreHookTest::Offsets::ProcessCastSpell,
    CoreHookTest::Offsets::OnUpdateChargeableSpell,
    CoreHookTest::Offsets::OnBuffGain,
    CoreHookTest::Offsets::OnCreate,
    CoreHookTest::Offsets::OnDelete,
    CoreHookTest::Offsets::OnMissileCreate,
    CoreHookTest::Offsets::OnMissileDelete,
    CoreHookTest::Offsets::OnNewPath,
    0,
};
