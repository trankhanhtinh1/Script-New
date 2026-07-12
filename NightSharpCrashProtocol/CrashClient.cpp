#include "CrashClient.h"

#include <algorithm>
#include <cstring>

namespace nscrash {

namespace {

std::uint64_t FileTimeNow() {
    FILETIME time{};
    GetSystemTimeAsFileTime(&time);
    ULARGE_INTEGER value{};
    value.LowPart = time.dwLowDateTime;
    value.HighPart = time.dwHighDateTime;
    return value.QuadPart;
}

bool WriteExact(HANDLE pipe, const void* data, DWORD size) {
    const BYTE* cursor = static_cast<const BYTE*>(data);
    DWORD remaining = size;
    while (remaining != 0) {
        DWORD written = 0;
        if (!WriteFile(pipe, cursor, remaining, &written, nullptr) || written == 0) {
            return false;
        }
        cursor += written;
        remaining -= written;
    }
    return true;
}

} // namespace

CrashClient::~CrashClient() {
    Uninstall();
}

bool CrashClient::Install(std::uintptr_t moduleBase, std::uint32_t moduleSize) {
    if (state_) {
        return true;
    }
    pid_ = GetCurrentProcessId();

    char mappingName[96]{};
    char readyName[96]{};
    char completeName[96]{};
    FormatMappingName(pid_, mappingName, sizeof(mappingName));
    FormatCrashReadyEventName(pid_, readyName, sizeof(readyName));
    FormatDumpCompleteEventName(pid_, completeName, sizeof(completeName));

    mapping_ = CreateFileMappingA(
        INVALID_HANDLE_VALUE,
        nullptr,
        PAGE_READWRITE,
        0,
        sizeof(SharedState),
        mappingName);
    if (!mapping_) {
        CloseResources();
        return false;
    }
    state_ = static_cast<SharedState*>(MapViewOfFile(
        mapping_,
        FILE_MAP_ALL_ACCESS,
        0,
        0,
        sizeof(SharedState)));
    if (!state_) {
        CloseResources();
        return false;
    }

    std::memset(state_, 0, sizeof(*state_));
    state_->magic = kMagic;
    state_->version = kVersion;
    state_->size = sizeof(*state_);
    state_->pid = pid_;
    state_->moduleBase = moduleBase;
    state_->moduleSize = moduleSize;
    state_->heartbeatTick = GetTickCount64();
    strcpy_s(state_->phase, "bridge-install");

    crashReady_ = CreateEventA(nullptr, FALSE, FALSE, readyName);
    dumpComplete_ = CreateEventA(nullptr, FALSE, FALSE, completeName);
    if (!crashReady_ || !dumpComplete_) {
        CloseResources();
        return false;
    }

    InterlockedExchange(&stopping_, 0);
    worker_ = CreateThread(nullptr, 0, &CrashClient::WorkerEntry, this, 0, nullptr);
    if (!worker_) {
        CloseResources();
        return false;
    }
    return true;
}

void CrashClient::Uninstall() {
    if (!state_ && !worker_) {
        return;
    }
    InterlockedExchange(&stopping_, 1);
    HANDLE pipe = static_cast<HANDLE>(InterlockedExchangePointer(
        reinterpret_cast<void* volatile*>(&pipe_),
        INVALID_HANDLE_VALUE));
    if (pipe && pipe != INVALID_HANDLE_VALUE) {
        CancelIoEx(pipe, nullptr);
        CloseHandle(pipe);
    }
    if (worker_) {
        WaitForSingleObject(worker_, 2500);
        CloseHandle(worker_);
        worker_ = nullptr;
    }
    CloseResources();
}

void CrashClient::CloseResources() {
    if (state_) {
        UnmapViewOfFile(state_);
        state_ = nullptr;
    }
    if (dumpComplete_) {
        CloseHandle(dumpComplete_);
        dumpComplete_ = nullptr;
    }
    if (crashReady_) {
        CloseHandle(crashReady_);
        crashReady_ = nullptr;
    }
    if (mapping_) {
        CloseHandle(mapping_);
        mapping_ = nullptr;
    }
    pid_ = 0;
}

void CrashClient::PublishHeartbeat(std::uint64_t tick) {
    if (state_) {
        InterlockedExchange64(
            reinterpret_cast<volatile LONG64*>(&state_->heartbeatTick),
            static_cast<LONG64>(tick));
    }
}

void CrashClient::PublishText(
    char* destination,
    std::size_t size,
    const char* text) {
    if (!state_ || !destination || size == 0) {
        return;
    }
    if (InterlockedCompareExchange(&state_->sequence, 1, 0) != 0) {
        return;
    }
    strncpy_s(destination, size, text && *text ? text : "unknown", _TRUNCATE);
    InterlockedExchange(&state_->sequence, 0);
}

void CrashClient::PublishPhase(const char* phase) {
    if (state_) {
        PublishText(state_->phase, sizeof(state_->phase), phase);
    }
}

void CrashClient::PublishStage(const char* stage) {
    if (state_) {
        PublishText(state_->lastStage, sizeof(state_->lastStage), stage);
    }
}

void CrashClient::EnqueueLog(const char* text, std::size_t length) {
    if (!state_ || !text || length == 0) {
        return;
    }
    const LONG serial = InterlockedIncrement(&state_->logSerial);
    RecentLogLine& slot = state_->recentLogs[
        static_cast<std::size_t>(serial) % kRecentLogCount];
    const std::size_t copy = std::min(length, sizeof(slot.text) - 1);
    std::memcpy(slot.text, text, copy);
    slot.text[copy] = '\0';
    MemoryBarrier();
    InterlockedExchange64(
        reinterpret_cast<volatile LONG64*>(&slot.serial),
        serial);
}

void CrashClient::Trace(
    TraceTag tag,
    std::uint64_t programCounter,
    std::uint64_t arg0,
    std::uint64_t arg1) {
    (void)tag;
    (void)programCounter;
    (void)arg0;
    (void)arg1;
}

std::uint64_t CrashClient::TraceDetailed(
    TraceTag tag,
    std::uint64_t programCounter,
    std::uint64_t arg0,
    std::uint64_t arg1,
    std::uint64_t arg2,
    std::uint64_t arg3,
    std::uint64_t arg4,
    std::uint64_t arg5,
    const char* text) {
    (void)tag;
    (void)programCounter;
    (void)arg0;
    (void)arg1;
    (void)arg2;
    (void)arg3;
    (void)arg4;
    (void)arg5;
    (void)text;
    return 0;
}

std::uint64_t CrashClient::TraceExtended(
    TraceTag tag,
    std::uint64_t programCounter,
    std::uint64_t arg0,
    std::uint64_t arg1,
    std::uint64_t arg2,
    std::uint64_t arg3,
    std::uint64_t arg4,
    std::uint64_t arg5,
    std::uint64_t arg6,
    std::uint64_t arg7,
    std::uint64_t arg8,
    std::uint64_t arg9,
    const char* text) {
    (void)tag;
    (void)programCounter;
    (void)arg0;
    (void)arg1;
    (void)arg2;
    (void)arg3;
    (void)arg4;
    (void)arg5;
    (void)arg6;
    (void)arg7;
    (void)arg8;
    (void)arg9;
    (void)text;
    return 0;
}

void CrashClient::ObserveFirstChance(EXCEPTION_POINTERS* exceptionPointers) {
    if (!state_ || !exceptionPointers || !exceptionPointers->ExceptionRecord) {
        return;
    }
    const LONG serial = InterlockedIncrement(&state_->firstChanceSerial);
    FirstChanceSummary& slot = state_->firstChance[
        static_cast<std::size_t>(serial) % kRecentExceptionCount];
    slot.timestamp100ns = FileTimeNow();
    slot.threadId = GetCurrentThreadId();
    slot.code = exceptionPointers->ExceptionRecord->ExceptionCode;
    slot.address = reinterpret_cast<std::uint64_t>(
        exceptionPointers->ExceptionRecord->ExceptionAddress);
    MemoryBarrier();
    InterlockedExchange64(
        reinterpret_cast<volatile LONG64*>(&slot.serial),
        serial);
}

bool CrashClient::CaptureException(
    CrashKind kind,
    const char* stage,
    EXCEPTION_POINTERS* exceptionPointers,
    DWORD timeoutMs) {
    if (!state_ || !crashReady_ || !dumpComplete_ || !exceptionPointers ||
        !exceptionPointers->ExceptionRecord) {
        return false;
    }
    if (InterlockedCompareExchange(&state_->crash.claimed, 1, 0) != 0) {
        return false;
    }

    PublishStage(stage);
    ResetEvent(dumpComplete_);
    state_->crash.kind = kind;
    state_->crash.threadId = GetCurrentThreadId();
    state_->crash.exceptionCode =
        exceptionPointers->ExceptionRecord->ExceptionCode;
    state_->crash.exceptionAddress = reinterpret_cast<std::uint64_t>(
        exceptionPointers->ExceptionRecord->ExceptionAddress);
    state_->crash.exceptionPointers = reinterpret_cast<std::uint64_t>(exceptionPointers);
    strncpy_s(
        state_->crash.stage,
        stage && *stage ? stage : "unknown",
        _TRUNCATE);
    InterlockedExchange(&state_->crash.dumpFinished, 0);
    InterlockedExchange(&state_->crash.dumpSucceeded, 0);
    state_->crash.dumpError = ERROR_SUCCESS;
    MemoryBarrier();
    SetEvent(crashReady_);

    const DWORD wait = WaitForSingleObject(dumpComplete_, timeoutMs);
    const bool success =
        wait == WAIT_OBJECT_0 &&
        InterlockedCompareExchange(&state_->crash.dumpSucceeded, 0, 0) != 0;
    if (kind == CrashKind::Handled) {
        InterlockedExchange(&state_->crash.claimed, 0);
    }
    return success;
}

DWORD WINAPI CrashClient::WorkerEntry(LPVOID parameter) {
    return static_cast<CrashClient*>(parameter)->WorkerLoop();
}

bool CrashClient::SendHello(HANDLE pipe, std::uint64_t sequence) {
    HelloPacket hello{};
    InitializeHeader(
        hello.header,
        MessageType::Hello,
        sizeof(hello),
        pid_,
        sequence);
    hello.moduleBase = state_->moduleBase;
    hello.moduleSize = state_->moduleSize;
    FormatMappingName(pid_, hello.mappingName, sizeof(hello.mappingName));
    FormatCrashReadyEventName(
        pid_,
        hello.crashReadyEventName,
        sizeof(hello.crashReadyEventName));
    FormatDumpCompleteEventName(
        pid_,
        hello.dumpCompleteEventName,
        sizeof(hello.dumpCompleteEventName));
    return WriteExact(pipe, &hello, sizeof(hello));
}

bool CrashClient::SendPendingLogs(HANDLE pipe, LONG& lastSent) {
    const LONG current = InterlockedCompareExchange(&state_->logSerial, 0, 0);
    LONG next = std::max<LONG>(lastSent + 1, current - static_cast<LONG>(kRecentLogCount) + 1);
    for (; next <= current; ++next) {
        RecentLogLine& slot = state_->recentLogs[
            static_cast<std::size_t>(next) % kRecentLogCount];
        const LONG64 serial = InterlockedCompareExchange64(
            reinterpret_cast<volatile LONG64*>(&slot.serial),
            0,
            0);
        if (serial != next) {
            InterlockedIncrement(&state_->droppedLogs);
            continue;
        }
        LogPacket packet{};
        InitializeHeader(
            packet.header,
            MessageType::Log,
            sizeof(packet),
            pid_,
            static_cast<std::uint64_t>(next));
        strncpy_s(packet.text, slot.text, _TRUNCATE);
        packet.length = static_cast<std::uint32_t>(std::strlen(packet.text));
        if (!WriteExact(pipe, &packet, sizeof(packet))) {
            return false;
        }
        lastSent = next;
    }
    return true;
}

DWORD CrashClient::WorkerLoop() {
    std::uint64_t helloSerial = 0;
    while (InterlockedCompareExchange(&stopping_, 0, 0) == 0) {
        if (!WaitNamedPipeA(kPipeName, 250)) {
            PublishHeartbeat(GetTickCount64());
            Sleep(250);
            continue;
        }
        HANDLE pipe = CreateFileA(
            kPipeName,
            GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);
        if (pipe == INVALID_HANDLE_VALUE) {
            Sleep(250);
            continue;
        }
        InterlockedExchangePointer(
            reinterpret_cast<void* volatile*>(&pipe_),
            pipe);

        LONG lastSent = 0;
        bool connected = SendHello(pipe, ++helloSerial);
        while (connected && InterlockedCompareExchange(&stopping_, 0, 0) == 0) {
            PublishHeartbeat(GetTickCount64());
            connected = SendPendingLogs(pipe, lastSent);
            if (connected) {
                Sleep(50);
            }
        }

        HANDLE owned = static_cast<HANDLE>(InterlockedCompareExchangePointer(
            reinterpret_cast<void* volatile*>(&pipe_),
            INVALID_HANDLE_VALUE,
            pipe));
        if (owned == pipe) {
            InterlockedExchangePointer(
                reinterpret_cast<void* volatile*>(&pipe_),
                INVALID_HANDLE_VALUE);
            CloseHandle(pipe);
        }
    }
    return 0;
}

} // namespace nscrash
