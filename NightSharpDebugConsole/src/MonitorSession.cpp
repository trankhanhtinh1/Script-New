#include "MonitorSession.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <utility>

namespace nsmonitor {

namespace {

bool ReadExact(HANDLE pipe, void* data, DWORD size) {
    BYTE* cursor = static_cast<BYTE*>(data);
    DWORD remaining = size;
    while (remaining != 0) {
        DWORD read = 0;
        if (!ReadFile(pipe, cursor, remaining, &read, nullptr) || read == 0) {
            return false;
        }
        cursor += read;
        remaining -= read;
    }
    return true;
}

bool WriteExact(HANDLE file, const void* data, DWORD size) {
    const BYTE* cursor = static_cast<const BYTE*>(data);
    DWORD remaining = size;
    while (remaining != 0) {
        DWORD written = 0;
        if (!WriteFile(file, cursor, remaining, &written, nullptr) || written == 0) {
            return false;
        }
        cursor += written;
        remaining -= written;
    }
    return true;
}

std::wstring JoinPath(const std::wstring& left, const std::wstring& right) {
    return left.empty() || left.back() == L'\\' ? left + right : left + L"\\" + right;
}

std::wstring SiblingFile(const wchar_t* name) {
    wchar_t path[MAX_PATH]{};
    const DWORD length = GetModuleFileNameW(nullptr, path, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) {
        return name;
    }
    wchar_t* slash = wcsrchr(path, L'\\');
    if (slash) {
        slash[1] = L'\0';
    } else {
        path[0] = L'\0';
    }
    return std::wstring(path) + name;
}

std::vector<std::wstring> EnumerateDumps(const std::wstring& directory) {
    std::vector<std::wstring> result;
    WIN32_FIND_DATAW data{};
    HANDLE find = FindFirstFileW(JoinPath(directory, L"*.dmp").c_str(), &data);
    if (find == INVALID_HANDLE_VALUE) {
        return result;
    }
    do {
        if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
            result.push_back(JoinPath(directory, data.cFileName));
        }
    } while (FindNextFileW(find, &data));
    FindClose(find);
    return result;
}

} // namespace

MonitorSession::MonitorSession(MonitorOptions options)
    : options_(std::move(options)) {
}

MonitorSession::~MonitorSession() {
    CloseSession();
}

bool MonitorSession::AcceptHello(nscrash::HelloPacket& hello) {
    nscrash::LocalObjectSecurity security;
    if (!security.Valid()) {
        std::fprintf(
            stderr,
            "[monitor] security descriptor setup failed: %lu\n",
            GetLastError());
        return false;
    }
    pipe_ = CreateNamedPipeA(
        nscrash::kPipeName,
        PIPE_ACCESS_INBOUND,
        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS,
        PIPE_UNLIMITED_INSTANCES,
        64 * 1024,
        64 * 1024,
        0,
        &security.attributes);
    if (pipe_ == INVALID_HANDLE_VALUE) {
        const DWORD err = GetLastError();
        if (err == ERROR_PIPE_BUSY || err == ERROR_ACCESS_DENIED) {
            std::fprintf(stderr, "[monitor] Pipe busy/in use (error %lu). Another console instance is already running.\n", err);
            Sleep(2000);
        } else {
            std::fprintf(stderr, "[monitor] CreateNamedPipe failed: %lu\n", err);
        }
        return false;
    }
    const BOOL connected = ConnectNamedPipe(pipe_, nullptr)
                               ? TRUE
                               : GetLastError() == ERROR_PIPE_CONNECTED;
    if (!connected) {
        return false;
    }
    if (!ReadExact(pipe_, &hello.header, sizeof(hello.header)) ||
        !nscrash::ValidateHeader(
            hello.header,
            nscrash::MessageType::Hello,
            sizeof(hello)) ||
        !ReadExact(
            pipe_,
            reinterpret_cast<BYTE*>(&hello) + sizeof(hello.header),
            sizeof(hello) - sizeof(hello.header))) {
        std::fprintf(stderr, "[monitor] invalid HELLO packet\n");
        return false;
    }
    return true;
}

