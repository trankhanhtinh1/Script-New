#pragma once

// ============================================================================
// PackmanHook - CRC Bypass for Riot Packman anti-cheat (Shadow Copy + NOP JNE)
// ============================================================================
//
// Method má»›i (thay cho ChaCha20 QR hook Ä‘Ã£ bá»‹ detect):
//   1) Shadow Copy: memcpy toÃ n bá»™ stub.dll â†’ RW buffer TRÆ¯á»šC khi patch.
//      Báº£n sao sáº¡ch giá»¯ CRC values Ä‘Ãºng cho má»i section.
//   2) NOP JNE: patch 6 bytes táº¡i CRC comparison point (cmp + jne).
//      AOB: 48 3B 05 ?? ?? ?? ?? 0F 85 â†’ jne â†’ nop x6
//      â†’ mismatch handler unreachable, CRC check trá»Ÿ thÃ nh no-op.
//
// KhÃ´ng inline hook, khÃ´ng faked regions, khÃ´ng AddPatchAddress.
// Footprint: 1 VirtualAlloc (shadow, RW) + 6 bytes NOP (jne).
// Logging chá»‰ qua OutputDebugString (Ä‘i vÃ o DbgPrint buffer).
// ============================================================================

#include <Windows.h>
#include <psapi.h>
#include <intrin.h>
#include <vector>
#include <mutex>
#include <cstdint>
#include <cstdio>
#include <cstdarg>
#include <cstring>

#include "CoreBypass.h"

#pragma warning(disable: 4996)

// â”€â”€ Master switch: CRC Bypass â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
// 0 = DISABLED. CRCBypass::Install / Uninstall trá»Ÿ thÃ nh no-op.
// 1 = ENABLED. Shadow Copy + NOP JNE bypass hoáº¡t Ä‘á»™ng.
#ifndef NIGHTSHARP_ENABLE_CRC_BYPASS
#define NIGHTSHARP_ENABLE_CRC_BYPASS 1
#endif

// â”€â”€ Logging â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
// Ghi vÃ o %TEMP%\ph.log + OutputDebugString. ÄÆ°á»ng dáº«n %TEMP% lÃ  user-scope,
// Ã­t bá»‹ anti-cheat sweep hÆ¡n C:\Users\Public. TÃªn file ngáº¯n, khÃ´ng cÃ³
// tá»« khoÃ¡ ("packman", "hook", "bypass") Ä‘á»ƒ giáº£m risk pattern match.

// â”€â”€ Log control (Module J) â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
inline volatile LONG g_logEnabled = 1;

inline void SetLogEnabled(bool en) {
    InterlockedExchange(&g_logEnabled, en ? 1 : 0);
}
inline bool IsLogEnabled() {
    return InterlockedCompareExchange(&g_logEnabled, 0, 0) != 0;
}

static const char* GetLogPath() {
#ifdef NS_PACKMAN_SILENT
    return "";   // khÃ´ng dÃ¹ng
#else
    static char path[MAX_PATH] = {};
    if (path[0]) return path;
    char tmp[MAX_PATH] = {};
    DWORD n = GetTempPathA(MAX_PATH, tmp);
    if (n == 0 || n >= MAX_PATH) {
        strcpy(path, "C:\\ph.log");
    } else {
        _snprintf(path, MAX_PATH, "%sph.log", tmp);
    }
    return path;
#endif
}

