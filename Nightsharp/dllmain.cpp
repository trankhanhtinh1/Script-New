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
#include "Core/IoctlFilter.h"
// REMOVED: PebLink.h not available in local tree
// #include "Core/PebLink.h"
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
static std::uint32_t g_workerModuleSize = 0;
static volatile LONG g_selfUnloading = 0;
static HANDLE g_ioctlThread = nullptr;

static void StopDeferredThreads() {
    HANDLE ioctlThread = g_ioctlThread;
    g_ioctlThread = nullptr;
    if (ioctlThread) {
        WaitForSingleObject(ioctlThread, 2500);
        CloseHandle(ioctlThread);
    }
}

// Gap 1: Deferred IoctlFilter installer — chờ stub.dll load rồi IAT hook
// DeviceIoControl để intercept IOCTL reports tới vgk.sys
static DWORD WINAPI DeferredIoctlInstallThread(LPVOID) {
    for (int i = 0; i < 120; ++i) {
        DirectSyscall::StealthSleep(500);
        if (GetModuleHandleA("stub.dll")) {
            IoctlFilter::Install();
            return 0;
        }
    }
    return 0;
}

static void ShutdownNightSharpRuntime() {
    OverlayManager::ShutdownCurrent();
    StopDeferredThreads();
    CRCBypass::Uninstall();
    IoctlFilter::Uninstall();
}

static DWORD WINAPI OverlayWorker(LPVOID param) {
    HMODULE module = reinterpret_cast<HMODULE>(param);

    NightSharpDebug::CrashBridge::Install(module, g_workerModuleSize);
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

static void StartOverlayWorker(HMODULE module, std::uint32_t moduleSize) {
    if (InterlockedCompareExchange(&g_workerStarted, 1, 0) != 0) {
        return;
    }
    g_workerModuleSize = moduleSize;

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
        // PEB scrub — clear debugger artifacts trước khi Packman init.
        //
        // PEB layout (Win10/11 x64 — stable từ Win7 x64, OS-dependent không game-dependent):
        //   +0x02  BeingDebugged   (BYTE)   — IsDebuggerPresent
        //   +0x30  ProcessHeap     (PVOID)  — pointer tới _HEAP
        //   +0xBC  NtGlobalFlag    (DWORD)  — debugger set FLG_HEAP_* bits
        //
        // _HEAP layout (Win10/11 x64):
        //   +0x14  Flags           (DWORD)  — HEAP_GROWABLE | HEAP_TAIL_CHECKING | ...
        //   +0x18  ForceFlags      (DWORD)  — forced heap flags
        //
        // Khi debugger attach: NtGlobalFlag |= 0x70, HeapFlags |= 0x02, ForceFlags |= 0x01.
        // Packman check các field này → zero tất cả.
        {
            auto* peb = reinterpret_cast<uint8_t*>(__readgsqword(0x60));
            if (peb) {
                // PEB.BeingDebugged = 0
                uint8_t& beingDebugged = peb[2];
                const uint8_t oldBD = beingDebugged;
                beingDebugged = 0;

                // PEB.NtGlobalFlag = 0
                DWORD& ntGlobalFlag = *reinterpret_cast<DWORD*>(peb + 0xBC);
                const DWORD oldGF = ntGlobalFlag;
                ntGlobalFlag = 0;

                // ProcessHeap->Flags = 0, ProcessHeap->ForceFlags = 0
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

        // PackmanHook: init syscalls
        // (ResetLogFile đã gọi ở đầu DllMain để không wipe log PEB scrub)
        DirectSyscall::InitAll();
        DirectSyscall::DumpSyscallTable();

        // Gap 1: Spawn deferred IoctlFilter installer — chờ stub.dll load
        // rồi IAT hook DeviceIoControl để intercept IOCTL reports tới vgk.sys
        HANDLE hIoctl = CoreBypass::CreateThreadSpoofed(DeferredIoctlInstallThread, nullptr);
        if (hIoctl) {
            g_ioctlThread = hIoctl;
        }

        const std::uint32_t moduleSize =
            NightSharpDebug::CrashBridge::ImageSize(hModule);
        NightSharpDebug::Logf(
            "[NightSharp] Captured module metadata base=%p size=0x%X before header scrub",
            hModule,
            moduleSize);
        StartOverlayWorker(hModule, moduleSize);

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

        // Gap 2: Erase PE headers — zero DOS+NT+section headers sau khi
        // tất cả init đã xong. Anti-cheat scan memory tìm "MZ"/"PE" signature
        // để identify injected modules. Sau erase: scanner không match.
        // Phải gọi SAU PebHide (cần DllBase để đọc SizeOfHeaders).
        {
            const bool erased = PebHide::ErasePeHeaders(hModule);
            DbgLogFmt("[PEB] ErasePeHeaders: %s for module=%p\r\n",
                      erased ? "OK" : "FAIL", hModule);
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
