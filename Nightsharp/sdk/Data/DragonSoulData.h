#pragma once

#include <Windows.h>

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

namespace SDK::Data::DragonSoulData {

struct DragonSoulEntry {
    std::uint8_t TerrainId = 0;
    std::string_view Name = {};
    std::string_view IconKey = {};
    std::string_view FileName = {};
};

inline constexpr std::array<DragonSoulEntry, 6> kDragonSouls = {
    DragonSoulEntry{ 1, "Infernal", "dragonsouliconinfernal", "dragonsouliconinfernal.png" },
    DragonSoulEntry{ 2, "Mountain", "dragonsouliconmountain", "dragonsouliconmountain.png" },
    DragonSoulEntry{ 3, "Ocean",    "dragonsouliconocean",    "dragonsouliconocean.png" },
    DragonSoulEntry{ 4, "Wind",     "dragonsouliconcloud",    "dragonsouliconcloud.png" },
    DragonSoulEntry{ 5, "Hextech",  "dragonsouliconhextech",  "dragonsouliconhextech.png" },
    DragonSoulEntry{ 6, "Chemtech", "dragonsouliconchemtech", "dragonsouliconchemtech.png" },
};

inline const DragonSoulEntry* FindByTerrainId(std::uint8_t terrainId) {
    for (const auto& entry : kDragonSouls) {
        if (entry.TerrainId == terrainId) {
            return &entry;
        }
    }
    return nullptr;
}

inline std::string_view Name(std::uint8_t terrainId) {
    if (const auto* entry = FindByTerrainId(terrainId)) {
        return entry->Name;
    }
    return "Unknown";
}

inline std::string_view IconKey(std::uint8_t terrainId) {
    if (const auto* entry = FindByTerrainId(terrainId)) {
        return entry->IconKey;
    }
    return {};
}

inline std::string_view FileName(std::uint8_t terrainId) {
    if (const auto* entry = FindByTerrainId(terrainId)) {
        return entry->FileName;
    }
    return {};
}

namespace detail {

inline std::string SelfModuleDirectory() {
    HMODULE self = nullptr;
    if (!::GetModuleHandleExA(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCSTR>(&SelfModuleDirectory),
            &self) || !self) {
        return {};
    }

    char path[MAX_PATH] = {};
    const DWORD len = ::GetModuleFileNameA(self, path, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) {
        return {};
    }

    std::string result(path, path + len);
    const auto slash = result.find_last_of("\\/");
    return slash == std::string::npos ? std::string() : result.substr(0, slash);
}

inline bool FileExists(const std::string& path) {
    const DWORD attr = ::GetFileAttributesA(path.c_str());
    return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

inline bool FolderHasDragonSoulIcons(const std::string& folder) {
    return !folder.empty() &&
           FileExists(folder + "\\dragonsouliconinfernal.png") &&
           FileExists(folder + "\\dragonsouliconchemtech.png");
}

} // namespace detail

inline std::string IconDirectory() {
    char envPath[MAX_PATH] = {};
    const DWORD envLen = ::GetEnvironmentVariableA(
        "NIGHTSHARP_DRAGONSOUL_DATA_DIR", envPath, MAX_PATH);
    if (envLen > 0 && envLen < MAX_PATH && detail::FolderHasDragonSoulIcons(envPath)) {
        return std::string(envPath, envPath + envLen);
    }

    const std::string selfDir = detail::SelfModuleDirectory();
    std::string candidates[4] = {};
    int candidateCount = 0;
    if (!selfDir.empty()) {
        candidates[candidateCount++] = selfDir + "\\..\\..\\SDK\\Data";
        candidates[candidateCount++] = selfDir + "\\SDK\\Data";
        candidates[candidateCount++] = selfDir + "\\Data";
    }
    candidates[candidateCount++] = "C:\\NightSharp\\SDK\\Data";

    for (int i = 0; i < candidateCount; ++i) {
        const auto& candidate = candidates[i];
        if (detail::FolderHasDragonSoulIcons(candidate)) {
            return candidate;
        }
    }

    return {};
}

inline std::string IconPath(std::uint8_t terrainId) {
    const auto fileName = FileName(terrainId);
    if (fileName.empty()) {
        return {};
    }
    const std::string directory = IconDirectory();
    return directory.empty() ? std::string() : directory + "\\" + std::string(fileName);
}

} // namespace SDK::Data::DragonSoulData
