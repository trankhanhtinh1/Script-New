/*
 * NightSharp v2.0 — DLL Entry Point
 *
 * ManualMapEntry: called by injector shellcode via APC
 *   - WinAPI-only (no CRT) — proven safe baseline from dllmain copy.cpp
 *   - Proof mechanisms: Beep, file marker, window title, OutputDebugString
 *   - Spawns OverlayWorker thread for CRT-free ImGui overlay
 *
 * OverlayWorker: runs on clean thread (NOT APC context)
 *   - NO CRT init — no _initterm, no __security_init_cookie
 *   - Creates private heap for ImGui allocations
 *   - Runs D3D11 overlay with CRT-free menu
 *
 * Codex Review Compliance:
 *   - ManualMapEntry stays WinAPI-only (review #97)
 *   - Worker thread owns all ImGui/overlay work (review #97)
 *   - No _initterm (confirmed crashes even on clean thread — review #98)
 *   - sscanf patched directly in ImGui source files (review #21, #22)
 *   - All ImGui source files in build (imgui_tables.cpp included, sscanf→ns_sscanf)
 *
 * Build: Release x64, /MT, no BufferSecurityCheck, no CFG
 */

#include <Windows.h>
#include <cstdint>
#include <new>

// Phase 2 finalization (Apr 26/2026): legacy_stubs.h retired.
// Real CrashTelemetry pulled directly. Offsets come via core/offset.h
// (transitively through CoreRuntime.h consumers).
#include "core/CrashTelemetry.h"
#include "core/CoreRuntime.h"

#pragma comment(lib, "user32.lib")

// ========================================================================
// Global C++ allocation override
// Manual-map safe: route STL/new allocations to Win32 heap directly.
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
// Telemetry flag page — shared between injector & DLL via:
//     uintptr_t flagAddr = *(uintptr_t*)(base + imageSize - 8);
// Offsets used:
//   flag[0..15]  — injection telemetry (written by shellcode + entry)
//   flag[16]     — OverlayWorker progress    (0x30/0x31 = overlay exited)
//   flag[17]     — reserved
//   flag[18]     — INJECTOR→DLL unload request (0x01 = please shut down)
//   flag[19]     — DLL→INJECTOR overlay-exited-clean marker (optional)
// ========================================================================
static volatile uint8_t* g_mmFlagPage = nullptr;

static void MmFlag(int offset, uint8_t value) {
    if (g_mmFlagPage) g_mmFlagPage[offset] = value;
}

// Accessor used by Overlay::Run() to poll flag[18] for a remote unload
// request issued by the injector on Ctrl+R.
extern "C" __declspec(dllexport)
volatile uint8_t* NightSharp_GetFlagPage() {
    return g_mmFlagPage;
}

// ========================================================================
// Forward declare overlay
// ========================================================================
namespace Overlay { void Run(); }

