#include "src/MonitorSession.h"
#include "src/WerLocalDumps.h"

#include <Windows.h>

#include <cstdio>
#include <cwchar>
#include <string>

namespace {

bool HasArgument(int argc, wchar_t** argv, const wchar_t* expected) {
    for (int i = 1; i < argc; ++i) {
        if (_wcsicmp(argv[i], expected) == 0) {
            return true;
        }
    }
    return false;
}

std::wstring ArgumentValue(
    int argc,
    wchar_t** argv,
    const wchar_t* name,
    const wchar_t* fallback) {
    for (int i = 1; i + 1 < argc; ++i) {
        if (_wcsicmp(argv[i], name) == 0) {
            return argv[i + 1];
        }
    }
    return fallback;
}

} // namespace

int wmain(int argc, wchar_t** argv) {
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);
    HANDLE hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hStdOut != INVALID_HANDLE_VALUE && hStdOut != nullptr) {
        DWORD mode = 0;
        if (GetConsoleMode(hStdOut, &mode)) {
            SetConsoleMode(hStdOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        }
    }
    nsmonitor::MonitorOptions options{};
    options.testMode = HasArgument(argc, argv, L"--test-mode");
    options.noWer = HasArgument(argc, argv, L"--no-wer");
    options.dumpFolder = ArgumentValue(
        argc, argv, L"--dump-folder", L"C:\\Users\\Public\\NightSharpDumps");

    nsmonitor::WerSettings wer = nsmonitor::DefaultLeagueWerSettings();
    wer.dumpFolder = options.dumpFolder;
    wer.applicationName = ArgumentValue(
        argc, argv, L"--application", wer.applicationName.c_str());
    if (HasArgument(argc, argv, L"--cleanup-wer")) {
        const auto result = nsmonitor::CleanupManagedWerLocalDumps(wer);
        std::printf(
            "WER cleanup: %s error=%lu\n",
            result.ok ? "ok" : "failed",
            static_cast<unsigned long>(result.error));
        return result.ok ? 0 : 1;
    }

    if (!CreateDirectoryW(options.dumpFolder.c_str(), nullptr) &&
        GetLastError() != ERROR_ALREADY_EXISTS) {
        std::fprintf(stderr, "Cannot create dump folder: %lu\n", GetLastError());
        return 2;
    }
    if (!options.testMode && !options.noWer) {
        const auto result = nsmonitor::ConfigureWerLocalDumps(wer);
        if (!result.ok) {
            std::fprintf(
                stderr,
                "[notice] WER setup skipped: %lu (run as Administrator for WER crash dumps)\n",
                result.error);
        }
    }

    std::wprintf(
        L"NightSharp external crash monitor\n"
        L"Dump folder: %ls\n"
        L"Pipe: %hs\n"
        L"Waiting for NightSharp...\n\n",
        options.dumpFolder.c_str(),
        nscrash::kPipeName);
    for (;;) {
        nsmonitor::MonitorSession session(options);
        session.RunOnce();
        Sleep(250);
    }
}
