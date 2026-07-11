#include "../src/WerLocalDumps.h"

#include <stdexcept>

namespace {

void Require(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

} // namespace

void RunWerLocalDumpsTests() {
    const auto settings = nsmonitor::DefaultLeagueWerSettings();
    Require(settings.applicationName == L"League of Legends.exe", "wrong WER app");
    Require(
        settings.dumpFolder == L"C:\\Users\\Public\\NightSharpDumps",
        "wrong WER dump folder");
    Require(settings.dumpCount == 5, "wrong WER dump count");
    Require(settings.dumpType == 2, "wrong WER dump type");
    Require(
        nsmonitor::BuildWerApplicationKey(settings.applicationName) ==
            L"SOFTWARE\\Microsoft\\Windows\\Windows Error Reporting\\LocalDumps\\League of Legends.exe",
        "wrong WER key");
}
