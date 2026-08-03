# Offset Crash Logger Design

## Goal

Create `C:\Users\Public\offset_crash.txt` when NightSharp encounters a serious handled or unhandled exception, with enough recent memory-access context to identify the source offset without changing game behavior.

## Chosen Approach

Use a fixed-size lock-free ring buffer in the injected DLL. Every `Globals::Read`, `Globals::Write`, and existing `CrashTrace` call records metadata in RAM only. Crash handlers flush the ring to the requested file.

## Recorded Data

- Monotonic sequence, timestamp, and thread ID.
- Operation kind: read, write, or native trace.
- Access address and byte size.
- Native trace tag and arguments already supplied by call/cast/order paths.
- Source file, source line, and function captured with `std::source_location`.
- Exception code, exception instruction, access type, fault address, registers, game base, NightSharp base, and both RVAs when applicable.

## Crash Integration

- `CrashReporter::LogAndDumpException` writes the report for handled top-level exceptions.
- `CrashReporter::UnhandledFilter` writes the report for unhandled exceptions.
- Existing minidump and crash bridge behavior remains unchanged.
- The report uses `CREATE_ALWAYS`, so it always represents the latest captured crash.

## Safety

- No heap allocation, STL containers, streams, mutexes, or game-memory reads occur while writing the crash report.
- Ring entries use fixed storage and interlocked publication.
- Recording happens immediately before memory access or native trace dispatch.
- Logger failures never alter the return value or exception behavior of existing code.

## Limitations

The report identifies the exact source line and recent resolved address. For field offsets, the runtime cannot mathematically recover the object base from a final absolute address; the source line is therefore the authoritative mapping back to the named constant in `core/offset.h`.
