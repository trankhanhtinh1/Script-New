#pragma once

// ============================================================================
// PackmanHook - CRC Bypass for Riot Packman anti-cheat (minimal footprint)
// ============================================================================
//
// Theo CRC bypass.txt (League CRC Byte Patching CRC Bypass).
//
// Cơ chế:
//   1) Hook 19 bytes tại CRC read site trong stub.dll:
//        49 8B 0E              mov rcx, [r14]
//        F3 44 0F 6F 04 29     movdqu xmm8, [rcx+rbp]
//        66 44 0F 7F 84 24 20 01 00 00   movdqa [rsp+120h], xmm8
//
//   2) Trước khi CRC đọc memory thật, intercept r14 (mảng 4 con trỏ)
//      và redirect mỗi pointer rơi vào "faked region" sang bản backup
//      bytes gốc đã lưu trước khi patch ⇒ hash trả về giá trị unmodified.
//
//   3) Footprint cố gắng giữ tối thiểu: KHÔNG console, KHÔNG VEH,
//      KHÔNG heartbeat thread, KHÔNG log file ở vị trí cố định.
//      Logging chỉ qua OutputDebugString (đi vào DbgPrint buffer).
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

// ── Logging ──────────────────────────────────────────────────────────────────
// Ghi vào %TEMP%\ph.log + OutputDebugString. Đường dẫn %TEMP% là user-scope,
// ít bị anti-cheat sweep hơn C:\Users\Public. Tên file ngắn, không có
// từ khoá ("packman", "hook", "bypass") để giảm risk pattern match.

// ── Log control (Module J) ───────────────────────────────────────────────────
inline volatile LONG g_logEnabled = 1;

inline void SetLogEnabled(bool en) {
    InterlockedExchange(&g_logEnabled, en ? 1 : 0);
}
inline bool IsLogEnabled() {
    return InterlockedCompareExchange(&g_logEnabled, 0, 0) != 0;
}

