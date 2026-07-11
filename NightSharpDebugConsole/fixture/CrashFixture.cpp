#include "../../NightSharpCrashProtocol/CrashClient.h"

#include <Windows.h>
#include <intrin.h>

#include <cstring>

namespace {

nscrash::CrashClient* g_client = nullptr;

LONG WINAPI FixtureUnhandledFilter(EXCEPTION_POINTERS* exceptionPointers) {
    if (g_client) {
        g_client->CaptureException(
            nscrash::CrashKind::Unhandled,
            "fixture/unhandled-av",
            exceptionPointers);
    }
    return EXCEPTION_EXECUTE_HANDLER;
}

void RunHandledAccessViolation(nscrash::CrashClient* client) {
    __try {
        *reinterpret_cast<volatile int*>(0) = 1;
    } __except (
        client->CaptureException(
            nscrash::CrashKind::Handled,
            "fixture/handled-av",
            GetExceptionInformation())
            ? EXCEPTION_EXECUTE_HANDLER
            : EXCEPTION_EXECUTE_HANDLER) {
    }
}

} // namespace

int main(int argc, char** argv) {
    nscrash::CrashClient client;
    if (!client.Install(
            reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr)),
            0x100000)) {
        return 10;
    }
    g_client = &client;
    SetUnhandledExceptionFilter(&FixtureUnhandledFilter);
    client.PublishPhase("fixture-running");

    const char* mode = argc > 1 ? argv[1] : "wait";
    if (std::strcmp(mode, "handled-av") == 0) {
        RunHandledAccessViolation(&client);
        client.Uninstall();
        return 0;
    }
    if (std::strcmp(mode, "unhandled-av") == 0) {
        *reinterpret_cast<volatile int*>(0) = 1;
    }
    if (std::strcmp(mode, "failfast") == 0) {
        client.EnqueueLog("fixture connected before failfast\r\n", 35);
        Sleep(750);
        __fastfail(FAST_FAIL_FATAL_APP_EXIT);
    }
    if (std::strcmp(mode, "forced") == 0) {
        client.EnqueueLog("fixture connected before forced exit\r\n", 38);
        Sleep(750);
        TerminateProcess(GetCurrentProcess(), 0x55);
    }

    for (int i = 0; i < 100; ++i) {
        client.PublishHeartbeat(GetTickCount64());
        Sleep(100);
    }
    client.Uninstall();
    return 0;
}
