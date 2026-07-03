#pragma once

#include <Windows.h>
#include <DbgHelp.h>
#include <TlHelp32.h>
#include <cctype>
#include <cstdio>
#include <cstring>

#include "DebugLog.h"

#pragma comment(lib, "Dbghelp.lib")

#ifndef NIGHTSHARP_DUMP_FIRST_CHANCE_EXCEPTIONS
#define NIGHTSHARP_DUMP_FIRST_CHANCE_EXCEPTIONS 0
#endif

#ifndef NIGHTSHARP_FULL_MEMORY_CRASH_DUMP
#define NIGHTSHARP_FULL_MEMORY_CRASH_DUMP 1
#endif

#ifndef NIGHTSHARP_LOG_FIRST_CHANCE_EXCEPTIONS
#define NIGHTSHARP_LOG_FIRST_CHANCE_EXCEPTIONS 0
#endif

namespace NightSharpDebug::CrashReporter {

inline HMODULE g_module = nullptr;
inline PVOID g_vectoredHandler = nullptr;
inline LPTOP_LEVEL_EXCEPTION_FILTER g_previousFilter = nullptr;
inline volatile LONG g_installed = 0;
inline volatile LONG g_dumping = 0;
inline volatile LONG g_dumpSerial = 0;
inline volatile LONG g_guardRunning = 0;
inline HANDLE g_guardThread = nullptr;

inline constexpr const char* kDumpDirectory = "C:\\Users\\Public\\NightSharpDumps";

inline bool IsSeriousException(DWORD code) {
    switch (code) {
    case EXCEPTION_ACCESS_VIOLATION:
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
    case EXCEPTION_DATATYPE_MISALIGNMENT:
    case EXCEPTION_FLT_DIVIDE_BY_ZERO:
    case EXCEPTION_FLT_INVALID_OPERATION:
    case EXCEPTION_ILLEGAL_INSTRUCTION:
    case EXCEPTION_IN_PAGE_ERROR:
    case EXCEPTION_INT_DIVIDE_BY_ZERO:
    case EXCEPTION_INVALID_DISPOSITION:
    case EXCEPTION_NONCONTINUABLE_EXCEPTION:
    case EXCEPTION_PRIV_INSTRUCTION:
    case EXCEPTION_STACK_OVERFLOW:
        return true;
    default:
        return false;
    }
}

inline void SanitizeForFileName(const char* text, char* out, size_t outSize) {
    if (!out || outSize == 0) {
        return;
    }

    size_t used = 0;
    if (text) {
        for (const char* p = text; *p && used + 1 < outSize; ++p) {
            const unsigned char ch = static_cast<unsigned char>(*p);
            if (std::isalnum(ch) || ch == '-' || ch == '_') {
                out[used++] = static_cast<char>(ch);
            } else {
                out[used++] = '_';
            }
        }
    }

    if (used == 0) {
        lstrcpynA(out, "unknown", static_cast<int>(outSize));
    } else {
        out[used] = '\0';
    }
}

inline bool EnsureDumpDirectory() {
    if (CreateDirectoryA(kDumpDirectory, nullptr) || GetLastError() == ERROR_ALREADY_EXISTS) {
        return true;
    }

    NightSharpDebug::Logf("[CrashReporter] CreateDirectory failed gle=%lu dir=%s",
                          GetLastError(),
                          kDumpDirectory);
    return false;
}

inline void BuildDumpBasePath(const char* stage, char* out, size_t outSize) {
    if (!out || outSize == 0) {
        return;
    }

    EnsureDumpDirectory();

    SYSTEMTIME st = {};
    GetLocalTime(&st);

    char safeStage[80] = {};
    SanitizeForFileName(stage, safeStage, sizeof(safeStage));

    const LONG serial = InterlockedIncrement(&g_dumpSerial);
    _snprintf_s(
        out,
        outSize,
        _TRUNCATE,
        "%s\\nightsharp_crash_%04u%02u%02u_%02u%02u%02u_%lu_%lu_%ld_%s",
        kDumpDirectory,
        static_cast<unsigned>(st.wYear),
        static_cast<unsigned>(st.wMonth),
        static_cast<unsigned>(st.wDay),
        static_cast<unsigned>(st.wHour),
        static_cast<unsigned>(st.wMinute),
        static_cast<unsigned>(st.wSecond),
        static_cast<unsigned long>(GetCurrentProcessId()),
        static_cast<unsigned long>(GetCurrentThreadId()),
        static_cast<long>(serial),
        safeStage);
}

inline void BuildPathWithExtension(const char* base, const char* ext, char* out, size_t outSize) {
    if (!out || outSize == 0) {
        return;
    }
    _snprintf_s(out, outSize, _TRUNCATE, "%s%s", base ? base : "", ext ? ext : "");
}

inline void LogAddressLine(const char* label, void* address) {
    char desc[256] = {};
    NightSharpDebug::DescribeAddress(address, desc, sizeof(desc));
    NightSharpDebug::Logf("[CrashReporter] %s=%s", label ? label : "address", desc);
}

inline void LogModuleSnapshot() {
    char exePath[MAX_PATH] = {};
    GetModuleFileNameA(nullptr, exePath, MAX_PATH);
    NightSharpDebug::Logf("[CrashReporter] process=%s pid=%lu",
                          exePath,
                          static_cast<unsigned long>(GetCurrentProcessId()));

    if (g_module) {
        char dllPath[MAX_PATH] = {};
        GetModuleFileNameA(g_module, dllPath, MAX_PATH);
        NightSharpDebug::Logf("[CrashReporter] module=%s base=%p",
                              dllPath,
                              g_module);
    }

    HANDLE snapshot = CreateToolhelp32Snapshot(
        TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32,
        GetCurrentProcessId());
    if (snapshot == INVALID_HANDLE_VALUE) {
        NightSharpDebug::Logf("[CrashReporter] module snapshot failed gle=%lu",
                              GetLastError());
        return;
    }

    MODULEENTRY32 entry = {};
    entry.dwSize = sizeof(entry);
    int count = 0;
    if (Module32First(snapshot, &entry)) {
        do {
            if (count < 48) {
                NightSharpDebug::Logf(
                    "[CrashReporter] loaded[%02d] base=%p size=0x%lx name=%s",
                    count,
                    entry.modBaseAddr,
                    static_cast<unsigned long>(entry.modBaseSize),
                    entry.szModule);
            }
            ++count;
        } while (Module32Next(snapshot, &entry));
    }
    CloseHandle(snapshot);

    if (count > 48) {
        NightSharpDebug::Logf("[CrashReporter] loaded modules truncated count=%d", count);
    }
}

inline void WriteTextFileLine(HANDLE file, const char* text) {
    if (file == INVALID_HANDLE_VALUE || !text) {
        return;
    }

    DWORD written = 0;
    WriteFile(file, text, static_cast<DWORD>(lstrlenA(text)), &written, nullptr);
}

inline void WriteLatestCrashPointer(const char* dumpPath, const char* metaPath) {
    char latestPath[MAX_PATH] = {};
    BuildPathWithExtension(kDumpDirectory, "\\latest_crash.txt", latestPath, sizeof(latestPath));
    HANDLE file = CreateFileA(
        latestPath,
        GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }

    char line[1024] = {};
    _snprintf_s(line, sizeof(line), _TRUNCATE, "dump=%s\r\nmeta=%s\r\n", dumpPath ? dumpPath : "", metaPath ? metaPath : "");
    WriteTextFileLine(file, line);
    CloseHandle(file);
}

inline void WriteCrashMetadata(const char* metaPath,
                               const char* dumpPath,
                               const char* stage,
                               EXCEPTION_POINTERS* exceptionPointers,
                               bool dumpOk) {
    if (!metaPath || !metaPath[0]) {
        return;
    }

    HANDLE file = CreateFileA(
        metaPath,
        GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        NightSharpDebug::Logf("[CrashReporter] metadata CreateFile failed gle=%lu path=%s",
                              GetLastError(),
                              metaPath);
        return;
    }

    SYSTEMTIME st = {};
    GetLocalTime(&st);

    char line[2048] = {};
    _snprintf_s(
        line,
        sizeof(line),
        _TRUNCATE,
        "NightSharp crash report\r\n"
        "time=%04u-%02u-%02u %02u:%02u:%02u.%03u\r\n"
        "stage=%s\r\n"
        "pid=%lu\r\n"
        "tid=%lu\r\n"
        "dump_ok=%d\r\n"
        "dump=%s\r\n"
        "metadata=%s\r\n",
        static_cast<unsigned>(st.wYear),
        static_cast<unsigned>(st.wMonth),
        static_cast<unsigned>(st.wDay),
        static_cast<unsigned>(st.wHour),
        static_cast<unsigned>(st.wMinute),
        static_cast<unsigned>(st.wSecond),
        static_cast<unsigned>(st.wMilliseconds),
        stage ? stage : "?",
        static_cast<unsigned long>(GetCurrentProcessId()),
        static_cast<unsigned long>(GetCurrentThreadId()),
        dumpOk ? 1 : 0,
        dumpPath ? dumpPath : "",
        metaPath);
    WriteTextFileLine(file, line);

    if (exceptionPointers && exceptionPointers->ExceptionRecord) {
        auto* record = exceptionPointers->ExceptionRecord;
        char exceptionAddress[256] = {};
        NightSharpDebug::DescribeAddress(record->ExceptionAddress, exceptionAddress, sizeof(exceptionAddress));
        _snprintf_s(
            line,
            sizeof(line),
            _TRUNCATE,
            "exception_code=0x%08lX\r\n"
            "exception_flags=0x%08lX\r\n"
            "exception_address=%s\r\n"
            "exception_parameters=%lu\r\n"
            "exception_info0=0x%llX\r\n"
            "exception_info1=0x%llX\r\n"
            "exception_info2=0x%llX\r\n",
            static_cast<unsigned long>(record->ExceptionCode),
            static_cast<unsigned long>(record->ExceptionFlags),
            exceptionAddress,
            static_cast<unsigned long>(record->NumberParameters),
            record->NumberParameters > 0 ? static_cast<unsigned long long>(record->ExceptionInformation[0]) : 0ULL,
            record->NumberParameters > 1 ? static_cast<unsigned long long>(record->ExceptionInformation[1]) : 0ULL,
            record->NumberParameters > 2 ? static_cast<unsigned long long>(record->ExceptionInformation[2]) : 0ULL);
        WriteTextFileLine(file, line);
    }

#ifdef _M_X64
    if (exceptionPointers && exceptionPointers->ContextRecord) {
        auto* ctx = exceptionPointers->ContextRecord;
        _snprintf_s(
            line,
            sizeof(line),
            _TRUNCATE,
            "rip=0x%llX\r\nrsp=0x%llX\r\nrbp=0x%llX\r\nrax=0x%llX\r\nrbx=0x%llX\r\n"
            "rcx=0x%llX\r\nrdx=0x%llX\r\nrsi=0x%llX\r\nrdi=0x%llX\r\n"
            "r8=0x%llX\r\nr9=0x%llX\r\nr10=0x%llX\r\nr11=0x%llX\r\n"
            "r12=0x%llX\r\nr13=0x%llX\r\nr14=0x%llX\r\nr15=0x%llX\r\n",
            static_cast<unsigned long long>(ctx->Rip),
            static_cast<unsigned long long>(ctx->Rsp),
            static_cast<unsigned long long>(ctx->Rbp),
            static_cast<unsigned long long>(ctx->Rax),
            static_cast<unsigned long long>(ctx->Rbx),
            static_cast<unsigned long long>(ctx->Rcx),
            static_cast<unsigned long long>(ctx->Rdx),
            static_cast<unsigned long long>(ctx->Rsi),
            static_cast<unsigned long long>(ctx->Rdi),
            static_cast<unsigned long long>(ctx->R8),
            static_cast<unsigned long long>(ctx->R9),
            static_cast<unsigned long long>(ctx->R10),
            static_cast<unsigned long long>(ctx->R11),
            static_cast<unsigned long long>(ctx->R12),
            static_cast<unsigned long long>(ctx->R13),
            static_cast<unsigned long long>(ctx->R14),
            static_cast<unsigned long long>(ctx->R15));
        WriteTextFileLine(file, line);
    }
#endif

    char exePath[MAX_PATH] = {};
    GetModuleFileNameA(nullptr, exePath, MAX_PATH);
    _snprintf_s(line, sizeof(line), _TRUNCATE, "process=%s\r\n", exePath);
    WriteTextFileLine(file, line);

    if (g_module) {
        char dllPath[MAX_PATH] = {};
        GetModuleFileNameA(g_module, dllPath, MAX_PATH);
        _snprintf_s(line, sizeof(line), _TRUNCATE, "nightsharp_module=%s\r\nnightsharp_base=%p\r\n", dllPath, g_module);
        WriteTextFileLine(file, line);
    }

    HANDLE snapshot = CreateToolhelp32Snapshot(
        TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32,
        GetCurrentProcessId());
    if (snapshot != INVALID_HANDLE_VALUE) {
        MODULEENTRY32 entry = {};
        entry.dwSize = sizeof(entry);
        int count = 0;
        if (Module32First(snapshot, &entry)) {
            do {
                _snprintf_s(
                    line,
                    sizeof(line),
                    _TRUNCATE,
                    "module[%03d]=base:%p size:0x%lx name:%s path:%s\r\n",
                    count,
                    entry.modBaseAddr,
                    static_cast<unsigned long>(entry.modBaseSize),
                    entry.szModule,
                    entry.szExePath);
                WriteTextFileLine(file, line);
                ++count;
            } while (Module32Next(snapshot, &entry));
        }
        CloseHandle(snapshot);
    }

    CloseHandle(file);
}

inline bool WriteMiniDump(EXCEPTION_POINTERS* exceptionPointers, const char* stage) {
    if (InterlockedCompareExchange(&g_dumping, 1, 0) != 0) {
        NightSharpDebug::Logf("[CrashReporter] dump skipped: already dumping");
        return false;
    }

    bool wrote = false;
    char basePath[MAX_PATH] = {};
    char dumpPath[MAX_PATH] = {};
    char metaPath[MAX_PATH] = {};
    BuildDumpBasePath(stage, basePath, sizeof(basePath));
    BuildPathWithExtension(basePath, ".dmp", dumpPath, sizeof(dumpPath));
    BuildPathWithExtension(basePath, ".txt", metaPath, sizeof(metaPath));

    HANDLE file = CreateFileA(
        dumpPath,
        GENERIC_WRITE,
        FILE_SHARE_READ,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);

    if (file == INVALID_HANDLE_VALUE) {
        NightSharpDebug::Logf("[CrashReporter] CreateFile dump failed gle=%lu path=%s",
                              GetLastError(),
                              dumpPath);
        WriteCrashMetadata(metaPath, dumpPath, stage, exceptionPointers, false);
        WriteLatestCrashPointer(dumpPath, metaPath);
        InterlockedExchange(&g_dumping, 0);
        return false;
    }

    MINIDUMP_EXCEPTION_INFORMATION exceptionInfo = {};
    exceptionInfo.ThreadId = GetCurrentThreadId();
    exceptionInfo.ExceptionPointers = exceptionPointers;
    exceptionInfo.ClientPointers = FALSE;

    DWORD dumpTypeFlags =
        MiniDumpNormal |
        MiniDumpWithDataSegs |
        MiniDumpWithHandleData |
        MiniDumpWithIndirectlyReferencedMemory |
        MiniDumpWithThreadInfo |
        MiniDumpWithUnloadedModules |
        MiniDumpWithProcessThreadData |
        MiniDumpWithPrivateReadWriteMemory |
        MiniDumpIgnoreInaccessibleMemory |
        MiniDumpWithFullMemoryInfo |
        MiniDumpWithModuleHeaders;

#if NIGHTSHARP_FULL_MEMORY_CRASH_DUMP
    dumpTypeFlags |= MiniDumpWithFullMemory;
#endif

    const MINIDUMP_TYPE dumpType = static_cast<MINIDUMP_TYPE>(dumpTypeFlags);

    const BOOL ok = MiniDumpWriteDump(
        GetCurrentProcess(),
        GetCurrentProcessId(),
        file,
        dumpType,
        exceptionPointers ? &exceptionInfo : nullptr,
        nullptr,
        nullptr);

    if (ok) {
        wrote = true;
        NightSharpDebug::Logf("[CrashReporter] minidump=%s", dumpPath);
    } else {
        NightSharpDebug::Logf("[CrashReporter] MiniDumpWriteDump failed gle=%lu path=%s",
                              GetLastError(),
                              dumpPath);
    }

    CloseHandle(file);
    WriteCrashMetadata(metaPath, dumpPath, stage, exceptionPointers, wrote);
    WriteLatestCrashPointer(dumpPath, metaPath);
    InterlockedExchange(&g_dumping, 0);
    return wrote;
}

inline LONG LogAndDumpException(const char* stage, EXCEPTION_POINTERS* exceptionPointers) {
    NightSharpDebug::LogException(stage, exceptionPointers);

    if (exceptionPointers && exceptionPointers->ExceptionRecord &&
        IsSeriousException(exceptionPointers->ExceptionRecord->ExceptionCode)) {
        LogModuleSnapshot();
        WriteMiniDump(exceptionPointers, stage);
    }

    return EXCEPTION_EXECUTE_HANDLER;
}

inline LONG WINAPI UnhandledFilter(EXCEPTION_POINTERS* exceptionPointers) {
    LogAndDumpException("UnhandledExceptionFilter", exceptionPointers);
    if (g_previousFilter) {
        return g_previousFilter(exceptionPointers);
    }
    return EXCEPTION_EXECUTE_HANDLER;
}

inline LONG WINAPI VectoredHandler(EXCEPTION_POINTERS* exceptionPointers) {
    if (!exceptionPointers || !exceptionPointers->ExceptionRecord) {
        return EXCEPTION_CONTINUE_SEARCH;
    }

    const DWORD code = exceptionPointers->ExceptionRecord->ExceptionCode;
    if (!IsSeriousException(code)) {
        return EXCEPTION_CONTINUE_SEARCH;
    }

    static volatile LONG s_firstSerious = 0;
    if (InterlockedCompareExchange(&s_firstSerious, 1, 0) == 0) {
#if NIGHTSHARP_LOG_FIRST_CHANCE_EXCEPTIONS
        NightSharpDebug::Logf("[CrashReporter] first-chance serious exception observed");
#if NIGHTSHARP_DUMP_FIRST_CHANCE_EXCEPTIONS
        LogAndDumpException("VectoredExceptionHandler", exceptionPointers);
#else
        NightSharpDebug::LogException("FirstChance/VectoredExceptionHandler", exceptionPointers);
#endif
#endif
    }

    return EXCEPTION_CONTINUE_SEARCH;
}

inline DWORD WINAPI FilterGuardThread(LPVOID) {
    NightSharpDebug::Logf("[CrashReporter] filter guard started");
    while (InterlockedCompareExchange(&g_guardRunning, 0, 0) != 0) {
        auto previous = SetUnhandledExceptionFilter(&UnhandledFilter);
        if (previous && previous != &UnhandledFilter) {
            g_previousFilter = previous;
            NightSharpDebug::Logf("[CrashReporter] unhandled filter restored previous=%p",
                                  previous);
        }
        Sleep(1000);
    }
    NightSharpDebug::Logf("[CrashReporter] filter guard stopped");
    return 0;
}

inline void Install(HMODULE module) {
    g_module = module;
    if (InterlockedCompareExchange(&g_installed, 1, 0) != 0) {
        return;
    }

    g_vectoredHandler = AddVectoredExceptionHandler(1, &VectoredHandler);
    g_previousFilter = SetUnhandledExceptionFilter(&UnhandledFilter);
    InterlockedExchange(&g_guardRunning, 1);
    g_guardThread = CreateThread(nullptr, 0, &FilterGuardThread, nullptr, 0, nullptr);
    NightSharpDebug::Logf("[CrashReporter] installed module=%p veh=%p",
                          module,
                          g_vectoredHandler);
}

inline void Uninstall() {
    if (InterlockedCompareExchange(&g_installed, 0, 1) != 1) {
        return;
    }

    if (g_vectoredHandler) {
        RemoveVectoredExceptionHandler(g_vectoredHandler);
        g_vectoredHandler = nullptr;
    }

    InterlockedExchange(&g_guardRunning, 0);
    if (g_guardThread) {
        WaitForSingleObject(g_guardThread, 2500);
        CloseHandle(g_guardThread);
        g_guardThread = nullptr;
    }

    SetUnhandledExceptionFilter(g_previousFilter);
    g_previousFilter = nullptr;
    NightSharpDebug::Logf("[CrashReporter] uninstalled");
}

} // namespace NightSharpDebug::CrashReporter