static const char* GetLogPath() {
#ifdef NS_PACKMAN_SILENT
    return "";   // không dùng
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

// ── Pattern Scanner ──────────────────────────────────────────────────────────

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

// ── DirectSyscall — bypass Packman's ntdll hooks ─────────────────────────────
// Packman hook nhiều Nt* function trong ntdll.dll (xem PackmanHook.txt):
//   NtProtectVirtualMemory (SSN 0x50), NtWriteVirtualMemory (SSN 0x3A),
//   NtContinue (0x43), NtDelayExecution (0x34), NtQueryVirtualMemory (0x23),
//   NtSuspendThread (0x1BE), NtContinueEx (0xA1),
//   NtSetContextThread (0x18D), NtGetContextThread (0xF3).
//
// Packman thay 5-14 byte đầu mỗi Nt* stub bằng FF 25 (jmp [rip+0]) → handler
// riêng. Để bypass, ta tự build syscall stub: mov r10,rcx; mov eax,SSN; syscall; ret.
// SSN trích từ ntdll trên disk (không bị hook) hoặc fallback cứng.

namespace DirectSyscall {

// ── djb2 hash (compile-time) ─────────────────────────────────────────────────
// Dùng để tránh string "Nt*" plaintext trong .rdata (anti-scan surface B).
constexpr uint32_t djb2(const char* s, uint32_t h = 5381u) {
    return *s ? djb2(s + 1, ((h << 5) + h) ^ static_cast<uint8_t>(*s)) : h;
}
#define NS_HASH(name) (::DirectSyscall::djb2(name))

// ── Function typedefs ────────────────────────────────────────────────────────
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

// ── Syscall entry table ──────────────────────────────────────────────────────
// Mỗi entry: tên function (cho resolve), SSN fallback (verify từ IDA ntdll),
// stub pointer, function pointer, SSN thực tế.
struct SyscallEntry {
    uint32_t    nameHash;      // djb2(name) — dùng để resolve ẩn danh
    const char* name;          // giữ cho debug/DumpSyscallTable; sẽ được stringify tùy build
    int         fallbackSsn;
    void*       stub;
    void*       fn;
    int         ssn;
};

// SSN fallback values verified từ IDA 13339 (ntdll.dll Win11 26100)
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
    SYSCALL_COUNT     = 12,
};

// Compile-time hash-collision guard cho g_syscalls[]. Nếu 2 entry cùng hash,
// static_assert này sẽ fail — đổi tên hoặc dùng hash algo khác.
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

// ── Backward compat globals (giữ API cũ cho CRCBypass) ───────────────────────
inline void*         g_protectStub = nullptr;
inline NtProtectFn   g_ntProtect   = nullptr;
inline int           g_protectSsn  = -1;
inline volatile LONG g_lastProtectStatus = 0;

inline void*       g_writeStub = nullptr;
inline NtWriteFn   g_ntWrite   = nullptr;
inline int         g_writeSsn  = -1;
inline volatile LONG g_lastWriteStatus = 0;

// ── Syscall gadget cache (Module A) ──────────────────────────────────────────
// Danh sách địa chỉ 0F 05 C3 trong ntdll .text. Cache 1 lần lúc InitAll,
// pick ngẫu nhiên cho mỗi stub build. Cỡ 64 đủ (ntdll có 30+ mặc định).
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

// Quét .text ntdll tìm mọi `0F 05 C3` (syscall;ret) → cache.
// Chỉ quét trong hàm UNHOOKED (byte đầu = 0x4C 8B D1) để tránh Packman patch
// tail lâu về sau. Fallback: quét toàn .text nếu không đủ candidate.
inline void EnumerateSyscallGadgets() {
    if (InterlockedCompareExchange(&g_gadgetInited, 1, 0) != 0) return;

    HMODULE h = GetModuleHandleW(L"ntdll.dll");
    if (!h) return;
    const auto* buf = reinterpret_cast<const uint8_t*>(h);
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(buf);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return;
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(buf + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return;

    // Tìm .text section
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

    // Pass 1: chỉ giữ gadget nằm sau head `4C 8B D1 B8 xx 00 00 00 F6 04 25`
    // (tail của Nt* stub UNHOOKED) — offset gadget = start + 0x12.
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
    // random dựa __rdtsc (dispersion đủ dùng)
    unsigned long long r = __rdtsc();
    return g_syscallGadgets[(size_t)(r % g_gadgetCount)];
}

// ── AllocSectionRWX (Module MM) ──────────────────────────────────────────────
// CreateFileMapping(INVALID_HANDLE_VALUE) → anonymous section backed by
// pagefile. MapViewOfFile → region hiện MEM_MAPPED thay vì MEM_PRIVATE khi
// NtQueryVirtualMemory (anti-cheat filter MEM_PRIVATE+PAGE_EXECUTE_* miss).
// Section handle được CloseHandle nhưng view giữ mapping alive qua kernel refcount.
inline void* AllocSectionRWX(SIZE_T size) {
    if (size == 0) return nullptr;
    HANDLE hMap = CreateFileMappingA(
        INVALID_HANDLE_VALUE, nullptr,
        PAGE_EXECUTE_READWRITE, 0, static_cast<DWORD>(size), nullptr);
    if (!hMap) return nullptr;
    void* view = MapViewOfFile(hMap,
        FILE_MAP_EXECUTE | FILE_MAP_WRITE | FILE_MAP_READ,
        0, 0, size);
    CloseHandle(hMap); // view giữ section alive qua kernel refcount
    return view;
}

inline bool BuildSyscallStub(int ssn, void** outStub) {
    if (!outStub || ssn < 0) return false;

    void* gadget = PickSyscallGadget();
    // Fallback nếu chưa enumerate (vd. InitSyscall gọi trước InitAll):
    // emit stub cũ (syscall inline) để giữ hoạt động, chấp nhận regression stealth.
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

    // Camouflaged indirect stub — 30 bytes (Module I).
    // 16 byte đầu KHỚP HOÀN TOÀN với ntdll syscall stub head:
    //   4C 8B D1                     mov r10, rcx                    ; 3
    //   B8 SSN 00 00 00              mov eax, SSN                    ; 5
    //   F6 04 25 08 03 FE 7F 01      test byte[0x7FFE0308], 1        ; 8  ← match ntdll
    //   FF 25 00 00 00 00            jmp qword ptr [rip+0]           ; 6
    //   <8 bytes gadget addr>                                        ; 8
    // → memcmp(stub, ntdll_stub_head, 16) = 0 (giống nhau bit-perfect).
    // `test` executed nhưng result IGNORED — chỉ để camouflage. Overhead <1 ns.
    // Module MM: alloc qua CreateFileMapping → region MEM_MAPPED.
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

    // Downgrade từ RWX → RX (giảm 1 anti-scan surface: RWX private region).
    DWORD oldProt = 0;
    VirtualProtect(stub, 32, PAGE_EXECUTE_READ, &oldProt);
    FlushInstructionCache(GetCurrentProcess(), stub, 32);

    *outStub = stub;
    return true;
}

// ── Generic syscall init (dùng table) ────────────────────────────────────────
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

// ── InitAll: khởi tạo tất cả syscall stub cùng lúc ────────────────────────────
inline bool InitAll() {
    DbgLogFmt("[SYS] InitAll: initializing %zu syscall stubs...\r\n", (size_t)SYSCALL_COUNT);
    EnumerateSyscallGadgets();  // NEW: cache gadgets trước khi build stub
    bool ok = true;
    for (size_t i = 0; i < SYSCALL_COUNT; ++i)
        if (!InitSyscall(i)) ok = false;
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
    if (!InitSyscall(IDX_WRITE)) return false;
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

// ── Wrapper functions cho các syscall bị Packman hook ────────────────────────
// Mỗi wrapper: resolve SSN → build stub → gọi trực tiếp kernel, bypass hook.

// NtContinue (SSN 0x43) — Packman hook FF 25, dùng trong exception dispatch
inline LONG NtContinueDirect(PCONTEXT ctx, BOOLEAN raiseAlert) {
    if (!InitSyscall(IDX_CONTINUE)) return -1;
    auto fn = reinterpret_cast<NtContinueFn>(g_syscalls[IDX_CONTINUE].fn);
    return fn(ctx, raiseAlert);
}

// NtDelayExecution (SSN 0x34) — Packman hook, thay Sleep khi cần stealth
inline LONG NtDelayExecutionDirect(PLARGE_INTEGER delay, BOOLEAN alertable) {
    if (!InitSyscall(IDX_DELAY)) return -1;
    auto fn = reinterpret_cast<NtDelayFn>(g_syscalls[IDX_DELAY].fn);
    return fn(delay, alertable);
}

// NtQueryVirtualMemory (SSN 0x23) — Packman hook, query memory info trực tiếp
inline LONG NtQueryVirtualMemoryDirect(HANDLE proc, PVOID base, ULONG cls,
                                       PVOID buf, SIZE_T len, PSIZE_T ret) {
    if (!InitSyscall(IDX_QUERYVIRT)) return -1;
    auto fn = reinterpret_cast<NtQueryVirtFn>(g_syscalls[IDX_QUERYVIRT].fn);
    return fn(proc, base, cls, buf, len, ret);
}

// NtSuspendThread (SSN 0x1BE) — Packman hook, suspend thread bypass
inline LONG NtSuspendThreadDirect(HANDLE thread, PULONG suspendCount) {
    if (!InitSyscall(IDX_SUSPEND)) return -1;
    auto fn = reinterpret_cast<NtSuspendFn>(g_syscalls[IDX_SUSPEND].fn);
    return fn(thread, suspendCount);
}

// NtContinueEx (SSN 0xA1) — Packman hook, extended continue
inline LONG NtContinueExDirect(PCONTEXT ctx, ULONG flags) {
    if (!InitSyscall(IDX_CONTINUEEX)) return -1;
    auto fn = reinterpret_cast<NtContExFn>(g_syscalls[IDX_CONTINUEEX].fn);
    return fn(ctx, flags);
}

// NtSetContextThread (SSN 0x18D) — Packman hook, set thread context bypass
inline LONG NtSetContextThreadDirect(HANDLE thread, PCONTEXT ctx) {
    if (!InitSyscall(IDX_SETCTX)) return -1;
    auto fn = reinterpret_cast<NtSetCtxFn>(g_syscalls[IDX_SETCTX].fn);
    return fn(thread, ctx);
}

// NtGetContextThread (SSN 0xF3) — Packman hook, get thread context bypass
inline LONG NtGetContextThreadDirect(HANDLE thread, PCONTEXT ctx) {
    if (!InitSyscall(IDX_GETCTX)) return -1;
    auto fn = reinterpret_cast<NtGetCtxFn>(g_syscalls[IDX_GETCTX].fn);
    return fn(thread, ctx);
}

// NtSetInformationThread (SSN 0x0D) — Module F ThreadHideFromDebugger.
// ThreadInfoClass = 0x11 (ThreadHideFromDebugger), buffer/length = null/0.
inline LONG NtSetInformationThreadDirect(HANDLE thread, ULONG infoClass,
                                         PVOID info, ULONG infoLen) {
    if (!InitSyscall(IDX_SETINFOTHREAD)) return -1;
    auto fn = reinterpret_cast<NtSetInfoThreadFn>(g_syscalls[IDX_SETINFOTHREAD].fn);
    return fn(thread, infoClass, info, infoLen);
}

// NtQueryInformationThread (SSN 0x0E) — Packman hook (runtime: 90 FF 25...).
// Query thread info trực tiếp kernel, bypass hook.
// ThreadInfoClass quan trọng:
//   0x00 = ThreadBasicInformation (TEB, PID, TID, start address)
//   0x11 = ThreadHideFromDebugger (check if thread đã hide)
//   0x1E = ThreadStartAddress (start routine address)
inline LONG NtQueryInformationThreadDirect(HANDLE thread, ULONG infoClass,
                                           PVOID info, ULONG infoLen,
                                           PULONG retLen) {
    if (!InitSyscall(IDX_QUERYINFOTHREAD)) return -1;
    auto fn = reinterpret_cast<NtQueryInfoThreadFn>(g_syscalls[IDX_QUERYINFOTHREAD].fn);
    return fn(thread, infoClass, info, infoLen, retLen);
}

// ── Stealth Sleep (dùng NtDelayExecution direct, bypass Packman hook) ────────
inline void StealthSleep(DWORD ms) {
    LARGE_INTEGER delay;
    delay.QuadPart = -static_cast<int64_t>(ms) * 10000LL; // 100ns units
    NtDelayExecutionDirect(&delay, FALSE);
}

// ── Dump SSN table (debug) ────────────────────────────────────────────────────
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

// ── CreateThreadDirect — spawn thread bypass Packman's INT3 on NtCreateThreadEx ─
// Packman patch byte đầu NtCreateThreadEx thành CC (INT3) → mỗi CreateThread
// đi qua ntdll đều bị trap. Wrapper này gọi NtCreateThreadEx trực tiếp qua
// syscall stub, bỏ qua entry ntdll. Nếu fail, fallback CreateThread thường.
inline HANDLE CreateThreadDirect(LPTHREAD_START_ROUTINE routine, PVOID param) {
    const auto trampoline = CoreBypass::ResolveThreadBeginTrampoline();
    const bool hasTrampoline = Globals::IsValidPtr(trampoline);

    if (!InitSyscall(IDX_CREATETHREADEX)) {
        // Fallback: CreateThread (với trampoline nếu có)
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
        0,                     // CreateFlags (CREATE_SUSPENDED=0 → chạy ngay)
        0,                     // ZeroBits
        0,                     // StackSize (default)
        0,                     // MaximumStackSize
        nullptr                // AttributeList
    );
    if (status >= 0 && hThread) {
        // Module F: ẩn thread khỏi debugger enumeration.
        NtSetInformationThreadDirect(hThread, 0x11 /*ThreadHideFromDebugger*/, nullptr, 0);
        DbgLogFmt("[SYS] CreateThreadDirect: NtCreateThreadEx OK (handle=%p, status=0x%X) %s\r\n",
                  hThread, (unsigned)status, hasTrampoline ? "spoofed" : "direct");
        return hThread;
    }
    if (hasTrampoline) {
        CoreBypass::g_pendingThreadBegin = {};
    }
    DbgLogFmt("[SYS] CreateThreadDirect: NtCreateThreadEx FAIL status=0x%X — fallback CreateThread\r\n",
              (unsigned)status);
    HANDLE hRetry = hasTrampoline
        ? CoreBypass::CreateThreadSpoofed(routine, param)
        : CreateThread(nullptr, 0, routine, param, 0, nullptr);
    if (hRetry) NtSetInformationThreadDirect(hRetry, 0x11 /*ThreadHideFromDebugger*/, nullptr, 0);
    return hRetry;
}

} // namespace DirectSyscall

// ── Hardware Breakpoint Detection ────────────────────────────────────────────
// Packman có thể set HW breakpoint (DR0-DR3) trên code/data của NightSharp
// để monitor hoạt động. Module này:
//   1) Đọc DR0-DR7 qua NtGetContextThreadDirect (bypass Packman NtGetContextThread hook)
//   2) Nếu DR0-DR3 != 0 và DR7 có enable bit → có HW BP → log + clear
//   3) Clear bằng NtSetContextThreadDirect (bypass Packman NtSetContextThread hook)
//
// CONTEXT_DEBUG_REGISTERS: chỉ đọc/ghi DR0-DR7, không đụng GP regs → an toàn.
// Chỉ check thread hiện tại (DLL load trên thread chính của game).

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
// Nếu bất kỳ L0/G0/L1/G1/L2/G2/L3/G3 = 1 → có HW BP active.

inline bool CheckAndClear() {
    if (InterlockedCompareExchange(&g_checked, 1, 0) != 0) return false;

    CONTEXT ctx = {};
    ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;

    // Đọc context thread hiện tại qua direct syscall (bypass Packman hook)
    LONG status = DirectSyscall::NtGetContextThreadDirect(GetCurrentThread(), &ctx);
    if (status < 0) {
        DbgLogFmt("[HWBP] NtGetContextThreadDirect FAIL status=0x%X\r\n", (unsigned)status);
        // Fallback: GetThreadContext user-mode (có thể bị Packman intercept)
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
        // DRi active nếu DRi != 0 VÀ enable bit tương ứng trong DR7 set
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

    // Clear DR0-DR3 + DR7 để neutralize HW BP
    ctx.Dr0 = 0;
    ctx.Dr1 = 0;
    ctx.Dr2 = 0;
    ctx.Dr3 = 0;
    ctx.Dr7 = 0;
    // Dr6 là status register, clear luôn
    ctx.Dr6 = 0;

    status = DirectSyscall::NtSetContextThreadDirect(GetCurrentThread(), &ctx);
    if (status < 0) {
        DbgLogFmt("[HWBP] NtSetContextThreadDirect FAIL status=0x%X — trying fallback\r\n",
                  (unsigned)status);
        if (!SetThreadContext(GetCurrentThread(), &ctx)) {
            DbgLogFmt("[HWBP] SetThreadContext fallback FAIL gle=%lu\r\n", GetLastError());
            return true; // vẫn report detected
        }
    }
    DbgLogFmt("[HWBP] Cleared DR0-DR3+DR7 (neutralized %d HW BP)\r\n", activeBp);
    return true;
}

} // namespace HwBpDetect

// ── Thread Info Audit ────────────────────────────────────────────────────────
// Query thread info qua direct syscall (bypass Packman NtQueryInformationThread hook).
// Verify:
//   1) ThreadStartAddress (0x1E) — start address có bị leak vào unlinked module không
//   2) ThreadHideFromDebugger (0x11) — flag đã set hay chưa
// Chỉ audit (read-only), không thay đổi gì.

namespace ThreadInfoAudit {

inline void Audit() {
    HANDLE h = GetCurrentThread();

    // ThreadHideFromDebugger (0x11) — SET-ONLY info class, không query được.
    // Chỉ log rằng ta đã set nó (via NtSetInformationThreadDirect) trên worker threads.
    DbgLogFmt("[THAUD] ThreadHideFromDebugger: set-only class, skip query (set on worker threads)\r\n");

    // ThreadQuerySetWin32StartAddress (0x1F) — query start address của thread
    PVOID startAddr = nullptr;
    ULONG retLen = 0;
    LONG s2 = DirectSyscall::NtQueryInformationThreadDirect(
        h, 0x1F, &startAddr, sizeof(startAddr), &retLen);
    DbgLogFmt("[THAUD] ThreadStartAddress(0x1F): status=0x%X addr=%p\r\n",
              (unsigned)s2, startAddr);

    // ThreadBasicInformation (0x00) — TEB, PID, TID
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

// ── Memory Region Audit ──────────────────────────────────────────────────────
// Quét memory regions từ trong process bằng NtQueryVirtualMemoryDirect
// (bypass Packman NtQueryVirtualMemory hook).
// Tìm MEM_PRIVATE + PAGE_EXECUTE regions — anti-cheat detection surface.
// NightSharp nên dùng AllocSectionRWX (MEM_MAPPED) cho code regions.
// Log warning nếu phát hiện MEM_PRIVATE executable regions.

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
            // Skip region — advance by page size
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
                // Log MEM_PRIVATE executable regions — potential detection surface
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
        DbgLogFmt("[MEMAUD] WARNING: %d MEM_PRIVATE executable regions detected — anti-cheat can scan these\r\n",
                  privateExec);
    } else {
        DbgLogFmt("[MEMAUD] OK: no MEM_PRIVATE executable regions\r\n");
    }
}

} // namespace MemRegionAudit

// ── Stack Walk Audit ─────────────────────────────────────────────────────────
// Capture current call stack, check return addresses có trỏ vào NightSharp
// module (unlinked từ PEB) không. Nếu có → Packman có thể detect khi walk stack.
// Read-only audit, 100% safe — không modify stack.
//
// Packman import RtlLookupFunctionEntry + RtlVirtualUnwind (trong anti-cheat
// functions sub_2C3B7D, sub_2C5791). Nếu Packman walk stack khi hooked Nt*
// function được gọi, return address trỏ vào NightSharp code → detect.
//
// NightSharp đã dùng direct syscall → không gọi qua hooked functions.
// Nhưng khi NightSharp code gọi Windows API thông thường, return address
// vẫn trỏ vào unlinked module.

namespace StackAudit {

inline volatile LONG g_audited = 0;

// Lưu module range của NightSharp để check
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

    // Lấy NightSharp module range — dùng VirtualQuery + PE header
    // (không phụ thuộc PEB loader list, hoạt động sau PebHide)
    MEMORY_BASIC_INFORMATION mbi = {};
    if (VirtualQuery(nightsharpModule, &mbi, sizeof(mbi)) != sizeof(mbi)) {
        DbgLogFmt("[STACKAUD] VirtualQuery FAIL gle=%lu\r\n", GetLastError());
        return;
    }
    const uintptr_t nsBase = reinterpret_cast<uintptr_t>(mbi.AllocationBase);
    // Đọc SizeOfImage từ PE header
    auto* dosHeader = reinterpret_cast<IMAGE_DOS_HEADER*>(nsBase);
    auto* ntHeaders = reinterpret_cast<IMAGE_NT_HEADERS*>(nsBase + dosHeader->e_lfanew);
    const size_t nsSize = ntHeaders->OptionalHeader.SizeOfImage;
    const uintptr_t nsEnd = nsBase + nsSize;
    DbgLogFmt("[STACKAUD] NightSharp module: %p-%p (size=0x%zX)\r\n",
              (void*)nsBase, (void*)nsEnd, nsSize);

    // Capture stack back trace — RtlCaptureStackBackTrace là safe, read-only
    // Capture tối đa 32 frames (đủ cho DllMain call chain)
    const int MAX_FRAMES = 32;
    void* frames[MAX_FRAMES] = {};
    USHORT captured = RtlCaptureStackBackTrace(0, MAX_FRAMES, frames, nullptr);

    DbgLogFmt("[STACKAUD] Captured %u frames\r\n", (unsigned)captured);

    int nsFrames = 0;
    int otherFrames = 0;
    int unknownFrames = 0;

    for (USHORT i = 0; i < captured; ++i) {
        const uintptr_t addr = reinterpret_cast<uintptr_t>(frames[i]);

        // Check nếu addr thuộc NightSharp module range
        if (addr >= nsBase && addr < nsEnd) {
            ++nsFrames;
            DbgLogFmt("[STACKAUD]   #%02d NS_MODULE  %p (RVA=0x%llX)\r\n",
                      i, (void*)addr, (unsigned long long)(addr - nsBase));
        } else {
            // Check thuộc module nào khác
            MEMORY_BASIC_INFORMATION mbi = {};
            if (VirtualQuery(reinterpret_cast<void*>(addr), &mbi, sizeof(mbi)) == sizeof(mbi)) {
                if (mbi.Type == MEM_IMAGE) {
                    ++otherFrames;
                    // Lấy module name
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
        DbgLogFmt("[STACKAUD] WARNING: %d return addresses point to NightSharp module — Packman can detect via stack walk\r\n",
                  nsFrames);
    } else {
        DbgLogFmt("[STACKAUD] OK: no return addresses in NightSharp module\r\n");
    }
}

} // namespace StackAudit

// ── CRC Bypass ───────────────────────────────────────────────────────────────

namespace CRCBypass {

struct MemoryRegion {
    uintptr_t address;
    size_t    size;
};

struct FakedMemoryRegion {
    MemoryRegion       region;
    std::vector<BYTE>  bytes;
};

static const unsigned char kPattern[] = {
    0x49, 0x8B, 0x0E, 0xF3, 0x44, 0x0F, 0x6F, 0x04, 0x29
};
static const size_t kPatternLen    = sizeof(kPattern);
static const size_t kHookSize      = 19;
static const size_t kCRCCheckCount = 4;

inline std::vector<FakedMemoryRegion> g_fakedRegions;
inline std::vector<BYTE>              g_originalStubBytes;
inline std::mutex                     g_patchMutex;

inline uintptr_t      g_hookAddr   = 0;
inline uintptr_t      g_jmpBack    = 0;
inline void*          g_hookStub   = nullptr;
inline size_t         g_hookStubSize = 0;
inline volatile LONG  g_installed  = 0;
inline volatile LONG  g_fakedReady = 0;

inline MemoryRegion GetMemoryRegion(uintptr_t address) {
    MEMORY_BASIC_INFORMATION mbi = {};
    if (!VirtualQuery(reinterpret_cast<void*>(address), &mbi, sizeof(mbi))) {
        return { 0, 0 };
    }
    return { reinterpret_cast<uintptr_t>(mbi.BaseAddress), mbi.RegionSize };
}

inline FakedMemoryRegion* FindFakedRegion(uintptr_t address) {
    for (auto& r : g_fakedRegions) {
        if (address >= r.region.address &&
            address <  r.region.address + r.region.size) {
            return &r;
        }
    }
    return nullptr;
}

// ── CheckMemoryBlocks ────────────────────────────────────────────────────────
// Hot path: KHÔNG cấp phát heap / log gì cả.

inline void __fastcall CheckMemoryBlocks(uintptr_t r14, uintptr_t /*rbp*/) {
    if (!g_fakedReady) return;
    for (size_t i = 0; i < kCRCCheckCount; ++i) {
        uintptr_t* slot = reinterpret_cast<uintptr_t*>(r14 + i * sizeof(uintptr_t));
        uintptr_t address = *slot;
        auto* fake = FindFakedRegion(address);
        if (fake) {
            uintptr_t offset = address - fake->region.address;
            *slot = reinterpret_cast<uintptr_t>(fake->bytes.data() + offset);
        }
    }
}

// SEH-memcpy helper (vì __try không xài chung lock_guard).
inline bool SafeMemCpyRead(void* dst, const void* src, size_t size) {
    __try {
        memcpy(dst, src, size);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

inline void AddPatchAddress(uintptr_t address) {
    std::lock_guard<std::mutex> lock(g_patchMutex);
    if (FindFakedRegion(address)) return;

    MemoryRegion region = GetMemoryRegion(address);
    if (region.size == 0) return;

    FakedMemoryRegion fake;
    fake.region = region;
    fake.bytes.resize(region.size);

    if (!SafeMemCpyRead(fake.bytes.data(),
                        reinterpret_cast<void*>(region.address),
                        region.size)) {
        return;
    }
    g_fakedRegions.push_back(std::move(fake));
}

// ── BuildHookStub ────────────────────────────────────────────────────────────
// MSVC x64 không hỗ trợ __asm. Tự emit byte.
//   push rax..r15 + pushfq (16 push = 128 bytes ⇒ RSP mod 16 = 0)
//   mov rcx, r14 ; mov rdx, rbp
//   sub rsp, 20h ; call CheckMemoryBlocks ; add rsp, 20h
//   popfq ; pop r15..rax
//   ; original 19 bytes
//   jmp [rip+0] ; .qword jmpBack

static void* BuildHookStub(uintptr_t checkFnAddr,
                           const unsigned char* origBytes,
                           size_t origSize,
                           uintptr_t jmpBackAddr) {
    constexpr size_t kStubMax = 512;
    // Module MM: alloc qua CreateFileMapping → region MEM_MAPPED (giấu khỏi
    // MEM_PRIVATE + PAGE_EXECUTE_* scanner).
    auto* stub = reinterpret_cast<unsigned char*>(
        DirectSyscall::AllocSectionRWX(kStubMax));
    if (!stub) return nullptr;

    size_t idx = 0;
    auto put8  = [&](uint8_t b)                  { stub[idx++] = b; };
    auto put   = [&](const uint8_t* p, size_t n) { memcpy(stub + idx, p, n); idx += n; };
    auto put64 = [&](uint64_t v)                 { memcpy(stub + idx, &v, 8); idx += 8; };

    // push rax..r15 + pushfq
    put8(0x50); put8(0x51); put8(0x52); put8(0x53);
    put8(0x55); put8(0x56); put8(0x57);
    put8(0x41); put8(0x50);    // push r8
    put8(0x41); put8(0x51);    // push r9
    put8(0x41); put8(0x52);    // push r10
    put8(0x41); put8(0x53);    // push r11
    put8(0x41); put8(0x54);    // push r12
    put8(0x41); put8(0x55);    // push r13
    put8(0x41); put8(0x56);    // push r14
    put8(0x41); put8(0x57);    // push r15
    put8(0x9C);

    // mov rcx, r14  /  mov rdx, rbp
    put8(0x4C); put8(0x89); put8(0xF1);
    put8(0x48); put8(0x89); put8(0xEA);

    // sub rsp, 20h
    put8(0x48); put8(0x83); put8(0xEC); put8(0x20);

    // mov rax, imm64 / call rax
    put8(0x48); put8(0xB8); put64(static_cast<uint64_t>(checkFnAddr));
    put8(0xFF); put8(0xD0);

    // add rsp, 20h
    put8(0x48); put8(0x83); put8(0xC4); put8(0x20);

    // popfq / pop r15..rax
    put8(0x9D);
    put8(0x41); put8(0x5F); put8(0x41); put8(0x5E);
    put8(0x41); put8(0x5D); put8(0x41); put8(0x5C);
    put8(0x41); put8(0x5B); put8(0x41); put8(0x5A);
    put8(0x41); put8(0x59); put8(0x41); put8(0x58);
    put8(0x5F); put8(0x5E); put8(0x5D); put8(0x5B);
    put8(0x5A); put8(0x59); put8(0x58);

    // original 19 bytes
    put(origBytes, origSize);

    // jmp [rip+0] / qword jmpBackAddr
    put8(0xFF); put8(0x25);
    put8(0x00); put8(0x00); put8(0x00); put8(0x00);
    put64(static_cast<uint64_t>(jmpBackAddr));

    g_hookStubSize = idx;
    return stub;
}

// ── WriteStubJmp ─────────────────────────────────────────────────────────────

static bool WriteStubJmp(uintptr_t address, uintptr_t destination, size_t size) {
    if (!address || !destination || size < 14) return false;

    // Lưu byte gốc + đăng ký region.
    g_originalStubBytes.clear();
    g_originalStubBytes.reserve(size);
    for (size_t i = 0; i < size; ++i) {
        AddPatchAddress(address + i);
        g_originalStubBytes.push_back(*reinterpret_cast<BYTE*>(address + i));
    }

    int selfTest = DirectSyscall::SelfTest(address);
    DWORD writeProt = PAGE_EXECUTE_WRITECOPY;
    if      (selfTest == 2) writeProt = PAGE_WRITECOPY;
    else if (selfTest == 4) writeProt = PAGE_EXECUTE_READWRITE;
    DbgLogFmt("[CRC] WriteStubJmp: selfTest=%d  writeProt=0x%X\r\n", selfTest, (unsigned)writeProt);

    DWORD oldProt = 0;
    BOOL  protOk = DirectSyscall::VirtualProtectDirect(
        reinterpret_cast<void*>(address), size, writeProt, &oldProt);
    DbgLogFmt("[CRC] WriteStubJmp: VirtualProtectDirect status=%d  oldProt=0x%X\r\n",
              (int)protOk, (unsigned)oldProt);
    if (!protOk) {
        DbgLogFmt("[CRC] WriteStubJmp: direct protect fail, trying fallback VirtualProtect...\r\n");
        if (!VirtualProtect(reinterpret_cast<void*>(address), size,
                            PAGE_EXECUTE_WRITECOPY, &oldProt) &&
            !VirtualProtect(reinterpret_cast<void*>(address), size,
                            PAGE_EXECUTE_READWRITE, &oldProt)) {
            DbgLogFmt("[CRC] WriteStubJmp: ALL protect methods FAIL\r\n");
            return false;
        }
        DbgLogFmt("[CRC] WriteStubJmp: fallback VirtualProtect OK\r\n");
    }

    unsigned char buf[32] = {};
    buf[0] = 0xFF; buf[1] = 0x25;
    memcpy(buf + 6, &destination, sizeof(uintptr_t));
    for (size_t i = 14; i < size; ++i) buf[i] = 0x90;

    // Bật flag NGAY trước khi ghi JMP (tránh race với CRC thread).
    MemoryBarrier();
    InterlockedExchange(&g_fakedReady, 1);
    MemoryBarrier();

    DbgLogFmt("[CRC] WriteStubJmp: writing %zu bytes JMP at 0x%llX\r\n",
              size, (unsigned long long)address);
    if (!DirectSyscall::WriteVirtualMemoryDirect(
            reinterpret_cast<void*>(address), buf, size)) {
        DbgLogFmt("[CRC] WriteStubJmp: direct write fail, trying memcpy...\r\n");
        __try {
            memcpy(reinterpret_cast<void*>(address), buf, size);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            DbgLogFmt("[CRC] WriteStubJmp: memcpy also FAIL (exception)\r\n");
            DirectSyscall::VirtualProtectDirect(
                reinterpret_cast<void*>(address), size, oldProt, &oldProt);
            return false;
        }
        DbgLogFmt("[CRC] WriteStubJmp: memcpy OK\r\n");
    }
    DbgLogFmt("[CRC] WriteStubJmp: write OK, restoring protection...\r\n");

    DWORD dummy = 0;
    DirectSyscall::VirtualProtectDirect(
        reinterpret_cast<void*>(address), size, oldProt, &dummy);
    FlushInstructionCache(GetCurrentProcess(),
                          reinterpret_cast<void*>(address), size);
    DbgLogFmt("[CRC] WriteStubJmp: DONE — JMP installed, fakedReady=1\r\n");
    return true;
}

// ── Install / Uninstall ──────────────────────────────────────────────────────

inline bool Install() {
    if (InterlockedCompareExchange(&g_installed, 1, 0) != 0) {
        DbgLog("[CRC] Install: đã cài rồi, bỏ qua\r\n");
        return true;
    }

    HMODULE hStub = GetModuleHandleA("stub.dll");
    if (!hStub) {
        DbgLog("[CRC] Install: stub.dll chưa load\r\n");
        InterlockedExchange(&g_installed, 0);
        return false;
    }

    MODULEINFO mi = {};
    if (!GetModuleInformation(GetCurrentProcess(), hStub, &mi, sizeof(mi))) {
        DbgLog("[CRC] Install: GetModuleInformation(stub.dll) FAIL\r\n");
        InterlockedExchange(&g_installed, 0);
        return false;
    }
    DbgLogFmt("[CRC] stub.dll base=0x%p size=0x%X\r\n",
              hStub, (unsigned)mi.SizeOfImage);

    DbgLogFmt("[CRC] Install: scanning stub.dll for CRC pattern (size=0x%X)...\r\n",
              (unsigned)mi.SizeOfImage);

    void* found = PatternScan(
        reinterpret_cast<const unsigned char*>(hStub),
        mi.SizeOfImage, kPattern, kPatternLen);
    if (!found) {
        DbgLogFmt("[CRC] Install: pattern 49 8B 0E F3 44 0F 6F 04 29 NOT FOUND\r\n");
        InterlockedExchange(&g_installed, 0);
        return false;
    }

    DbgLogFmt("[CRC] Install: pattern found at stub.dll+0x%llX\r\n",
              (unsigned long long)(reinterpret_cast<uintptr_t>(found) - 
              reinterpret_cast<uintptr_t>(hStub)));

    g_hookAddr = reinterpret_cast<uintptr_t>(found);
    g_jmpBack  = g_hookAddr + kHookSize;
    DbgLogFmt("[CRC] hook tại stub.dll+0x%llX (abs 0x%llX), jmpBack=0x%llX\r\n",
              (unsigned long long)(g_hookAddr - reinterpret_cast<uintptr_t>(hStub)),
              (unsigned long long)g_hookAddr,
              (unsigned long long)g_jmpBack);

    g_hookStub = BuildHookStub(
        reinterpret_cast<uintptr_t>(&CheckMemoryBlocks),
        reinterpret_cast<const unsigned char*>(g_hookAddr),
        kHookSize,
        g_jmpBack);
    if (!g_hookStub) {
        DbgLog("[CRC] Install: BuildHookStub FAIL\r\n");
        InterlockedExchange(&g_installed, 0);
        return false;
    }
    DbgLogFmt("[CRC] hook stub @ %p (%zu bytes)\r\n",
              g_hookStub, g_hookStubSize);

    if (!WriteStubJmp(g_hookAddr,
                      reinterpret_cast<uintptr_t>(g_hookStub),
                      kHookSize)) {
        DbgLogFmt("[CRC] Install: WriteStubJmp FAIL\r\n");
        VirtualFree(g_hookStub, 0, MEM_RELEASE);
        g_hookStub = nullptr;
        InterlockedExchange(&g_installed, 0);
        return false;
    }

    DbgLogFmt("[CRC] === CRC Bypass Installed ===\r\n");
    DbgLogFmt("[CRC]   hookAddr  = 0x%llX (stub.dll+0x%llX)\r\n",
              (unsigned long long)g_hookAddr,
              (unsigned long long)(g_hookAddr - reinterpret_cast<uintptr_t>(hStub)));
    DbgLogFmt("[CRC]   hookStub  = %p (%zu bytes)\r\n", g_hookStub, g_hookStubSize);
    DbgLogFmt("[CRC]   jmpBack   = 0x%llX\r\n", (unsigned long long)g_jmpBack);
    DbgLogFmt("[CRC]   fakedRegions = %zu\r\n", g_fakedRegions.size());
    DbgLogFmt("[CRC]   fakedReady   = %d\r\n", (int)g_fakedReady);
    DbgLogFmt("[CRC] ============================\r\n");
    return true;
}

inline void Uninstall() {
    if (InterlockedCompareExchange(&g_installed, 0, 1) != 1) return;
    if (!g_hookAddr || g_originalStubBytes.size() != kHookSize) return;

    DWORD oldProt = 0;
    BOOL ok = DirectSyscall::VirtualProtectDirect(
        reinterpret_cast<void*>(g_hookAddr), kHookSize,
        PAGE_EXECUTE_WRITECOPY, &oldProt);
    if (!ok) {
        ok = VirtualProtect(reinterpret_cast<void*>(g_hookAddr), kHookSize,
                            PAGE_EXECUTE_WRITECOPY, &oldProt) ||
             VirtualProtect(reinterpret_cast<void*>(g_hookAddr), kHookSize,
                            PAGE_EXECUTE_READWRITE, &oldProt);
    }
    if (ok) {
        if (!DirectSyscall::WriteVirtualMemoryDirect(
                reinterpret_cast<void*>(g_hookAddr),
                g_originalStubBytes.data(), kHookSize)) {
            __try {
                memcpy(reinterpret_cast<void*>(g_hookAddr),
                       g_originalStubBytes.data(), kHookSize);
            } __except (EXCEPTION_EXECUTE_HANDLER) {}
        }
        DWORD dummy = 0;
        DirectSyscall::VirtualProtectDirect(
            reinterpret_cast<void*>(g_hookAddr), kHookSize, oldProt, &dummy);
        FlushInstructionCache(GetCurrentProcess(),
                              reinterpret_cast<void*>(g_hookAddr), kHookSize);
    }

    if (g_hookStub) {
        VirtualFree(g_hookStub, 0, MEM_RELEASE);
        g_hookStub = nullptr;
    }
    InterlockedExchange(&g_fakedReady, 0);
}

} // namespace CRCBypass

// ── Deferred installer ───────────────────────────────────────────────────────

inline volatile LONG g_crcInstallStarted = 0;
inline volatile LONG g_crcInstallShutdown = 0;

inline void ResetDeferredCRCInstallShutdown() {
    InterlockedExchange(&g_crcInstallShutdown, 0);
}

inline void RequestDeferredCRCInstallShutdown() {
    InterlockedExchange(&g_crcInstallShutdown, 1);
}

// DeferredCRCInstallThread: alternative installer (dùng khi cần
// spawn riêng, không gộp vào BootstrapWorker). Hiện không dùng trong
// dllmain.cpp vì BootstrapWorker đã cover logic này. Giữ lại cho reference.
inline DWORD WINAPI DeferredCRCInstallThread(LPVOID) {
    InterlockedExchange(&g_crcInstallStarted, 1);
    DbgLogTs("[CRC] Deferred thread: init direct syscalls...\r\n");
    DirectSyscall::InitAll();
    DirectSyscall::DumpSyscallTable();

    DbgLogTs("[CRC] Deferred thread: chờ stub.dll load (max 60s)\r\n");
    for (int i = 0; i < 120; ++i) {
        if (InterlockedCompareExchange(&g_crcInstallShutdown, 0, 0) != 0) {
            DbgLogTs("[CRC] Deferred: shutdown requested, exit\r\n");
            InterlockedExchange(&g_crcInstallStarted, 0);
            return 0;
        }
        DirectSyscall::StealthSleep(500);
        if (InterlockedCompareExchange(&g_crcInstallShutdown, 0, 0) != 0) {
            DbgLogTs("[CRC] Deferred: shutdown requested after sleep, exit\r\n");
            InterlockedExchange(&g_crcInstallStarted, 0);
            return 0;
        }
        if (GetModuleHandleA("stub.dll")) {
            DbgLogFmt("[CRC] Deferred: stub.dll xuất hiện sau %d ms, install...\r\n",
                      (i + 1) * 500);
            CRCBypass::Install();
            InterlockedExchange(&g_crcInstallStarted, 0);
            return 0;
        }
    }
    DbgLogTs("[CRC] Deferred: stub.dll KHÔNG load trong 60s, bỏ\r\n");
    InterlockedExchange(&g_crcInstallStarted, 0);
    return 0;
}

// ── Init log ─────────────────────────────────────────────────────────────────
// Xoá file log cũ khi DLL load để mỗi session có log riêng.
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