bool MonitorSession::OpenTarget(const nscrash::HelloPacket& hello) {
    pid_ = hello.header.pid;
    char mappingName[96]{};
    char readyName[96]{};
    char completeName[96]{};
    nscrash::FormatMappingName(pid_, mappingName, sizeof(mappingName));
    nscrash::FormatCrashReadyEventName(pid_, readyName, sizeof(readyName));
    nscrash::FormatDumpCompleteEventName(pid_, completeName, sizeof(completeName));
    const bool objectNamesTerminated =
        std::memchr(hello.mappingName, '\0', sizeof(hello.mappingName)) != nullptr &&
        std::memchr(
            hello.crashReadyEventName,
            '\0',
            sizeof(hello.crashReadyEventName)) != nullptr &&
        std::memchr(
            hello.dumpCompleteEventName,
            '\0',
            sizeof(hello.dumpCompleteEventName)) != nullptr;
    const bool objectNamesMatch =
        objectNamesTerminated &&
        std::strcmp(mappingName, hello.mappingName) == 0 &&
        std::strcmp(readyName, hello.crashReadyEventName) == 0 &&
        std::strcmp(completeName, hello.dumpCompleteEventName) == 0;
    // A zero image size is valid when PE headers were erased before the
    // deferred worker could inspect them. The module base is still checked
    // against a committed region below.
    const bool moduleMetadataValid =
        hello.moduleBase != 0 &&
        (hello.moduleSize == 0 ||
         hello.moduleSize <= 512u * 1024u * 1024u);
    if (!objectNamesMatch || !moduleMetadataValid) {
        std::fprintf(
            stderr,
            "[monitor] HELLO object metadata rejected pid=%lu names=%d base=%p size=0x%X\n",
            static_cast<unsigned long>(pid_),
            objectNamesMatch ? 1 : 0,
            reinterpret_cast<void*>(hello.moduleBase),
            hello.moduleSize);
        return false;
    }
    mapping_ = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, mappingName);
    crashReady_ = OpenEventA(SYNCHRONIZE, FALSE, readyName);
    dumpComplete_ = OpenEventA(EVENT_MODIFY_STATE, FALSE, completeName);
    process_ = OpenProcess(
        PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | PROCESS_DUP_HANDLE | SYNCHRONIZE,
        FALSE,
        pid_);
    if (!mapping_ || !crashReady_ || !dumpComplete_ || !process_) {
        std::fprintf(stderr, "[monitor] open target objects failed: %lu\n", GetLastError());
        return false;
    }
    if (!options_.testMode) {
        wchar_t imagePath[1024]{};
        DWORD imageLength = _countof(imagePath);
        if (!QueryFullProcessImageNameW(process_, 0, imagePath, &imageLength)) {
            return false;
        }
        const wchar_t* baseName = wcsrchr(imagePath, L'\\');
        baseName = baseName ? baseName + 1 : imagePath;
        if (_wcsicmp(baseName, L"League of Legends.exe") != 0) {
            std::fprintf(stderr, "[monitor] non-League target rejected\n");
            return false;
        }
    }
    state_ = static_cast<nscrash::SharedState*>(MapViewOfFile(
        mapping_, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(nscrash::SharedState)));
    MEMORY_BASIC_INFORMATION mbi{};
    if (!state_ || !nscrash::ValidateSharedState(*state_, pid_) ||
        state_->moduleBase != hello.moduleBase ||
        state_->moduleSize != hello.moduleSize ||
        VirtualQueryEx(
            process_,
            reinterpret_cast<const void*>(hello.moduleBase),
            &mbi,
            sizeof(mbi)) != sizeof(mbi) ||
        mbi.State != MEM_COMMIT) {
        std::fprintf(stderr, "[monitor] invalid shared state\n");
        return false;
    }
    return true;
}

