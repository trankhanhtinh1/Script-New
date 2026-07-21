# Anti-Ban Upgrade Design

**Date:** 2026-07-20
**Status:** Approved (pending spec review)
**Score target:** 6.5/10 → 8.5/10

## Context

NightSharp hiện có anti-ban score 6.5/10. Ba điểm yếu chính:

1. **Stack Stealth (3/10):** 3 return addresses trong DllMain call stack trỏ vào NightSharp module. Packman import `RtlLookupFunctionEntry` + `RtlVirtualUnwind` (trong `sub_2E92C4`, 25KB anti-cheat function) — có khả năng walk stack và detect.

2. **Memory Stealth (4/10):** 146 MEM_PRIVATE + PAGE_EXECUTE regions. `AllocSectionRWX` hiện tại dùng `CreateFileMappingA` + `MapViewOfFile` nhưng Win11 vẫn hiện MEM_PRIVATE. Packman scan `MEM_PRIVATE + PAGE_EXECUTE` sẽ thấy tất cả code regions.

3. **Thread Stealth (6/10):** Main thread (DllMain) chưa set `ThreadHideFromDebugger`. Worker threads đã có qua `CreateThreadSpoofed` nhưng main thread vẫn expose.

## Packman Capabilities (verified via IDA 13338)

| Capability | Status | Evidence |
|---|---|---|
| Stack walk | YES | `RtlLookupFunctionEntry` + `RtlVirtualUnwind` in `sub_2E92C4` |
| Debugger detect | YES | `IsDebuggerPresent` + `CheckRemoteDebuggerPresent` |
| Kernel driver | YES | `DeviceIoControl` — ring0 communication |
| Memory scan | YES | `VirtualProtect` + `VirtualAlloc` + `VirtualAllocEx` |
| Module enum | YES | `GetModuleHandleW` + `GetModuleHandleExW` |
| Thread monitor | YES | `GetThreadTimes` + `CreateThread` |
| Nt* hooks | YES | `.stub2` segment (17MB jump table) |
| VEH walk | NO | No `AddVectoredExceptionHandler` import |
| StackWalk64 | NO | Uses low-level `RtlVirtualUnwind` instead |

## Design

### Module 1: Stack Stealth — Minimal DllMain + spoofed worker + spoof_call

**Approach:** C (combined minimal DllMain + spoof_call)

**Changes:**

#### 1a. Minimal DllMain

Current DllMain contains all init logic (PEB scrub, syscall init, HWBP clear, PebHide, CRC bypass, overlay). This creates 3 NS_MODULE return addresses on stack.

New DllMain:
```cpp
case DLL_PROCESS_ATTACH:
    // 1. Set ThreadHideFromDebugger on main thread (direct syscall)
    DirectSyscall::NtSetInformationThreadDirect(
        GetCurrentThread(), 0x11, nullptr, 0);

    // 2. Save module handle
    g_hModule = hModule;

    // 3. Spawn spoofed worker thread — all logic moves here
    HANDLE hWorker = CoreBypass::CreateThreadSpoofed(
        NightSharpWorker, hModule);
    if (hWorker) CloseHandle(hWorker);

    // 4. Return immediately — stack has only 1 NS frame
    return TRUE;
```

#### 1b. NightSharpWorker function

All existing DllMain logic moves to `NightSharpWorker`:
```cpp
static DWORD WINAPI NightSharpWorker(LPVOID param) {
    HMODULE hModule = reinterpret_cast<HMODULE>(param);

    // Set ThreadHideFromDebugger on worker thread
    DirectSyscall::NtSetInformationThreadDirect(
        GetCurrentThread(), 0x11, nullptr, 0);

    // --- All existing DllMain logic ---
    PebHide::ScrubDebugFlags();
    DirectSyscall::InitAll();
    DirectSyscall::DumpSyscallTable();
    HwBpDetect::CheckAndClear();
    ThreadInfoAudit::Audit();

    ResetDeferredCRCInstallShutdown();
    HANDLE hCrc = CoreBypass::CreateThreadSpoofed(DeferredCRCInstallThread, nullptr);
    // ...

    StartOverlayWorker(hModule);

    PebHide::HideAndErase(hModule);
    MemRegionAudit::Audit();
    StackAudit::Audit(hModule);

    // Wait for shutdown (NEW: g_shutdownEvent cần tạo thêm — HANDLE tạo bằng CreateEvent)
    static HANDLE g_shutdownEvent = nullptr; // thêm vào globals
    WaitForSingleObject(g_shutdownEvent, INFINITE);
    return 0;
}
```

#### 1c. spoof_call for API calls in worker

Wrap Windows API calls in worker thread using existing `spoof_call` infrastructure:

```cpp
// NEW: ResolveCallGadget — tìm "call [rbx]" hoặc "jmp [rbx]" gadget trong game .text
// Tương tự ResolveThreadBeginTrampoline nhưng tìm call gadget thay vì jmp gadget.
// Cần implement mới trong CoreBypass.h
auto callGadget = ResolveCallGadget();

// Usage: spoof_call return address points to game module
auto result = spoof_call(callGadget, SomeWindowsAPI, arg1, arg2);
```