// ========================================================================
// Overlay worker thread — CRT-free ImGui menu
// ========================================================================
static DWORD WINAPI OverlayWorker(LPVOID param)
{
    CrashTelemetry::SetStage("OverlayWorker::Enter");
    MmFlag(16, 0x21);  // Worker entered
    OutputDebugStringA("[NightSharp] OverlayWorker entered\n");

    // ── Key Verification (TEMPORARILY DISABLED) ──
    /*
    {
        CrashTelemetry::SetStage("OverlayWorker::KeyAuth");
        OutputDebugStringA("[NightSharp] Verifying license key...\n");

        KeyAuth::Status authStatus = KeyAuth::Verify();

        if (authStatus != KeyAuth::Status::Valid) {
            char msg[512] = {};
            wsprintfA(msg, "[NightSharp] Key verification FAILED: %s\n", KeyAuth::GetStatusText());
            OutputDebugStringA(msg);

            // Show error to user via MessageBox
            char userMsg[512] = {};
            switch (authStatus) {
                case KeyAuth::Status::Expired:
                    wsprintfA(userMsg, "NightSharp: Key has expired!\n\nPlease renew your license.");
                    break;
                case KeyAuth::Status::InvalidKey:
                    wsprintfA(userMsg, "NightSharp: Invalid key!\n\nError: %s", KeyAuth::GetErrorMessage());
                    break;
                case KeyAuth::Status::HWIDMismatch:
                    wsprintfA(userMsg, "NightSharp: HWID mismatch!\n\nPlease reset your HWID in the loader.");
                    break;
                case KeyAuth::Status::NetworkError:
                    wsprintfA(userMsg, "NightSharp: Cannot connect to server!\n\nError: %s", KeyAuth::GetErrorMessage());
                    break;
                case KeyAuth::Status::FileNotFound:
                    wsprintfA(userMsg, "NightSharp: License key not found!\n\nPlease login via NightSharp Loader first.");
                    break;
                default:
                    wsprintfA(userMsg, "NightSharp: Authentication failed!\n\nError: %s", KeyAuth::GetErrorMessage());
                    break;
            }

            // Show on separate thread to prevent blocking game
            struct MsgData { char text[512]; };
            static MsgData data;
            lstrcpyA(data.text, userMsg);
            CreateThread(nullptr, 0, [](LPVOID p) -> DWORD {
                MsgData* d = (MsgData*)p;
                MessageBoxA(NULL, d->text, "NightSharp - License Error", MB_OK | MB_ICONERROR);
                return 0;
            }, &data, 0, nullptr);

            MmFlag(16, 0xAA);  // Auth failed
            CrashTelemetry::SetStage("OverlayWorker::KeyAuthFailed");
            return 0;  // Exit worker — don't load overlay
        }

        // Key valid!
        char successMsg[256] = {};
        wsprintfA(successMsg, "[NightSharp] Key verified! Remaining: %s\n", KeyAuth::GetRemainingTime());
        OutputDebugStringA(successMsg);
        MmFlag(16, 0x20);  // Key OK
    }
    */

    // ── Wait for game to fully load (GameTime > 3.0f = on fountain) ──
    // Matches leagueoflegends-master pattern: prevents menu from showing
    // during loading screen.
    {
        CrashTelemetry::SetStage("OverlayWorker::WaitForGame");
        OutputDebugStringA("[NightSharp] Waiting for game to load (GameTime > 3.0)...\n");

        const uintptr_t base = (uintptr_t)GetModuleHandle(nullptr);
        const uintptr_t gameTimeAddr = base + Offset::Global::GameTime;

        for (int i = 0; i < 240; ++i) {  // max 240 * 500ms = 120 sec
            __try {
                if (gameTimeAddr > 0x10000) {
                    float gt = *(volatile float*)gameTimeAddr;
                    if (gt > 3.0f && gt < 1000000.0f) {
                        char buf[128] = {};
                        wsprintfA(buf, "[NightSharp] Game loaded! GameTime = %d.%02d\r\n",
                            (int)gt, (int)(gt * 100) % 100);
                        //OutputDebugStringA(buf);
                        break;
                    }
                }
            } __except(1) {}

            if (i > 0 && (i % 20) == 0) {
                char buf[128] = {};
                wsprintfA(buf, "[NightSharp] Still waiting... (%d sec)\r\n", (i * 500) / 1000);
                OutputDebugStringA(buf);
            }
            Sleep(500);
        }

        OutputDebugStringA("[NightSharp] Game ready, starting overlay\n");
    }

    MmFlag(16, 0x22);  // About to start overlay
    CrashTelemetry::SetStage("OverlayWorker::OverlayRun");

    __try {
        Overlay::Run();
        MmFlag(16, 0x30);  // Overlay exited normally
        OutputDebugStringA("[NightSharp] Overlay::Run() exited normally\n");
    } __except(CrashTelemetry::ReportAndHandle("OverlayWorker", GetExceptionInformation())) {
        MmFlag(16, 0xEF);  // Overlay crashed
        OutputDebugStringA("[NightSharp] Overlay::Run() CRASHED!\n");
    }

    CrashTelemetry::SetStage("OverlayWorker::Exit");
    MmFlag(16, 0x31);  // Worker exiting
    OutputDebugStringA("[NightSharp] OverlayWorker exiting\n");
    return 0;
}

