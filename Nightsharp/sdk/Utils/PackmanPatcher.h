#pragma once
// ============================================================================
// PackmanPatcher.h — Neutralize Riot Packman Anti-Cheat (stub.dll)
//
// Full reverse engineering via IDA MCP on stub.dll (rebased to 0).
//
// ============================================================================
// ARCHITECTURE OF PACKMAN's DETECTION SYSTEM:
//
// 1. DETECTION ENGINE (sub_C9C40, size 0x1C9C)
//    Main function with 11 anti-debug checks selected via rdtsc % 11:
//    Case 0:  PEB->BeingDebugged
//    Case 1:  PEB->NtGlobalFlag & 0x70 (HEAP flags)
//    Case 2:  sub_28720 -> sub_287B0 -> __debugbreak() (INT3 trap)
//    Case 3:  sub_28750 -> sub_287C0 -> __int2c()       (INT 2C trap)
//    Case 4:  IsDebuggerPresent() (Win32 API)
//    Case 5:  CheckRemoteDebuggerPresent() (Win32 API)
//    Case 6:  NtQueryInformationThread(DebugPort=7) via VirtualAlloc syscall shellcode
//    Case 7:  NtQueryInformationThread(DebugObjectHandle=0x1E) via syscall
//    Case 8:  SharedUserData->KdDebuggerEnabled (Kernel debugger)
//    Case 9:  NtQuerySystemInformation(SystemCodeIntegrity=0x67) via syscall
//    Case 10: PEB->Ldr module scan with FNV-1a hash 0xF2BB46C9BBD0AC40
//             (scans for suspicious DLLs in loaded module list)
//    All cases call sub_9BE90(case_id) when detection triggers.
//
// 2. REPORT FUNCTION (sub_9BE90)
//    Called with detection case ID. Logs detection details into a B-tree,
//    writes detection markers to PEB area (offset 2888 from PEB base),
//    then calls sub_31510 which:
//      - Writes detection info to PEB spinlock-guarded structure
//      - Calls sub_FB4F0 which calls sub_C9480 and sub_2FF90
//      - sub_2FF90 calls NtQuerySystemInformation(57) to enumerate
//        all system modules and compares them with FNV-1a hashes
//
// 3. KILL FUNCTIONS (TerminateProcess callers)
//    sub_C9990: Shows MessageBox then TerminateProcess (exit code 0xFFFFFFFE)
//    sub_65F30: Direct TerminateProcess (exit code 0)
//    sub_7FF90: Crash dump handler; SetTimer -> TimerFunc; then TerminateProcess
//    TimerFunc (0x10E550): Timer callback that fires TerminateProcess (0xFFFFFFFD)
//    sub_2C4940: Obfuscated TerminateProcess
//    sub_29B28C: Obfuscated TerminateProcess via jmp rax
//
// 4. SECONDARY DETECTION (sub_F2740, sub_F6A60, sub_3080B7)
//    Additional detection engines that also call sub_9BE90
//    sub_F2740: Uses IsDebuggerPresent, multiple report calls
//    sub_F6A60: Similar pattern, 3 report calls
//    sub_3080B7: Direct PEB->BeingDebugged check in .stub2 section
//
// 5. INT3/INT2C TRAP GENERATORS
//    sub_287B0: Executes __debugbreak() (INT3) — used by Case 2
//    sub_287C0: Executes __int2c() (INT 2C)  — used by Case 3
//
// 6. MODULE ENUM (sub_2FF90)
//    Called from report chain. Uses NtQuerySystemInformation(57)
//    to enumerate ALL system modules. Walks PEB->Ldr module list,
//    resolves NtQuerySystemInformation via FNV-1a hash
//    (hash 0x7FBAA47E01567AB8 = "NtQuerySystemInformation").
//    Compares loaded module names against known DLL name
//    (hash 0xBB7BB9A74C2F14FB = "ntdll.dll").
//
// STRATEGY:
//   Patch detection functions to return 0 immediately, so no detection
//   data is ever collected or reported. Also patch kill functions to
//   prevent process termination, and INT3/INT2C traps to prevent crashes.
//
// Usage: Call PackmanPatcher::PatchAll() from DLL_PROCESS_ATTACH.
// ============================================================================

#include <Windows.h>
#include <Psapi.h>

#pragma comment(lib, "Psapi.lib")

namespace PackmanPatcher {

    // ==================================================================
    // Stub.dll offsets (rebased to 0, added to actual module base)
    // Updated 2026-03-12 for new stub.dll (0x1399000 size)
    // ==================================================================
    namespace StubOff {
        // --- Detection Engines ---
        constexpr uintptr_t DetectionMain       = 0xC9A00;  // sub_C9A00
        constexpr uintptr_t DetectionSecondary  = 0xEFC10;  // sub_EFC10
        constexpr uintptr_t DetectionTertiary   = 0xF4C10;  // sub_F4C10

        // --- Report & Data Collection ---
        constexpr uintptr_t ReportFunction      = 0x9B040;  // sub_9B040: PEB reporter (IDA xref verified)
        constexpr uintptr_t ReportToProcess     = 0x31080;  // sub_31080: PEB data writer
        // ReportChain: inlined by compiler in this build, no standalone function
        constexpr uintptr_t ModuleEnumerator    = 0x2FF90;  // sub_2FF90 (unchanged)

