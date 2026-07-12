#pragma once

#include "CrashProtocol.h"

#include <Windows.h>

#include <cstddef>
#include <cstdint>

namespace nscrash {

class CrashClient {
public:
    CrashClient() = default;
    ~CrashClient();

    CrashClient(const CrashClient&) = delete;
    CrashClient& operator=(const CrashClient&) = delete;

    bool Install(std::uintptr_t moduleBase, std::uint32_t moduleSize);
    void Uninstall();

    void PublishHeartbeat(std::uint64_t tick);
    void PublishPhase(const char* phase);
    void PublishStage(const char* stage);
    void EnqueueLog(const char* text, std::size_t length);
    void Trace(
        TraceTag tag,
        std::uint64_t programCounter,
        std::uint64_t arg0,
        std::uint64_t arg1);
    std::uint64_t TraceDetailed(
        TraceTag tag,
        std::uint64_t programCounter,
        std::uint64_t arg0,
        std::uint64_t arg1,
        std::uint64_t arg2,
        std::uint64_t arg3,
        std::uint64_t arg4,
        std::uint64_t arg5,
        const char* text);
    std::uint64_t TraceExtended(
        TraceTag tag,
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
        const char* text);
    void ObserveFirstChance(EXCEPTION_POINTERS* exceptionPointers);
    bool CaptureException(
        CrashKind kind,
        const char* stage,
        EXCEPTION_POINTERS* exceptionPointers,
        DWORD timeoutMs = 15000);

    SharedState* State() const { return state_; }

private:
    static DWORD WINAPI WorkerEntry(LPVOID parameter);
    DWORD WorkerLoop();
    bool SendHello(HANDLE pipe, std::uint64_t sequence);
    bool SendPendingLogs(HANDLE pipe, LONG& lastSent);
    void PublishText(char* destination, std::size_t size, const char* text);
    void CloseResources();

    DWORD pid_{};
    HANDLE mapping_{};
    HANDLE crashReady_{};
    HANDLE dumpComplete_{};
    HANDLE worker_{};
    HANDLE pipe_{INVALID_HANDLE_VALUE};
    SharedState* state_{};
    volatile LONG stopping_{};
};

} // namespace nscrash
