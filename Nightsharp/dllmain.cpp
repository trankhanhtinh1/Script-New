/*
 * NightSharp - DLL Entry Point
 *
 * Overlay-only bootstrap.
 *   - Keeps the DLL/worker-thread shape from the original entry point.
 *   - Starts only the D3D11 + ImGui overlay.
 *   - No core, sdk, plugin, hook, or game-memory logic is initialized here.
 */

#include <Windows.h>
#include <cstddef>
#include <cstdint>
#include <new>

#include "CrashReporter.h"
#include "DebugLog.h"
#include "overlay/Overlay.h"

// PackmanHook disabled — CRC bypass not yet stable
// #include "Core/PackmanHook.h"
// using namespace PackmanTest;

#pragma comment(lib, "user32.lib")

// ========================================================================
// Global C++ allocation override
// Route allocations to the Win32 process heap so ImGui/backend allocations
// do not depend on any project-specific allocator state.
// ========================================================================
void* operator new(size_t sz) {
    if (void* p = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sz ? sz : 1)) {
        return p;
    }
    throw std::bad_alloc();
}

void operator delete(void* ptr) noexcept {
    if (ptr) {
        HeapFree(GetProcessHeap(), 0, ptr);
    }
}

void* operator new[](size_t sz) {
    if (void* p = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sz ? sz : 1)) {
        return p;
    }
    throw std::bad_alloc();
}

void operator delete[](void* ptr) noexcept {
    if (ptr) {
        HeapFree(GetProcessHeap(), 0, ptr);
    }
}

void* operator new(size_t sz, const std::nothrow_t&) noexcept {
    return HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sz ? sz : 1);
}

void operator delete(void* ptr, const std::nothrow_t&) noexcept {
    if (ptr) {
        HeapFree(GetProcessHeap(), 0, ptr);
    }
}

void* operator new[](size_t sz, const std::nothrow_t&) noexcept {
    return HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sz ? sz : 1);
}

void operator delete[](void* ptr, const std::nothrow_t&) noexcept {
    if (ptr) {
        HeapFree(GetProcessHeap(), 0, ptr);
    }
}

void* operator new(size_t sz, std::align_val_t al) {
    const SIZE_T align = (SIZE_T)al;
    const SIZE_T total = (sz ? sz : 1) + align + sizeof(void*);
    void* raw = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, total);
    if (!raw) {
        throw std::bad_alloc();
    }

    uintptr_t base = (uintptr_t)raw + sizeof(void*);
    uintptr_t aligned = (base + (align - 1)) & ~(uintptr_t)(align - 1);
    ((void**)aligned)[-1] = raw;
    return (void*)aligned;
}

void operator delete(void* ptr, std::align_val_t) noexcept {
    if (ptr) {
        HeapFree(GetProcessHeap(), 0, ((void**)ptr)[-1]);
    }
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
// Overlay worker thread
// ========================================================================
static volatile LONG g_workerStarted = 0;

static DWORD WINAPI OverlayWorker(LPVOID) {
    NightSharpDebug::Phase("overlay-worker-enter");
    NightSharpDebug::Logf("[NightSharp] OverlayWorker entered");

    __try {
        NightSharpDebug::Phase("overlay-run");
        Overlay::Run();
        NightSharpDebug::Logf("[NightSharp] Overlay::Run() exited");
    } __except (NightSharpDebug::CrashReporter::LogAndDumpException(
                    "OverlayWorker/Overlay::Run",
                    GetExceptionInformation())) {
        NightSharpDebug::Logf("[NightSharp] Overlay::Run() crashed");
    }

    InterlockedExchange(&g_workerStarted, 0);
    NightSharpDebug::Phase("overlay-worker-exit");
    NightSharpDebug::Logf("[NightSharp] OverlayWorker exiting");
    return 0;
}

static void StartOverlayWorker() {
    if (InterlockedCompareExchange(&g_workerStarted, 1, 0) != 0) {
        return;
    }

    HANDLE hThread = CreateThread(nullptr, 0, OverlayWorker, nullptr, 0, nullptr);
    if (hThread) {
        CloseHandle(hThread);
        NightSharpDebug::Logf("[NightSharp] Overlay worker thread created");
    } else {
        InterlockedExchange(&g_workerStarted, 0);
        NightSharpDebug::Logf("[NightSharp] Failed to create overlay worker thread gle=%lu",
                              GetLastError());
    }
}

// ========================================================================
// NextHook — Exported hook procedure for SetWindowsHookEx injection
// ========================================================================
extern "C" __declspec(dllexport) LRESULT CALLBACK NextHook(int code, WPARAM wParam, LPARAM lParam) {
    // Simply pass the hook along so we don't break the game's message chain
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
        NightSharpDebug::Phase("dll-attach");
        NightSharpDebug::Logf("[NightSharp] DllMain attach module=%p", hModule);
        // PackmanHook disabled — CRC bypass not yet stable
        // PackmanTest::InstallAndLog(hModule);
        StartOverlayWorker();
        break;
    case DLL_PROCESS_DETACH:
        NightSharpDebug::Phase("dll-detach");
        NightSharpDebug::Logf("[NightSharp] DllMain detach");
        Overlay::RequestShutdown();
        NightSharpDebug::CrashReporter::Uninstall();
        break;
    default:
        break;
    }

    return TRUE;
}
