#pragma once

#include "../NightSharpCrashProtocol/CrashClient.h"

#include <Windows.h>

#include <cstddef>
#include <cstdint>

namespace NightSharpDebug::CrashBridge {

inline nscrash::CrashClient g_client;

inline std::uint32_t ImageSize(HMODULE module) {
    if (!module) {
        return 0;
    }
    const auto* base = reinterpret_cast<const std::uint8_t*>(module);
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0) {
        return 0;
    }
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(
        base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) {
        return 0;
    }
    return nt->OptionalHeader.SizeOfImage;
}

inline bool Install(HMODULE module, std::uint32_t moduleSize) {
    return g_client.Install(
        reinterpret_cast<std::uintptr_t>(module),
        moduleSize);
}

inline bool Install(HMODULE module) {
    return Install(module, ImageSize(module));
}

inline void Uninstall() {
    g_client.Uninstall();
}

inline void PublishHeartbeat(std::uint64_t tick) {
    g_client.PublishHeartbeat(tick);
}

inline void PublishPhase(const char* phase) {
    g_client.PublishPhase(phase);
}

inline void PublishStage(const char* stage) {
    g_client.PublishStage(stage);
}

inline void EnqueueLog(const char* text, std::size_t length) {
    g_client.EnqueueLog(text, length);
}

inline void Trace(
    nscrash::TraceTag tag,
    std::uint64_t programCounter,
    std::uint64_t arg0,
    std::uint64_t arg1) {
    g_client.Trace(tag, programCounter, arg0, arg1);
}

inline std::uint64_t TraceDetailed(
    nscrash::TraceTag tag,
    std::uint64_t programCounter,
    std::uint64_t arg0,
    std::uint64_t arg1,
    std::uint64_t arg2,
    std::uint64_t arg3,
    std::uint64_t arg4,
    std::uint64_t arg5,
    const char* text) {
    return g_client.TraceDetailed(
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

inline std::uint64_t TraceExtended(
    nscrash::TraceTag tag,
    std::uint64_t programCounter,
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
    const char* text) {
    return g_client.TraceExtended(
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

inline void ObserveFirstChance(EXCEPTION_POINTERS* exceptionPointers) {
    g_client.ObserveFirstChance(exceptionPointers);
}

inline bool CaptureException(
    nscrash::CrashKind kind,
    const char* stage,
    EXCEPTION_POINTERS* exceptionPointers) {
    return g_client.CaptureException(kind, stage, exceptionPointers);
}

} // namespace NightSharpDebug::CrashBridge
