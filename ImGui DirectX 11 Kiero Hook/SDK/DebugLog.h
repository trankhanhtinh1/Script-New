#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <cstdint>

namespace SDK {
namespace Debug {

inline bool g_debugEnabled = true;
inline char g_sdkDebugPath[MAX_PATH] = {0};
inline bool g_sdkDebugInit = false;
inline int g_logCount = 0;
inline const int MAX_LOG_COUNT = 5000;

inline void InitPath() {
    if (g_sdkDebugInit) return;
    HMODULE hMod = nullptr;
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCSTR)InitPath, &hMod)) {
        if (GetModuleFileNameA(hMod, g_sdkDebugPath, MAX_PATH)) {
            char* lastSlash = g_sdkDebugPath;
            for (char* p = g_sdkDebugPath; *p; p++) {
                if (*p == '\\' || *p == '/') lastSlash = p;
            }
            if (lastSlash != g_sdkDebugPath) {
                *(lastSlash + 1) = 0;
                lstrcatA(g_sdkDebugPath, "debug.txt");
            }
        }
    }
    if (!g_sdkDebugPath[0]) {
        lstrcpyA(g_sdkDebugPath, "C:\\debug.txt");
    }
    g_sdkDebugInit = true;
}

inline void Log(const char* msg) {
    if (!g_debugEnabled) return;
    InitPath();
    if (g_logCount >= MAX_LOG_COUNT && msg[0] != '!') return;
    g_logCount++;
    HANDLE hFile = CreateFileA(g_sdkDebugPath, FILE_APPEND_DATA, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        DWORD written;
        SYSTEMTIME st;
        GetLocalTime(&st);
        char timeBuf[64];
        wsprintfA(timeBuf, "[%02d:%02d:%02d.%03d] ", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
        WriteFile(hFile, timeBuf, lstrlenA(timeBuf), &written, NULL);
        WriteFile(hFile, msg, lstrlenA(msg), &written, NULL);
        WriteFile(hFile, "\r\n", 2, &written, NULL);
        CloseHandle(hFile);
    }
}

inline void LogPtr(const char* name, void* ptr) {
    char buf[256];
    wsprintfA(buf, "%s: %p", name, ptr);
    Log(buf);
}

inline void LogHex(const char* name, uint64_t val) {
    char buf[256];
    wsprintfA(buf, "%s: 0x%I64X", name, val);
    Log(buf);
}

inline void LogInt(const char* name, int val) {
    char buf[256];
    wsprintfA(buf, "%s: %d", name, val);
    Log(buf);
}

inline void LogOffset(const char* name, uint64_t base, uint64_t offset, uint64_t value) {
    char buf[256];
    wsprintfA(buf, "%s: 0x%I64X + 0x%I64X = 0x%I64X", name, base, offset, value);
    Log(buf);
}

} // namespace Debug
} // namespace SDK
