#pragma once

#include "../Data/EmbeddedAssets.h"
#include "Logging.h"

#include <Windows.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>

namespace SDK::Utils::AssetInstaller {

namespace detail {

inline std::string NormalizePath(std::string path) {
    for (char& ch : path) {
        if (ch == '/') {
            ch = '\\';
        }
    }
    return path;
}

inline std::string AppDataDirectory() {
    char appData[MAX_PATH] = {};
    const DWORD len = ::GetEnvironmentVariableA("APPDATA", appData, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) {
        return {};
    }
    return std::string(appData, appData + len);
}

inline std::string Combine(const std::string& base, const char* relative) {
    if (base.empty()) {
        return {};
    }

    std::string result = NormalizePath(base);
    if (!result.empty() && result.back() != '\\') {
        result.push_back('\\');
    }
    if (relative && relative[0]) {
        result += NormalizePath(relative);
    }
    return result;
}

inline bool DirectoryExists(const std::string& path) {
    const DWORD attr = ::GetFileAttributesA(path.c_str());
    return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

inline bool FileExists(const std::string& path) {
    const DWORD attr = ::GetFileAttributesA(path.c_str());
    return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

inline bool EnsureDirectory(const std::string& directory) {
    const std::string path = NormalizePath(directory);
    if (path.empty()) {
        return false;
    }
    if (DirectoryExists(path)) {
        return true;
    }

    std::size_t cursor = 0;
    if (path.size() >= 2 && path[1] == ':') {
        cursor = 2;
    }

    while (true) {
        cursor = path.find('\\', cursor + 1);
        if (cursor == std::string::npos) {
            break;
        }

        const std::string part = path.substr(0, cursor);
        if (part.size() <= 2 || DirectoryExists(part)) {
            continue;
        }

        if (!::CreateDirectoryA(part.c_str(), nullptr) &&
            ::GetLastError() != ERROR_ALREADY_EXISTS) {
            return false;
        }
    }

    if (!DirectoryExists(path) &&
        !::CreateDirectoryA(path.c_str(), nullptr) &&
        ::GetLastError() != ERROR_ALREADY_EXISTS) {
        return false;
    }

    return DirectoryExists(path);
}

inline bool EnsureParentDirectory(const std::string& filePath) {
    const std::string path = NormalizePath(filePath);
    const std::size_t slash = path.find_last_of('\\');
    if (slash == std::string::npos) {
        return false;
    }
    return EnsureDirectory(path.substr(0, slash));
}

inline bool FileSizeMatches(const std::string& path, std::size_t expectedSize) {
    WIN32_FILE_ATTRIBUTE_DATA data = {};
    if (!::GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, &data) ||
        (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
        return false;
    }

    ULARGE_INTEGER size = {};
    size.LowPart = data.nFileSizeLow;
    size.HighPart = data.nFileSizeHigh;
    return size.QuadPart == static_cast<ULONGLONG>(expectedSize);
}

inline bool WriteAllBytes(const std::string& path, const std::uint8_t* bytes, std::size_t size) {
    if (!bytes || size == 0 || !EnsureParentDirectory(path)) {
        return false;
    }

    HANDLE file = ::CreateFileA(
        path.c_str(),
        GENERIC_WRITE,
        FILE_SHARE_READ,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }

    bool ok = true;
    const std::uint8_t* cursor = bytes;
    std::size_t remaining = size;
    while (remaining > 0) {
        const DWORD chunk = static_cast<DWORD>(std::min<std::size_t>(remaining, 1u << 20));
        DWORD written = 0;
        if (!::WriteFile(file, cursor, chunk, &written, nullptr) || written != chunk) {
            ok = false;
            break;
        }
        cursor += chunk;
        remaining -= chunk;
    }

    ::CloseHandle(file);
    return ok;
}

inline std::string AssetsRoot() {
    return Combine(AppDataDirectory(), "NightSharp\\assets");
}

inline bool InstallTable(
    const Data::EmbeddedAssets::AssetEntry* assets,
    std::size_t count,
    const char* label) {
    const std::string root = AssetsRoot();
    if (root.empty() || !EnsureDirectory(root)) {
        Utils::Logging::Write()(LogLevel::Warn, "Assets: cannot create AppData asset root");
        return false;
    }

    int written = 0;
    int cached = 0;
    int failed = 0;
    for (std::size_t i = 0; i < count; ++i) {
        const auto& asset = assets[i];
        const std::string path = Combine(root, asset.RelativePath);
        if (FileSizeMatches(path, asset.Size)) {
            ++cached;
            continue;
        }

        if (WriteAllBytes(path, asset.Bytes, asset.Size)) {
            ++written;
        } else {
            ++failed;
        }
    }

    Utils::Logging::Write()(
        failed == 0 ? LogLevel::Info : LogLevel::Warn,
        "Assets: %s -> %s (written=%d cached=%d failed=%d)",
        label ? label : "?",
        root.c_str(),
        written,
        cached,
        failed);
    return failed == 0;
}

} // namespace detail

inline std::string AssetsRoot() {
    return detail::AssetsRoot();
}

inline bool InstallDragonSoulAssets() {
    static bool attempted = false;
    static bool ok = false;
    if (attempted) {
        return ok;
    }
    attempted = true;

    std::size_t count = 0;
    const auto* assets = Data::EmbeddedAssets::DragonSoulAssets(count);
    ok = detail::InstallTable(assets, count, "dragonsoul");
    return ok;
}

inline bool InstallImageAssets() {
    static bool attempted = false;
    static bool ok = false;
    if (attempted) {
        return ok;
    }
    attempted = true;

    std::size_t count = 0;
    const auto* assets = Data::EmbeddedAssets::ImageAssets(count);
    ok = detail::InstallTable(assets, count, "images");
    return ok;
}

inline bool InstallCursorAssets() {
    static bool attempted = false;
    static bool ok = false;
    if (attempted) {
        return ok;
    }
    attempted = true;

    std::size_t count = 0;
    const auto* assets = Data::EmbeddedAssets::CursorAssets(count);
    ok = detail::InstallTable(assets, count, "cursor");
    return ok;
}

inline std::string DragonSoulDirectory() {
    (void)InstallDragonSoulAssets();
    return detail::Combine(AssetsRoot(), "dragonsoul");
}

inline std::string ImagesRoot() {
    (void)InstallImageAssets();
    return detail::Combine(AssetsRoot(), "Images");
}

inline std::string ImagesChampionsDirectory() {
    (void)InstallImageAssets();
    return detail::Combine(AssetsRoot(), "Images\\Champions");
}

inline std::string ImagesSpellsDirectory() {
    (void)InstallImageAssets();
    return detail::Combine(AssetsRoot(), "Images\\Spells");
}

inline std::string ImagesSummonerSpellsDirectory() {
    (void)InstallImageAssets();
    return detail::Combine(AssetsRoot(), "Images\\SummonerSpells");
}

inline std::string CursorHandPath() {
    (void)InstallCursorAssets();
    return detail::Combine(AssetsRoot(), "cursor\\hand1.png");
}

} // namespace SDK::Utils::AssetInstaller