// ========================================================================
// ManualMapEntry — called by injector shellcode (WinAPI-only, NO CRT)
// Identical to dllmain copy.cpp + CreateThread for overlay worker
// ========================================================================
// NOTE on DLL_PROCESS_DETACH: we intentionally do NOT perform a clean
// shutdown here. The manual-map injector does not deliver a legitimate
// DETACH (there is no loader entry to invoke), so this path never fires
// during normal operation. The user tears the current session down by
// pressing VK_END inside the game — that hotkey drives the overlay's
// own cleanup (which calls `CoreEventHook::UninstallAll()` and exits the
// render loop). Re-inject from the external injector afterwards.
extern "C" __declspec(dllexport)
BOOL WINAPI ManualMapEntry(HMODULE hModule, DWORD reason, LPVOID reserved)
{
    (void)hModule; (void)reserved;

    if (reason != DLL_PROCESS_ATTACH) return TRUE;

    CrashTelemetry::Install();
    CrashTelemetry::SetStage("ManualMapEntry::Begin");

    // Read flag page from injector
    __try {
        CrashTelemetry::SetStage("ManualMapEntry::ReadFlagPage");
        auto* dos = (IMAGE_DOS_HEADER*)hModule;
        auto* nt  = (IMAGE_NT_HEADERS64*)((uint8_t*)hModule + dos->e_lfanew);
        uintptr_t flagAddr = *(uintptr_t*)((uint8_t*)hModule + nt->OptionalHeader.SizeOfImage - 8);
        if (flagAddr > 0x10000)
            g_mmFlagPage = (volatile uint8_t*)flagAddr;
    } __except(1) {}

    MmFlag(13, 0x01);  // Entry called

    // ---- Proof #1: OutputDebugString (DebugView) ----
    OutputDebugStringA("[NightSharp] ManualMapEntry — injection successful!\n");

    // ---- Proof #2: Beep (audible confirmation) ----
    CrashTelemetry::SetStage("ManualMapEntry::Beep");
    Beep(800, 200);

    // ---- Proof #3: File marker ----
    __try {
        CrashTelemetry::SetStage("ManualMapEntry::WriteFileMarker");
        HANDLE hFile = CreateFileA("C:\\nightsharp_injected.txt",
            GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hFile != INVALID_HANDLE_VALUE) {
            const char msg[] = "NightSharp injection confirmed\r\n";
            DWORD written = 0;
            WriteFile(hFile, msg, sizeof(msg) - 1, &written, nullptr);
            CloseHandle(hFile);
        }
    } __except(1) {}

    // ---- Proof #4: Change game window title (visible!) ----
    __try {
        CrashTelemetry::SetStage("ManualMapEntry::SetWindowText");
        HWND hGame = FindWindowA("RiotWindowClass", nullptr);
        if (hGame) {
            SetWindowTextA(hGame, "League of Legends [NightSharp]");
        }
    } __except(1) {}

    MmFlag(14, 0x01);  // Proof mechanisms done

    // ---- Spawn overlay worker thread ----
    // Worker handles: HeapCreate → ImGui allocator → D3D11 overlay → menu render
    CrashTelemetry::SetStage("ManualMapEntry::CreateOverlayThread");
    HANDLE hThread = CreateThread(nullptr, 0, OverlayWorker, hModule, 0, nullptr);
    if (hThread) {
        CloseHandle(hThread);
        MmFlag(15, 0x02);  // Thread created
        OutputDebugStringA("[NightSharp] Overlay worker thread created\n");
    } else {
        MmFlag(15, 0xEE);  // Thread creation failed
        OutputDebugStringA("[NightSharp] Failed to create overlay worker thread\n");
    }

    CrashTelemetry::SetStage("ManualMapEntry::Return");
    MmFlag(15, 0x01);  // Entry returning
    return TRUE;
}

// ========================================================================
// DllMain — LoadLibrary fallback (not used in manual map path)
// ========================================================================
BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID reserved)
{
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        OutputDebugStringA("[NightSharp] DllMain — LoadLibrary path\n");
    }
    return TRUE;
}
