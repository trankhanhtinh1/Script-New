#include "WerLocalDumps.h"

#include <vector>

namespace nsmonitor {

namespace {

constexpr wchar_t kWerRoot[] =
    L"SOFTWARE\\Microsoft\\Windows\\Windows Error Reporting\\LocalDumps\\";

bool SetDword(HKEY key, const wchar_t* name, DWORD value, bool& changed) {
    DWORD current = 0;
    DWORD type = 0;
    DWORD size = sizeof(current);
    const LONG query = RegQueryValueExW(
        key,
        name,
        nullptr,
        &type,
        reinterpret_cast<BYTE*>(&current),
        &size);
    if (query == ERROR_SUCCESS && type == REG_DWORD && current == value) {
        return true;
    }
    const LONG result = RegSetValueExW(
        key,
        name,
        0,
        REG_DWORD,
        reinterpret_cast<const BYTE*>(&value),
        sizeof(value));
    if (result == ERROR_SUCCESS) {
        changed = true;
        return true;
    }
    SetLastError(static_cast<DWORD>(result));
    return false;
}

bool SetExpandableString(
    HKEY key,
    const wchar_t* name,
    const std::wstring& value,
    bool& changed) {
    DWORD type = 0;
    DWORD bytes = 0;
    LONG query = RegQueryValueExW(key, name, nullptr, &type, nullptr, &bytes);
    if (query == ERROR_SUCCESS && type == REG_EXPAND_SZ && bytes >= sizeof(wchar_t)) {
        std::vector<wchar_t> current(bytes / sizeof(wchar_t) + 1, L'\0');
        if (RegQueryValueExW(
                key,
                name,
                nullptr,
                &type,
                reinterpret_cast<BYTE*>(current.data()),
                &bytes) == ERROR_SUCCESS &&
            value == current.data()) {
            return true;
        }
    }
    const DWORD valueBytes = static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t));
    const LONG result = RegSetValueExW(
        key,
        name,
        0,
        REG_EXPAND_SZ,
        reinterpret_cast<const BYTE*>(value.c_str()),
        valueBytes);
    if (result == ERROR_SUCCESS) {
        changed = true;
        return true;
    }
    SetLastError(static_cast<DWORD>(result));
    return false;
}

void DeleteMatchingDword(HKEY key, const wchar_t* name, DWORD expected, bool& changed) {
    DWORD current = 0;
    DWORD type = 0;
    DWORD size = sizeof(current);
    if (RegQueryValueExW(
            key,
            name,
            nullptr,
            &type,
            reinterpret_cast<BYTE*>(&current),
            &size) == ERROR_SUCCESS &&
        type == REG_DWORD &&
        current == expected &&
        RegDeleteValueW(key, name) == ERROR_SUCCESS) {
        changed = true;
    }
}

void DeleteMatchingString(
    HKEY key,
    const wchar_t* name,
    const std::wstring& expected,
    bool& changed) {
    DWORD type = 0;
    DWORD bytes = 0;
    if (RegQueryValueExW(key, name, nullptr, &type, nullptr, &bytes) != ERROR_SUCCESS ||
        type != REG_EXPAND_SZ || bytes < sizeof(wchar_t)) {
        return;
    }
    std::vector<wchar_t> current(bytes / sizeof(wchar_t) + 1, L'\0');
    if (RegQueryValueExW(
            key,
            name,
            nullptr,
            &type,
            reinterpret_cast<BYTE*>(current.data()),
            &bytes) == ERROR_SUCCESS &&
        expected == current.data() &&
        RegDeleteValueW(key, name) == ERROR_SUCCESS) {
        changed = true;
    }
}

} // namespace

WerSettings DefaultLeagueWerSettings() {
    return {
        L"League of Legends.exe",
        L"C:\\Users\\Public\\NightSharpDumps",
        5,
        2,
    };
}

std::wstring BuildWerApplicationKey(const std::wstring& applicationName) {
    return std::wstring(kWerRoot) + applicationName;
}

WerResult ConfigureWerLocalDumps(const WerSettings& settings) {
    WerResult result{};
    if (!CreateDirectoryW(settings.dumpFolder.c_str(), nullptr) &&
        GetLastError() != ERROR_ALREADY_EXISTS) {
        result.error = GetLastError();
        return result;
    }

    HKEY key = nullptr;
    const LONG open = RegCreateKeyExW(
        HKEY_LOCAL_MACHINE,
        BuildWerApplicationKey(settings.applicationName).c_str(),
        0,
        nullptr,
        0,
        KEY_QUERY_VALUE | KEY_SET_VALUE,
        nullptr,
        &key,
        nullptr);
    if (open != ERROR_SUCCESS) {
        result.error = static_cast<DWORD>(open);
        return result;
    }

    const bool ok =
        SetExpandableString(key, L"DumpFolder", settings.dumpFolder, result.changed) &&
        SetDword(key, L"DumpCount", settings.dumpCount, result.changed) &&
        SetDword(key, L"DumpType", settings.dumpType, result.changed);
    result.error = ok ? ERROR_SUCCESS : GetLastError();
    result.ok = ok;
    RegCloseKey(key);
    return result;
}

WerResult CleanupManagedWerLocalDumps(const WerSettings& settings) {
    WerResult result{};
    const std::wstring path = BuildWerApplicationKey(settings.applicationName);
    HKEY key = nullptr;
    const LONG open = RegOpenKeyExW(
        HKEY_LOCAL_MACHINE,
        path.c_str(),
        0,
        KEY_QUERY_VALUE | KEY_SET_VALUE,
        &key);
    if (open == ERROR_FILE_NOT_FOUND) {
        result.ok = true;
        return result;
    }
    if (open != ERROR_SUCCESS) {
        result.error = static_cast<DWORD>(open);
        return result;
    }

    DeleteMatchingString(key, L"DumpFolder", settings.dumpFolder, result.changed);
    DeleteMatchingDword(key, L"DumpCount", settings.dumpCount, result.changed);
    DeleteMatchingDword(key, L"DumpType", settings.dumpType, result.changed);
    RegCloseKey(key);

    DWORD subKeys = 0;
    DWORD values = 0;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, path.c_str(), 0, KEY_QUERY_VALUE, &key) == ERROR_SUCCESS) {
        RegQueryInfoKeyW(
            key,
            nullptr,
            nullptr,
            nullptr,
            &subKeys,
            nullptr,
            nullptr,
            &values,
            nullptr,
            nullptr,
            nullptr,
            nullptr);
        RegCloseKey(key);
        if (subKeys == 0 && values == 0) {
            RegDeleteKeyW(HKEY_LOCAL_MACHINE, path.c_str());
        }
    }
    result.ok = true;
    return result;
}

} // namespace nsmonitor
