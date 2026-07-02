#pragma once

#include "../../DebugLog.h"
#include "../Constants.h"
#include "../Enumerations/LogLevel.h"

#include <Windows.h>
#include <cstdarg>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <fstream>
#include <string>
#include <type_traits>

namespace SDK::Core::Utils {

class Logging {
public:
    class WriteDelegate {
    public:
        WriteDelegate(bool logToFile, bool printColor, const char* memberName)
            : logToFile_(logToFile), printColor_(printColor), memberName_(memberName ? memberName : "") {}

        void operator()(LogLevel logLevel, const char* value) const {
            WriteLine(logLevel, value ? value : "", logToFile_, printColor_, memberName_.c_str());
        }

        void operator()(LogLevel logLevel, const std::string& value) const {
            WriteLine(logLevel, value.c_str(), logToFile_, printColor_, memberName_.c_str());
        }

        void operator()(LogLevel logLevel, const std::exception& value) const {
            WriteLine(logLevel, value.what(), logToFile_, printColor_, memberName_.c_str());
        }

        template <typename... Args>
        void operator()(LogLevel logLevel, const char* fmt, Args... args) const {
            char buffer[1536] = {};
            if (fmt) {
                _snprintf_s(buffer, sizeof(buffer), _TRUNCATE, fmt, args...);
            }
            WriteLine(logLevel, buffer, logToFile_, printColor_, memberName_.c_str());
        }

        template <typename T>
        void operator()(LogLevel logLevel, const T& value) const {
            if constexpr (std::is_arithmetic_v<T>) {
                const std::string text = std::to_string(value);
                WriteLine(logLevel, text.c_str(), logToFile_, printColor_, memberName_.c_str());
            } else {
                WriteLine(logLevel, "<unprintable>", logToFile_, printColor_, memberName_.c_str());
            }
        }

    private:
        bool logToFile_ = false;
        bool printColor_ = true;
        std::string memberName_;
    };

    static WriteDelegate Write(bool logToFile = false,
                               bool printColor = true,
                               const char* memberName = "") {
        return WriteDelegate(logToFile, printColor, memberName);
    }

    template <typename... Args>
    static void Write(LogLevel logLevel,
                      const char* fmt,
                      bool logToFile,
                      bool printColor,
                      const char* memberName,
                      Args... args) {
        char buffer[1536] = {};
        if (fmt) {
            _snprintf_s(buffer, sizeof(buffer), _TRUNCATE, fmt, args...);
        }
        WriteLine(logLevel, buffer, logToFile, printColor, memberName);
    }

private:
    static const char* LevelName(LogLevel level) {
        switch (level) {
        case LogLevel::Debug: return "Debug";
        case LogLevel::Error: return "Error";
        case LogLevel::Fatal: return "Fatal";
        case LogLevel::Info: return "Info";
        case LogLevel::Trace: return "Trace";
        case LogLevel::Warn: return "Warn";
        default: return "Unknown";
        }
    }

    static std::filesystem::path LogDirectory() {
        return SDK::Constants::LogDirectory();
    }

    static void WriteLine(LogLevel level,
                          const char* message,
                          bool logToFile,
                          bool /*printColor*/,
                          const char* memberName) {
        SYSTEMTIME st{};
        GetLocalTime(&st);

        char line[2048] = {};
        _snprintf_s(
            line,
            sizeof(line),
            _TRUNCATE,
            "[%02u:%02u:%02u.%03u - %s]: (%s) -> %s",
            static_cast<unsigned>(st.wHour),
            static_cast<unsigned>(st.wMinute),
            static_cast<unsigned>(st.wSecond),
            static_cast<unsigned>(st.wMilliseconds),
            LevelName(level),
            memberName ? memberName : "",
            message ? message : "");

        NightSharpDebug::Logf("%s", line);

        if (!logToFile && static_cast<int>(level) < 3) {
            return;
        }

        try {
            const auto dir = LogDirectory();
            std::filesystem::create_directories(dir);
            std::ofstream stream(dir / SDK::Constants::LogFileName(), std::ios::app);
            stream << line << "\n";
        } catch (...) {}
    }
};

} // namespace SDK::Core::Utils

namespace SDK::Utils {
    using Logging = ::SDK::Core::Utils::Logging;
} // namespace SDK::Utils
