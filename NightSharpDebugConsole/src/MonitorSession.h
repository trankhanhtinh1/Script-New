#pragma once

#include "CrashReport.h"

#include <Windows.h>

#include <mutex>
#include <string>
#include <vector>

namespace nsmonitor {

struct MonitorOptions {
    std::wstring dumpFolder{L"C:\\Users\\Public\\NightSharpDumps"};
    bool testMode{};
    bool noWer{};
};

class MonitorSession {
public:
    explicit MonitorSession(MonitorOptions options);
    ~MonitorSession();
    MonitorSession(const MonitorSession&) = delete;
    MonitorSession& operator=(const MonitorSession&) = delete;

    bool RunOnce();

private:
    static DWORD WINAPI PipeReaderEntry(LPVOID parameter);
    DWORD PipeReaderLoop();
    bool AcceptHello(nscrash::HelloPacket& hello);
    bool OpenTarget(const nscrash::HelloPacket& hello);
    void CloseSession();
    bool HandleBridgeCrash();
    void HandleProcessExit();
    bool Snapshot(nscrash::SharedState& out) const;
    bool WriteLogSnapshot(const std::wstring& path, const nscrash::SharedState& state);
    void CaptureDumpBaseline();
    std::wstring FindNewWerDump() const;

    MonitorOptions options_;
    HANDLE pipe_{INVALID_HANDLE_VALUE};
    HANDLE pipeReader_{};
    HANDLE mapping_{};
    HANDLE crashReady_{};
    HANDLE dumpComplete_{};
    HANDLE process_{};
    nscrash::SharedState* state_{};
    DWORD pid_{};
    volatile LONG stopPipe_{};
    bool capturedBridgeCrash_{};
    std::mutex liveLogMutex_;
    std::vector<std::string> liveLogs_;
    std::vector<std::wstring> baselineDumps_;
};

} // namespace nsmonitor