        // --- Kill Functions (TerminateProcess) ---
        constexpr uintptr_t KillMsgBox          = 0xC9750;  // sub_C9750
        constexpr uintptr_t KillDirect          = 0x63B30;  // sub_63B30
        constexpr uintptr_t KillCrashDump       = 0x7F7C0;  // sub_7F7C0
        constexpr uintptr_t KillTimer           = 0x10E140; // TimerFunc: 0x10E140

        // --- INT3 / INT2C Trap Generators ---
        constexpr uintptr_t TrapINT3            = 0x28700;  // sub_28700
        constexpr uintptr_t TrapINT2C           = 0x28700;  // sub_28700
    }

    // Find stub.dll base address by scanning loaded modules
    inline uintptr_t FindStubBase() {
        HMODULE modules[1024];
        DWORD needed = 0;
        if (!EnumProcessModules(GetCurrentProcess(), modules, sizeof(modules), &needed))
            return 0;

        int count = needed / sizeof(HMODULE);
        for (int i = 0; i < count; i++) {
            wchar_t name[MAX_PATH] = {};
            GetModuleFileNameW(modules[i], name, MAX_PATH);

            wchar_t* filename = wcsrchr(name, L'\\');
            if (!filename) filename = name; else filename++;

            if (_wcsicmp(filename, L"stub.dll") == 0) {
                return (uintptr_t)modules[i];
            }
        }
        return 0;
    }

    // Patch bytes at address via VirtualProtect
    inline bool PatchBytes(uintptr_t address, const uint8_t* patch, size_t size) {
        DWORD oldProtect = 0;
        if (!VirtualProtect((LPVOID)address, size, PAGE_EXECUTE_READWRITE, &oldProtect))
            return false;
        memcpy((void*)address, patch, size);
        DWORD dummy = 0;
        VirtualProtect((LPVOID)address, size, oldProtect, &dummy);
        return true;
    }

    // Patch function to: xor eax, eax; ret  (return 0, __int64)
    inline bool ReturnZero(uintptr_t addr) {
        // 48 31 C0 = xor rax, rax
        // C3       = ret
        static const uint8_t p[] = { 0x48, 0x31, 0xC0, 0xC3 };
        return PatchBytes(addr, p, sizeof(p));
    }

    // Patch function to: ret (void functions, just return immediately)
    inline bool ReturnVoid(uintptr_t addr) {
        static const uint8_t p[] = { 0xC3 };
        return PatchBytes(addr, p, sizeof(p));
    }

    // ===================================================================
    // PatchAll — call once from DLL_PROCESS_ATTACH
    // Returns number of successful patches, -1 if stub.dll not found
    // ===================================================================
    inline int PatchAll() {
        uintptr_t base = FindStubBase();
        if (!base) return -1;

        int ok = 0;

        // =========================================================
        // TIER 1: Core Detection (MOST CRITICAL)
        // =========================================================

        // 1. Main Detection Engine — 11-case anti-debug switch
        if (ReturnZero(base + StubOff::DetectionMain))     ok++;

        // 2. Secondary Detection Engines
        if (ReturnZero(base + StubOff::DetectionSecondary)) ok++;
        if (ReturnZero(base + StubOff::DetectionTertiary))  ok++;

        // =========================================================
        // TIER 2: Report Chain (prevent data from being sent)
        // =========================================================

        // 3. Report Function — prevents any detection report (PEB writer)
        if (ReturnVoid(base + StubOff::ReportFunction))    ok++;

        // 4. Report-to-PEB writer — prevents PEB marking
        if (ReturnZero(base + StubOff::ReportToProcess))   ok++;

        // 5. Report chain — inlined in this build, no standalone function to patch

        // 6. Module Enumerator (NtQuerySystemInformation walker)
        if (ReturnVoid(base + StubOff::ModuleEnumerator))  ok++;

        // =========================================================
        // TIER 3: Kill Functions (prevent TerminateProcess)
        // =========================================================

        // 7. Kill via MessageBox + Terminate
        if (ReturnVoid(base + StubOff::KillMsgBox))        ok++;

        // 8. Kill Direct
        if (ReturnVoid(base + StubOff::KillDirect))        ok++;

        // 9. Kill via crash dump handler
        if (ReturnVoid(base + StubOff::KillCrashDump))     ok++;

        // 10. Kill via timer callback
        if (ReturnVoid(base + StubOff::KillTimer))         ok++;

        // =========================================================
        // TIER 4: INT3 / INT2C Traps (prevent crash on debug check)
        // =========================================================

        // 11. INT3 trap -> just RET
        if (ReturnVoid(base + StubOff::TrapINT3))          ok++;

        // 12. INT2C trap -> just RET
        if (ReturnVoid(base + StubOff::TrapINT2C))         ok++;

        return ok;
    }

    // Delayed patch: retry if stub.dll wasn't loaded during DLL_PROCESS_ATTACH
    inline void PatchWithRetry(int maxRetries = 30, int delayMs = 300) {
        for (int i = 0; i < maxRetries; i++) {
            int result = PatchAll();
            if (result > 0) return;  // Success
            Sleep(delayMs);
        }
    }

} // namespace PackmanPatcher
