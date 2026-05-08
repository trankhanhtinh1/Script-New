#pragma once

#include <Windows.h>

#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace NightSharpDebug {

inline constexpr const char* kPipeName = R"(\\.\pipe\NightSharpDebugConsole)";

inline HANDLE& PipeHandle() {
    static HANDLE handle = INVALID_HANDLE_VALUE;
    return handle;
}

inline DWORD& LastConnectAttempt() {
    static DWORD tick = 0;
    return tick;
}

inline void ClosePipe() {
    HANDLE& handle = PipeHandle();
    if (handle != INVALID_HANDLE_VALUE) {
        CloseHandle(handle);
        handle = INVALID_HANDLE_VALUE;
    }
}

inline bool TryConnect() {
    HANDLE& handle = PipeHandle();
    if (handle != INVALID_HANDLE_VALUE) {
        return true;
    }

    const DWORD now = GetTickCount();
    DWORD& lastAttempt = LastConnectAttempt();
    if (lastAttempt != 0 && now - lastAttempt < 1000) {
        return false;
    }
    lastAttempt = now;

    if (!WaitNamedPipeA(kPipeName, 0)) {
        return false;
    }

    handle = CreateFileA(
        kPipeName,
        GENERIC_WRITE,
        0,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);

    if (handle == INVALID_HANDLE_VALUE) {
        return false;
    }

    DWORD mode = PIPE_READMODE_BYTE;
    SetNamedPipeHandleState(handle, &mode, nullptr, nullptr);
    return true;
}

inline void WriteRaw(const char* text, size_t length) {
    if (!text || length == 0 || !TryConnect()) {
        return;
    }

    DWORD written = 0;
    if (!WriteFile(PipeHandle(), text, static_cast<DWORD>(length), &written, nullptr)) {
        ClosePipe();
    }
}

inline void Log(const char* text) {
    if (!text || !*text) {
        return;
    }

    OutputDebugStringA(text);
    OutputDebugStringA("\n");
    WriteRaw(text, std::strlen(text));
    WriteRaw("\n", 1);
}

inline void Logf(const char* format, ...) {
    if (!format || !*format) {
        return;
    }

    char buffer[2048] = {};
    va_list args;
    va_start(args, format);
    std::vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    buffer[sizeof(buffer) - 1] = 0;

    Log(buffer);
}

} // namespace NightSharpDebug
