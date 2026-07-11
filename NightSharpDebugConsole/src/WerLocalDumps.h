#pragma once

#include <Windows.h>

#include <string>

namespace nsmonitor {

struct WerSettings {
    std::wstring applicationName;
    std::wstring dumpFolder;
    DWORD dumpCount{};
    DWORD dumpType{};
};

struct WerResult {
    bool ok{};
    DWORD error{};
    bool changed{};
};

WerSettings DefaultLeagueWerSettings();
std::wstring BuildWerApplicationKey(const std::wstring& applicationName);
WerResult ConfigureWerLocalDumps(const WerSettings& settings);
WerResult CleanupManagedWerLocalDumps(const WerSettings& settings);

} // namespace nsmonitor
