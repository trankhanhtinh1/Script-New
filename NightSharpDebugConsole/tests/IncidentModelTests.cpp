#include "../src/IncidentModel.h"

#include <stdexcept>
#include <string>

namespace {

void Require(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

void Touch(const std::wstring& path, std::uint64_t writeTime) {
    HANDLE file = CreateFileW(
        path.c_str(),
        GENERIC_WRITE,
        0,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        throw std::runtime_error("cannot create retention fixture");
    }
    ULARGE_INTEGER value{};
    value.QuadPart = writeTime;
    FILETIME time{value.LowPart, value.HighPart};
    SetFileTime(file, nullptr, nullptr, &time);
    CloseHandle(file);
}

} // namespace

void RunIncidentModelTests() {
    using namespace nsmonitor;

    Require(
        ClassifyIncident({true, false, false, nscrash::CrashKind::Handled, false}) ==
            IncidentClass::HandledNightSharpException,
        "handled exception misclassified");
    Require(
        ClassifyIncident({true, false, true, nscrash::CrashKind::Unhandled, false}) ==
            IncidentClass::UnhandledException,
        "unhandled exception misclassified");
    Require(
        ClassifyIncident({false, true, true, nscrash::CrashKind::None, false}) ==
            IncidentClass::WerFallback,
        "WER incident misclassified");
    Require(
        ClassifyIncident({false, false, true, nscrash::CrashKind::None, false}) ==
            IncidentClass::ForcedTerminationNoException,
        "forced exit misclassified");
    Require(
        ClassifyIncident({true, false, false, nscrash::CrashKind::Unhandled, true}) ==
            IncidentClass::MonitorTimeout,
        "timeout misclassified");

    SYSTEMTIME time{};
    time.wYear = 2026;
    time.wMonth = 7;
    time.wDay = 11;
    time.wHour = 1;
    time.wMinute = 2;
    time.wSecond = 3;
    const auto base = BuildArtifactBaseName(time, 4321, 77, "Core::Events/callback[3]");
    Require(
        base == L"nightsharp_crash_20260711_010203_4321_77_Core__Events_callback_3_",
        "artifact name mismatch");

    wchar_t temp[MAX_PATH]{};
    GetTempPathW(MAX_PATH, temp);
    const std::wstring directory =
        std::wstring(temp) + L"NightSharpRetention_" + std::to_wstring(GetTickCount64());
    Require(CreateDirectoryW(directory.c_str(), nullptr) != FALSE, "cannot create retention dir");
    for (int i = 0; i < 6; ++i) {
        const std::wstring item = directory + L"\\nightsharp_crash_set_" +
                                  std::to_wstring(i);
        Touch(item + L".txt", 1000 + i);
        Touch(item + L".log", 1000 + i);
    }
    const std::wstring partial = directory + L"\\nightsharp_crash_partial.tmp";
    Touch(partial, 2000);
    DWORD error = 0;
    Require(RotateCompletedIncidentSets(directory, 5, error), "rotation failed");

    WIN32_FIND_DATAW data{};
    HANDLE find = FindFirstFileW((directory + L"\\*.txt").c_str(), &data);
    int textCount = 0;
    if (find != INVALID_HANDLE_VALUE) {
        do {
            ++textCount;
        } while (FindNextFileW(find, &data));
        FindClose(find);
    }
    Require(textCount == 5, "rotation did not retain exactly five sets");
    Require(GetFileAttributesW(partial.c_str()) != INVALID_FILE_ATTRIBUTES,
            "rotation deleted partial incident");

    find = FindFirstFileW((directory + L"\\*").c_str(), &data);
    if (find != INVALID_HANDLE_VALUE) {
        do {
            if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
                DeleteFileW((directory + L"\\" + data.cFileName).c_str());
            }
        } while (FindNextFileW(find, &data));
        FindClose(find);
    }
    RemoveDirectoryW(directory.c_str());
}
