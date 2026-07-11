#include "IncidentModel.h"

#include <algorithm>
#include <cwchar>
#include <vector>

namespace nsmonitor {

IncidentClass ClassifyIncident(const IncidentEvidence& evidence) {
    if (evidence.timedOut) {
        return IncidentClass::MonitorTimeout;
    }
    if (evidence.bridgeCrash && evidence.crashKind == nscrash::CrashKind::Handled) {
        return IncidentClass::HandledNightSharpException;
    }
    if (evidence.bridgeCrash && evidence.crashKind == nscrash::CrashKind::Unhandled) {
        return IncidentClass::UnhandledException;
    }
    if (evidence.werDump) {
        return IncidentClass::WerFallback;
    }
    return IncidentClass::ForcedTerminationNoException;
}

const wchar_t* IncidentClassName(IncidentClass value) {
    switch (value) {
    case IncidentClass::HandledNightSharpException:
        return L"handled-nightsharp-exception";
    case IncidentClass::UnhandledException:
        return L"unhandled-exception";
    case IncidentClass::WerFallback:
        return L"wer-fallback";
    case IncidentClass::MonitorTimeout:
        return L"monitor-timeout";
    default:
        return L"forced-termination-no-exception";
    }
}

std::wstring BuildArtifactBaseName(
    const SYSTEMTIME& time,
    DWORD pid,
    DWORD tid,
    const char* stage) {
    wchar_t prefix[160] = {};
    _snwprintf_s(
        prefix,
        _countof(prefix),
        _TRUNCATE,
        L"nightsharp_crash_%04u%02u%02u_%02u%02u%02u_%lu_%lu_",
        time.wYear,
        time.wMonth,
        time.wDay,
        time.wHour,
        time.wMinute,
        time.wSecond,
        static_cast<unsigned long>(pid),
        static_cast<unsigned long>(tid));

    std::wstring result(prefix);
    if (!stage || !*stage) {
        result += L"unknown";
        return result;
    }
    for (const unsigned char* p = reinterpret_cast<const unsigned char*>(stage);
         *p && result.size() < 240;
         ++p) {
        const bool alphaNumeric =
            (*p >= 'a' && *p <= 'z') ||
            (*p >= 'A' && *p <= 'Z') ||
            (*p >= '0' && *p <= '9');
        result.push_back(alphaNumeric || *p == '-' || *p == '_'
                             ? static_cast<wchar_t>(*p)
                             : L'_');
    }
    return result;
}

namespace {

struct FinalizedSet {
    std::wstring base;
    FILETIME writeTime{};
};

bool Older(const FinalizedSet& left, const FinalizedSet& right) {
    return CompareFileTime(&left.writeTime, &right.writeTime) < 0;
}

void DeleteArtifactSet(const std::wstring& base) {
    for (const wchar_t* ext : {L".dmp", L".txt", L".log"}) {
        DeleteFileW((base + ext).c_str());
    }
}

} // namespace

bool RotateCompletedIncidentSets(
    const std::wstring& directory,
    std::size_t keepCount,
    DWORD& error) {
    error = ERROR_SUCCESS;
    std::wstring pattern = directory + L"\\nightsharp_crash_*.txt";
    WIN32_FIND_DATAW data{};
    HANDLE find = FindFirstFileW(pattern.c_str(), &data);
    if (find == INVALID_HANDLE_VALUE) {
        error = GetLastError();
        return error == ERROR_FILE_NOT_FOUND;
    }

    std::vector<FinalizedSet> sets;
    do {
        std::wstring fileName(data.cFileName);
        if (fileName.size() <= 4) {
            continue;
        }
        fileName.resize(fileName.size() - 4);
        sets.push_back({directory + L"\\" + fileName, data.ftLastWriteTime});
    } while (FindNextFileW(find, &data));
    FindClose(find);

    std::sort(sets.begin(), sets.end(), Older);
    while (sets.size() > keepCount) {
        DeleteArtifactSet(sets.front().base);
        sets.erase(sets.begin());
    }
    return true;
}

} // namespace nsmonitor
