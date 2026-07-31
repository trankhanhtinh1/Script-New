#pragma once

#include <Windows.h>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "CrashBridge.h"

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
        NightSharpDebug::CrashBridge::EnqueueLog(text, lstrlenA(text));
        return;
    }

    DWORD written = 0;
    WriteFile(hFile, text, static_cast<DWORD>(lstrlenA(text)), &written, nullptr);
    CloseHandle(hFile);
    NightSharpDebug::CrashBridge::EnqueueLog(text, lstrlenA(text));
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
    NightSharpDebug::CrashBridge::PublishPhase(current);
    WriteRaw("[NightSharp] phase=");
    WriteRaw(current);
    WriteRaw("\r\n");
}

enum class Color {
    Default = 0,
    Red,
    Green,
    Yellow,
    Blue,
    Magenta,
    Cyan,
    White,
    BrightRed,
    BrightGreen,
    BrightYellow,
    BrightBlue,
    BrightMagenta,
    BrightCyan,
    BrightWhite,
    Gray
};

namespace ANSI {
    inline constexpr const char* Reset = "\033[0m";
    inline constexpr const char* Red = "\033[31m";
    inline constexpr const char* Green = "\033[32m";
    inline constexpr const char* Yellow = "\033[33m";
    inline constexpr const char* Blue = "\033[34m";
    inline constexpr const char* Magenta = "\033[35m";
    inline constexpr const char* Cyan = "\033[36m";
    inline constexpr const char* White = "\033[37m";
    inline constexpr const char* BrightRed = "\033[1;31m";
    inline constexpr const char* BrightGreen = "\033[1;32m";
    inline constexpr const char* BrightYellow = "\033[1;33m";
    inline constexpr const char* BrightBlue = "\033[1;34m";
    inline constexpr const char* BrightMagenta = "\033[1;35m";
    inline constexpr const char* BrightCyan = "\033[1;36m";
    inline constexpr const char* BrightWhite = "\033[1;37m";
    inline constexpr const char* Gray = "\033[90m";

    inline const char* Code(Color color) {
        switch (color) {
            case Color::Red:           return Red;
            case Color::Green:         return Green;
            case Color::Yellow:        return Yellow;
            case Color::Blue:          return Blue;
            case Color::Magenta:       return Magenta;
            case Color::Cyan:          return Cyan;
            case Color::White:         return White;
            case Color::BrightRed:     return BrightRed;
            case Color::BrightGreen:   return BrightGreen;
            case Color::BrightYellow:  return BrightYellow;
            case Color::BrightBlue:    return BrightBlue;
            case Color::BrightMagenta: return BrightMagenta;
            case Color::BrightCyan:    return BrightCyan;
            case Color::BrightWhite:   return BrightWhite;
            case Color::Gray:          return Gray;
            default:                   return Reset;
        }
    }
} // namespace ANSI

inline void ProcessColorTags(const char* input, char* output, size_t outputSize) {
    if (!input || !output || outputSize == 0) return;

    size_t inIdx = 0;
    size_t outIdx = 0;
    const size_t maxOut = outputSize - 1;

    auto appendStr = [&](const char* s) {
        while (*s && outIdx < maxOut) {
            output[outIdx++] = *s++;
        }
    };

    while (input[inIdx] && outIdx < maxOut) {
        if (input[inIdx] == '<') {
            const char* tagStart = input + inIdx;
            const char* tagEnd = strchr(tagStart, '>');
            if (tagEnd) {
                const size_t tagLen = tagEnd - tagStart + 1;
                const char* ansi = nullptr;

                if (_strnicmp(tagStart, "<red>", 5) == 0) ansi = "\033[31m";
                else if (_strnicmp(tagStart, "<green>", 7) == 0) ansi = "\033[32m";
                else if (_strnicmp(tagStart, "<yellow>", 8) == 0) ansi = "\033[33m";
                else if (_strnicmp(tagStart, "<blue>", 6) == 0) ansi = "\033[34m";
                else if (_strnicmp(tagStart, "<magenta>", 9) == 0) ansi = "\033[35m";
                else if (_strnicmp(tagStart, "<cyan>", 6) == 0) ansi = "\033[36m";
                else if (_strnicmp(tagStart, "<white>", 7) == 0) ansi = "\033[37m";
                else if (_strnicmp(tagStart, "<gray>", 6) == 0) ansi = "\033[90m";
                else if (_strnicmp(tagStart, "<b-red>", 7) == 0) ansi = "\033[1;31m";
                else if (_strnicmp(tagStart, "<b-green>", 9) == 0) ansi = "\033[1;32m";
                else if (_strnicmp(tagStart, "<b-yellow>", 10) == 0) ansi = "\033[1;33m";
                else if (_strnicmp(tagStart, "<b-blue>", 8) == 0) ansi = "\033[1;34m";
                else if (_strnicmp(tagStart, "<b-magenta>", 11) == 0) ansi = "\033[1;35m";
                else if (_strnicmp(tagStart, "<b-cyan>", 8) == 0) ansi = "\033[1;36m";
                else if (_strnicmp(tagStart, "<b-white>", 9) == 0) ansi = "\033[1;37m";
                else if (tagStart[1] == '/' || _strnicmp(tagStart, "<reset>", 7) == 0) ansi = "\033[0m";

                if (ansi) {
                    appendStr(ansi);
                    inIdx += tagLen;
                    continue;
                }
            }
        }
        output[outIdx++] = input[inIdx++];
    }
    output[outIdx] = '\0';
}

inline void Logf(const char* fmt, ...) {
    char rawBuf[1536] = {};

    SYSTEMTIME st = {};
    GetLocalTime(&st);
    int prefix = _snprintf_s(
        rawBuf,
        sizeof(rawBuf),
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
    int body = _vsnprintf_s(rawBuf + prefix, sizeof(rawBuf) - prefix, _TRUNCATE, fmt, args);
    va_end(args);
    if (body < 0) {
        body = 0;
    }

    char processedBuf[2048] = {};
    ProcessColorTags(rawBuf, processedBuf, sizeof(processedBuf));

    size_t total = strlen(processedBuf);
    if (total > sizeof(processedBuf) - 3) {
        total = sizeof(processedBuf) - 3;
    }

    processedBuf[total++] = '\r';
    processedBuf[total++] = '\n';
    processedBuf[total] = '\0';
    WriteRaw(processedBuf);
}

inline void LogColorf(Color color, const char* fmt, ...) {
    char buffer[1536] = {};

    SYSTEMTIME st = {};
    GetLocalTime(&st);
    int prefix = _snprintf_s(
        buffer,
        sizeof(buffer),
        _TRUNCATE,
        "[%02u:%02u:%02u.%03u T%u] %s",
        static_cast<unsigned>(st.wHour),
        static_cast<unsigned>(st.wMinute),
        static_cast<unsigned>(st.wSecond),
        static_cast<unsigned>(st.wMilliseconds),
        static_cast<unsigned>(GetCurrentThreadId()),
        ANSI::Code(color));
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
    if (total > static_cast<int>(sizeof(buffer)) - 8) {
        total = static_cast<int>(sizeof(buffer)) - 8;
    }

    const char* resetStr = ANSI::Reset;
    const int resetLen = 4;
    memcpy(buffer + total, resetStr, resetLen);
    total += resetLen;

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
    case 0xC0000374: return "HEAP_CORRUPTION";
    case 0xC0000409: return "STACK_BUFFER_OVERRUN_OR_FAIL_FAST";
    case 0xC0000602: return "FAIL_FAST_EXCEPTION";
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
