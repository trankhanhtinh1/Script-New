#pragma once

#include "CrashBridge.h"
#include "OffsetCrashLogger.h"

#include <intrin.h>
#include <source_location>

#pragma intrinsic(_ReturnAddress)

namespace NightSharpDebug::CrashTrace {

enum class LifecycleAction : std::uint32_t {
    None = 0,
    Create = 1,
    Delete = 2,
};

inline void Record(
    nscrash::TraceTag tag,
    std::uint64_t arg0 = 0,
    std::uint64_t arg1 = 0,
    const std::source_location& location =
        std::source_location::current()) noexcept {
    const auto programCounter =
        reinterpret_cast<std::uint64_t>(_ReturnAddress());
    const std::uint64_t arguments[] = {arg0, arg1};
    OffsetCrashLog::RecordTrace(
        static_cast<std::uint32_t>(tag),
        static_cast<std::uintptr_t>(programCounter),
        arguments,
        _countof(arguments),
        location);
    CrashBridge::Trace(
        tag,
        programCounter,
        arg0,
        arg1);
}

inline std::uint64_t RecordDetailed(
    nscrash::TraceTag tag,
    std::uint64_t arg0,
    std::uint64_t arg1,
    std::uint64_t arg2,
    std::uint64_t arg3,
    std::uint64_t arg4,
    std::uint64_t arg5,
    const char* text,
    const std::source_location& location =
        std::source_location::current()) noexcept {
    const auto programCounter =
        reinterpret_cast<std::uint64_t>(_ReturnAddress());
    const std::uint64_t arguments[] = {
        arg0,
        arg1,
        arg2,
        arg3,
        arg4,
        arg5,
    };
    OffsetCrashLog::RecordTrace(
        static_cast<std::uint32_t>(tag),
        static_cast<std::uintptr_t>(programCounter),
        arguments,
        _countof(arguments),
        location);
    return CrashBridge::TraceDetailed(
        tag,
        programCounter,
        arg0,
        arg1,
        arg2,
        arg3,
        arg4,
        arg5,
        text);
}

inline std::uint64_t RecordExtended(
    nscrash::TraceTag tag,
    std::uint64_t arg0,
    std::uint64_t arg1,
    std::uint64_t arg2,
    std::uint64_t arg3,
    std::uint64_t arg4,
    std::uint64_t arg5,
    std::uint64_t arg6,
    std::uint64_t arg7,
    std::uint64_t arg8,
    std::uint64_t arg9,
    const char* text,
    const std::source_location& location =
        std::source_location::current()) noexcept {
    const auto programCounter =
        reinterpret_cast<std::uint64_t>(_ReturnAddress());
    const std::uint64_t arguments[] = {
        arg0,
        arg1,
        arg2,
        arg3,
        arg4,
        arg5,
        arg6,
        arg7,
        arg8,
        arg9,
    };
    OffsetCrashLog::RecordTrace(
        static_cast<std::uint32_t>(tag),
        static_cast<std::uintptr_t>(programCounter),
        arguments,
        _countof(arguments),
        location);
    return CrashBridge::TraceExtended(
        tag,
        programCounter,
        arg0,
        arg1,
        arg2,
        arg3,
        arg4,
        arg5,
        arg6,
        arg7,
        arg8,
        arg9,
        text);
}

} // namespace NightSharpDebug::CrashTrace