**spoof_call mechanism (already implemented in spoof.asm):**
1. Pop return address (NS code) → save in shell_param
2. Replace with trampoline address (game module)
3. Jump to target function
4. On return, jump to fixup label → restore original return address
5. Stack walk sees: game_module → target_api (not NS_module → target_api)

**Scope — cụ thể các calls cần wrap:**
- `StackAudit::Audit()`: `VirtualQuery`, `GetModuleFileNameA` (2 calls)
- `MemRegionAudit::Audit()`: `GetSystemInfo` (1 call)
- `DebugLog`: `CreateFileA`, `WriteFile`, `CloseHandle` (3 calls)
- `CrashReporter::Install`: `AddVectoredExceptionHandler`, `SetUnhandledExceptionFilter` (2 calls)
- NOT needed for direct syscalls (already bypass hooks)
- NOT needed for `spoof_call` itself (assembly stub, no Windows API)

**Fallback:** If trampoline not found, call directly (same as current behavior).

### Module 2: Memory Stealth — NtCreateSection + NtMapViewOfSection direct

**Approach:** A (direct syscall section creation)

**Changes:**

#### 2a. Add 2 syscall entries

```
NtCreateSection      SSN 0x55  (fallback, verify via memory-hash)
NtMapViewOfSection   SSN 0x28  (fallback, verify via memory-hash)
```

Typedefs:
```cpp
using NtCreateSectionFn = LONG(NTAPI*)(PHANDLE, ACCESS_MASK, PVOID*, PLARGE_INTEGER, ULONG, ULONG, HANDLE);
using NtMapViewOfSectionFn = LONG(NTAPI*)(HANDLE, HANDLE, PVOID*, ULONG_PTR, SIZE_T, PLARGE_INTEGER, PSIZE_T, DWORD, ULONG, ULONG);
```

#### 2b. AllocSectionRWX_V2

Replace `AllocSectionRWX` implementation:
```cpp
inline void* AllocSectionRWX_V2(SIZE_T size) {
    HANDLE hSection = nullptr;
    LARGE_INTEGER sectionSize;
    sectionSize.QuadPart = size;

    // NtCreateSection — SEC_COMMIT, PAGE_EXECUTE_READWRITE, anonymous (pagefile-backed)
    LONG s1 = NtCreateSectionDirect(
        &hSection, SECTION_ALL_ACCESS, nullptr,
        &sectionSize, PAGE_EXECUTE_READWRITE, SEC_COMMIT, nullptr);
    if (s1 < 0 || !hSection) return nullptr;

    // NtMapViewOfSection — map into current process
    PVOID baseAddr = nullptr;
    SIZE_T viewSize = 0;
    LONG s2 = NtMapViewOfSectionDirect(
        hSection, GetCurrentProcess(), &baseAddr,
        0, size, nullptr, &viewSize, ViewShare, 0, PAGE_EXECUTE_READWRITE);

    CloseHandle(hSection); // view keeps section alive via kernel refcount

    if (s2 < 0 || !baseAddr) return nullptr;
    return baseAddr;
}
```

#### 2c. Replace all callers

Replace `AllocSectionRWX` calls with `AllocSectionRWX_V2`:
- `Corehook.h:448` — trampoline alloc
- `PackmanHook.h:484` — syscall stub alloc
- Any other callers

**Expected result:** Regions show `MEM_MAPPED` (0x40000) instead of `MEM_PRIVATE` (0x20000). Packman scan `MEM_PRIVATE + PAGE_EXECUTE` misses.

### Module 3: Thread Stealth — ThreadHideFromDebugger on main thread

**Approach:** A (set on main thread)

**Changes:**

Single line in DllMain (before spawning worker):
```cpp
DirectSyscall::NtSetInformationThreadDirect(
    GetCurrentThread(), 0x11, nullptr, 0);
```

Worker thread also sets it in entry function (via `NtSetInformationThreadDirect`).

**Result:** Both main + worker threads have `ThreadHideFromDebugger` set. Packman's `NtQueryInformationThread` hook is bypassed by direct syscall.

## Implementation Order

1. **Module 3 first** (1 line, zero risk, immediate benefit)
2. **Module 2 next** (new syscalls + replace alloc function)
3. **Module 1 last** (largest refactor — move DllMain logic to worker)

## Testing

After each module:
1. Build + inject
2. Check `%TEMP%\ph.log` for audit results
3. Verify:
   - Module 3: `[THAUD]` shows hide flag set
   - Module 2: `[MEMAUD]` shows reduced MEM_PRIVATE count
   - Module 1: `[STACKAUD]` shows 0-1 NS frames (down from 3)

## Risk Assessment

| Risk | Mitigation |
|---|---|
| NtCreateSection/NtMapViewOfSection SSN wrong | Fallback + memory-hash resolve (existing mechanism) |
| spoof_call trampoline not found | Fallback to direct call (current behavior) |
| Worker thread crash before init | CrashReporter already installed early |
| DllMain returns before init complete | Loader lock: DllMain should not wait for worker anyway |
