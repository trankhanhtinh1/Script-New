#pragma once

#include <Windows.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <source_location>

#ifndef NIGHTSHARP_ENABLE_OFFSET_CRASH_TRACE
#define NIGHTSHARP_ENABLE_OFFSET_CRASH_TRACE 0
#endif

namespace NightSharpDebug::OffsetCrashLog {

inline constexpr const char* kCrashReportPath =
    "C:\\Users\\Public\\offset_crash.txt";
inline constexpr std::size_t kRecordCapacity = 256;
inline constexpr std::size_t kTraceArgumentCapacity = 10;

enum class Operation : std::uint32_t {
    None = 0,
    Read = 1,
    Write = 2,
    Trace = 3,
};

struct Record {
    volatile LONG64 sequence{};
    std::uint64_t tick{};
    std::uint32_t threadId{};
    Operation operation{Operation::None};
    std::uint32_t accessSize{};
    std::uint32_t traceTag{};
    std::uint32_t traceArgumentCount{};
    std::uintptr_t address{};
    std::uintptr_t programCounter{};
    std::uint64_t traceArguments[kTraceArgumentCapacity]{};
    const char* sourceFile{};
    const char* functionName{};
    std::uint32_t sourceLine{};
    std::uint32_t sourceColumn{};
};

struct RecordSnapshot {
    LONG64 sequence{};
    std::uint64_t tick{};
    std::uint32_t threadId{};
    Operation operation{Operation::None};
    std::uint32_t accessSize{};
    std::uint32_t traceTag{};
    std::uint32_t traceArgumentCount{};
    std::uintptr_t address{};
    std::uintptr_t programCounter{};
    std::uint64_t traceArguments[kTraceArgumentCapacity]{};
    const char* sourceFile{};
    const char* functionName{};
    std::uint32_t sourceLine{};
    std::uint32_t sourceColumn{};
};

inline Record g_records[kRecordCapacity]{};
inline volatile LONG64 g_recordSequence = 0;
inline volatile LONG g_reportWriting = 0;

inline const char* OperationName(Operation operation) noexcept {
    switch (operation) {
    case Operation::Read:
        return "read";
    case Operation::Write:
        return "write";
    case Operation::Trace:
        return "trace";
    default:
        return "unknown";
    }
}

inline void PublishRecord(
    Operation operation,
    std::uintptr_t address,
    std::size_t accessSize,
    std::uint32_t traceTag,
    std::uintptr_t programCounter,
    const std::uint64_t* traceArguments,
    std::size_t traceArgumentCount,
    const std::source_location& location) noexcept {
    const LONG64 sequence = InterlockedIncrement64(&g_recordSequence);
    Record& record = g_records[
        static_cast<std::size_t>(sequence) % kRecordCapacity];

    InterlockedExchange64(&record.sequence, 0);
    record.tick = GetTickCount64();
    record.threadId = GetCurrentThreadId();
    record.operation = operation;
    record.accessSize = static_cast<std::uint32_t>(
        (std::min)(accessSize, static_cast<std::size_t>(UINT32_MAX)));
    record.traceTag = traceTag;
    record.traceArgumentCount = static_cast<std::uint32_t>(
        (std::min)(traceArgumentCount, kTraceArgumentCapacity));
    record.address = address;
    record.programCounter = programCounter;
    for (std::size_t index = 0; index < kTraceArgumentCapacity; ++index) {
        record.traceArguments[index] =
            traceArguments && index < record.traceArgumentCount
                ? traceArguments[index]
                : 0;
    }
    record.sourceFile = location.file_name();
    record.functionName = location.function_name();
    record.sourceLine = location.line();
    record.sourceColumn = location.column();
    MemoryBarrier();
    InterlockedExchange64(&record.sequence, sequence);
}

inline void RecordMemory(
    Operation operation,
    std::uintptr_t address,
    std::size_t accessSize,
    const std::source_location& location =
        std::source_location::current()) noexcept {
#if NIGHTSHARP_ENABLE_OFFSET_CRASH_TRACE
    PublishRecord(
        operation,
        address,
        accessSize,
        0,
        0,
        nullptr,
        0,
        location);
#else
    (void)operation;
    (void)address;
    (void)accessSize;
    (void)location;
#endif
}

inline void RecordTrace(
    std::uint32_t traceTag,
    std::uintptr_t programCounter,
    const std::uint64_t* traceArguments,
    std::size_t traceArgumentCount,
    const std::source_location& location =
        std::source_location::current()) noexcept {
#if NIGHTSHARP_ENABLE_OFFSET_CRASH_TRACE
    PublishRecord(
        Operation::Trace,
        0,
        0,
        traceTag,
        programCounter,
        traceArguments,
        traceArgumentCount,
        location);
#else
    (void)traceTag;
    (void)programCounter;
    (void)traceArguments;
    (void)traceArgumentCount;
    (void)location;
#endif
}

inline bool SnapshotRecord(LONG64 sequence, RecordSnapshot& snapshot) noexcept {
    if (sequence <= 0) {
        return false;
    }

    Record& record = g_records[
        static_cast<std::size_t>(sequence) % kRecordCapacity];
    const LONG64 published = InterlockedCompareExchange64(
        &record.sequence,
        0,
        0);
    if (published != sequence) {
        return false;
    }

    snapshot.sequence = published;
    snapshot.tick = record.tick;
    snapshot.threadId = record.threadId;
    snapshot.operation = record.operation;
    snapshot.accessSize = record.accessSize;
    snapshot.traceTag = record.traceTag;
    snapshot.traceArgumentCount = record.traceArgumentCount;
    snapshot.address = record.address;
    snapshot.programCounter = record.programCounter;
    for (std::size_t index = 0; index < kTraceArgumentCapacity; ++index) {
        snapshot.traceArguments[index] = record.traceArguments[index];
    }
    snapshot.sourceFile = record.sourceFile;
    snapshot.functionName = record.functionName;
    snapshot.sourceLine = record.sourceLine;
    snapshot.sourceColumn = record.sourceColumn;
    MemoryBarrier();

    return InterlockedCompareExchange64(&record.sequence, 0, 0) == sequence;
}

inline std::uint32_t ImageSize(std::uintptr_t moduleBase) noexcept {
    if (!moduleBase) {
        return 0;
    }

    __try {
        const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(moduleBase);
        if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0) {
            return 0;
        }
        const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(
            moduleBase + static_cast<std::uintptr_t>(dos->e_lfanew));
        if (nt->Signature != IMAGE_NT_SIGNATURE) {
            return 0;
        }
        return nt->OptionalHeader.SizeOfImage;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}

inline bool IsInsideImage(
    std::uintptr_t address,
    std::uintptr_t moduleBase,
    std::uint32_t moduleSize) noexcept {
    if (!address || !moduleBase || !moduleSize || address < moduleBase) {
        return false;
    }
    return address - moduleBase < moduleSize;
}

inline void WriteText(HANDLE file, const char* text) noexcept {
    if (file == INVALID_HANDLE_VALUE || !text) {
        return;
    }
    DWORD written = 0;
    WriteFile(
        file,
        text,
        static_cast<DWORD>(lstrlenA(text)),
        &written,
        nullptr);
}

inline const char* AccessMode(EXCEPTION_POINTERS* exceptionPointers) noexcept {
    if (!exceptionPointers || !exceptionPointers->ExceptionRecord) {
        return "unknown";
    }

    const EXCEPTION_RECORD* record = exceptionPointers->ExceptionRecord;
    if (record->ExceptionCode != EXCEPTION_ACCESS_VIOLATION &&
        record->ExceptionCode != EXCEPTION_IN_PAGE_ERROR) {
        return "not-applicable";
    }
    if (record->NumberParameters < 1) {
        return "unknown";
    }

    switch (record->ExceptionInformation[0]) {
    case 0:
        return "read";
    case 1:
        return "write";
    case 8:
        return "execute";
    default:
        return "unknown";
    }
}

inline bool WriteReport(
    const char* path,
    const char* stage,
    EXCEPTION_POINTERS* exceptionPointers,
    HMODULE nightsharpModule,
    std::uintptr_t gameBase) noexcept {
    if (!path || !path[0] ||
        InterlockedCompareExchange(&g_reportWriting, 1, 0) != 0) {
        return false;
    }

    HANDLE file = CreateFileA(
        path,
        GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        InterlockedExchange(&g_reportWriting, 0);
        return false;
    }

    const std::uintptr_t nightsharpBase =
        reinterpret_cast<std::uintptr_t>(nightsharpModule);
    const std::uint32_t nightsharpSize = ImageSize(nightsharpBase);
    const std::uint32_t gameSize = ImageSize(gameBase);

    SYSTEMTIME time{};
    GetLocalTime(&time);

    char line[2048]{};
    _snprintf_s(
        line,
        sizeof(line),
        _TRUNCATE,
        "NightSharp offset crash report\r\n"
        "time=%04u-%02u-%02u %02u:%02u:%02u.%03u\r\n"
        "stage=%s\r\n"
        "pid=%lu\r\n"
        "tid=%lu\r\n"
        "game_base=0x%016llX\r\n"
        "game_size=0x%08X\r\n"
        "nightsharp_base=0x%016llX\r\n"
        "nightsharp_size=0x%08X\r\n",
        static_cast<unsigned>(time.wYear),
        static_cast<unsigned>(time.wMonth),
        static_cast<unsigned>(time.wDay),
        static_cast<unsigned>(time.wHour),
        static_cast<unsigned>(time.wMinute),
        static_cast<unsigned>(time.wSecond),
        static_cast<unsigned>(time.wMilliseconds),
        stage && stage[0] ? stage : "unknown",
        static_cast<unsigned long>(GetCurrentProcessId()),
        static_cast<unsigned long>(GetCurrentThreadId()),
        static_cast<unsigned long long>(gameBase),
        gameSize,
        static_cast<unsigned long long>(nightsharpBase),
        nightsharpSize);
    WriteText(file, line);

    if (exceptionPointers && exceptionPointers->ExceptionRecord) {
        const EXCEPTION_RECORD* record = exceptionPointers->ExceptionRecord;
        const std::uintptr_t exceptionAddress =
            reinterpret_cast<std::uintptr_t>(record->ExceptionAddress);
        const std::uint64_t faultAddress = record->NumberParameters > 1
            ? static_cast<std::uint64_t>(record->ExceptionInformation[1])
            : 0;
        _snprintf_s(
            line,
            sizeof(line),
            _TRUNCATE,
            "exception_code=0x%08lX\r\n"
            "exception_instruction=0x%016llX\r\n"
            "access_mode=%s\r\n"
            "fault_address=0x%016llX\r\n",
            static_cast<unsigned long>(record->ExceptionCode),
            static_cast<unsigned long long>(exceptionAddress),
            AccessMode(exceptionPointers),
            static_cast<unsigned long long>(faultAddress));
        WriteText(file, line);

        if (IsInsideImage(exceptionAddress, gameBase, gameSize)) {
            _snprintf_s(
                line,
                sizeof(line),
                _TRUNCATE,
                "exception_game_rva=0x%llX\r\n",
                static_cast<unsigned long long>(exceptionAddress - gameBase));
            WriteText(file, line);
        }
        if (IsInsideImage(exceptionAddress, nightsharpBase, nightsharpSize)) {
            _snprintf_s(
                line,
                sizeof(line),
                _TRUNCATE,
                "exception_nightsharp_rva=0x%llX\r\n",
                static_cast<unsigned long long>(exceptionAddress - nightsharpBase));
            WriteText(file, line);
        }
    }

#if defined(_M_X64)
    if (exceptionPointers && exceptionPointers->ContextRecord) {
        const CONTEXT* context = exceptionPointers->ContextRecord;
        _snprintf_s(
            line,
            sizeof(line),
            _TRUNCATE,
            "rip=0x%016llX rsp=0x%016llX rbp=0x%016llX\r\n"
            "rax=0x%016llX rbx=0x%016llX rcx=0x%016llX rdx=0x%016llX\r\n"
            "rsi=0x%016llX rdi=0x%016llX r8=0x%016llX r9=0x%016llX\r\n"
            "r10=0x%016llX r11=0x%016llX r12=0x%016llX r13=0x%016llX\r\n"
            "r14=0x%016llX r15=0x%016llX\r\n",
            static_cast<unsigned long long>(context->Rip),
            static_cast<unsigned long long>(context->Rsp),
            static_cast<unsigned long long>(context->Rbp),
            static_cast<unsigned long long>(context->Rax),
            static_cast<unsigned long long>(context->Rbx),
            static_cast<unsigned long long>(context->Rcx),
            static_cast<unsigned long long>(context->Rdx),
            static_cast<unsigned long long>(context->Rsi),
            static_cast<unsigned long long>(context->Rdi),
            static_cast<unsigned long long>(context->R8),
            static_cast<unsigned long long>(context->R9),
            static_cast<unsigned long long>(context->R10),
            static_cast<unsigned long long>(context->R11),
            static_cast<unsigned long long>(context->R12),
            static_cast<unsigned long long>(context->R13),
            static_cast<unsigned long long>(context->R14),
            static_cast<unsigned long long>(context->R15));
        WriteText(file, line);
    }
#endif

    const LONG64 latestSequence = InterlockedCompareExchange64(
        &g_recordSequence,
        0,
        0);
    const DWORD crashThreadId = GetCurrentThreadId();
    for (std::size_t distance = 0;
         distance < kRecordCapacity &&
         latestSequence - static_cast<LONG64>(distance) > 0;
         ++distance) {
        RecordSnapshot suspect{};
        const LONG64 sequence = latestSequence - static_cast<LONG64>(distance);
        if (!SnapshotRecord(sequence, suspect) ||
            suspect.threadId != crashThreadId) {
            continue;
        }

        _snprintf_s(
            line,
            sizeof(line),
            _TRUNCATE,
            "suspect_sequence=%lld\r\n"
            "suspect_operation=%s\r\n"
            "suspect_address=0x%016llX\r\n"
            "suspect_size=%u\r\n"
            "suspect_pc=0x%016llX\r\n"
            "suspect_trace_tag=%u\r\n"
            "suspect_source=%s:%u:%u\r\n"
            "suspect_function=%s\r\n",
            static_cast<long long>(suspect.sequence),
            OperationName(suspect.operation),
            static_cast<unsigned long long>(suspect.address),
            suspect.accessSize,
            static_cast<unsigned long long>(suspect.programCounter),
            suspect.traceTag,
            suspect.sourceFile ? suspect.sourceFile : "unknown",
            suspect.sourceLine,
            suspect.sourceColumn,
            suspect.functionName ? suspect.functionName : "unknown");
        WriteText(file, line);

        const std::size_t argumentCount = (std::min)(
            static_cast<std::size_t>(suspect.traceArgumentCount),
            kTraceArgumentCapacity);
        for (std::size_t argumentIndex = 0;
             argumentIndex < argumentCount;
             ++argumentIndex) {
            _snprintf_s(
                line,
                sizeof(line),
                _TRUNCATE,
                "suspect_trace_arg%zu=0x%016llX\r\n",
                argumentIndex,
                static_cast<unsigned long long>(
                    suspect.traceArguments[argumentIndex]));
            WriteText(file, line);
        }
        break;
    }

    _snprintf_s(
        line,
        sizeof(line),
        _TRUNCATE,
        "record_sequence=%lld\r\n"
        "record_order=newest-first\r\n"
        "records_begin\r\n",
        static_cast<long long>(latestSequence));
    WriteText(file, line);

    std::size_t writtenRecords = 0;
    for (std::size_t distance = 0;
         distance < kRecordCapacity &&
         latestSequence - static_cast<LONG64>(distance) > 0;
         ++distance) {
        const LONG64 sequence = latestSequence - static_cast<LONG64>(distance);
        RecordSnapshot snapshot{};
        if (!SnapshotRecord(sequence, snapshot)) {
            continue;
        }

        _snprintf_s(
            line,
            sizeof(line),
            _TRUNCATE,
            "record[%03zu] sequence=%lld tick=%llu tid=%lu operation=%s "
            "address=0x%016llX size=%u pc=0x%016llX trace_tag=%u "
            "same_crash_thread=%u source=%s:%u:%u function=%s\r\n",
            writtenRecords,
            static_cast<long long>(snapshot.sequence),
            static_cast<unsigned long long>(snapshot.tick),
            static_cast<unsigned long>(snapshot.threadId),
            OperationName(snapshot.operation),
            static_cast<unsigned long long>(snapshot.address),
            snapshot.accessSize,
            static_cast<unsigned long long>(snapshot.programCounter),
            snapshot.traceTag,
            snapshot.threadId == crashThreadId ? 1u : 0u,
            snapshot.sourceFile ? snapshot.sourceFile : "unknown",
            snapshot.sourceLine,
            snapshot.sourceColumn,
            snapshot.functionName ? snapshot.functionName : "unknown");
        WriteText(file, line);

        if (IsInsideImage(snapshot.address, gameBase, gameSize)) {
            _snprintf_s(
                line,
                sizeof(line),
                _TRUNCATE,
                "record[%03zu].address_game_rva=0x%llX\r\n",
                writtenRecords,
                static_cast<unsigned long long>(snapshot.address - gameBase));
            WriteText(file, line);
        }
        if (IsInsideImage(
                snapshot.programCounter,
                nightsharpBase,
                nightsharpSize)) {
            _snprintf_s(
                line,
                sizeof(line),
                _TRUNCATE,
                "record[%03zu].pc_nightsharp_rva=0x%llX\r\n",
                writtenRecords,
                static_cast<unsigned long long>(
                    snapshot.programCounter - nightsharpBase));
            WriteText(file, line);
        }

        const std::size_t argumentCount = (std::min)(
            static_cast<std::size_t>(snapshot.traceArgumentCount),
            kTraceArgumentCapacity);
        for (std::size_t argumentIndex = 0;
             argumentIndex < argumentCount;
             ++argumentIndex) {
            _snprintf_s(
                line,
                sizeof(line),
                _TRUNCATE,
                "record[%03zu].trace_arg%zu=0x%016llX",
                writtenRecords,
                argumentIndex,
                static_cast<unsigned long long>(
                    snapshot.traceArguments[argumentIndex]));
            WriteText(file, line);
            if (IsInsideImage(
                    static_cast<std::uintptr_t>(
                        snapshot.traceArguments[argumentIndex]),
                    gameBase,
                    gameSize)) {
                _snprintf_s(
                    line,
                    sizeof(line),
                    _TRUNCATE,
                    " game_rva=0x%llX",
                    static_cast<unsigned long long>(
                        snapshot.traceArguments[argumentIndex] - gameBase));
                WriteText(file, line);
            }
            WriteText(file, "\r\n");
        }
        ++writtenRecords;
    }

    WriteText(file, "records_end\r\n");
    CloseHandle(file);
    InterlockedExchange(&g_reportWriting, 0);
    return true;
}

inline bool WriteCrashReport(
    const char* stage,
    EXCEPTION_POINTERS* exceptionPointers,
    HMODULE nightsharpModule,
    std::uintptr_t gameBase) noexcept {
#if NIGHTSHARP_ENABLE_OFFSET_CRASH_TRACE
    return WriteReport(
        kCrashReportPath,
        stage,
        exceptionPointers,
        nightsharpModule,
        gameBase);
#else
    (void)stage;
    (void)exceptionPointers;
    (void)nightsharpModule;
    (void)gameBase;
    return false;
#endif
}

} // namespace NightSharpDebug::OffsetCrashLog