static void DbgLog(const char* msg) {
#ifdef NS_PACKMAN_SILENT
    (void)msg;
    return;
#else
    if (!IsLogEnabled()) return;
    OutputDebugStringA(msg);
    HANDLE hFile = CreateFileA(GetLogPath(), FILE_APPEND_DATA, FILE_SHARE_READ,
                               nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return;
    DWORD w; WriteFile(hFile, msg, static_cast<DWORD>(strlen(msg)), &w, nullptr);
    CloseHandle(hFile);
#endif
}

// Timestamp prefix: [HH:MM:SS.mmm]
static void GetTimestamp(char* out, size_t outLen) {
    SYSTEMTIME st;
    GetLocalTime(&st);
    _snprintf(out, outLen, "%02d:%02d:%02d.%03d",
              st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
}

static void DbgLogTs(const char* msg) {
#ifdef NS_PACKMAN_SILENT
    (void)msg;
    return;
#else
    if (!IsLogEnabled()) return;
    char ts[32];
    GetTimestamp(ts, sizeof(ts));
    char line[600];
    _snprintf(line, sizeof(line), "[%s] %s", ts, msg);
    line[sizeof(line) - 1] = 0;
    DbgLog(line);
#endif
}

static void DbgLogFmt(const char* fmt, ...) {
#ifdef NS_PACKMAN_SILENT
    (void)fmt;
    return;
#else
    if (!IsLogEnabled()) return;
    char ts[32];
    GetTimestamp(ts, sizeof(ts));
    char body[512];
    va_list args;
    va_start(args, fmt);
    _vsnprintf(body, sizeof(body), fmt, args);
    va_end(args);
    body[sizeof(body) - 1] = 0;
    char line[600];
    _snprintf(line, sizeof(line), "[%s] %s", ts, body);
    line[sizeof(line) - 1] = 0;
    DbgLog(line);
#endif
}

// â”€â”€ Pattern Scanner â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

static void* PatternScan(const unsigned char* base, size_t size,
                         const unsigned char* pattern, size_t patternLen) {
    if (!base || !pattern || patternLen == 0 || size < patternLen) return nullptr;
    for (size_t i = 0; i + patternLen <= size; ++i) {
        bool found = true;
        for (size_t j = 0; j < patternLen; ++j) {
            if (base[i + j] != pattern[j]) { found = false; break; }
        }
        if (found) return const_cast<unsigned char*>(base + i);
    }
    return nullptr;
}

// â”€â”€ DirectSyscall â€” bypass Packman's ntdll hooks â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
// Packman hook nhiá»u Nt* function trong ntdll.dll (xem PackmanHook.txt):
//   NtProtectVirtualMemory (SSN 0x50), NtWriteVirtualMemory (SSN 0x3A),
//   NtContinue (0x43), NtDelayExecution (0x34), NtQueryVirtualMemory (0x23),
//   NtSuspendThread (0x1BE), NtContinueEx (0xA1),
//   NtSetContextThread (0x18D), NtGetContextThread (0xF3).
//
// Packman thay 5-14 byte Ä‘áº§u má»—i Nt* stub báº±ng FF 25 (jmp [rip+0]) â†’ handler
// riÃªng. Äá»ƒ bypass, ta tá»± build syscall stub: mov r10,rcx; mov eax,SSN; syscall; ret.
// SSN trÃ­ch tá»« ntdll trÃªn disk (khÃ´ng bá»‹ hook) hoáº·c fallback cá»©ng.

namespace DirectSyscall {

// â”€â”€ djb2 hash (compile-time) â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
// DÃ¹ng Ä‘á»ƒ trÃ¡nh string "Nt*" plaintext trong .rdata (anti-scan surface B).
constexpr uint32_t djb2(const char* s, uint32_t h = 5381u) {
    return *s ? djb2(s + 1, ((h << 5) + h) ^ static_cast<uint8_t>(*s)) : h;
}
#define NS_HASH(name) (::DirectSyscall::djb2(name))

// â”€â”€ Function typedefs â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
using NtProtectFn  = LONG(NTAPI*)(HANDLE, PVOID*, PSIZE_T, ULONG, PULONG);
using NtWriteFn    = LONG(NTAPI*)(HANDLE, PVOID, PVOID, SIZE_T, PSIZE_T);
using NtContinueFn = LONG(NTAPI*)(PCONTEXT, BOOLEAN);
using NtDelayFn    = LONG(NTAPI*)(PLARGE_INTEGER, BOOLEAN);
using NtQueryVirtFn= LONG(NTAPI*)(HANDLE, PVOID, ULONG, PVOID, SIZE_T, PSIZE_T);
using NtSuspendFn  = LONG(NTAPI*)(HANDLE, PULONG);
using NtContExFn   = LONG(NTAPI*)(PCONTEXT, ULONG);
using NtSetCtxFn   = LONG(NTAPI*)(HANDLE, PCONTEXT);
using NtGetCtxFn   = LONG(NTAPI*)(HANDLE, PCONTEXT);
using NtCreateThreadExFn = LONG(NTAPI*)(PHANDLE, ACCESS_MASK, PVOID, HANDLE,
    LPTHREAD_START_ROUTINE, PVOID, ULONG, SIZE_T, SIZE_T, SIZE_T, PVOID);
using NtSetInfoThreadFn = LONG(NTAPI*)(HANDLE, ULONG, PVOID, ULONG);
using NtQueryInfoThreadFn = LONG(NTAPI*)(HANDLE, ULONG, PVOID, ULONG, PULONG);
using NtCreateSectionFn = LONG(NTAPI*)(PHANDLE, ACCESS_MASK, PVOID*, PLARGE_INTEGER, ULONG, ULONG, HANDLE);
using NtMapViewOfSectionFn = LONG(NTAPI*)(HANDLE, HANDLE, PVOID*, ULONG_PTR, SIZE_T, PLARGE_INTEGER, PSIZE_T, DWORD, ULONG, ULONG);

// â”€â”€ Syscall entry table â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
// Má»—i entry: tÃªn function (cho resolve), SSN fallback (verify tá»« IDA ntdll),
// stub pointer, function pointer, SSN thá»±c táº¿.
struct SyscallEntry {
    uint32_t    nameHash;      // djb2(name) â€” dÃ¹ng Ä‘á»ƒ resolve áº©n danh
    const char* name;          // giá»¯ cho debug/DumpSyscallTable; sáº½ Ä‘Æ°á»£c stringify tÃ¹y build
    int         fallbackSsn;
    void*       stub;
    void*       fn;
    int         ssn;
};

// SSN fallback values verified tá»« IDA 13339 (ntdll.dll Win11 26100)
inline SyscallEntry g_syscalls[] = {
    { NS_HASH("NtProtectVirtualMemory"), "NtProtectVirtualMemory", 0x050, nullptr, nullptr, -1 },
    { NS_HASH("NtWriteVirtualMemory"),   "NtWriteVirtualMemory",   0x03A, nullptr, nullptr, -1 },
    { NS_HASH("NtContinue"),             "NtContinue",             0x043, nullptr, nullptr, -1 },
    { NS_HASH("NtDelayExecution"),       "NtDelayExecution",       0x034, nullptr, nullptr, -1 },
    { NS_HASH("NtQueryVirtualMemory"),   "NtQueryVirtualMemory",   0x023, nullptr, nullptr, -1 },
    { NS_HASH("NtSuspendThread"),        "NtSuspendThread",        0x1BE, nullptr, nullptr, -1 },
    { NS_HASH("NtContinueEx"),           "NtContinueEx",           0x0A1, nullptr, nullptr, -1 },
    { NS_HASH("NtSetContextThread"),     "NtSetContextThread",     0x18D, nullptr, nullptr, -1 },
    { NS_HASH("NtGetContextThread"),     "NtGetContextThread",     0x0F3, nullptr, nullptr, -1 },
    { NS_HASH("NtCreateThreadEx"),       "NtCreateThreadEx",       0x0C2, nullptr, nullptr, -1 },
    { NS_HASH("NtSetInformationThread"), "NtSetInformationThread", 0x00D, nullptr, nullptr, -1 },
    { NS_HASH("NtQueryInformationThread"),"NtQueryInformationThread",0x025, nullptr, nullptr, -1 },
    { NS_HASH("NtCreateSection"),       "NtCreateSection",       0x055, nullptr, nullptr, -1 },
    { NS_HASH("NtMapViewOfSection"),    "NtMapViewOfSection",    0x028, nullptr, nullptr, -1 },
};

enum SyscallIdx : size_t {
    IDX_PROTECT       = 0,
    IDX_WRITE         = 1,
    IDX_CONTINUE      = 2,
    IDX_DELAY         = 3,
    IDX_QUERYVIRT     = 4,
    IDX_SUSPEND       = 5,
    IDX_CONTINUEEX    = 6,
    IDX_SETCTX        = 7,
    IDX_GETCTX        = 8,
    IDX_CREATETHREADEX= 9,
    IDX_SETINFOTHREAD = 10,
    IDX_QUERYINFOTHREAD= 11,
    IDX_CREATESECTION  = 12,
    IDX_MAPVIEWSECTION = 13,
    SYSCALL_COUNT     = 14,
};

// Compile-time hash-collision guard cho g_syscalls[]. Náº¿u 2 entry cÃ¹ng hash,
// static_assert nÃ y sáº½ fail â€” Ä‘á»•i tÃªn hoáº·c dÃ¹ng hash algo khÃ¡c.
static_assert(NS_HASH("NtProtectVirtualMemory") != NS_HASH("NtWriteVirtualMemory"), "hash collision");
static_assert(NS_HASH("NtProtectVirtualMemory") != NS_HASH("NtContinue"),           "hash collision");
static_assert(NS_HASH("NtWriteVirtualMemory")   != NS_HASH("NtContinue"),           "hash collision");
static_assert(NS_HASH("NtDelayExecution")       != NS_HASH("NtQueryVirtualMemory"), "hash collision");
static_assert(NS_HASH("NtSuspendThread")        != NS_HASH("NtContinueEx"),         "hash collision");
static_assert(NS_HASH("NtSetContextThread")     != NS_HASH("NtGetContextThread"),   "hash collision");
static_assert(NS_HASH("NtCreateThreadEx")       != NS_HASH("NtSuspendThread"),      "hash collision");
static_assert(NS_HASH("NtSetInformationThread") != NS_HASH("NtCreateThreadEx"),     "hash collision");
static_assert(NS_HASH("NtSetInformationThread") != NS_HASH("NtSetContextThread"),   "hash collision");
static_assert(NS_HASH("NtQueryInformationThread")!= NS_HASH("NtSetInformationThread"),"hash collision");
static_assert(NS_HASH("NtQueryInformationThread")!= NS_HASH("NtCreateThreadEx"),     "hash collision");
static_assert(NS_HASH("NtCreateSection")       != NS_HASH("NtMapViewOfSection"),    "hash collision");
static_assert(NS_HASH("NtCreateSection")       != NS_HASH("NtQueryInformationThread"),"hash collision");
static_assert(NS_HASH("NtMapViewOfSection")    != NS_HASH("NtQueryInformationThread"),"hash collision");

// â”€â”€ Backward compat globals (giá»¯ API cÅ© cho CRCBypass) â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
inline void*         g_protectStub = nullptr;
inline NtProtectFn   g_ntProtect   = nullptr;
inline int           g_protectSsn  = -1;
inline volatile LONG g_lastProtectStatus = 0;

inline void*       g_writeStub = nullptr;
inline NtWriteFn   g_ntWrite   = nullptr;
inline int         g_writeSsn  = -1;
inline volatile LONG g_lastWriteStatus = 0;

// â”€â”€ Syscall gadget cache (Module A) â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
// Danh sÃ¡ch Ä‘á»‹a chá»‰ 0F 05 C3 trong ntdll .text. Cache 1 láº§n lÃºc InitAll,
// pick ngáº«u nhiÃªn cho má»—i stub build. Cá»¡ 64 Ä‘á»§ (ntdll cÃ³ 30+ máº·c Ä‘á»‹nh).
inline void*    g_syscallGadgets[64] = {};
inline size_t   g_gadgetCount        = 0;
inline volatile LONG g_gadgetInited  = 0;

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

inline int ExtractSSNFromImageByHash(const uint8_t* buf, uint32_t nameHash) {
    if (!buf) return -1;
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
        if (djb2(name) != nameHash) continue;
        const auto* stub = buf + RvaToFileOff(nt, funcs[ords[i]]);
        if (stub[0] == 0x4C && stub[1] == 0x8B && stub[2] == 0xD1 && stub[3] == 0xB8) {
            return static_cast<int>(*reinterpret_cast<const uint32_t*>(stub + 4));
        }
        return -1;
    }
    return -1;
}

inline int ExtractSSNFromImage(const uint8_t* buf, const char* funcName) {
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
        if (strcmp(name, funcName) != 0) continue;
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

inline int ResolveSSNInMemoryByHash(uint32_t nameHash) {
    HMODULE h = GetModuleHandleW(L"ntdll.dll");
    if (!h) return -1;
    const auto* buf = reinterpret_cast<const uint8_t*>(h);
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(buf);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return -1;
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(buf + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return -1;
    const auto& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    if (!dir.VirtualAddress) return -1;
    const auto* exp   = reinterpret_cast<const IMAGE_EXPORT_DIRECTORY*>(buf + dir.VirtualAddress);
    const auto* names = reinterpret_cast<const DWORD*>(buf + exp->AddressOfNames);
    const auto* ords  = reinterpret_cast<const WORD*>(buf + exp->AddressOfNameOrdinals);
    const auto* funcs = reinterpret_cast<const DWORD*>(buf + exp->AddressOfFunctions);
    for (DWORD i = 0; i < exp->NumberOfNames; ++i) {
        const char* name = reinterpret_cast<const char*>(buf + names[i]);
        if (djb2(name) != nameHash) continue;
        const uint8_t* p = buf + funcs[ords[i]];
        if (p[0] == 0x4C && p[1] == 0x8B && p[2] == 0xD1 && p[3] == 0xB8) {
            return static_cast<int>(*reinterpret_cast<const uint32_t*>(p + 4));
        }
        return -1;
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

    auto* buf = reinterpret_cast<uint8_t*>(VirtualAlloc(nullptr,
        static_cast<SIZE_T>(sz.QuadPart),
        MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    if (!buf) { CloseHandle(hFile); return -1; }

    DWORD total = 0;
    while (total < static_cast<DWORD>(sz.QuadPart)) {
        DWORD chunk = 0;
        if (!ReadFile(hFile, buf + total,
                      static_cast<DWORD>(sz.QuadPart) - total, &chunk, nullptr) ||
            chunk == 0) break;
        total += chunk;
    }
    CloseHandle(hFile);

    const int ssn = (total == static_cast<DWORD>(sz.QuadPart))
        ? ExtractSSNFromImage(buf, funcName) : -1;
    VirtualFree(buf, 0, MEM_RELEASE);
    return ssn;
}

inline int ResolveSSNFromDiskByHash(uint32_t nameHash) {
    HANDLE hFile = CreateFileW(L"\\\\?\\C:\\Windows\\System32\\ntdll.dll",
        GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return -1;
    LARGE_INTEGER sz{};
    if (!GetFileSizeEx(hFile, &sz) || sz.QuadPart <= 0 || sz.QuadPart > (64LL << 20)) {
        CloseHandle(hFile); return -1;
    }
    auto* buf = reinterpret_cast<uint8_t*>(VirtualAlloc(nullptr,
        static_cast<SIZE_T>(sz.QuadPart), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    if (!buf) { CloseHandle(hFile); return -1; }
    DWORD total = 0;
    while (total < static_cast<DWORD>(sz.QuadPart)) {
        DWORD chunk = 0;
        if (!ReadFile(hFile, buf + total,
                      static_cast<DWORD>(sz.QuadPart) - total, &chunk, nullptr) ||
            chunk == 0) break;
        total += chunk;
    }
    CloseHandle(hFile);
    const int ssn = (total == static_cast<DWORD>(sz.QuadPart))
        ? ExtractSSNFromImageByHash(buf, nameHash) : -1;
    VirtualFree(buf, 0, MEM_RELEASE);
    return ssn;
}

// QuÃ©t .text ntdll tÃ¬m má»i `0F 05 C3` (syscall;ret) â†’ cache.
// Chá»‰ quÃ©t trong hÃ m UNHOOKED (byte Ä‘áº§u = 0x4C 8B D1) Ä‘á»ƒ trÃ¡nh Packman patch
// tail lÃ¢u vá» sau. Fallback: quÃ©t toÃ n .text náº¿u khÃ´ng Ä‘á»§ candidate.
inline void EnumerateSyscallGadgets() {
    if (InterlockedCompareExchange(&g_gadgetInited, 1, 0) != 0) return;

    HMODULE h = GetModuleHandleW(L"ntdll.dll");
    if (!h) return;
    const auto* buf = reinterpret_cast<const uint8_t*>(h);
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(buf);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return;
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(buf + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return;

    // TÃ¬m .text section
    const IMAGE_SECTION_HEADER* sec = IMAGE_FIRST_SECTION(nt);
    const uint8_t* textStart = nullptr; size_t textSize = 0;
    for (int i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++sec) {
        if (memcmp(sec->Name, ".text", 5) == 0) {
            textStart = buf + sec->VirtualAddress;
            textSize  = sec->Misc.VirtualSize;
            break;
        }
    }
    if (!textStart || textSize == 0) return;

    // Pass 1: chá»‰ giá»¯ gadget náº±m sau head `4C 8B D1 B8 xx 00 00 00 F6 04 25`
    // (tail cá»§a Nt* stub UNHOOKED) â€” offset gadget = start + 0x12.
    for (size_t i = 0; i + 3 < textSize && g_gadgetCount < 64; ++i) {
        if (textStart[i]   == 0x4C && textStart[i+1] == 0x8B &&
            textStart[i+2] == 0xD1 && textStart[i+3] == 0xB8) {
            // stub start ok. gadget candidate = i + 0x12
            size_t g = i + 0x12;
            if (g + 3 <= textSize &&
                textStart[g]   == 0x0F &&
                textStart[g+1] == 0x05 &&
                textStart[g+2] == 0xC3) {
                g_syscallGadgets[g_gadgetCount++] =
                    const_cast<void*>(reinterpret_cast<const void*>(textStart + g));
            }
            i += 0x14; // skip past this stub
        }
    }
    DbgLogFmt("[SYS] EnumerateSyscallGadgets: found %zu candidates\r\n", g_gadgetCount);
}

inline void* PickSyscallGadget() {
    if (g_gadgetCount == 0) return nullptr;
    // random dá»±a __rdtsc (dispersion Ä‘á»§ dÃ¹ng)
    unsigned long long r = __rdtsc();
    return g_syscallGadgets[(size_t)(r % g_gadgetCount)];
}

// â”€â”€ AllocSectionRWX (Module MM + Module 2 upgrade) â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
// V2: NtCreateSection + NtMapViewOfSection qua direct syscall (bypass Packman hook).
// Region hiá»‡n MEM_MAPPED (0x40000) thay vÃ¬ MEM_PRIVATE (0x20000).
// Anti-cheat scan MEM_PRIVATE+PAGE_EXECUTE sáº½ miss.
inline void* AllocSectionRWX(SIZE_T size) {
    if (size == 0) return nullptr;

    // V2 path: direct syscall NtCreateSection + NtMapViewOfSection
    // Check náº¿u syscalls Ä‘Ã£ Ä‘Æ°á»£c init (fn != nullptr)
    if (g_syscalls[IDX_CREATESECTION].fn && g_syscalls[IDX_MAPVIEWSECTION].fn) {
        HANDLE hSection = nullptr;
        LARGE_INTEGER sectionSize;
        sectionSize.QuadPart = static_cast<LONGLONG>(size);

        auto createFn = reinterpret_cast<NtCreateSectionFn>(g_syscalls[IDX_CREATESECTION].fn);
        LONG s1 = createFn(
            &hSection,                 // SectionHandle
            SECTION_ALL_ACCESS,        // DesiredAccess
            nullptr,                   // ObjectAttributes
            &sectionSize,              // MaximumSize
            PAGE_EXECUTE_READWRITE,    // SectionPageProtection
            SEC_COMMIT,                // AllocationAttributes (pagefile-backed)
            nullptr);                  // FileHandle (anonymous)
        if (s1 < 0 || !hSection) {
            DbgLogFmt("[ALLOC] NtCreateSection FAIL status=0x%X\r\n", (unsigned)s1);
        } else {
            PVOID baseAddr = nullptr;
            SIZE_T viewSize = 0;
            auto mapFn = reinterpret_cast<NtMapViewOfSectionFn>(g_syscalls[IDX_MAPVIEWSECTION].fn);
            LONG s2 = mapFn(
                hSection,              // SectionHandle
                GetCurrentProcess(),   // ProcessHandle
                &baseAddr,             // BaseAddress
                0,                     // ZeroBits
                size,                  // CommitSize
                nullptr,               // SectionOffset
                &viewSize,             // ViewSize
                1,                     // InheritDisposition (ViewShare)
                0,                     // AllocationType
                PAGE_EXECUTE_READWRITE // Win32Protect
            );
            CloseHandle(hSection);     // view giá»¯ section alive qua kernel refcount

            if (s2 < 0 || !baseAddr) {
                DbgLogFmt("[ALLOC] NtMapViewOfSection FAIL status=0x%X\r\n", (unsigned)s2);
            } else {
                return baseAddr;
            }
        }
    }

    // Fallback: Win32 CreateFileMappingA + MapViewOfFile (old V1 path)
    DbgLogFmt("[ALLOC] V2 path failed, falling back to V1 (CreateFileMapping)\r\n");
    HANDLE hMap = CreateFileMappingA(
        INVALID_HANDLE_VALUE, nullptr,
        PAGE_EXECUTE_READWRITE, 0, static_cast<DWORD>(size), nullptr);
    if (!hMap) return nullptr;
    void* view = MapViewOfFile(hMap,
        FILE_MAP_EXECUTE | FILE_MAP_WRITE | FILE_MAP_READ,
        0, 0, size);
    CloseHandle(hMap);
    return view;
}

inline bool BuildSyscallStub(int ssn, void** outStub) {
    if (!outStub || ssn < 0) return false;

    void* gadget = PickSyscallGadget();
    // Fallback náº¿u chÆ°a enumerate (vd. InitSyscall gá»i trÆ°á»›c InitAll):
    // emit stub cÅ© (syscall inline) Ä‘á»ƒ giá»¯ hoáº¡t Ä‘á»™ng, cháº¥p nháº­n regression stealth.
    if (!gadget) {
        auto* stub = reinterpret_cast<uint8_t*>(AllocSectionRWX(32));
        if (!stub) return false;
        stub[0] = 0x4C; stub[1] = 0x8B; stub[2] = 0xD1;
        stub[3] = 0xB8;
        *reinterpret_cast<uint32_t*>(stub + 4) = static_cast<uint32_t>(ssn);
        stub[8] = 0x0F; stub[9] = 0x05;
        stub[10] = 0xC3;
        *outStub = stub;
        DbgLogFmt("[SYS] BuildSyscallStub: FALLBACK inline syscall (gadgetCount=0)\r\n");
        return true;
    }

    // Camouflaged indirect stub â€” 30 bytes (Module I).
    // 16 byte Ä‘áº§u KHá»šP HOÃ€N TOÃ€N vá»›i ntdll syscall stub head:
    //   4C 8B D1                     mov r10, rcx                    ; 3
    //   B8 SSN 00 00 00              mov eax, SSN                    ; 5
    //   F6 04 25 08 03 FE 7F 01      test byte[0x7FFE0308], 1        ; 8  â† match ntdll
    //   FF 25 00 00 00 00            jmp qword ptr [rip+0]           ; 6
    //   <8 bytes gadget addr>                                        ; 8
    // â†’ memcmp(stub, ntdll_stub_head, 16) = 0 (giá»‘ng nhau bit-perfect).
    // `test` executed nhÆ°ng result IGNORED â€” chá»‰ Ä‘á»ƒ camouflage. Overhead <1 ns.
    // Module MM: alloc qua CreateFileMapping â†’ region MEM_MAPPED.
    auto* stub = reinterpret_cast<uint8_t*>(AllocSectionRWX(32));
    if (!stub) return false;

    // mov r10, rcx
    stub[0] = 0x4C; stub[1] = 0x8B; stub[2] = 0xD1;
    // mov eax, SSN
    stub[3] = 0xB8;
    *reinterpret_cast<uint32_t*>(stub + 4) = static_cast<uint32_t>(ssn);
    // test byte ptr [0x7FFE0308], 1  (KUSER_SHARED_DATA.SystemCallStub check)
    stub[8]  = 0xF6; stub[9]  = 0x04; stub[10] = 0x25;
    stub[11] = 0x08; stub[12] = 0x03; stub[13] = 0xFE; stub[14] = 0x7F;
    stub[15] = 0x01;
    // jmp qword ptr [rip+0]  (indirect to gadget)
    stub[16] = 0xFF; stub[17] = 0x25;
    stub[18] = 0x00; stub[19] = 0x00; stub[20] = 0x00; stub[21] = 0x00;
    // gadget address qword
    *reinterpret_cast<uint64_t*>(stub + 22) = reinterpret_cast<uint64_t>(gadget);

    // Downgrade tá»« RWX â†’ RX (giáº£m 1 anti-scan surface: RWX private region).
    DWORD oldProt = 0;
    VirtualProtect(stub, 32, PAGE_EXECUTE_READ, &oldProt);
    FlushInstructionCache(GetCurrentProcess(), stub, 32);

    *outStub = stub;
    return true;
}

// â”€â”€ Generic syscall init (dÃ¹ng table) â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
inline bool InitSyscall(size_t idx) {
    if (idx >= SYSCALL_COUNT) return false;
    auto& e = g_syscalls[idx];
    if (e.fn) return true;

    int ssn = ResolveSSNInMemoryByHash(e.nameHash);
    const char* src = "memory-hash";
    if (ssn < 0) {
        ssn = ResolveSSNFromDiskByHash(e.nameHash);
        src = "disk-hash";
    }
    if (ssn < 0) {
        ssn = e.fallbackSsn;
        src = "fallback";
    }
    DbgLogFmt("[SYS] Init %-28s: SSN=0x%X (src=%s)\r\n", e.name, ssn, src);

    if (!BuildSyscallStub(ssn, &e.stub)) {
        DbgLogFmt("[SYS] BuildSyscallStub FAIL for %s\r\n", e.name);
        return false;
    }
    e.ssn = ssn;
    e.fn  = e.stub;
    DbgLogFmt("[SYS]   stub @ %p  fn @ %p  OK\r\n", e.stub, e.fn);
    return true;
}

// â”€â”€ InitAll: khá»Ÿi táº¡o táº¥t cáº£ syscall stub cÃ¹ng lÃºc â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
// Init NtCreateSection + NtMapViewOfSection FIRST Ä‘á»ƒ AllocSectionRWX V2 path
// hoáº¡t Ä‘á»™ng cho 12 stubs cÃ²n láº¡i (chicken-and-egg fix).
inline bool InitAll() {
    DbgLogFmt("[SYS] InitAll: initializing %zu syscall stubs...\r\n", (size_t)SYSCALL_COUNT);
    EnumerateSyscallGadgets();
    bool ok = true;

    // Phase 1: Init section syscalls first (their own stubs use V1 fallback)
    if (!InitSyscall(IDX_CREATESECTION))  ok = false;
    if (!InitSyscall(IDX_MAPVIEWSECTION)) ok = false;
    DbgLogFmt("[SYS] InitAll: section syscalls ready, V2 path active\r\n");

    // Phase 2: Init remaining stubs (AllocSectionRWX now uses V2 â†’ MEM_MAPPED)
    for (size_t i = 0; i < SYSCALL_COUNT; ++i) {
        if (i == IDX_CREATESECTION || i == IDX_MAPVIEWSECTION) continue;
        if (!InitSyscall(i)) ok = false;
    }

    DbgLogFmt("[SYS] InitAll: %s (%zu/%zu OK)\r\n",
              ok ? "ALL OK" : "PARTIAL FAIL",
              ok ? (size_t)SYSCALL_COUNT : (size_t)0, (size_t)SYSCALL_COUNT);
    return ok;
}

inline bool InitProtect() {
    if (g_ntProtect) return true;
    if (!InitSyscall(IDX_PROTECT)) return false;
    g_protectStub = g_syscalls[IDX_PROTECT].stub;
    g_protectSsn  = g_syscalls[IDX_PROTECT].ssn;
    g_ntProtect   = reinterpret_cast<NtProtectFn>(g_protectStub);
    return true;
}

inline bool InitWrite() {
    if (g_ntWrite) return true;
    if (!InitSyscall(IDX_WRITE)) {
        return false;
    }
    g_writeStub = g_syscalls[IDX_WRITE].stub;
    g_writeSsn  = g_syscalls[IDX_WRITE].ssn;
    g_ntWrite   = reinterpret_cast<NtWriteFn>(g_writeStub);
    return true;
}

inline BOOL VirtualProtectDirect(void* addr, SIZE_T size, DWORD newProt, DWORD* oldProt) {
    if (!addr || size == 0) return FALSE;
    if (!InitProtect()) return VirtualProtect(addr, size, newProt, oldProt);
    PVOID  base   = addr;
    SIZE_T region = size;
    ULONG  oldU   = 0;
    const LONG s = g_ntProtect(GetCurrentProcess(), &base, &region, newProt, &oldU);
    g_lastProtectStatus = s;
    if (oldProt) *oldProt = static_cast<DWORD>(oldU);
    return s >= 0;
}

inline BOOL WriteVirtualMemoryDirect(void* dst, const void* src, SIZE_T size) {
    if (!dst || !src || size == 0) return FALSE;
    if (!InitWrite()) return FALSE;
    SIZE_T written = 0;
    const LONG s = g_ntWrite(GetCurrentProcess(), dst,
                             const_cast<void*>(src), size, &written);
    g_lastWriteStatus = s;
    return s >= 0 && written == size;
}

inline int SelfTest(uintptr_t anyCodePageAddr) {
    if (!anyCodePageAddr || !InitProtect()) return 3;
    void* page = reinterpret_cast<void*>(anyCodePageAddr & ~static_cast<uintptr_t>(0xFFF));
    DWORD oldProt = 0;
    if (VirtualProtectDirect(page, 0x1000, PAGE_EXECUTE_WRITECOPY, &oldProt)) {
        DWORD d = 0; VirtualProtectDirect(page, 0x1000, oldProt, &d);
        return 1;
    }
    if (VirtualProtectDirect(page, 0x1000, PAGE_WRITECOPY, &oldProt)) {
        DWORD d = 0; VirtualProtectDirect(page, 0x1000, oldProt, &d);
        return 2;
    }
    if (VirtualProtectDirect(page, 0x1000, PAGE_EXECUTE_READWRITE, &oldProt)) {
        DWORD d = 0; VirtualProtectDirect(page, 0x1000, oldProt, &d);
        return 4;
    }
    return 3;
}

// â”€â”€ Wrapper functions cho cÃ¡c syscall bá»‹ Packman hook â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
// Má»—i wrapper: resolve SSN â†’ build stub â†’ gá»i trá»±c tiáº¿p kernel, bypass hook.

// NtContinue (SSN 0x43) â€” Packman hook FF 25, dÃ¹ng trong exception dispatch
inline LONG NtContinueDirect(PCONTEXT ctx, BOOLEAN raiseAlert) {
    if (!InitSyscall(IDX_CONTINUE)) return -1;
    auto fn = reinterpret_cast<NtContinueFn>(g_syscalls[IDX_CONTINUE].fn);
    return fn(ctx, raiseAlert);
}

// NtDelayExecution (SSN 0x34) â€” Packman hook, thay Sleep khi cáº§n stealth
inline LONG NtDelayExecutionDirect(PLARGE_INTEGER delay, BOOLEAN alertable) {
    if (!InitSyscall(IDX_DELAY)) return -1;
    auto fn = reinterpret_cast<NtDelayFn>(g_syscalls[IDX_DELAY].fn);
    return fn(delay, alertable);
}

// NtQueryVirtualMemory (SSN 0x23) â€” Packman hook, query memory info trá»±c tiáº¿p
inline LONG NtQueryVirtualMemoryDirect(HANDLE proc, PVOID base, ULONG cls,
                                       PVOID buf, SIZE_T len, PSIZE_T ret) {
    if (!InitSyscall(IDX_QUERYVIRT)) return -1;
    auto fn = reinterpret_cast<NtQueryVirtFn>(g_syscalls[IDX_QUERYVIRT].fn);
    return fn(proc, base, cls, buf, len, ret);
}

// NtSuspendThread (SSN 0x1BE) â€” Packman hook, suspend thread bypass
inline LONG NtSuspendThreadDirect(HANDLE thread, PULONG suspendCount) {
    if (!InitSyscall(IDX_SUSPEND)) return -1;
    auto fn = reinterpret_cast<NtSuspendFn>(g_syscalls[IDX_SUSPEND].fn);
    return fn(thread, suspendCount);
}

// NtContinueEx (SSN 0xA1) â€” Packman hook, extended continue
inline LONG NtContinueExDirect(PCONTEXT ctx, ULONG flags) {
    if (!InitSyscall(IDX_CONTINUEEX)) return -1;
    auto fn = reinterpret_cast<NtContExFn>(g_syscalls[IDX_CONTINUEEX].fn);
    return fn(ctx, flags);
}

// NtSetContextThread (SSN 0x18D) â€” Packman hook, set thread context bypass
inline LONG NtSetContextThreadDirect(HANDLE thread, PCONTEXT ctx) {
    if (!InitSyscall(IDX_SETCTX)) return -1;
    auto fn = reinterpret_cast<NtSetCtxFn>(g_syscalls[IDX_SETCTX].fn);
    return fn(thread, ctx);
}

// NtGetContextThread (SSN 0xF3) â€” Packman hook, get thread context bypass
inline LONG NtGetContextThreadDirect(HANDLE thread, PCONTEXT ctx) {
    if (!InitSyscall(IDX_GETCTX)) return -1;
    auto fn = reinterpret_cast<NtGetCtxFn>(g_syscalls[IDX_GETCTX].fn);
    return fn(thread, ctx);
}

// NtSetInformationThread (SSN 0x0D) â€” Module F ThreadHideFromDebugger.
// ThreadInfoClass = 0x11 (ThreadHideFromDebugger), buffer/length = null/0.
inline LONG NtSetInformationThreadDirect(HANDLE thread, ULONG infoClass,
                                         PVOID info, ULONG infoLen) {
    if (!InitSyscall(IDX_SETINFOTHREAD)) return -1;
    auto fn = reinterpret_cast<NtSetInfoThreadFn>(g_syscalls[IDX_SETINFOTHREAD].fn);
    return fn(thread, infoClass, info, infoLen);
}

// NtQueryInformationThread (SSN 0x0E) â€” Packman hook (runtime: 90 FF 25...).
// Query thread info trá»±c tiáº¿p kernel, bypass hook.
// ThreadInfoClass quan trá»ng:
//   0x00 = ThreadBasicInformation (TEB, PID, TID, start address)
//   0x11 = ThreadHideFromDebugger (check if thread Ä‘Ã£ hide)
//   0x1E = ThreadStartAddress (start routine address)
inline LONG NtQueryInformationThreadDirect(HANDLE thread, ULONG infoClass,
                                           PVOID info, ULONG infoLen,
                                           PULONG retLen) {
    if (!InitSyscall(IDX_QUERYINFOTHREAD)) return -1;
    auto fn = reinterpret_cast<NtQueryInfoThreadFn>(g_syscalls[IDX_QUERYINFOTHREAD].fn);
    return fn(thread, infoClass, info, infoLen, retLen);
}

// â”€â”€ Stealth Sleep (dÃ¹ng NtDelayExecution direct, bypass Packman hook) â”€â”€â”€â”€â”€â”€â”€â”€
inline void StealthSleep(DWORD ms) {
    LARGE_INTEGER delay;
    delay.QuadPart = -static_cast<int64_t>(ms) * 10000LL; // 100ns units
    NtDelayExecutionDirect(&delay, FALSE);
}

// â”€â”€ Dump SSN table (debug) â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
inline void DumpSyscallTable() {
    DbgLogFmt("[SYS] === Syscall Table Dump ===\r\n");
    for (size_t i = 0; i < SYSCALL_COUNT; ++i) {
        const auto& e = g_syscalls[i];
        DbgLogFmt("[SYS]   [%zu] %-28s SSN=0x%-3X  stub=%p  fn=%p  %s\r\n",
                  i, e.name, (unsigned)e.ssn, e.stub, e.fn,
                  e.fn ? "OK" : "NOT_INIT");
    }
    DbgLogFmt("[SYS] === End Dump ===\r\n");
}

// â”€â”€ CreateThreadDirect â€” spawn thread bypass Packman's INT3 on NtCreateThreadEx â”€
// Packman patch byte Ä‘áº§u NtCreateThreadEx thÃ nh CC (INT3) â†’ má»—i CreateThread
// Ä‘i qua ntdll Ä‘á»u bá»‹ trap. Wrapper nÃ y gá»i NtCreateThreadEx trá»±c tiáº¿p qua
// syscall stub, bá» qua entry ntdll. Náº¿u fail, fallback CreateThread thÆ°á»ng.
inline HANDLE CreateThreadDirect(LPTHREAD_START_ROUTINE routine, PVOID param) {
    const auto trampoline = CoreBypass::ResolveThreadBeginTrampoline();
    const bool hasTrampoline = Globals::IsValidPtr(trampoline);

    if (!InitSyscall(IDX_CREATETHREADEX)) {
        // Fallback: CreateThread (vá»›i trampoline náº¿u cÃ³)
        HANDLE h = hasTrampoline
            ? CoreBypass::CreateThreadSpoofed(routine, param)
            : CreateThread(nullptr, 0, routine, param, 0, nullptr);
        if (h) {
            NtSetInformationThreadDirect(h, 0x11 /*ThreadHideFromDebugger*/, nullptr, 0);
            DbgLog("[SYS] CreateThreadDirect: fallback CreateThread OK\r\n");
        }
        return h;
    }
    auto fn = reinterpret_cast<NtCreateThreadExFn>(g_syscalls[IDX_CREATETHREADEX].fn);
    HANDLE hThread = nullptr;

    LPTHREAD_START_ROUTINE startRoutine = routine;
    PVOID startArg = param;

    if (hasTrampoline) {
        CoreBypass::g_pendingThreadBegin = { routine, param };
        startRoutine = reinterpret_cast<LPTHREAD_START_ROUTINE>(trampoline);
        startArg = reinterpret_cast<PVOID>(CoreBypass::ThreadBeginWrapper);
    }

    LONG status = fn(
        &hThread,
        THREAD_ALL_ACCESS,
        nullptr,               // ObjectAttributes
        GetCurrentProcess(),
        startRoutine,
        startArg,
        0,                     // CreateFlags (CREATE_SUSPENDED=0 â†’ cháº¡y ngay)
        0,                     // ZeroBits
        0,                     // StackSize (default)
        0,                     // MaximumStackSize
        nullptr                // AttributeList
    );
    if (status >= 0 && hThread) {
        // Module F: áº©n thread khá»i debugger enumeration.
        NtSetInformationThreadDirect(hThread, 0x11 /*ThreadHideFromDebugger*/, nullptr, 0);
        DbgLogFmt("[SYS] CreateThreadDirect: NtCreateThreadEx OK (handle=%p, status=0x%X) %s\r\n",
                  hThread, (unsigned)status, hasTrampoline ? "spoofed" : "direct");
        return hThread;
    }
    if (hasTrampoline) {
        CoreBypass::g_pendingThreadBegin = {};
    }
    DbgLogFmt("[SYS] CreateThreadDirect: NtCreateThreadEx FAIL status=0x%X â€” fallback CreateThread\r\n",
              (unsigned)status);
    HANDLE hRetry = hasTrampoline
        ? CoreBypass::CreateThreadSpoofed(routine, param)
        : CreateThread(nullptr, 0, routine, param, 0, nullptr);
    if (hRetry) NtSetInformationThreadDirect(hRetry, 0x11 /*ThreadHideFromDebugger*/, nullptr, 0);
    return hRetry;
}

} // namespace DirectSyscall

// â”€â”€ Hardware Breakpoint Detection â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
// Packman cÃ³ thá»ƒ set HW breakpoint (DR0-DR3) trÃªn code/data cá»§a NightSharp
// Ä‘á»ƒ monitor hoáº¡t Ä‘á»™ng. Module nÃ y:
//   1) Äá»c DR0-DR7 qua NtGetContextThreadDirect (bypass Packman NtGetContextThread hook)
//   2) Náº¿u DR0-DR3 != 0 vÃ  DR7 cÃ³ enable bit â†’ cÃ³ HW BP â†’ log + clear
//   3) Clear báº±ng NtSetContextThreadDirect (bypass Packman NtSetContextThread hook)
//
// CONTEXT_DEBUG_REGISTERS: chá»‰ Ä‘á»c/ghi DR0-DR7, khÃ´ng Ä‘á»¥ng GP regs â†’ an toÃ n.
// Chá»‰ check thread hiá»‡n táº¡i (DLL load trÃªn thread chÃ­nh cá»§a game).

namespace HwBpDetect {

inline volatile LONG g_checked = 0;
inline volatile LONG g_detected = 0;

// Bit layout DR7 (x64):
//   Bits 0-1:   L0/G0 (local/global enable DR0)
//   Bits 2-3:   L1/G1 (DR1)
//   Bits 4-5:   L2/G2 (DR2)
//   Bits 6-7:   L3/G3 (DR3)
//   Bit 8:      LE (local exact)
//   Bit 9:      GE (global exact)
//   Bit 10:     reserved
//   Bits 16-31: conditions per DR (R/W + length)
// Náº¿u báº¥t ká»³ L0/G0/L1/G1/L2/G2/L3/G3 = 1 â†’ cÃ³ HW BP active.

inline bool CheckAndClear() {
    if (InterlockedCompareExchange(&g_checked, 1, 0) != 0) return false;

    CONTEXT ctx = {};
    ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;

    // Äá»c context thread hiá»‡n táº¡i qua direct syscall (bypass Packman hook)
    LONG status = DirectSyscall::NtGetContextThreadDirect(GetCurrentThread(), &ctx);
    if (status < 0) {
        DbgLogFmt("[HWBP] NtGetContextThreadDirect FAIL status=0x%X\r\n", (unsigned)status);
        // Fallback: GetThreadContext user-mode (cÃ³ thá»ƒ bá»‹ Packman intercept)
        if (!GetThreadContext(GetCurrentThread(), &ctx)) {
            DbgLogFmt("[HWBP] GetThreadContext fallback FAIL gle=%lu\r\n", GetLastError());
            return false;
        }
        DbgLogFmt("[HWBP] GetThreadContext fallback OK (may be intercepted)\r\n");
    }

    // Check DR0-DR3 + DR7 enable bits
    const uintptr_t dr[] = { ctx.Dr0, ctx.Dr1, ctx.Dr2, ctx.Dr3 };
    const DWORD dr7 = static_cast<DWORD>(ctx.Dr7);
    // Enable mask: L0|G0|L1|G1|L2|G2|L3|G3 = bits 0-7
    const DWORD enableMask = 0xFF;

    bool detected = false;
    int activeBp = 0;
    for (int i = 0; i < 4; ++i) {
        // DRi active náº¿u DRi != 0 VÃ€ enable bit tÆ°Æ¡ng á»©ng trong DR7 set
        bool enL = (dr7 >> (i * 2))     & 1;  // Li
        bool enG = (dr7 >> (i * 2 + 1)) & 1;  // Gi
        if (dr[i] != 0 && (enL || enG)) {
            detected = true;
            ++activeBp;
            DbgLogFmt("[HWBP] DR%d = 0x%llX ACTIVE (L=%d G=%d)\r\n",
                      i, (unsigned long long)dr[i], (int)enL, (int)enG);
        }
    }
    DbgLogFmt("[HWBP] DR7=0x%X  active=%d  detected=%d\r\n",
              (unsigned)dr7, activeBp, (int)detected);

    if (!detected) {
        DbgLogFmt("[HWBP] No hardware breakpoints detected\r\n");
        return false;
    }

    InterlockedExchange(&g_detected, 1);

    // Clear DR0-DR3 + DR7 Ä‘á»ƒ neutralize HW BP
    ctx.Dr0 = 0;
    ctx.Dr1 = 0;
    ctx.Dr2 = 0;
    ctx.Dr3 = 0;
    ctx.Dr7 = 0;
    // Dr6 lÃ  status register, clear luÃ´n
    ctx.Dr6 = 0;

    status = DirectSyscall::NtSetContextThreadDirect(GetCurrentThread(), &ctx);
    if (status < 0) {
        DbgLogFmt("[HWBP] NtSetContextThreadDirect FAIL status=0x%X â€” trying fallback\r\n",
                  (unsigned)status);
        if (!SetThreadContext(GetCurrentThread(), &ctx)) {
            DbgLogFmt("[HWBP] SetThreadContext fallback FAIL gle=%lu\r\n", GetLastError());
            return true; // váº«n report detected
        }
    }
    DbgLogFmt("[HWBP] Cleared DR0-DR3+DR7 (neutralized %d HW BP)\r\n", activeBp);
    return true;
}

} // namespace HwBpDetect

// â”€â”€ Thread Info Audit â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
// Query thread info qua direct syscall (bypass Packman NtQueryInformationThread hook).
// Verify:
//   1) ThreadStartAddress (0x1E) â€” start address cÃ³ bá»‹ leak vÃ o unlinked module khÃ´ng
//   2) ThreadHideFromDebugger (0x11) â€” flag Ä‘Ã£ set hay chÆ°a
// Chá»‰ audit (read-only), khÃ´ng thay Ä‘á»•i gÃ¬.

namespace ThreadInfoAudit {

inline void Audit() {
    HANDLE h = GetCurrentThread();

    // ThreadHideFromDebugger (0x11) â€” SET-ONLY info class, khÃ´ng query Ä‘Æ°á»£c.
    // Chá»‰ log ráº±ng ta Ä‘Ã£ set nÃ³ (via NtSetInformationThreadDirect) trÃªn worker threads.
    DbgLogFmt("[THAUD] ThreadHideFromDebugger: set-only class, skip query (set on worker threads)\r\n");

    // ThreadQuerySetWin32StartAddress (0x1F) â€” query start address cá»§a thread
    PVOID startAddr = nullptr;
    ULONG retLen = 0;
    LONG s2 = DirectSyscall::NtQueryInformationThreadDirect(
        h, 0x1F, &startAddr, sizeof(startAddr), &retLen);
    DbgLogFmt("[THAUD] ThreadStartAddress(0x1F): status=0x%X addr=%p\r\n",
              (unsigned)s2, startAddr);

    // ThreadBasicInformation (0x00) â€” TEB, PID, TID
    struct THREAD_BASIC_INFORMATION {
        PVOID ExitStatus;
        PVOID TebBase;
        struct { HANDLE UniqueProcess; HANDLE UniqueThread; } ClientId;
        PVOID AffinityMask;
        LONG Priority;
        LONG BasePriority;
    };
    THREAD_BASIC_INFORMATION tbi = {};
    retLen = 0;
    LONG s3 = DirectSyscall::NtQueryInformationThreadDirect(
        h, 0x00, &tbi, sizeof(tbi), &retLen);
    DbgLogFmt("[THAUD] ThreadBasicInfo: status=0x%X TEB=%p PID=%lu TID=%lu\r\n",
              (unsigned)s3, tbi.TebBase,
              (unsigned long)(uintptr_t)tbi.ClientId.UniqueProcess,
              (unsigned long)(uintptr_t)tbi.ClientId.UniqueThread);
}

} // namespace ThreadInfoAudit

// â”€â”€ Memory Region Audit â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
// QuÃ©t memory regions tá»« trong process báº±ng NtQueryVirtualMemoryDirect
// (bypass Packman NtQueryVirtualMemory hook).
// TÃ¬m MEM_PRIVATE + PAGE_EXECUTE regions â€” anti-cheat detection surface.
// NightSharp nÃªn dÃ¹ng AllocSectionRWX (MEM_MAPPED) cho code regions.
// Log warning náº¿u phÃ¡t hiá»‡n MEM_PRIVATE executable regions.

namespace MemRegionAudit {

inline void Audit() {
    // MEMORY_BASIC_INFORMATION: Type = MEM_PRIVATE(0x20000) | MEM_MAPPED(0x40000) | MEM_IMAGE(0x1000000)
    // Protect: PAGE_EXECUTE(0x10) | PAGE_EXECUTE_READ(0x20) | PAGE_EXECUTE_READWRITE(0x40) | PAGE_EXECUTE_WRITECOPY(0x80)
    const DWORD EXEC_MASK = 0xF0; // PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY

    SYSTEM_INFO si = {};
    GetSystemInfo(&si);
    uintptr_t addr = reinterpret_cast<uintptr_t>(si.lpMinimumApplicationAddress);
    const uintptr_t maxAddr = reinterpret_cast<uintptr_t>(si.lpMaximumApplicationAddress);

    int privateExec = 0;
    int mappedExec = 0;
    int imageExec = 0;
    int totalRegions = 0;
    int nightsharpRegions = 0;

    DbgLogFmt("[MEMAUD] Scanning %p-%p...\r\n", (void*)addr, (void*)maxAddr);

    while (addr < maxAddr) {
        MEMORY_BASIC_INFORMATION mbi = {};
        SIZE_T ret = 0;
        LONG status = DirectSyscall::NtQueryVirtualMemoryDirect(
            GetCurrentProcess(), reinterpret_cast<PVOID>(addr),
            0 /*MemoryBasicInformation*/, &mbi, sizeof(mbi), &ret);

        if (status < 0 || ret == 0) {
            // Skip region â€” advance by page size
            addr += si.dwPageSize;
            continue;
        }

        ++totalRegions;

        if (mbi.State == MEM_COMMIT && (mbi.Protect & EXEC_MASK)) {
            const bool isPrivate = (mbi.Type == MEM_PRIVATE);
            const bool isMapped  = (mbi.Type == MEM_MAPPED);
            const bool isImage   = (mbi.Type == MEM_IMAGE);

            if (isPrivate) {
                ++privateExec;
                // Log MEM_PRIVATE executable regions â€” potential detection surface
                DbgLogFmt("[MEMAUD] MEM_PRIVATE EXEC: base=%p size=0x%zX prot=0x%X\r\n",
                          (void*)addr, (size_t)mbi.RegionSize, (unsigned)mbi.Protect);
            } else if (isMapped) {
                ++mappedExec;
            } else if (isImage) {
                ++imageExec;
            }
        }

        addr += mbi.RegionSize;
    }

    DbgLogFmt("[MEMAUD] Total=%d  PrivateExec=%d  MappedExec=%d  ImageExec=%d\r\n",
              totalRegions, privateExec, mappedExec, imageExec);

    if (privateExec > 0) {
        DbgLogFmt("[MEMAUD] WARNING: %d MEM_PRIVATE executable regions detected â€” anti-cheat can scan these\r\n",
                  privateExec);
    } else {
        DbgLogFmt("[MEMAUD] OK: no MEM_PRIVATE executable regions\r\n");
    }
}

} // namespace MemRegionAudit

// â”€â”€ Stack Walk Audit â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
// Capture current call stack, check return addresses cÃ³ trá» vÃ o NightSharp
// module (unlinked tá»« PEB) khÃ´ng. Náº¿u cÃ³ â†’ Packman cÃ³ thá»ƒ detect khi walk stack.
// Read-only audit, 100% safe â€” khÃ´ng modify stack.
//
// Packman import RtlLookupFunctionEntry + RtlVirtualUnwind (trong anti-cheat
// functions sub_2C3B7D, sub_2C5791). Náº¿u Packman walk stack khi hooked Nt*
// function Ä‘Æ°á»£c gá»i, return address trá» vÃ o NightSharp code â†’ detect.
//
// NightSharp Ä‘Ã£ dÃ¹ng direct syscall â†’ khÃ´ng gá»i qua hooked functions.
// NhÆ°ng khi NightSharp code gá»i Windows API thÃ´ng thÆ°á»ng, return address
// váº«n trá» vÃ o unlinked module.

namespace StackAudit {

inline volatile LONG g_audited = 0;

// LÆ°u module range cá»§a NightSharp Ä‘á»ƒ check
struct ModuleRange {
    uintptr_t base;
    size_t    size;
};

inline void Audit(HMODULE nightsharpModule) {
    if (InterlockedCompareExchange(&g_audited, 1, 0) != 0) return;
    if (!nightsharpModule) {
        DbgLogFmt("[STACKAUD] No module handle, skip\r\n");
        return;
    }

    // Láº¥y NightSharp module range â€” dÃ¹ng VirtualQuery + PE header
    // (khÃ´ng phá»¥ thuá»™c PEB loader list, hoáº¡t Ä‘á»™ng sau PebHide)
    MEMORY_BASIC_INFORMATION mbi = {};
    if (VirtualQuery(nightsharpModule, &mbi, sizeof(mbi)) != sizeof(mbi)) {
        DbgLogFmt("[STACKAUD] VirtualQuery FAIL gle=%lu\r\n", GetLastError());
        return;
    }
    const uintptr_t nsBase = reinterpret_cast<uintptr_t>(mbi.AllocationBase);
    // Äá»c SizeOfImage tá»« PE header
    auto* dosHeader = reinterpret_cast<IMAGE_DOS_HEADER*>(nsBase);
    auto* ntHeaders = reinterpret_cast<IMAGE_NT_HEADERS*>(nsBase + dosHeader->e_lfanew);
    const size_t nsSize = ntHeaders->OptionalHeader.SizeOfImage;
    const uintptr_t nsEnd = nsBase + nsSize;
    DbgLogFmt("[STACKAUD] NightSharp module: %p-%p (size=0x%zX)\r\n",
              (void*)nsBase, (void*)nsEnd, nsSize);

    // Capture stack back trace â€” RtlCaptureStackBackTrace lÃ  safe, read-only
    // Capture tá»‘i Ä‘a 32 frames (Ä‘á»§ cho DllMain call chain)
    const int MAX_FRAMES = 32;
    void* frames[MAX_FRAMES] = {};
    USHORT captured = RtlCaptureStackBackTrace(0, MAX_FRAMES, frames, nullptr);

    DbgLogFmt("[STACKAUD] Captured %u frames\r\n", (unsigned)captured);

    int nsFrames = 0;
    int otherFrames = 0;
    int unknownFrames = 0;

    for (USHORT i = 0; i < captured; ++i) {
        const uintptr_t addr = reinterpret_cast<uintptr_t>(frames[i]);

        // Check náº¿u addr thuá»™c NightSharp module range
        if (addr >= nsBase && addr < nsEnd) {
            ++nsFrames;
            DbgLogFmt("[STACKAUD]   #%02d NS_MODULE  %p (RVA=0x%llX)\r\n",
                      i, (void*)addr, (unsigned long long)(addr - nsBase));
        } else {
            // Check thuá»™c module nÃ o khÃ¡c
            MEMORY_BASIC_INFORMATION mbi = {};
            if (VirtualQuery(reinterpret_cast<void*>(addr), &mbi, sizeof(mbi)) == sizeof(mbi)) {
                if (mbi.Type == MEM_IMAGE) {
                    ++otherFrames;
                    // Láº¥y module name
                    char modName[MAX_PATH] = {};
                    GetModuleFileNameA(reinterpret_cast<HMODULE>(mbi.AllocationBase),
                                       modName, MAX_PATH);
                    const char* baseName = strrchr(modName, '\\');
                    baseName = baseName ? baseName + 1 : modName;
                    DbgLogFmt("[STACKAUD]   #%02d %-12s %p\r\n", i, baseName, (void*)addr);
                } else {
                    ++unknownFrames;
                    DbgLogFmt("[STACKAUD]   #%02d UNKNOWN     %p (type=0x%X prot=0x%X)\r\n",
                              i, (void*)addr, (unsigned)mbi.Type, (unsigned)mbi.Protect);
                }
            } else {
                ++unknownFrames;
                DbgLogFmt("[STACKAUD]   #%02d UNMAPPED   %p\r\n", i, (void*)addr);
            }
        }
    }

    DbgLogFmt("[STACKAUD] Summary: NS=%d Other=%d Unknown=%d\r\n",
              nsFrames, otherFrames, unknownFrames);

    if (nsFrames > 0) {
        DbgLogFmt("[STACKAUD] WARNING: %d return addresses point to NightSharp module â€” Packman can detect via stack walk\r\n",
                  nsFrames);
    } else {
        DbgLogFmt("[STACKAUD] OK: no return addresses in NightSharp module\r\n");
    }
}

} // namespace StackAudit

// â”€â”€ CRC Bypass (Shadow Copy + NOP JNE) â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
// Method má»›i thay cho ChaCha20 QR hook (Ä‘Ã£ bá»‹ detect).
// Xem CRC_Bypass_Integration_Plan.md vÃ  CRC_Bypass_Analysis.md.

namespace CRCBypass {

// â”€â”€ State â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
inline volatile LONG g_primaryInstalled = 0;
inline volatile LONG g_shadowCreated    = 0;

// NOP JNE state
inline uintptr_t g_crcJneAddr    = 0;
inline uint8_t   g_crcJneOrig[6] = {};

// Shadow copy state
inline void*     g_shadowBase   = nullptr;
inline size_t    g_shadowSize   = 0;
inline uintptr_t g_stubBase     = 0;

// â”€â”€ Shadow Copy â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
// Táº¡o báº£n sao sáº¡ch stub.dll TRÆ¯á»šC khi patch.
// DÃ¹ng cho fallback redirect náº¿u NOP JNE bá»‹ phÃ¡t hiá»‡n.
inline bool CreateShadowCopy() {
    if (InterlockedCompareExchange(&g_shadowCreated, 1, 0) != 0) return true;

    HMODULE hStub = GetModuleHandleA("stub.dll");
    if (!hStub) { g_shadowCreated = 0; return false; }

    g_stubBase = reinterpret_cast<uintptr_t>(hStub);

    // Láº¥y size tá»« PE header
    auto dosHdr = reinterpret_cast<const IMAGE_DOS_HEADER*>(g_stubBase);
    if (dosHdr->e_magic != IMAGE_DOS_SIGNATURE) { g_shadowCreated = 0; return false; }
    auto ntHdr = reinterpret_cast<const IMAGE_NT_HEADERS64*>(g_stubBase + dosHdr->e_lfanew);
    g_shadowSize = ntHdr->OptionalHeader.SizeOfImage;  // ~20.7MB

    // Allocate shadow (RW, khÃ´ng cáº§n execute)
    g_shadowBase = VirtualAlloc(nullptr, g_shadowSize,
                                MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!g_shadowBase) { g_shadowCreated = 0; return false; }

    // Copy toÃ n bá»™ stub.dll â†’ shadow (Báº¢N Sáº CH)
    std::memcpy(g_shadowBase, reinterpret_cast<const void*>(g_stubBase), g_shadowSize);

    DbgLogFmt("[CRC] Shadow copy: base=%p size=0x%llX\r\n",
              g_shadowBase, (unsigned long long)g_shadowSize);
    return true;
}

inline void DestroyShadowCopy() {
    if (g_shadowBase) {
        VirtualFree(g_shadowBase, 0, MEM_RELEASE);
        g_shadowBase = nullptr;
    }
    g_shadowSize    = 0;
    g_stubBase      = 0;
    g_shadowCreated = 0;
}

// â”€â”€ NOP JNE â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
// AOB: 48 3B 05 ?? ?? ?? ?? 0F 85 â†’ cmp rax,[rip+disp32] + jne
// 1 match trong stub.dll code section (verified via CE MCP 2026-08-08).
// Patch jne (6 bytes táº¡i match+7) â†’ 90 90 90 90 90 90
inline bool InstallPrimary() {
    if (InterlockedCompareExchange(&g_primaryInstalled, 1, 0) != 0) return true;

    const uint8_t* stubBase = reinterpret_cast<const uint8_t*>(g_stubBase);
    if (!stubBase) { g_primaryInstalled = 0; return false; }

    // Scan code section cho pattern: cmp rax,[rip+disp32] + jne
    // 48 3B 05 ?? ?? ?? ?? 0F 85
    for (unsigned i = 0; i + 9 <= 0x20000; ++i) {
        if (stubBase[i]     != 0x48) continue;
        if (stubBase[i+1]   != 0x3B) continue;
        if (stubBase[i+2]   != 0x05) continue;
        if (stubBase[i+8]   != 0x0F) continue;
        if (stubBase[i+9]   != 0x85) continue;

        // jne táº¡i match + 7
        g_crcJneAddr = reinterpret_cast<uintptr_t>(stubBase) + i + 7;
        std::memcpy(g_crcJneOrig, reinterpret_cast<const void*>(g_crcJneAddr), 6);

        DbgLogFmt("[CRC] NOP JNE: found at stub.dll+0x%X (abs 0x%llX)\r\n",
                  i + 7, (unsigned long long)g_crcJneAddr);

        // Patch: jne â†’ nop x6
        DWORD oldProt = 0;
        BOOL protOk = DirectSyscall::VirtualProtectDirect(
            reinterpret_cast<void*>(g_crcJneAddr), 6,
            PAGE_EXECUTE_READWRITE, &oldProt);
        if (!protOk) {
            protOk = VirtualProtect(reinterpret_cast<void*>(g_crcJneAddr), 6,
                                    PAGE_EXECUTE_READWRITE, &oldProt);
        }
        if (!protOk) {
            DbgLogFmt("[CRC] NOP JNE: VirtualProtect FAIL\r\n");
            g_crcJneAddr = 0;
            g_primaryInstalled = 0;
            return false;
        }

        std::memset(reinterpret_cast<void*>(g_crcJneAddr), 0x90, 6);
        DWORD dummy = 0;
        DirectSyscall::VirtualProtectDirect(
            reinterpret_cast<void*>(g_crcJneAddr), 6, oldProt, &dummy);
        FlushInstructionCache(GetCurrentProcess(),
                              reinterpret_cast<void*>(g_crcJneAddr), 6);

        DbgLogFmt("[CRC] NOP JNE: patched 6 bytes â†’ NOP (mismatch handler unreachable)\r\n");
        return true;
    }

    DbgLogFmt("[CRC] NOP JNE: pattern NOT FOUND in first 128KB\r\n");
    g_primaryInstalled = 0;
    return false;
}

inline void UninstallPrimary() {
    if (!g_crcJneAddr) return;

    DWORD oldProt = 0;
    BOOL protOk = DirectSyscall::VirtualProtectDirect(
        reinterpret_cast<void*>(g_crcJneAddr), 6,
        PAGE_EXECUTE_READWRITE, &oldProt);
    if (!protOk) {
        VirtualProtect(reinterpret_cast<void*>(g_crcJneAddr), 6,
                       PAGE_EXECUTE_READWRITE, &oldProt);
    }
    std::memcpy(reinterpret_cast<void*>(g_crcJneAddr), g_crcJneOrig, 6);
    DWORD dummy = 0;
    DirectSyscall::VirtualProtectDirect(
        reinterpret_cast<void*>(g_crcJneAddr), 6, oldProt, &dummy);
    FlushInstructionCache(GetCurrentProcess(),
                          reinterpret_cast<void*>(g_crcJneAddr), 6);

    DbgLogFmt("[CRC] NOP JNE: restored original bytes\r\n");
    g_crcJneAddr = 0;
    g_primaryInstalled = 0;
}

// â”€â”€ Combined Install / Uninstall â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

inline bool Install() {
#if !NIGHTSHARP_ENABLE_CRC_BYPASS
    DbgLog("[CRC] Install: CRC bypass DISABLED -> skip\r\n");
    return false;
#else
    // 1. Shadow copy TRÆ¯á»šC (báº£n sao sáº¡ch trÆ°á»›c khi patch)
    if (!CreateShadowCopy()) {
        DbgLogFmt("[CRC] Install: CreateShadowCopy FAIL\r\n");
        return false;
    }
    // 2. NOP JNE (skip mismatch handler)
    if (!InstallPrimary()) {
        DbgLogFmt("[CRC] Install: InstallPrimary FAIL â€” shadow copy available as fallback\r\n");
        // KhÃ´ng return false â€” shadow copy váº«n cÃ³ thá»ƒ dÃ¹ng
    }
    DbgLogFmt("[CRC] === CRC Bypass Installed (Shadow Copy + NOP JNE) ===\r\n");
    return true;
#endif
}

inline void Uninstall() {
#if !NIGHTSHARP_ENABLE_CRC_BYPASS
    return;
#else
    UninstallPrimary();
    DestroyShadowCopy();
    DbgLogFmt("[CRC] === CRC Bypass Uninstalled ===\r\n");
#endif
}

} // namespace CRCBypass

// â”€â”€ Init log â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
// XoÃ¡ file log cÅ© khi DLL load Ä‘á»ƒ má»—i session cÃ³ log riÃªng.
inline void ResetLogFile() {
#ifdef NS_PACKMAN_SILENT
    return;
#else
    DeleteFileA(GetLogPath());
    HANDLE h = CreateFileA(GetLogPath(), GENERIC_WRITE, 0, nullptr,
                           CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
#endif
}
