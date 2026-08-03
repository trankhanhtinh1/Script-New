# Offset Crash Logger Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a crash-safe offset access ring buffer that writes `C:\Users\Public\offset_crash.txt` without changing game logic.

**Architecture:** A header-only logger owns fixed-size interlocked records and formats the latest records with Win32 file APIs. `Globals` records all generic memory reads/writes through `std::source_location`; `CrashTrace` records native-call context; `CrashReporter` flushes the report from existing handled and unhandled exception paths.

**Tech Stack:** C++20, Win32, MSVC v145, SEH, `std::source_location`

## Global Constraints

- Build with `E:\Visual Studio\Community\MSBuild\Current\Bin\MSBuild.exe` and toolset `v145`.
- Write the latest report to exactly `C:\Users\Public\offset_crash.txt`.
- Do not modify gameplay, orbwalker, target selector, spell, or offset resolution logic.
- Do not allocate memory or take locks in crash-report writing.
- Do not create a git commit unless explicitly requested.

---

### Task 1: Crash Logger Contract Tests

**Files:**
- Modify: `../NightSharpDebugConsole/tests/NightSharpCrashBridgeCompileTest.cpp`
- Test: `../NightSharpDebugConsole/NightSharpDebugConsoleTests.vcxproj`

**Interfaces:**
- Consumes: `NightSharpDebug::OffsetCrashLog::RecordMemory`, `RecordTrace`, and `WriteReport`.
- Produces: Assertions for report headers, exception details, source location, and ring ordering.

- [ ] **Step 1: Add a report-format test**

Create synthetic `EXCEPTION_RECORD`, `CONTEXT`, and `EXCEPTION_POINTERS`, record read/write/trace entries, write to a temporary file, then assert that the file contains `exception_code`, `fault_address`, `source=`, and the operation names.

- [ ] **Step 2: Build tests and verify failure**

Run: `& 'E:\Visual Studio\Community\MSBuild\Current\Bin\MSBuild.exe' '..\NightSharpDebugConsole\NightSharpDebugConsoleTests.vcxproj' /t:Build /p:Configuration=Release /p:Platform=x64 /m`

Expected: build fails because `OffsetCrashLogger.h` and its interfaces do not exist.

### Task 2: Fixed Ring And Report Writer

**Files:**
- Create: `OffsetCrashLogger.h`
- Modify: `NightSharp.vcxproj`

**Interfaces:**
- Produces: `RecordMemory(AccessKind, uintptr_t, size_t, source_location) noexcept`.
- Produces: `RecordTrace(uint32_t, uintptr_t, uint64_t..., source_location) noexcept`.
- Produces: `WriteReport(const char*, const char*, EXCEPTION_POINTERS*, HMODULE, uintptr_t) noexcept`.
- Produces: `WriteCrashReport(const char*, EXCEPTION_POINTERS*, HMODULE, uintptr_t) noexcept` using the fixed public path.

- [ ] **Step 1: Implement fixed record storage**

Use a power-of-two array, `InterlockedIncrement64`, a zero sequence during writes, `MemoryBarrier`, and a final sequence publication. Store only scalar values and static source strings.

- [ ] **Step 2: Implement crash-safe formatting**

Use stack buffers, `_snprintf_s`, `CreateFileA`, `WriteFile`, and `CloseHandle`. Write exception access mode from `ExceptionInformation[0]`, fault address from `[1]`, registers, module RVAs, and records in newest-first order.

- [ ] **Step 3: Register the header in the project**

Add `<ClInclude Include="OffsetCrashLogger.h" />` beside the other crash headers.

- [ ] **Step 4: Build tests and verify pass**

Run the Task 1 MSBuild command.

Expected: zero errors and all test assertions pass when the test executable runs.

### Task 3: Memory And Native Trace Instrumentation

**Files:**
- Modify: `core/Globals.h`
- Modify: `CrashTrace.h`

**Interfaces:**
- Consumes: logger methods from Task 2.
- Produces: transparent instrumentation on every existing generic read/write and native trace call.

- [ ] **Step 1: Instrument generic memory access**

Add a trailing default `std::source_location` parameter to `Read` and `Write`, and call `RecordMemory` immediately before the existing `__try` access. Preserve all validation, return values, and exception handlers.

- [ ] **Step 2: Instrument native traces**

Add a trailing default `std::source_location` parameter to `Record`, `RecordDetailed`, and `RecordExtended`, then mirror their tag, program counter, and arguments into `RecordTrace` before calling the existing crash bridge.

- [ ] **Step 3: Build the DLL**

Run: `& 'E:\Visual Studio\Community\MSBuild\Current\Bin\MSBuild.exe' 'NightSharp.vcxproj' /t:Build /p:Configuration=Release /p:Platform=x64 /m`

Expected: zero errors and zero warnings.

### Task 4: Existing Crash Handler Integration

**Files:**
- Modify: `CrashReporter.h`

**Interfaces:**
- Consumes: `WriteCrashReport` from Task 2 and `Globals::base`.
- Produces: latest offset report for handled and unhandled serious exceptions.

- [ ] **Step 1: Flush handled crashes**

Call `WriteCrashReport` in `LogAndDumpException` before the existing crash bridge capture.

- [ ] **Step 2: Flush unhandled crashes**

Call `WriteCrashReport` in `UnhandledFilter` before the existing crash bridge capture.

- [ ] **Step 3: Run focused and full validation**

Build and run `NightSharpDebugConsoleTests.exe`, then build `NightSharp.vcxproj` Release x64. Inspect the generated test report and confirm chronological ordering and required fields.

Expected: tests pass; DLL builds with zero errors and warnings; no gameplay code differs.
