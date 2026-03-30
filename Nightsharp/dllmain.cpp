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

#include "core/CrashTelemetry.h"

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
// Telemetry flag page
// ========================================================================
static volatile uint8_t* g_mmFlagPage = nullptr;

static void MmFlag(int offset, uint8_t value) {
    if (g_mmFlagPage) g_mmFlagPage[offset] = value;
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
extern "C" __declspec(dllexport)
BOOL WINAPI ManualMapEntry(HMODULE hModule, DWORD reason, LPVOID reserved)
{
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