bool MonitorSession::RunOnce() {
    capturedBridgeCrash_ = false;
    nscrash::HelloPacket hello{};
    if (!AcceptHello(hello) || !OpenTarget(hello)) {
        CloseSession();
        return false;
    }
    std::printf(
        "[connected] pid=%lu NightSharp=%p size=0x%X\n",
        static_cast<unsigned long>(pid_),
        reinterpret_cast<void*>(hello.moduleBase),
        hello.moduleSize);
    std::fflush(stdout);
    CaptureDumpBaseline();

    InterlockedExchange(&stopPipe_, 0);
    pipeReader_ = CreateThread(nullptr, 0, &MonitorSession::PipeReaderEntry, this, 0, nullptr);
    HANDLE waits[2] = {crashReady_, process_};
    bool running = true;
    while (running) {
        const DWORD wait = WaitForMultipleObjects(2, waits, FALSE, 250);
        if (wait == WAIT_OBJECT_0) {
            HandleBridgeCrash();
        } else if (wait == WAIT_OBJECT_0 + 1) {
            HandleProcessExit();
            running = false;
        } else if (wait == WAIT_TIMEOUT) {
            // Heartbeat output disabled per user request
        } else {
            running = false;
        }
    }
    std::printf("\n[disconnected] pid=%lu\n", static_cast<unsigned long>(pid_));
    CloseSession();
    return true;
}

bool MonitorSession::Snapshot(nscrash::SharedState& out) const {
    if (!state_) {
        return false;
    }
    for (int attempt = 0; attempt < 8; ++attempt) {
        if (InterlockedCompareExchange(&state_->sequence, 0, 0) != 0) {
            Sleep(1);
            continue;
        }
        std::memcpy(&out, state_, sizeof(out));
        out.phase[sizeof(out.phase) - 1] = '\0';
        out.lastStage[sizeof(out.lastStage) - 1] = '\0';
        out.crash.stage[sizeof(out.crash.stage) - 1] = '\0';
        MemoryBarrier();
        if (InterlockedCompareExchange(&state_->sequence, 0, 0) == 0 &&
            nscrash::ValidateSharedState(out, pid_)) {
            return true;
        }
    }
    return false;
}

bool MonitorSession::HandleBridgeCrash() {
    nscrash::SharedState snapshot{};
    if (!Snapshot(snapshot) || snapshot.crash.claimed == 0) {
        return false;
    }
    capturedBridgeCrash_ = true;
    SYSTEMTIME time{};
    GetLocalTime(&time);
    const std::wstring basePath = JoinPath(
        options_.dumpFolder,
        BuildArtifactBaseName(
            time, pid_, snapshot.crash.threadId, snapshot.crash.stage));
    const std::wstring dumpPath = basePath + L".dmp";

    RemoteCrashData crash = ReadRemoteCrashData(
        process_,
        pid_,
        snapshot.crash.threadId,
        static_cast<std::uintptr_t>(snapshot.crash.exceptionPointers),
        snapshot.moduleBase,
        snapshot.moduleSize);
    if (!options_.testMode) {
        ResolveSymbolsAndStack(process_, SiblingFile(L"NightSharp.dll"), crash);
    }
    const DumpResult dump = WriteExternalDump({
        process_,
        pid_,
        snapshot.crash.threadId,
        reinterpret_cast<PEXCEPTION_POINTERS>(snapshot.crash.exceptionPointers),
        dumpPath,
    });
    const IncidentClass classification = snapshot.crash.kind == nscrash::CrashKind::Handled
                                             ? IncidentClass::HandledNightSharpException
                                             : IncidentClass::UnhandledException;
    const std::string report = FormatIncidentReport(
        classification,
        nscrash::CaptureSource::Bridge,
        snapshot,
        crash,
        dump,
        dumpPath,
        crash.exceptionCode);
    DWORD reportError = 0;
    const bool reportOk = WriteIncidentReport(basePath + L".txt", report, reportError);
    const bool logOk = WriteLogSnapshot(basePath + L".log", snapshot);

    state_->crash.dumpError = dump.ok ? ERROR_SUCCESS : dump.error;
    InterlockedExchange(&state_->crash.dumpSucceeded, dump.ok ? 1 : 0);
    InterlockedExchange(&state_->crash.dumpFinished, 1);
    MemoryBarrier();
    SetEvent(dumpComplete_);

    DWORD rotateError = 0;
    RotateCompletedIncidentSets(options_.dumpFolder, 5, rotateError);
    std::printf(
        "\n[crash] code=0x%08lX tid=%lu dump=%s report=%s log=%s\n",
        static_cast<unsigned long>(snapshot.crash.exceptionCode),
        static_cast<unsigned long>(snapshot.crash.threadId),
        dump.ok ? "ok" : "failed",
        reportOk ? "ok" : "failed",
        logOk ? "ok" : "failed");
    return dump.ok && reportOk && logOk;
}

