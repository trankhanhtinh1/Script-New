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
