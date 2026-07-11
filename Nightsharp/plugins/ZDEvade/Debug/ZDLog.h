#pragma once

#include <Windows.h>
#include <cstdarg>
#include <cstring>

namespace ZDEvade {

inline constexpr const char* kZDEvadeLogPath = "C:\\Users\\Public\\ZDEvade.txt";

inline void ZDLog(const char* fmt, ...) {
    char buffer[1024] = {};
    va_list args;
    va_start(args, fmt);
    _vsnprintf_s(buffer, sizeof(buffer), _TRUNCATE, fmt, args);
    va_end(args);
    for (char* p = buffer; *p; ++p) {
        const unsigned char ch = static_cast<unsigned char>(*p);
        if ((ch < 32 || ch > 126) && ch != '\t') *p = '?';
    }

    HANDLE hFile = CreateFileA(
        kZDEvadeLogPath, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return;
    DWORD written = 0;
    WriteFile(hFile, buffer, static_cast<DWORD>(lstrlenA(buffer)), &written, nullptr);
    WriteFile(hFile, "\r\n", 2, &written, nullptr);
    CloseHandle(hFile);
}

} // namespace ZDEvade
