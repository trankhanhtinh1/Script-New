#pragma once

#include "../Core/Constants.h"
#include "../Enumerations/LogLevel.h"

#include <Windows.h>

#include <cstdio>
#include <fstream>
#include <string>

namespace SDK::Utils {

namespace Logging {

inline const char* ToString(LogLevel level) {
    switch (level) {
    case LogLevel::Trace: return "Trace";
    case LogLevel::Debug: return "Debug";
    case LogLevel::Info: return "Info";
    case LogLevel::Warn: return "Warn";
    case LogLevel::Error: return "Error";
    case LogLevel::Fatal: return "Fatal";
    default: return "Unknown";
    }
}

inline std::string LogFilePath() {
    return Constants::PublicLogDirectory() + "\\ns_sdk_utils.log";
}

inline void WriteRaw(LogLevel level,
                     const char* message,
                     bool logToFile = false,
                     bool printColor = true,
                     const char* memberName = "") {
    char buffer[4096] = {};
    std::snprintf(
        buffer,
        sizeof(buffer),
        "[SDK][%s] (%s) %s\r\n",
        ToString(level),
        memberName ? memberName : "",
        message ? message : "");

    ::OutputDebugStringA(buffer);

    if (printColor) {
        std::fputs(buffer, stdout);
        std::fflush(stdout);
    }

    if (!logToFile && static_cast<int>(level) < static_cast<int>(LogLevel::Warn)) {
        return;
    }

    std::ofstream stream(LogFilePath(), std::ios::app | std::ios::binary);
    if (stream.is_open()) {
        stream << buffer;
    }
}

struct Writer {
    bool LogToFile = false;
    bool PrintColor = true;
    const char* MemberName = "";

    template<typename... Args>
    void operator()(LogLevel level, const char* format, Args... args) const {
        char formatted[3072] = {};
        if constexpr (sizeof...(args) == 0) {
            std::snprintf(formatted, sizeof(formatted), "%s", format ? format : "");
        } else {
            std::snprintf(formatted, sizeof(formatted), format ? format : "", args...);
        }
        WriteRaw(level, formatted, LogToFile, PrintColor, MemberName);
    }

    void operator()(LogLevel level, const std::string& value) const {
        WriteRaw(level, value.c_str(), LogToFile, PrintColor, MemberName);
    }
};

inline Writer Write(bool logToFile = false, bool printColor = true, const char* memberName = "") {
    return Writer{ logToFile, printColor, memberName };
}

} // namespace Logging
} // namespace SDK::Utils
