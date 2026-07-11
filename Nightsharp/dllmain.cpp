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
#include "overlay/OverlayManager.h"

#include "Core/PackmanHook.h"
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
    NightSharpDebug::Phase("overlay-worker-enter");
    NightSharpDebug::Logf("[NightSharp] OverlayWorker entered");

    __try {
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

    HANDLE hThread = CreateThread(nullptr, 0, OverlayWorker, module, 0, nullptr);
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
    switch (reason) {
    case DLL_PROCESS_ATTACH: {
        DisableThreadLibraryCalls(hModule);
        NightSharpDebug::CrashReporter::Install(hModule);
        NightSharpDebug::Phase("dll-attach");
        NightSharpDebug::Logf("[NightSharp] DllMain attach module=%p", hModule);

        // PHẢI gọi ResetLogFile TRƯỚC mọi DbgLog — nó xóa+tạo lại file, nếu
        // gọi sau sẽ wipe log của các block PEB scrub bên dưới.
        ResetLogFile();

        // PEB.BeingDebugged = 0 — Packman checks IsDebuggerPresent
        {
            auto* peb = reinterpret_cast<uint8_t*>(__readgsqword(0x60));
            if (peb) {
                uint8_t& beingDebugged = peb[2];
                const uint8_t old = beingDebugged;
                beingDebugged = 0;
                DbgLogFmt("[PEB] BeingDebugged cleared: old=%u new=0 peb=%p\r\n",
                          (unsigned)old, (void*)peb);
            } else {
                DbgLogFmt("[PEB] Failed to read PEB from GS:0x60\r\n");
            }
        }

        // PackmanHook: init syscalls + deferred CRC bypass install
        // (ResetLogFile đã gọi ở đầu DllMain để không wipe log PEB scrub)
        DirectSyscall::InitAll();
        DirectSyscall::DumpSyscallTable();
        ResetDeferredCRCInstallShutdown();
        HANDLE hCrc = CreateThread(nullptr, 0, DeferredCRCInstallThread, nullptr, 0, nullptr);
        if (hCrc) {
            g_crcThread = hCrc;
        }

        StartOverlayWorker(hModule);

        // Module E — PEB Ldr Unlink. Gọi CUỐI, sau khi overlay worker
        // + deferred CRC thread đã spawn (không cần module lookup nữa).
        // Sau lệnh này: GetModuleHandleW(L"KiteMod.dll") trả nullptr,
        // EnumProcessModulesEx không list module, string "KiteMod" bị xóa
        // khỏi UNICODE_STRING trong LDR_DATA_TABLE_ENTRY.
        {
            const int nUnlinked = PebHide::HideAndErase(hModule);
            DbgLogFmt("[PEB] HideAndErase: unlinked %d/3 list(s) for module=%p\r\n",
                      nUnlinked, hModule);
        }
        break;
    }
    case DLL_PROCESS_DETACH:
        if (reserved == nullptr) {
            NightSharpDebug::Phase("dll-detach");
            NightSharpDebug::Logf("[NightSharp] DllMain detach");
            if (InterlockedCompareExchange(&g_selfUnloading, 0, 0) == 0) {
                ShutdownNightSharpRuntime();
            }
            NightSharpDebug::CrashReporter::StopGuard();
            NightSharpDebug::CrashBridge::Uninstall();
            NightSharpDebug::CrashReporter::Uninstall();
        }
        break;
    default:
        break;
    }

    return TRUE;
}
