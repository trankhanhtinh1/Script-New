#include "OverlayManager.h"

#include "D3D11Hook.h"
#include "Overlay.h"
#include "../DebugLog.h"

#include <Windows.h>

namespace {

volatile LONG g_currentMode = static_cast<LONG>(OverlayManager::Mode::Internal);
volatile LONG g_switchRequested = 0;
volatile LONG g_shutdownRequested = 0;
volatile LONG g_running = 0;
volatile LONG g_lastSwitchTick = 0;

constexpr DWORD kModeRestartDelayMs = 350;
constexpr DWORD kSwitchDebounceMs = 900;
constexpr DWORD kSwitchReleaseWaitMs = 600;
constexpr DWORD kSwitchReleasePollMs = 25;

OverlayManager::Mode ReadMode() {
    const LONG value = InterlockedCompareExchange(&g_currentMode, 0, 0);
    return value == static_cast<LONG>(OverlayManager::Mode::External)
        ? OverlayManager::Mode::External
        : OverlayManager::Mode::Internal;
}

OverlayManager::Mode OppositeMode(OverlayManager::Mode mode) {
    return mode == OverlayManager::Mode::Internal
        ? OverlayManager::Mode::External
        : OverlayManager::Mode::Internal;
}

const char* ModeName(OverlayManager::Mode mode) {
    return mode == OverlayManager::Mode::Internal ? "Internal" : "External";
}

bool ShutdownRequested() {
    return InterlockedCompareExchange(&g_shutdownRequested, 0, 0) != 0;
}

void StopMode(OverlayManager::Mode mode) {
    if (mode == OverlayManager::Mode::Internal) {
        D3D11Hook::RequestShutdown();
    } else {
        Overlay::RequestShutdown();
    }
}

void DrainSwitchHotkey() {
    const DWORD startTick = GetTickCount();
    while ((GetAsyncKeyState(VK_F8) & 0x8000) != 0) {
        if (GetTickCount() - startTick >= kSwitchReleaseWaitMs) {
            break;
        }
        Sleep(kSwitchReleasePollMs);
    }

    // Clear the low "pressed since last call" bit before the next backend starts.
    (void)GetAsyncKeyState(VK_F8);
}

} // namespace

namespace OverlayManager {

void Run() {
    if (InterlockedCompareExchange(&g_running, 1, 0) != 0) {
        NightSharpDebug::Logf("[OverlayManager] Run ignored, manager already running");
        return;
    }

    InterlockedExchange(&g_shutdownRequested, 0);
    InterlockedExchange(&g_switchRequested, 0);

    Mode mode = ReadMode();

    while (!ShutdownRequested()) {
        InterlockedExchange(&g_currentMode, static_cast<LONG>(mode));

        NightSharpDebug::Phase(mode == Mode::Internal
                                   ? "overlay-manager-internal"
                                   : "overlay-manager-external");
        NightSharpDebug::Logf("[OverlayManager] Starting %s overlay", ModeName(mode));

        bool modeCompleted = true;
        if (mode == Mode::Internal) {
            modeCompleted = D3D11Hook::Install();
        } else {
            Overlay::Run();
        }

        const LONG switchRequested = InterlockedExchange(&g_switchRequested, 0);
        NightSharpDebug::Logf("[OverlayManager] %s overlay exited switch=%ld shutdown=%ld",
                              ModeName(mode),
                              switchRequested,
                              InterlockedCompareExchange(&g_shutdownRequested, 0, 0));

        if (ShutdownRequested()) {
            break;
        }

        if (switchRequested != 0) {
            mode = OppositeMode(mode);
            InterlockedExchange(&g_currentMode, static_cast<LONG>(mode));
            DrainSwitchHotkey();
            Sleep(kModeRestartDelayMs);
            DrainSwitchHotkey();
            continue;
        }

        NightSharpDebug::Logf(
            "[OverlayManager] %s overlay exited without an explicit switch (completed=%d); stopping",
            ModeName(mode),
            modeCompleted ? 1 : 0);
        InterlockedExchange(&g_shutdownRequested, 1);
        break;
    }

    InterlockedExchange(&g_running, 0);
    NightSharpDebug::Logf("[OverlayManager] Manager exited");
}

void RequestSwitch() {
    const DWORD now = GetTickCount();
    const DWORD last = static_cast<DWORD>(InterlockedCompareExchange(&g_lastSwitchTick, 0, 0));
    if (last != 0 && now - last < kSwitchDebounceMs) {
        NightSharpDebug::Logf("[OverlayManager] F8 switch ignored by debounce delta=%lu",
                              static_cast<unsigned long>(now - last));
        return;
    }
    InterlockedExchange(&g_lastSwitchTick, static_cast<LONG>(now));

    const Mode mode = ReadMode();
    const Mode next = OppositeMode(mode);

    InterlockedExchange(&g_switchRequested, 1);
    NightSharpDebug::Logf("[OverlayManager] F8 switch requested current=%s next=%s",
                          ModeName(mode),
                          ModeName(next));

    StopMode(mode);
}

void RequestShutdown() {
    InterlockedExchange(&g_shutdownRequested, 1);
    D3D11Hook::RequestShutdown();
    Overlay::RequestShutdown();
}

void ShutdownCurrent() {
    RequestShutdown();
    D3D11Hook::Uninstall();
}

Mode CurrentMode() {
    return ReadMode();
}

const char* CurrentModeName() {
    return ModeName(ReadMode());
}

bool IsSwitchRequested() {
    return InterlockedCompareExchange(&g_switchRequested, 0, 0) != 0;
}

} // namespace OverlayManager