void MonitorSession::HandleProcessExit() {
    DWORD exitCode = 0;
    GetExitCodeProcess(process_, &exitCode);
    if (capturedBridgeCrash_) {
        return;
    }
    std::wstring werDump;
    if (!options_.testMode && !options_.noWer) {
        for (int attempt = 0; attempt < 40 && werDump.empty(); ++attempt) {
            werDump = FindNewWerDump();
            if (werDump.empty()) {
                Sleep(250);
            }
        }
    }

    nscrash::SharedState snapshot{};
    Snapshot(snapshot);
    snapshot.pid = pid_;
    SYSTEMTIME time{};
    GetLocalTime(&time);
    const bool hasWerDump = !werDump.empty();
    const std::wstring basePath = JoinPath(
        options_.dumpFolder,
        BuildArtifactBaseName(
            time,
            pid_,
            0,
            hasWerDump ? "wer-fallback" : "forced-termination"));
    const RemoteCrashData unavailable{};
    DumpResult dump{false, ERROR_NO_MORE_FILES, 0};
    if (hasWerDump) {
        HANDLE file = CreateFileW(
            werDump.c_str(),
            GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);
        LARGE_INTEGER size{};
        if (file != INVALID_HANDLE_VALUE) {
            if (GetFileSizeEx(file, &size)) {
                dump.bytes = static_cast<std::uint64_t>(size.QuadPart);
            }
            CloseHandle(file);
        }
        dump.ok = true;
        dump.error = ERROR_SUCCESS;
    }
    const std::string report = FormatIncidentReport(
        hasWerDump ? IncidentClass::WerFallback
                   : IncidentClass::ForcedTerminationNoException,
        hasWerDump ? nscrash::CaptureSource::Wer
                   : nscrash::CaptureSource::ExitOnly,
        snapshot,
        unavailable,
        dump,
        werDump,
        exitCode);
    DWORD error = 0;
    WriteIncidentReport(basePath + L".txt", report, error);
    WriteLogSnapshot(basePath + L".log", snapshot);
    DWORD rotateError = 0;
    RotateCompletedIncidentSets(options_.dumpFolder, 5, rotateError);
}

void MonitorSession::CaptureDumpBaseline() {
    baselineDumps_ = EnumerateDumps(options_.dumpFolder);
}

std::wstring MonitorSession::FindNewWerDump() const {
    const auto current = EnumerateDumps(options_.dumpFolder);
    for (const std::wstring& path : current) {
        if (std::find(baselineDumps_.begin(), baselineDumps_.end(), path) ==
            baselineDumps_.end()) {
            return path;
        }
    }
    return {};
}

bool MonitorSession::WriteLogSnapshot(
    const std::wstring& path,
    const nscrash::SharedState& state) {
    HANDLE file = CreateFileW(
        path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }
    bool ok = true;
    const LONG current = state.logSerial;
    const LONG first = std::max<LONG>(
        1, current - static_cast<LONG>(nscrash::kRecentLogCount) + 1);
    for (LONG serial = first; serial <= current; ++serial) {
        const auto& slot = state.recentLogs[
            static_cast<std::size_t>(serial) % nscrash::kRecentLogCount];
        if (slot.serial != static_cast<std::uint64_t>(serial)) {
            continue;
        }
        const DWORD length = static_cast<DWORD>(strnlen_s(slot.text, sizeof(slot.text)));
        ok = WriteExact(file, slot.text, length) && ok;
        if (length == 0 || slot.text[length - 1] != '\n') {
            ok = WriteExact(file, "\r\n", 2) && ok;
        }
    }
    std::lock_guard<std::mutex> lock(liveLogMutex_);
    if (current == 0) {
        for (const std::string& line : liveLogs_) {
            ok = WriteExact(file, line.data(), static_cast<DWORD>(line.size())) && ok;
        }
    }
    FlushFileBuffers(file);
    CloseHandle(file);
    return ok;
}

