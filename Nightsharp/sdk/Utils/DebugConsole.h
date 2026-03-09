#pragma once

#include <Windows.h>
#include <cstdarg>
#include <cstdio>
#include <fstream>
#include <mutex>
#include <string>

namespace DebugConsole {

    inline bool enabled = true;
    inline bool writeToConsole = false;
    inline bool writeToFile = true;
    inline bool writeToDebugger = true;

    inline std::ofstream logFile;
    inline std::mutex logMutex;
    inline HANDLE consoleHandle = INVALID_HANDLE_VALUE;
    inline bool consoleAllocated = false;
    inline bool initialized = false;
    inline unsigned long long messageCount = 0;
    inline std::string lastPayload;
    inline unsigned int repeatCount = 0;

    inline std::string GetModuleDirectory() {
        char path[MAX_PATH] = {};
        HMODULE module = nullptr;
        if (!GetModuleHandleExA(
                GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                reinterpret_cast<LPCSTR>(&GetModuleDirectory),
                &module)) {
            return ".";
        }

        if (!GetModuleFileNameA(module, path, MAX_PATH)) {
            return ".";
        }

        std::string result(path);
        const size_t slash = result.find_last_of("\\/");
        return slash == std::string::npos ? "." : result.substr(0, slash);
    }

    inline void WriteLineLocked(const std::string& line) {
        const std::string withNewline = line + "\r\n";

        if (writeToDebugger) {
            OutputDebugStringA(withNewline.c_str());
        }

        if (writeToConsole && consoleHandle != INVALID_HANDLE_VALUE) {
            DWORD written = 0;
            if (!WriteConsoleA(consoleHandle,
                               withNewline.c_str(),
                               static_cast<DWORD>(withNewline.size()),
                               &written,
                               nullptr)) {
                consoleHandle = INVALID_HANDLE_VALUE;
            }
        }

        if (writeToFile && logFile.is_open()) {
            logFile << withNewline;
            logFile.flush();
        }
    }

    inline void FlushRepeatLocked() {
        if (repeatCount == 0 || lastPayload.empty()) {
            return;
        }

        char summary[256] = {};
        snprintf(summary, sizeof(summary),
                 "[repeat x%u] %s",
                 repeatCount + 1,
                 lastPayload.c_str());
        WriteLineLocked(summary);
        repeatCount = 0;
    }

    inline void EnsureInitializedLocked() {
        if (initialized) {
            return;
        }

        if (writeToConsole) {
            if (!AttachConsole(ATTACH_PARENT_PROCESS)) {
                if (AllocConsole()) {
                    consoleAllocated = true;
                }
            }
            consoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);
            if (consoleHandle == nullptr || consoleHandle == INVALID_HANDLE_VALUE) {
                consoleHandle = INVALID_HANDLE_VALUE;
            }
        }

        if (writeToFile) {
            const std::string logDir = GetModuleDirectory() + "\\logs";
            CreateDirectoryA(logDir.c_str(), nullptr);
            logFile.open(logDir + "\\debug_console.txt", std::ios::out | std::ios::trunc);
        }

        initialized = true;
        WriteLineLocked("[DEBUG] Console initialized");
    }

    inline void Init() {
        std::lock_guard<std::mutex> lock(logMutex);
        EnsureInitializedLocked();
    }

    inline void LogV(const char* format, va_list args) {
        if (!enabled || format == nullptr) {
            return;
        }

        char buffer[1024] = {};
        vsnprintf(buffer, sizeof(buffer), format, args);

        std::lock_guard<std::mutex> lock(logMutex);
        EnsureInitializedLocked();

        const std::string payload(buffer);
        if (payload == lastPayload) {
            ++repeatCount;
            return;
        }

        FlushRepeatLocked();
        lastPayload = payload;

        char line[1200] = {};
        snprintf(line, sizeof(line), "[%llu] %s", messageCount++, payload.c_str());
        WriteLineLocked(line);
    }

    inline void Log(const char* format, ...) {
        va_list args;
        va_start(args, format);
        LogV(format, args);
        va_end(args);
    }

    inline void LogTagged(const char* tag, const char* format, ...) {
        if (!enabled || format == nullptr) {
            return;
        }

        char payload[1024] = {};
        va_list args;
        va_start(args, format);
        vsnprintf(payload, sizeof(payload), format, args);
        va_end(args);

        if (tag != nullptr && tag[0] != '\0') {
            Log("[%s] %s", tag, payload);
            return;
        }

        Log("%s", payload);
    }

    inline void Close() {
        std::lock_guard<std::mutex> lock(logMutex);
        if (!initialized) {
            return;
        }

        FlushRepeatLocked();
        WriteLineLocked("[DEBUG] Console closed");

        if (logFile.is_open()) {
            logFile.close();
        }

        if (consoleAllocated) {
            FreeConsole();
            consoleAllocated = false;
        }

        consoleHandle = INVALID_HANDLE_VALUE;
        initialized = false;
        lastPayload.clear();
        repeatCount = 0;
    }
}

#ifndef DEBUG_LOG
#define DEBUG_LOG(...) ::DebugConsole::Log(__VA_ARGS__)
#endif

#ifndef DEBUG_LOG_TAG
#define DEBUG_LOG_TAG(tag, ...) ::DebugConsole::LogTagged(tag, __VA_ARGS__)
#endif
