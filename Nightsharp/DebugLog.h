#pragma once

#include <Windows.h>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace NightSharpDebug {

inline constexpr const char* kCrashLogPath =
    "C:\\Users\\Public\\nightsharp_crash.txt";

inline volatile LONG g_phaseLock = 0;
inline char g_phase[128] = "dll-load";

inline void WriteRaw(const char* text) {
    if (!text || !*text) {
        return;
    }

    OutputDebugStringA(text);

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
