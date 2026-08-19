/*
 * NightSharp v2.0 — DLL Entry Point
 *
 * ManualMapEntry: called by injector shellcode via APC
 * Proof of injection: Beep + file marker + OutputDebugString
 * NO CRT init — WinAPI only to avoid deadlock.
 *
 * Build: Release x64, /MT, no BufferSecurityCheck, no CFG
 */

#include <Windows.h>
#include <cstdint>

#pragma comment(lib, "user32.lib")

// ========================================================================
// Telemetry flag page
// ========================================================================
static volatile uint8_t* g_mmFlagPage = nullptr;

static void MmFlag(int offset, uint8_t value) {
    if (g_mmFlagPage) g_mmFlagPage[offset] = value;
}

// ========================================================================
// ManualMapEntry — called by injector shellcode
// ========================================================================
extern "C" __declspec(dllexport)
BOOL WINAPI ManualMapEntry(HMODULE hModule, DWORD reason, LPVOID reserved)
{
    if (reason != DLL_PROCESS_ATTACH) return TRUE;

    // Read flag page from injector
    __try {
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
    } __except(1) {}

    // ---- Proof #4: Change game window title (visible!) ----
    __try {
        HWND hGame = FindWindowA("RiotWindowClass", nullptr);
        if (hGame) {
            SetWindowTextA(hGame, "League of Legends [NightSharp v2.0]");
        }
    } __except(1) {}

    MmFlag(14, 0x01);  // Proof mechanisms done
    MmFlag(15, 0x01);  // Entry returning

    return TRUE;
}

// ========================================================================
// DllMain — LoadLibrary fallback
// ========================================================================
BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID reserved)
{
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        OutputDebugStringA("[NightSharp] DllMain — LoadLibrary path\n");
    }
    return TRUE;
}
