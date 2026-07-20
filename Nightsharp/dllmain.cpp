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
#include "CrashTrace.h"
#include "DebugLog.h"
#include "overlay/OverlayManager.h"

#include "Core/PackmanHook.h"
#include "Core/CoreBypass.h"
#include "Core/PebHide.h"
#pragma comment(lib, "psapi.lib")

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
static volatile LONG g_selfUnloading = 0;
static HANDLE g_crcThread = nullptr;
static HANDLE g_shutdownEvent = nullptr;
static HMODULE g_hModule = nullptr;

static void StopDeferredCRCThread() {
    RequestDeferredCRCInstallShutdown();

    HANDLE thread = g_crcThread;
    g_crcThread = nullptr;
    if (thread) {
        WaitForSingleObject(thread, 2500);
        CloseHandle(thread);
    }
}

static void ShutdownNightSharpRuntime() {
    OverlayManager::ShutdownCurrent();
    StopDeferredCRCThread();
    CRCBypass::Uninstall();
}

static DWORD WINAPI OverlayWorker(LPVOID param) {
    HMODULE module = reinterpret_cast<HMODULE>(param);

    NightSharpDebug::CrashBridge::Install(module);
    NightSharpDebug::CrashReporter::StartGuard();
    NightSharpDebug::CrashTrace::Record(
        nscrash::TraceTag::OverlayWorkerEnter,
        reinterpret_cast<std::uintptr_t>(module));
    NightSharpDebug::Phase("overlay-worker-enter");
    NightSharpDebug::Logf("[NightSharp] OverlayWorker entered");

    __try {
        NightSharpDebug::CrashTrace::Record(nscrash::TraceTag::OverlayRun);
        NightSharpDebug::Phase("overlay-manager-run");
        OverlayManager::Run();
        NightSharpDebug::Logf("[NightSharp] OverlayManager exited");
    } __except (NightSharpDebug::CrashReporter::LogAndDumpException(
                    "OverlayWorker/OverlayManager::Run",
                    GetExceptionInformation())) {
        NightSharpDebug::Logf("[NightSharp] OverlayManager crashed");
    }

    InterlockedExchange(&g_workerStarted, 0);
    NightSharpDebug::Phase("overlay-worker-exit");
    NightSharpDebug::Logf("[NightSharp] OverlayWorker exiting");

    if (module) {
        InterlockedExchange(&g_selfUnloading, 1);
        ShutdownNightSharpRuntime();
        NightSharpDebug::Phase("self-unload");
        NightSharpDebug::Logf("[NightSharp] FreeLibraryAndExitThread module=%p", module);
        NightSharpDebug::CrashReporter::StopGuard();
        NightSharpDebug::CrashBridge::Uninstall();
        FreeLibraryAndExitThread(module, 0);
    }

    return 0;
}

