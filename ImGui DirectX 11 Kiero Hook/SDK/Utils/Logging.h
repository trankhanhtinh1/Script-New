#pragma once
// ============================================================================
// Logging.h — Debug logging with levels and optional file output
// Ported from EnsoulSharp.SDK/Core/Utils/Logging.cs
// ============================================================================

#include <string>
#include <cstdio>
#include <cstdarg>
#include <ctime>
#include <fstream>
#include <mutex>
#include <Windows.h>

namespace SDK {

    // Log severity levels
    enum class LogLevel {
        Trace = 0,
        Debug = 1,
        Info  = 2,
        Warn  = 3,
        Error = 4,
        Fatal = 5
    };

    // ========================================================================
    // Logging — thread-safe logging to OutputDebugString and optionally a file
    // ========================================================================
    class Logging {
    public:
        // --------------------------------------------------------------------
        // Config
        // --------------------------------------------------------------------
        static void SetLogToFile(bool enabled) { s_logToFile = enabled; }
        static void SetMinLevel(LogLevel level) { s_minLevel = level; }
        static void SetLogDirectory(const std::string& dir) { s_logDir = dir; }
        static void SetLogFileName(const std::string& name) { s_logFileName = name; }

        // --------------------------------------------------------------------
        // Write — formatted log output
        //   Logging::Write(LogLevel::Info, "Player HP: %.1f", hp);
        // --------------------------------------------------------------------
        static void Write(LogLevel level, const char* fmt, ...) {
            if (level < s_minLevel) return;

            char msgBuf[2048];
            va_list args;
            va_start(args, fmt);
            vsnprintf(msgBuf, sizeof(msgBuf), fmt, args);
            va_end(args);

            // Build formatted string with timestamp and level
            char timeBuf[64];
            GetTimeString(timeBuf, sizeof(timeBuf));

            const char* levelStr = GetLevelString(level);

            char finalBuf[2200];
            snprintf(finalBuf, sizeof(finalBuf), "[%s - %s]: %s\n", timeBuf, levelStr, msgBuf);

            // Output to debug console (visible in VS Output or DebugView)
            OutputDebugStringA(finalBuf);

            // Optionally write to file
            if (s_logToFile || level >= LogLevel::Error) {
                WriteToFile(finalBuf);
            }
        }

        // Convenience helpers
        static void Trace(const char* fmt, ...) {
            char buf[2048]; va_list a; va_start(a, fmt); vsnprintf(buf, sizeof(buf), fmt, a); va_end(a);
            Write(LogLevel::Trace, "%s", buf);
        }
        static void Debug(const char* fmt, ...) {
            char buf[2048]; va_list a; va_start(a, fmt); vsnprintf(buf, sizeof(buf), fmt, a); va_end(a);
            Write(LogLevel::Debug, "%s", buf);
        }
        static void Info(const char* fmt, ...) {
            char buf[2048]; va_list a; va_start(a, fmt); vsnprintf(buf, sizeof(buf), fmt, a); va_end(a);
            Write(LogLevel::Info, "%s", buf);
        }
        static void Warn(const char* fmt, ...) {
            char buf[2048]; va_list a; va_start(a, fmt); vsnprintf(buf, sizeof(buf), fmt, a); va_end(a);
            Write(LogLevel::Warn, "%s", buf);
        }
        static void Error(const char* fmt, ...) {
            char buf[2048]; va_list a; va_start(a, fmt); vsnprintf(buf, sizeof(buf), fmt, a); va_end(a);
            Write(LogLevel::Error, "%s", buf);
        }
        static void Fatal(const char* fmt, ...) {
            char buf[2048]; va_list a; va_start(a, fmt); vsnprintf(buf, sizeof(buf), fmt, a); va_end(a);
            Write(LogLevel::Fatal, "%s", buf);
        }

    private:
        static inline bool        s_logToFile = false;
        static inline LogLevel    s_minLevel  = LogLevel::Debug;
        static inline std::string s_logDir    = "";
        static inline std::string s_logFileName = "sdk_log.txt";

        static const char* GetLevelString(LogLevel level) {
            switch (level) {
                case LogLevel::Trace: return "TRACE";
                case LogLevel::Debug: return "DEBUG";
                case LogLevel::Info:  return "INFO";
                case LogLevel::Warn:  return "WARN";
                case LogLevel::Error: return "ERROR";
                case LogLevel::Fatal: return "FATAL";
                default:              return "???";
            }
        }

        static void GetTimeString(char* buf, size_t bufSize) {
            SYSTEMTIME st;
            GetLocalTime(&st);
            snprintf(buf, bufSize, "%02d:%02d:%02d.%03d",
                st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
        }

        static void WriteToFile(const char* message) {
            static std::mutex fileMutex;
            std::lock_guard<std::mutex> lock(fileMutex);

            try {
                std::string path;
                if (s_logDir.empty()) {
                    // Use DLL directory
                    char buf[MAX_PATH] = {};
                    HMODULE hm = NULL;
                    GetModuleHandleExA(
                        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                        GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                        (LPCSTR)&WriteToFile, &hm);
                    GetModuleFileNameA(hm, buf, MAX_PATH);
                    path = std::string(buf);
                    path = path.substr(0, path.find_last_of("\\/")) + "\\logs";
                }
                else {
                    path = s_logDir;
                }

                // Create directory if needed
                CreateDirectoryA(path.c_str(), NULL);

                std::string fullPath = path + "\\" + s_logFileName;
                std::ofstream ofs(fullPath, std::ios::app);
                if (ofs.is_open()) {
                    ofs << message;
                }
            }
            catch (...) {
                // Silently fail — logging should never crash
            }
        }
    };

} // namespace SDK
