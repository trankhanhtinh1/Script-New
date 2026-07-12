#pragma once

#include "CrashBridge.h"

#include <intrin.h>

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
    std::uint64_t arg1 = 0) noexcept {
    CrashBridge::Trace(
        tag,
        reinterpret_cast<std::uint64_t>(_ReturnAddress()),
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
    const char* text) noexcept {
    return CrashBridge::TraceDetailed(
        tag,
        reinterpret_cast<std::uint64_t>(_ReturnAddress()),
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
    const char* text) noexcept {
    return CrashBridge::TraceExtended(
        tag,
        reinterpret_cast<std::uint64_t>(_ReturnAddress()),
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
