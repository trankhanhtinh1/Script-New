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
static volatile LONG g_isManualMapped = 0;  // 1 = entry via ManualMapEntry (no LDR)
static HANDLE g_ioctlThread = nullptr;
static HANDLE g_stubHardThread = nullptr;

static void StopDeferredThreads() {
    HANDLE ioctlThread = g_ioctlThread;
    g_ioctlThread = nullptr;
    if (ioctlThread) {
        WaitForSingleObject(ioctlThread, 2500);
        CloseHandle(ioctlThread);
    }
    HANDLE stubThread = g_stubHardThread;
    g_stubHardThread = nullptr;
    if (stubThread) {
        WaitForSingleObject(stubThread, 2500);
        CloseHandle(stubThread);
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

// Deferred StubHardening installer — chờ stub.dll load rồi scan inline syscall
static DWORD WINAPI DeferredStubHardeningThread(LPVOID) {
    DbgLogFmt("[SH] DeferredStubHardeningThread: started\r\n");
    for (int i = 0; i < 120; ++i) {
        DirectSyscall::StealthSleep(500);
        if (GetModuleHandleA("stub.dll")) {
            DbgLogFmt("[SH] stub.dll found after %d iterations, running ScanInlineSyscalls\r\n", i);
            StubHardening::ScanInlineSyscalls();
            return 0;
        }
    }
    DbgLogFmt("[SH] StubHardening: stub.dll not loaded after 60s, abort\r\n");
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
        // Manual-mapped DLLs are NOT registered with the loader, so
        // FreeLibraryAndExitThread would crash (ntdll!LdrpDecrementModuleLoadCountEx
        // tries to deref the LDR_DATA_TABLE_ENTRY which doesn't exist).
        // Just exit the thread cleanly instead.
        if (InterlockedCompareExchange(&g_isManualMapped, 0, 0) != 0) {
            NightSharpDebug::Logf("[NightSharp] Manual-map path: ExitThread (no FreeLibrary)");
            ExitThread(0);
        }
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
// PerformNightSharpStartup — full overlay runtime bootstrap.
// Shared by BOTH injection paths so they boot the identical overlay stack:
//   crash bridge -> PEB scrub -> syscalls -> deferred threads ->
//   overlay worker -> PEB unlink -> header erase.
//   * LoadLibrary path : called from DllMain (DLL_PROCESS_ATTACH).
//   * Manual-map/APC    : called from ManualMapEntry (injector shellcode),
//                         because manual mapping bypasses DllMain.
// ========================================================================
static void PerformNightSharpStartup(HMODULE hModule) {
    // DisableThreadLibraryCalls is a NOP for manual-mapped DLLs (no LDR entry),
    // but it's safe to call — it returns FALSE and does nothing if the module
    // is not in the loaded-modules list.
    DisableThreadLibraryCalls(hModule);
    NightSharpDebug::CrashReporter::Install(hModule);
    NightSharpDebug::Phase("dll-attach");
    NightSharpDebug::Logf("[NightSharp] RuntimeStartup attach module=%p", hModule);

    // PHẢI gọi ResetLogFile TRƯỚC mọi DbgLog — nó xóa+tạo lại file, nếu
    // gọi sau sẽ wipe log của các block PEB scrub bên dưới.
    ResetLogFile();
    // PEB scrub — clear debugger artifacts trước khi Packman init.
    //
    // 2026-08-16: PEB scrub DISABLED. Clearing NtGlobalFlag/HeapFlags corrupts
    // ntdll internal heap metadata (SRWLOCK state or segment linkage), causing
    // ACCESS_VIOLATION (write to 0x24 from NULL) inside RtlpAllocateHeapInternal
    // when SDK Bootstrap allocates std::unordered_map buckets ~3s later.
    // The Packman anti-debug checks run on the game's OWN PEB read, not ours;
    // and BeingDebugged is cleared by the injector before APC dispatch anyway.
    // TODO: re-enable selectively after confirming which fields are safe.
#if 0
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
                    // IMPORTANT: Do NOT modify HeapFlags at all.
                    // Zeroing/masking bits corrupts ntdll's internal heap metadata
                    // linkage, causing ACCESS_VIOLATION inside RtlpAllocateHeap when
                    // the SDK later does std::unordered_map allocations.
                    // Only clear ForceFlags (debug-only field) which Packman checks.
                    // *heapFlags = oldHeapFlags;  // LEAVE UNCHANGED
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
#else
    DbgLogFmt("[PEB] PEB scrub DISABLED (heap corruption workaround)\r\n");
#endif

    // PackmanHook: init syscalls
    // (ResetLogFile đã gọi ở đầu để không wipe log PEB scrub)
    DirectSyscall::InitAll();
    DirectSyscall::DumpSyscallTable();

    // Gap 1: Spawn deferred IoctlFilter installer — chờ stub.dll load
    // rồi IAT hook DeviceIoControl để intercept IOCTL reports tới vgk.sys
    HANDLE hIoctl = CoreBypass::CreateThreadSpoofed(DeferredIoctlInstallThread, nullptr);
    if (hIoctl) {
        g_ioctlThread = hIoctl;
    }

    // StubHardening: deferred scan inline syscall trong stub.dll
    // Chờ stub.dll load (tương tự IoctlFilter), rồi NOP inline NtTerminateProcess
    HANDLE hStub = CreateThread(nullptr, 0, DeferredStubHardeningThread, nullptr, 0, nullptr);
    DbgLogFmt("[SH] CreateThread(StubHardening) = %p (err=%lu)\r\n", (void*)hStub, GetLastError());
    if (hStub) {
        g_stubHardThread = hStub;
    }

    const std::uint32_t moduleSize =
        NightSharpDebug::CrashBridge::ImageSize(hModule);
    NightSharpDebug::Logf(
        "[NightSharp] Captured module metadata base=%p size=0x%X before header scrub",
        hModule,
        moduleSize);
    StartOverlayWorker(hModule, moduleSize);

    // Module E — PEB Ldr Unlink. Skip for manual-mapped images (no LDR entry exists).
    // Gọi CUỐI, sau khi overlay worker + deferred CRC thread đã spawn.
    if (InterlockedCompareExchange(&g_isManualMapped, 0, 0) == 0) {
        const int nUnlinked = PebHide::HideAndErase(hModule);
        DbgLogFmt("[PEB] HideAndErase: unlinked %d/3 list(s) for module=%p\r\n",
                  nUnlinked, hModule);
    } else {
        DbgLogFmt("[PEB] HideAndErase: SKIPPED (manual-mapped, no LDR entry)\r\n");
    }

    // Gap 2: Erase PE headers. Skip for manual-mapped images — injector may have
    // already erased headers, and the flag page pointer at SizeOfImage-8 would be
    // lost (though already read). Zero DOS+NT+section headers for LoadLibrary path.
    if (InterlockedCompareExchange(&g_isManualMapped, 0, 0) == 0) {
        const bool erased = PebHide::ErasePeHeaders(hModule);
        DbgLogFmt("[PEB] ErasePeHeaders: %s for module=%p\r\n",
                  erased ? "OK" : "FAIL", hModule);
    } else {
        DbgLogFmt("[PEB] ErasePeHeaders: SKIPPED (manual-mapped)\r\n");
    }
}

// ========================================================================
// ManualMapEntry — Proof-of-injection entry called by injector shellcode
// (manual-map / APC). Signals the injector via the flag page and exposes
// four visible/audible proofs: OutputDebugString + Beep + file marker +
// game window title change.
// ========================================================================
static volatile uint8_t* g_mmFlagPage = nullptr;
static volatile LONG g_startupRuns = 0;

static void MmFlag(int offset, uint8_t value) {
    if (g_mmFlagPage) g_mmFlagPage[offset] = value;
}

extern "C" __declspec(dllexport)
BOOL WINAPI ManualMapEntry(HMODULE hModule, DWORD reason, LPVOID reserved)
{
    if (reason != DLL_PROCESS_ATTACH) return TRUE;

    // Read flag page address placed by injector at SizeOfImage - 8
    __try {
        auto* dos = (IMAGE_DOS_HEADER*)hModule;
        auto* nt  = (IMAGE_NT_HEADERS64*)((uint8_t*)hModule + dos->e_lfanew);
        uintptr_t flagAddr = *(uintptr_t*)((uint8_t*)hModule + nt->OptionalHeader.SizeOfImage - 8);
        if (flagAddr > 0x10000)
            g_mmFlagPage = (volatile uint8_t*)flagAddr;
    } __except (1) {}

    MmFlag(13, 0x01);  // Entry called
    InterlockedExchange(&g_isManualMapped, 1);  // Mark manual-map path

    // ---- Proof #1: OutputDebugString (DebugView) ----
    OutputDebugStringA("[NightSharp] ManualMapEntry — injection successful!\n");

    // ---- Proof #2: Beep (audible confirmation) ----
    Beep(800, 200);

    // ---- Proof #3: File marker ----
    __try {
        HANDLE hFile = CreateFileA("C:\\nightsharp_injected.txt",
            GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hFile != INVALID_HANDLE_VALUE) {
            const char msg[] = "NightSharp v2.0 injection confirmed\r\n";
            DWORD written = 0;
            WriteFile(hFile, msg, sizeof(msg) - 1, &written, nullptr);
            CloseHandle(hFile);
        }
    } __except (1) {}

    // ---- Proof #4: Change game window title (visible!) ----
    __try {
        HWND hGame = FindWindowA("RiotWindowClass", nullptr);
        if (hGame) {
            SetWindowTextA(hGame, "League of Legends [NightSharp v2.0]");
        }
    } __except (1) {}

    MmFlag(14, 0x01);  // Proof mechanisms done

    // Boot the overlay runtime exactly as DllMain would, because manual-map
    // injection bypasses DllMain. Guarded so it runs only once even if the
    // loader also fires DllMain afterwards.
    if (InterlockedCompareExchange(&g_startupRuns, 1, 0) == 0) {
        PerformNightSharpStartup(hModule);
    }

    MmFlag(15, 0x01);  // Entry returning

    return TRUE;
}

// ========================================================================
// DllMain
// ========================================================================
BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID reserved) {
    switch (reason) {
    case DLL_PROCESS_ATTACH: {
        // Full overlay bootstrap is shared with ManualMapEntry
        // (manual-map injection bypasses DllMain, so it calls the same
        // PerformNightSharpStartup). Guarded to run once regardless of order.
        if (InterlockedCompareExchange(&g_startupRuns, 1, 0) == 0) {
            PerformNightSharpStartup(hModule);
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