static void StartOverlayWorker(HMODULE module) {
    if (InterlockedCompareExchange(&g_workerStarted, 1, 0) != 0) {
        return;
    }

    HANDLE hThread = CoreBypass::CreateThreadSpoofed(OverlayWorker, module);
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
// ========================================================================
// NightSharpWorker — all init logic moved here from DllMain
// Spoofed start address (game module) + ThreadHideFromDebugger
// ========================================================================
static DWORD WINAPI NightSharpWorker(LPVOID param) {
    HMODULE hModule = reinterpret_cast<HMODULE>(param);

    NightSharpDebug::CrashReporter::Install(hModule);
    NightSharpDebug::Phase("worker-attach");
    NightSharpDebug::Logf("[NightSharp] NightSharpWorker entered module=%p", hModule);

    // PHẢI gọi ResetLogFile TRƯỚC mọi DbgLog
    ResetLogFile();

    // PEB scrub — clear debugger artifacts trước khi Packman init.
    {
        auto* peb = reinterpret_cast<uint8_t*>(__readgsqword(0x60));
        if (peb) {
            uint8_t& beingDebugged = peb[2];
            const uint8_t oldBD = beingDebugged;
            beingDebugged = 0;

            DWORD& ntGlobalFlag = *reinterpret_cast<DWORD*>(peb + 0xBC);
            const DWORD oldGF = ntGlobalFlag;
            ntGlobalFlag = 0;

            PVOID processHeap = *reinterpret_cast<PVOID*>(peb + 0x30);
            DWORD oldHeapFlags = 0, oldHeapForceFlags = 0;
            if (processHeap) {
                __try {
                    DWORD* heapFlags = reinterpret_cast<DWORD*>(
                        reinterpret_cast<uint8_t*>(processHeap) + 0x14);
                    DWORD* heapForceFlags = reinterpret_cast<DWORD*>(
                        reinterpret_cast<uint8_t*>(processHeap) + 0x18);
                    oldHeapFlags = *heapFlags;
                    oldHeapForceFlags = *heapForceFlags;
                    *heapFlags = 0;
                    *heapForceFlags = 0;
                } __except (EXCEPTION_EXECUTE_HANDLER) {
                    DbgLogFmt("[PEB] Heap scrub SEH exception (heap=%p)\r\n", processHeap);
                    processHeap = nullptr;
                }
            }

            DbgLogFmt("[PEB] BeingDebugged: old=%u new=0\r\n", (unsigned)oldBD);
            DbgLogFmt("[PEB] NtGlobalFlag: old=0x%X new=0\r\n", (unsigned)oldGF);
            DbgLogFmt("[PEB] HeapFlags: old=0x%X new=0  ForceFlags: old=0x%X new=0  heap=%p\r\n",
                      (unsigned)oldHeapFlags, (unsigned)oldHeapForceFlags, processHeap);
            DbgLogFmt("[PEB] PEB=%p\r\n", (void*)peb);
        } else {
            DbgLogFmt("[PEB] Failed to read PEB from GS:0x60\r\n");
        }
    }

    // PackmanHook: init syscalls + deferred CRC bypass install
    DirectSyscall::InitAll();
    DirectSyscall::DumpSyscallTable();

    // Module 3: Set ThreadHideFromDebugger on worker thread (direct syscall)
    // Phải gọi SAU InitAll (cần IDX_SETINFOTHREAD init)
    DirectSyscall::NtSetInformationThreadDirect(
        GetCurrentThread(), 0x11, nullptr, 0);
    DbgLogFmt("[THAUD] ThreadHideFromDebugger set on worker thread\r\n");

    // HW Breakpoint Detection
    HwBpDetect::CheckAndClear();

    // Thread Info Audit
    ThreadInfoAudit::Audit();

    ResetDeferredCRCInstallShutdown();
    HANDLE hCrc = CoreBypass::CreateThreadSpoofed(DeferredCRCInstallThread, nullptr);
    if (hCrc) {
        g_crcThread = hCrc;
    }

    StartOverlayWorker(hModule);

    // PEB Ldr Unlink — gọi CUỐI, sau khi overlay worker + CRC thread đã spawn
    {
        const int nUnlinked = PebHide::HideAndErase(hModule);
        DbgLogFmt("[PEB] HideAndErase: unlinked %d/3 list(s) for module=%p\r\n",
                  nUnlinked, hModule);
    }

    // Memory Region Audit
    MemRegionAudit::Audit();

    // Stack Walk Audit
    StackAudit::Audit(hModule);

    // Wait for shutdown signal
    if (g_shutdownEvent) {
        WaitForSingleObject(g_shutdownEvent, INFINITE);
    }
    return 0;
}

// ========================================================================
// DllMain — minimal: set ThreadHideFromDebugger + spawn spoofed worker
// Stack walk in DllMain sees only 1 NS frame (this function).
// ========================================================================
BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID reserved) {
    switch (reason) {
    case DLL_PROCESS_ATTACH: {
        DisableThreadLibraryCalls(hModule);

        g_hModule = hModule;
        g_shutdownEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);

        // Spawn spoofed worker thread — all logic moves here
        // DllMain returns immediately → stack has only 1 NS frame
        HANDLE hWorker = CoreBypass::CreateThreadSpoofed(NightSharpWorker, hModule);
        if (hWorker) {
            CloseHandle(hWorker);
        }
        break;
    }
    case DLL_PROCESS_DETACH:
        if (reserved == nullptr) {
            NightSharpDebug::Phase("dll-detach");
            NightSharpDebug::Logf("[NightSharp] DllMain detach");
            if (g_shutdownEvent) {
                SetEvent(g_shutdownEvent);
            }
            if (InterlockedCompareExchange(&g_selfUnloading, 0, 0) == 0) {
                ShutdownNightSharpRuntime();
            }
            NightSharpDebug::CrashReporter::StopGuard();
            NightSharpDebug::CrashBridge::Uninstall();
            NightSharpDebug::CrashReporter::Uninstall();
            if (g_shutdownEvent) {
                CloseHandle(g_shutdownEvent);
                g_shutdownEvent = nullptr;
            }
        }
        break;
    default:
        break;
    }

    return TRUE;
}
