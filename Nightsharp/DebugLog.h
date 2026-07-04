#pragma once

#include <Windows.h>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>

#ifndef NIGHTSHARP_DEBUG_FILE_LOG
#define NIGHTSHARP_DEBUG_FILE_LOG 1
#endif

#ifndef NIGHTSHARP_DEBUG_EXTERNAL_CONSOLE
#define NIGHTSHARP_DEBUG_EXTERNAL_CONSOLE 1
#endif

#ifndef NIGHTSHARP_DEBUG_EXTERNAL_CONSOLE_AUTOSTART
#define NIGHTSHARP_DEBUG_EXTERNAL_CONSOLE_AUTOSTART 1
#endif

#ifndef NIGHTSHARP_DEBUG_INTERNAL_CONSOLE
#define NIGHTSHARP_DEBUG_INTERNAL_CONSOLE 1
#endif

namespace NightSharpDebug {

inline constexpr const char* kCrashLogPath =
    "C:\\Users\\Public\\nightsharp_crash.txt";
inline constexpr const char* kDebugConsolePipeName =
    R"(\\.\pipe\NightSharpDebugConsole)";
inline constexpr const char* kDebugConsoleExeName = "console.exe";
inline constexpr DWORD kDebugConsoleConnectRetryMs = 250;

inline volatile LONG g_phaseLock = 0;
inline char g_phase[128] = "dll-load";
inline volatile LONG g_consolePipeLock = 0;
inline volatile LONG g_consoleLaunchAttempted = 0;
inline DWORD g_lastConsoleConnectAttempt = 0;
inline HANDLE g_consolePipe = INVALID_HANDLE_VALUE;

#if NIGHTSHARP_DEBUG_INTERNAL_CONSOLE
inline constexpr int kInternalConsoleMaxLines = 512;
inline constexpr int kInternalConsoleLineSize = 384;
inline volatile LONG g_internalConsoleLock = 0;
inline char g_internalConsoleLines[kInternalConsoleMaxLines][kInternalConsoleLineSize] = {};
inline char g_internalConsoleCurrent[kInternalConsoleLineSize] = {};
inline int g_internalConsoleCurrentLen = 0;
inline int g_internalConsoleStart = 0;
inline int g_internalConsoleCount = 0;
inline unsigned g_internalConsoleDropped = 0;

inline void LockInternalConsole() {
    while (InterlockedCompareExchange(&g_internalConsoleLock, 1, 0) != 0) {
        Sleep(0);
    }
}

inline void UnlockInternalConsole() {
    InterlockedExchange(&g_internalConsoleLock, 0);
}

inline void CommitInternalConsoleLineLocked() {
    if (g_internalConsoleCurrentLen <= 0) {
        return;
    }

    g_internalConsoleCurrent[g_internalConsoleCurrentLen] = '\0';

    int index = 0;
    if (g_internalConsoleCount < kInternalConsoleMaxLines) {
        index = (g_internalConsoleStart + g_internalConsoleCount) % kInternalConsoleMaxLines;
        ++g_internalConsoleCount;
    } else {
        index = g_internalConsoleStart;
        g_internalConsoleStart = (g_internalConsoleStart + 1) % kInternalConsoleMaxLines;
        ++g_internalConsoleDropped;
    }

    lstrcpynA(
        g_internalConsoleLines[index],
        g_internalConsoleCurrent,
        kInternalConsoleLineSize);
    g_internalConsoleCurrentLen = 0;
    g_internalConsoleCurrent[0] = '\0';
}

inline void AppendInternalConsoleRaw(const char* text) {
    if (!text || !*text) {
        return;
    }

    LockInternalConsole();
    for (const char* p = text; *p; ++p) {
        const char ch = *p;
        if (ch == '\r') {
            continue;
        }
        if (ch == '\n') {
            CommitInternalConsoleLineLocked();
            continue;
        }

        if (g_internalConsoleCurrentLen >= kInternalConsoleLineSize - 2) {
            CommitInternalConsoleLineLocked();
        }

        g_internalConsoleCurrent[g_internalConsoleCurrentLen++] = ch;
        g_internalConsoleCurrent[g_internalConsoleCurrentLen] = '\0';
    }
    UnlockInternalConsole();
}

inline void ClearInternalConsole() {
    LockInternalConsole();
    for (int i = 0; i < kInternalConsoleMaxLines; ++i) {
        g_internalConsoleLines[i][0] = '\0';
    }
    g_internalConsoleCurrent[0] = '\0';
    g_internalConsoleCurrentLen = 0;
    g_internalConsoleStart = 0;
    g_internalConsoleCount = 0;
    g_internalConsoleDropped = 0;
    UnlockInternalConsole();
}

inline int InternalConsoleLineCountUnsafe() {
    return g_internalConsoleCount;
}

inline unsigned InternalConsoleDroppedUnsafe() {
    return g_internalConsoleDropped;
}

inline const char* InternalConsoleLineUnsafe(int index) {
    if (index < 0 || index >= g_internalConsoleCount) {
        return "";
    }
    const int physical = (g_internalConsoleStart + index) % kInternalConsoleMaxLines;
    return g_internalConsoleLines[physical];
}
#else
inline void AppendInternalConsoleRaw(const char*) {}
inline void ClearInternalConsole() {}
inline void LockInternalConsole() {}
inline void UnlockInternalConsole() {}
inline int InternalConsoleLineCountUnsafe() { return 0; }
inline unsigned InternalConsoleDroppedUnsafe() { return 0; }
inline const char* InternalConsoleLineUnsafe(int) { return ""; }
#endif

inline void CloseExternalConsolePipe() {
#if NIGHTSHARP_DEBUG_EXTERNAL_CONSOLE
    HANDLE pipe = g_consolePipe;
    if (pipe != INVALID_HANDLE_VALUE) {
        g_consolePipe = INVALID_HANDLE_VALUE;
        CloseHandle(pipe);
    }
#endif
}

inline bool TryConnectExternalConsole() {
#if NIGHTSHARP_DEBUG_EXTERNAL_CONSOLE
    if (g_consolePipe != INVALID_HANDLE_VALUE) {
        return true;
    }

    const DWORD now = GetTickCount();
    if (g_lastConsoleConnectAttempt != 0 &&
        now - g_lastConsoleConnectAttempt < kDebugConsoleConnectRetryMs) {
        return false;
    }
    g_lastConsoleConnectAttempt = now;

    if (!WaitNamedPipeA(kDebugConsolePipeName, 0)) {
        return false;
    }

    HANDLE pipe = CreateFileA(
        kDebugConsolePipeName,
        GENERIC_WRITE,
        0,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (pipe == INVALID_HANDLE_VALUE) {
        return false;
    }

    DWORD mode = PIPE_READMODE_BYTE;
    SetNamedPipeHandleState(pipe, &mode, nullptr, nullptr);
    g_consolePipe = pipe;
    return true;
#else
    return false;
#endif
}

inline void WriteExternalConsoleRaw(const char* text, DWORD length) {
#if NIGHTSHARP_DEBUG_EXTERNAL_CONSOLE
    if (!text || length == 0) {
        return;
    }

    if (InterlockedCompareExchange(&g_consolePipeLock, 1, 0) != 0) {
        return;
    }

    if (TryConnectExternalConsole()) {
        DWORD written = 0;
        if (!WriteFile(g_consolePipe, text, length, &written, nullptr)) {
            CloseExternalConsolePipe();
        }
    }

    InterlockedExchange(&g_consolePipeLock, 0);
#else
    (void)text;
    (void)length;
#endif
}

inline bool BuildSiblingConsolePath(HMODULE module, char* out, DWORD outSize) {
    if (!out || outSize == 0) {
        return false;
    }
    out[0] = '\0';

    DWORD len = 0;
    if (module) {
        len = GetModuleFileNameA(module, out, outSize);
    }
    if (len == 0 || len >= outSize) {
        len = GetModuleFileNameA(nullptr, out, outSize);
    }
    if (len == 0 || len >= outSize) {
        out[0] = '\0';
        return false;
    }

    char* slash = nullptr;
    for (char* p = out; *p; ++p) {
        if (*p == '\\' || *p == '/') {
            slash = p;
        }
    }
    if (!slash) {
        return false;
    }

    slash[1] = '\0';
    strcat_s(out, outSize, kDebugConsoleExeName);
    return true;
}

inline void StartExternalConsole(HMODULE module) {
#if NIGHTSHARP_DEBUG_EXTERNAL_CONSOLE && NIGHTSHARP_DEBUG_EXTERNAL_CONSOLE_AUTOSTART
    if (InterlockedCompareExchange(&g_consoleLaunchAttempted, 1, 0) != 0) {
        return;
    }

    if (WaitNamedPipeA(kDebugConsolePipeName, 0)) {
        return;
    }

    char path[MAX_PATH] = {};
    if (!BuildSiblingConsolePath(module, path, MAX_PATH)) {
        return;
    }

    if (GetFileAttributesA(path) == INVALID_FILE_ATTRIBUTES) {
        return;
    }

    char workDir[MAX_PATH] = {};
    lstrcpynA(workDir, path, MAX_PATH);
    char* slash = nullptr;
    for (char* p = workDir; *p; ++p) {
        if (*p == '\\' || *p == '/') {
            slash = p;
        }
    }
    if (slash) {
        *slash = '\0';
    }

    char commandLine[MAX_PATH + 4] = {};
    _snprintf_s(commandLine, sizeof(commandLine), _TRUNCATE, "\"%s\"", path);

    STARTUPINFOA si = {};
    PROCESS_INFORMATION pi = {};
    si.cb = sizeof(si);

    if (CreateProcessA(
            path,
            commandLine,
            nullptr,
            nullptr,
            FALSE,
            CREATE_NEW_CONSOLE,
            nullptr,
            workDir[0] ? workDir : nullptr,
            &si,
            &pi)) {
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        g_lastConsoleConnectAttempt = 0;
    }
#else
    (void)module;
#endif
}

inline void ResetFileLog() {
#if NIGHTSHARP_DEBUG_FILE_LOG
    HANDLE hFile = CreateFileA(
        kCrashLogPath,
        GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (hFile != INVALID_HANDLE_VALUE) {
        CloseHandle(hFile);
    }
#endif
}

inline void WriteRaw(const char* text) {
    if (!text || !*text) {
        return;
    }

    AppendInternalConsoleRaw(text);
    OutputDebugStringA(text);

#if NIGHTSHARP_DEBUG_EXTERNAL_CONSOLE
    WriteExternalConsoleRaw(text, static_cast<DWORD>(lstrlenA(text)));
#endif

#if NIGHTSHARP_DEBUG_FILE_LOG
    HANDLE hFile = CreateFileA(
        kCrashLogPath,
        FILE_APPEND_DATA,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        return;
    }

    DWORD written = 0;
    WriteFile(hFile, text, static_cast<DWORD>(lstrlenA(text)), &written, nullptr);
    CloseHandle(hFile);
#endif
}

inline const char* BaseName(const char* path) {
    if (!path || !*path) {
        return "";
    }

    const char* base = path;
    for (const char* p = path; *p; ++p) {
        if (*p == '\\' || *p == '/') {
            base = p + 1;
        }
    }
    return base;
}

inline void SetPhase(const char* phase) {
    if (InterlockedCompareExchange(&g_phaseLock, 1, 0) != 0) {
        return;
    }

    if (!phase || !*phase) {
        lstrcpynA(g_phase, "unknown", static_cast<int>(sizeof(g_phase)));
    } else {
        lstrcpynA(g_phase, phase, static_cast<int>(sizeof(g_phase)));
    }

    InterlockedExchange(&g_phaseLock, 0);
}

inline void GetPhase(char* out, size_t outSize) {
    if (!out || outSize == 0) {
        return;
    }

    if (InterlockedCompareExchange(&g_phaseLock, 1, 0) != 0) {
        lstrcpynA(out, "phase-busy", static_cast<int>(outSize));
        return;
    }

    lstrcpynA(out, g_phase, static_cast<int>(outSize));
    InterlockedExchange(&g_phaseLock, 0);
}

inline void Phase(const char* phase) {
    SetPhase(phase);
    char current[128] = {};
    GetPhase(current, sizeof(current));
    WriteRaw("[NightSharp] phase=");
    WriteRaw(current);
    WriteRaw("\r\n");
}

inline void Logf(const char* fmt, ...) {
    char buffer[1536] = {};

    SYSTEMTIME st = {};
    GetLocalTime(&st);
    int prefix = _snprintf_s(
        buffer,
        sizeof(buffer),
        _TRUNCATE,
        "[%02u:%02u:%02u.%03u T%u] ",
        static_cast<unsigned>(st.wHour),
        static_cast<unsigned>(st.wMinute),
        static_cast<unsigned>(st.wSecond),
        static_cast<unsigned>(st.wMilliseconds),
        static_cast<unsigned>(GetCurrentThreadId()));
    if (prefix < 0) {
        prefix = 0;
    }

    va_list args;
    va_start(args, fmt);
    int body = _vsnprintf_s(buffer + prefix, sizeof(buffer) - prefix, _TRUNCATE, fmt, args);
    va_end(args);
    if (body < 0) {
        body = 0;
    }

    int total = prefix + body;
    if (total > static_cast<int>(sizeof(buffer)) - 3) {
        total = static_cast<int>(sizeof(buffer)) - 3;
    }

    buffer[total++] = '\r';
    buffer[total++] = '\n';
    buffer[total] = '\0';
    WriteRaw(buffer);
}

inline void DescribeAddress(void* address, char* out, size_t outSize) {
    if (!out || outSize == 0) {
        return;
    }
    out[0] = '\0';

    if (!address) {
        lstrcpynA(out, "null", static_cast<int>(outSize));
        return;
    }

    MEMORY_BASIC_INFORMATION mbi = {};
    if (!VirtualQuery(address, &mbi, sizeof(mbi))) {
        _snprintf_s(out, outSize, _TRUNCATE, "%p (VirtualQuery failed gle=%lu)",
                    address, GetLastError());
        return;
    }

    char modulePath[MAX_PATH] = {};
    const HMODULE module = reinterpret_cast<HMODULE>(mbi.AllocationBase);
    if (module && GetModuleFileNameA(module, modulePath, MAX_PATH)) {
        const auto base = reinterpret_cast<uintptr_t>(module);
        const auto at = reinterpret_cast<uintptr_t>(address);
        _snprintf_s(
            out,
            outSize,
            _TRUNCATE,
            "%p %s+0x%llX",
            address,
            BaseName(modulePath),
            static_cast<unsigned long long>(at - base));
        return;
    }

    _snprintf_s(
        out,
        outSize,
        _TRUNCATE,
        "%p allocBase=%p protect=0x%lx state=0x%lx type=0x%lx",
        address,
        mbi.AllocationBase,
        static_cast<unsigned long>(mbi.Protect),
        static_cast<unsigned long>(mbi.State),
        static_cast<unsigned long>(mbi.Type));
}

inline const char* ExceptionName(DWORD code) {
    switch (code) {
    case EXCEPTION_ACCESS_VIOLATION: return "ACCESS_VIOLATION";
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED: return "ARRAY_BOUNDS_EXCEEDED";
    case EXCEPTION_BREAKPOINT: return "BREAKPOINT";
    case EXCEPTION_DATATYPE_MISALIGNMENT: return "DATATYPE_MISALIGNMENT";
    case EXCEPTION_FLT_DENORMAL_OPERAND: return "FLT_DENORMAL_OPERAND";
    case EXCEPTION_FLT_DIVIDE_BY_ZERO: return "FLT_DIVIDE_BY_ZERO";
    case EXCEPTION_FLT_INEXACT_RESULT: return "FLT_INEXACT_RESULT";
    case EXCEPTION_FLT_INVALID_OPERATION: return "FLT_INVALID_OPERATION";
    case EXCEPTION_FLT_OVERFLOW: return "FLT_OVERFLOW";
    case EXCEPTION_FLT_STACK_CHECK: return "FLT_STACK_CHECK";
    case EXCEPTION_FLT_UNDERFLOW: return "FLT_UNDERFLOW";
    case EXCEPTION_ILLEGAL_INSTRUCTION: return "ILLEGAL_INSTRUCTION";
    case EXCEPTION_IN_PAGE_ERROR: return "IN_PAGE_ERROR";
    case EXCEPTION_INT_DIVIDE_BY_ZERO: return "INT_DIVIDE_BY_ZERO";
    case EXCEPTION_INT_OVERFLOW: return "INT_OVERFLOW";
    case EXCEPTION_INVALID_DISPOSITION: return "INVALID_DISPOSITION";
    case EXCEPTION_NONCONTINUABLE_EXCEPTION: return "NONCONTINUABLE_EXCEPTION";
    case EXCEPTION_PRIV_INSTRUCTION: return "PRIV_INSTRUCTION";
    case EXCEPTION_STACK_OVERFLOW: return "STACK_OVERFLOW";
    default: return "UNKNOWN";
    }
}

inline LONG LogException(const char* stage, EXCEPTION_POINTERS* exceptionPointers) {
    if (!exceptionPointers || !exceptionPointers->ExceptionRecord) {
        Logf("[Crash] %s exception: missing exception pointers", stage ? stage : "?");
        return EXCEPTION_EXECUTE_HANDLER;
    }

    const auto* record = exceptionPointers->ExceptionRecord;
    const auto code = record->ExceptionCode;
    const auto* ctx = exceptionPointers->ContextRecord;
    char phase[128] = {};
    char exceptionAddress[256] = {};
    GetPhase(phase, sizeof(phase));
    DescribeAddress(record->ExceptionAddress, exceptionAddress, sizeof(exceptionAddress));

    Logf(
        "[Crash] stage=%s phase=%s code=0x%08lX (%s) address=%s flags=0x%08lX params=%lu",
        stage ? stage : "?",
        phase,
        static_cast<unsigned long>(code),
        ExceptionName(code),
        exceptionAddress,
        static_cast<unsigned long>(record->ExceptionFlags),
        static_cast<unsigned long>(record->NumberParameters));

    if (record->NumberParameters > 0) {
        Logf(
            "[Crash] param0=0x%llX param1=0x%llX param2=0x%llX",
            record->NumberParameters > 0 ? static_cast<unsigned long long>(record->ExceptionInformation[0]) : 0ULL,
            record->NumberParameters > 1 ? static_cast<unsigned long long>(record->ExceptionInformation[1]) : 0ULL,
            record->NumberParameters > 2 ? static_cast<unsigned long long>(record->ExceptionInformation[2]) : 0ULL);
    }

#if defined(_M_X64)
    if (ctx) {
        char ripAddress[256] = {};
        DescribeAddress(reinterpret_cast<void*>(ctx->Rip), ripAddress, sizeof(ripAddress));
        Logf(
            "[Crash] RIP=0x%llX RSP=0x%llX RBP=0x%llX RAX=0x%llX RBX=0x%llX RCX=0x%llX RDX=0x%llX R8=0x%llX R9=0x%llX",
            static_cast<unsigned long long>(ctx->Rip),
            static_cast<unsigned long long>(ctx->Rsp),
            static_cast<unsigned long long>(ctx->Rbp),
            static_cast<unsigned long long>(ctx->Rax),
            static_cast<unsigned long long>(ctx->Rbx),
            static_cast<unsigned long long>(ctx->Rcx),
            static_cast<unsigned long long>(ctx->Rdx),
            static_cast<unsigned long long>(ctx->R8),
            static_cast<unsigned long long>(ctx->R9));
        Logf("[Crash] RIP module=%s", ripAddress);
    }
#endif

    return EXCEPTION_EXECUTE_HANDLER;
}

} // namespace NightSharpDebug
