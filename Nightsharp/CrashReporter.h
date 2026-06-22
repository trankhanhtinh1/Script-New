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

inline void BuildDumpPath(const char* stage, char* out, size_t outSize) {
    if (!out || outSize == 0) {
        return;
    }

    SYSTEMTIME st = {};
    GetLocalTime(&st);

    char safeStage[80] = {};
    SanitizeForFileName(stage, safeStage, sizeof(safeStage));

    const LONG serial = InterlockedIncrement(&g_dumpSerial);
    _snprintf_s(
        out,
        outSize,
        _TRUNCATE,
        "C:\\Users\\Public\\nightsharp_crash_%04u%02u%02u_%02u%02u%02u_%lu_%lu_%ld_%s.dmp",
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

inline bool WriteMiniDump(EXCEPTION_POINTERS* exceptionPointers, const char* stage) {
    if (InterlockedCompareExchange(&g_dumping, 1, 0) != 0) {
        NightSharpDebug::Logf("[CrashReporter] dump skipped: already dumping");
        return false;
    }

    bool wrote = false;
    char path[MAX_PATH] = {};
    BuildDumpPath(stage, path, sizeof(path));

    HANDLE file = CreateFileA(
        path,
        GENERIC_WRITE,
        FILE_SHARE_READ,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);

    if (file == INVALID_HANDLE_VALUE) {
        NightSharpDebug::Logf("[CrashReporter] CreateFile dump failed gle=%lu path=%s",
                              GetLastError(),
                              path);
        InterlockedExchange(&g_dumping, 0);
        return false;
    }

    MINIDUMP_EXCEPTION_INFORMATION exceptionInfo = {};
    exceptionInfo.ThreadId = GetCurrentThreadId();
    exceptionInfo.ExceptionPointers = exceptionPointers;
    exceptionInfo.ClientPointers = FALSE;

    const MINIDUMP_TYPE dumpType = static_cast<MINIDUMP_TYPE>(
        MiniDumpNormal |
        MiniDumpWithDataSegs |
        MiniDumpWithHandleData |
        MiniDumpWithIndirectlyReferencedMemory |
        MiniDumpWithThreadInfo |
        MiniDumpWithUnloadedModules);

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
        NightSharpDebug::Logf("[CrashReporter] minidump=%s", path);
    } else {
        NightSharpDebug::Logf("[CrashReporter] MiniDumpWriteDump failed gle=%lu path=%s",
                              GetLastError(),
                              path);
    }

    CloseHandle(file);
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

inline void Install(HMODULE module) {
    g_module = module;
    if (InterlockedCompareExchange(&g_installed, 1, 0) != 0) {
        return;
    }

    g_vectoredHandler = AddVectoredExceptionHandler(1, &VectoredHandler);
    g_previousFilter = SetUnhandledExceptionFilter(&UnhandledFilter);
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

    SetUnhandledExceptionFilter(g_previousFilter);
    g_previousFilter = nullptr;
    NightSharpDebug::Logf("[CrashReporter] uninstalled");
}

} // namespace NightSharpDebug::CrashReporter