DWORD WINAPI MonitorSession::PipeReaderEntry(LPVOID parameter) {
    return static_cast<MonitorSession*>(parameter)->PipeReaderLoop();
}

static inline void WriteUnicodeLog(const char* text, std::size_t length) {
    if (!text || length == 0) return;

    const bool needsNewline = (text[length - 1] != '\n');

    HANDLE hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    const bool isConsole = (hStdOut != INVALID_HANDLE_VALUE && hStdOut != nullptr && GetConsoleMode(hStdOut, &mode));

    if (isConsole) {
        int wideLen = MultiByteToWideChar(CP_UTF8, 0, text, static_cast<int>(length), nullptr, 0);
        if (wideLen > 0) {
            std::wstring wideStr(wideLen, L'\0');
            MultiByteToWideChar(CP_UTF8, 0, text, static_cast<int>(length), &wideStr[0], wideLen);
            if (needsNewline) {
                wideStr.push_back(L'\n');
            }
            DWORD written = 0;
            WriteConsoleW(hStdOut, wideStr.c_str(), static_cast<DWORD>(wideStr.size()), &written, nullptr);
            return;
        }
    }

    std::fwrite(text, 1, length, stdout);
    if (needsNewline) {
        std::fwrite("\n", 1, 1, stdout);
    }
    std::fflush(stdout);
}

DWORD MonitorSession::PipeReaderLoop() {
    while (InterlockedCompareExchange(&stopPipe_, 0, 0) == 0) {
        nscrash::LogPacket packet{};
        if (!ReadExact(pipe_, &packet.header, sizeof(packet.header)) ||
            !nscrash::ValidateHeader(
                packet.header, nscrash::MessageType::Log, sizeof(packet)) ||
            packet.header.pid != pid_ ||
            !ReadExact(
                pipe_,
                reinterpret_cast<BYTE*>(&packet) + sizeof(packet.header),
                sizeof(packet) - sizeof(packet.header))) {
            break;
        }
        packet.text[sizeof(packet.text) - 1] = '\0';
        const std::size_t length = std::min<std::size_t>(
            packet.length, strnlen_s(packet.text, sizeof(packet.text)));
        WriteUnicodeLog(packet.text, length);
        std::lock_guard<std::mutex> lock(liveLogMutex_);
        if (liveLogs_.size() == 512) {
            liveLogs_.erase(liveLogs_.begin());
        }
        if (length > 0 && packet.text[length - 1] != '\n') {
            std::string line(packet.text, length);
            line.push_back('\n');
            liveLogs_.push_back(std::move(line));
        } else {
            liveLogs_.emplace_back(packet.text, length);
        }
    }
    return 0;
}

void MonitorSession::CloseSession() {
    InterlockedExchange(&stopPipe_, 1);
    if (pipe_ != INVALID_HANDLE_VALUE) {
        CancelIoEx(pipe_, nullptr);
    }
    if (pipeReader_) {
        WaitForSingleObject(pipeReader_, 2000);
        CloseHandle(pipeReader_);
        pipeReader_ = nullptr;
    }
    if (state_) {
        UnmapViewOfFile(state_);
        state_ = nullptr;
    }
    if (process_) {
        CloseHandle(process_);
        process_ = nullptr;
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
    if (pipe_ != INVALID_HANDLE_VALUE) {
        DisconnectNamedPipe(pipe_);
        CloseHandle(pipe_);
        pipe_ = INVALID_HANDLE_VALUE;
    }
    pid_ = 0;
    liveLogs_.clear();
    baselineDumps_.clear();
}

} // namespace nsmonitor
