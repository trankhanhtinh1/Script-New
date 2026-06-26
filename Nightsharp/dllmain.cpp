#include <Windows.h>
#include <cstddef>
#include <cstdint>
#include <new>

#include "CrashReporter.h"
#include "DebugLog.h"
#include "overlay/Overlay.h"
#include "overlay/D3D11Hook.h"
#include "menu/MenuConfig.h"
#include "core/PackmanHook.h"

#pragma comment(lib, "user32.lib")

void* operator new(size_t sz) {
    if (void* p = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sz ? sz : 1)) {
        return p;
    }
    throw std::bad_alloc();
}

void operator delete(void* ptr) noexcept {
    if (ptr) { HeapFree(GetProcessHeap(), 0, ptr); }
}

void* operator new[](size_t sz) {
    if (void* p = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sz ? sz : 1)) {
        return p;
    }
    throw std::bad_alloc();
}

void operator delete[](void* ptr) noexcept {
    if (ptr) { HeapFree(GetProcessHeap(), 0, ptr); }
}

void* operator new(size_t sz, const std::nothrow_t&) noexcept {
    return HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sz ? sz : 1);
}

void operator delete(void* ptr, const std::nothrow_t&) noexcept {
    if (ptr) { HeapFree(GetProcessHeap(), 0, ptr); }
}

void* operator new[](size_t sz, const std::nothrow_t&) noexcept {
    return HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sz ? sz : 1);
}

void operator delete[](void* ptr, const std::nothrow_t&) noexcept {
    if (ptr) { HeapFree(GetProcessHeap(), 0, ptr); }
}

void* operator new(size_t sz, std::align_val_t al) {
    const SIZE_T align = (SIZE_T)al;
    const SIZE_T total = (sz ? sz : 1) + align + sizeof(void*);
    void* raw = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, total);
    if (!raw) { throw std::bad_alloc(); }

    uintptr_t base = (uintptr_t)raw + sizeof(void*);
    uintptr_t aligned = (base + (align - 1)) & ~(uintptr_t)(align - 1);
    ((void**)aligned)[-1] = raw;
    return (void*)aligned;
}

void operator delete(void* ptr, std::align_val_t) noexcept {
    if (ptr) { HeapFree(GetProcessHeap(), 0, ((void**)ptr)[-1]); }
}

void* operator new[](size_t sz, std::align_val_t al) {
    return ::operator new(sz, al);
}

void operator delete[](void* ptr, std::align_val_t al) noexcept {
    ::operator delete(ptr, al);
}

void operator delete(void* ptr, size_t) noexcept {
    ::operator delete(ptr);
}

void operator delete[](void* ptr, size_t) noexcept {
    ::operator delete[](ptr);
}

void operator delete(void* ptr, size_t, std::align_val_t al) noexcept {
    ::operator delete(ptr, al);
}

void operator delete[](void* ptr, size_t, std::align_val_t al) noexcept {
    ::operator delete[](ptr, al);
}

// ========================================================================
// Worker thread
// ========================================================================
static volatile LONG g_workerStarted = 0;

static DWORD WINAPI NightSharpWorker(LPVOID) {
    NightSharpDebug::Phase("worker-enter");
    NightSharpDebug::Logf("[NightSharp] Worker entered");

    if (Config::Rendering::useInternal) {
        // === INTERNAL MODE ===
        // Hook the game's IDXGISwapChain::Present and render ImGui directly.
        // Blocks until plugins are bootstrapped, then returns and idles.
        NightSharpDebug::Phase("worker-d3d11hook-install");
        __try {
            if (D3D11Hook::Install()) {
                NightSharpDebug::Logf("[NightSharp] D3D11Hook installed, idling");
                // Idle while hooks are active
                while (D3D11Hook::IsActive() &&
                       !Overlay::IsRunning() &&
                       GetAsyncKeyState(VK_END) >= 0) {
                    Sleep(250);
                }
            } else {
                NightSharpDebug::Logf("[NightSharp] D3D11Hook install failed, falling back to overlay");
                // Fall through to external overlay below
            }
        }
        __except (NightSharpDebug::CrashReporter::LogAndDumpException(
                      "NightSharpWorker/D3D11Hook::Install",
                      GetExceptionInformation())) {
            NightSharpDebug::Logf("[NightSharp] D3D11Hook::Install crashed");
        }
    } else {
        // === EXTERNAL MODE (legacy overlay) ===
        __try {
            NightSharpDebug::Phase("overlay-run");
            Overlay::Run();
        }
        __except (NightSharpDebug::CrashReporter::LogAndDumpException(
                      "NightSharpWorker/Overlay::Run",
                      GetExceptionInformation())) {
            NightSharpDebug::Logf("[NightSharp] Overlay::Run() crashed");
        }
    }

    // If internal install failed and we haven't started the overlay, start it now
    if (!D3D11Hook::IsActive() && !Overlay::IsRunning()) {
        __try {
            NightSharpDebug::Phase("overlay-run-fallback");
            Overlay::Run();
        }
        __except (NightSharpDebug::CrashReporter::LogAndDumpException(
                      "NightSharpWorker/Overlay::Run-fallback",
                      GetExceptionInformation())) {
            NightSharpDebug::Logf("[NightSharp] Fallback overlay crashed");
        }
    }

    InterlockedExchange(&g_workerStarted, 0);
    NightSharpDebug::Phase("worker-exit");
    NightSharpDebug::Logf("[NightSharp] Worker exiting");
    return 0;
}

static void StartWorker() {
    if (InterlockedCompareExchange(&g_workerStarted, 1, 0) != 0) {
        return;
    }

    HANDLE hThread = CreateThread(nullptr, 0, NightSharpWorker, nullptr, 0, nullptr);
    if (hThread) {
        CloseHandle(hThread);
        NightSharpDebug::Logf("[NightSharp] Worker thread created");
    } else {
        InterlockedExchange(&g_workerStarted, 0);
        NightSharpDebug::Logf("[NightSharp] Failed to create worker thread gle=%lu",
                              GetLastError());
    }
}

// ========================================================================
// NextHook — Exported hook procedure for SetWindowsHookEx injection
// ========================================================================
extern "C" __declspec(dllexport) LRESULT CALLBACK NextHook(int code, WPARAM wParam, LPARAM lParam) {
    return CallNextHookEx(NULL, code, wParam, lParam);
}

// ========================================================================
// DllMain
// ========================================================================
BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID reserved) {
    (void)reserved;

    switch (reason) {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);
        NightSharpDebug::CrashReporter::Install(hModule);
        PackmanHook::InstallAndLog();
        NightSharpDebug::Phase("dll-attach");
        NightSharpDebug::Logf("[NightSharp] DllMain attach module=%p (internal=%d)",
                              hModule, Config::Rendering::useInternal ? 1 : 0);
        StartWorker();
        break;
    case DLL_PROCESS_DETACH:
        NightSharpDebug::Phase("dll-detach");
        NightSharpDebug::Logf("[NightSharp] DllMain detach");
        PackmanHook::Shutdown();
        Overlay::RequestShutdown();
        D3D11Hook::Uninstall();
        NightSharpDebug::CrashReporter::Uninstall();
        break;
    default:
        break;
    }

    return TRUE;
}
