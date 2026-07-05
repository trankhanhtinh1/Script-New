#pragma once

// ============================================================================
// GameData.h - Static unit/spell/item data + icon registry
// ----------------------------------------------------------------------------
// Port of LView/GameData.{h,cpp}.
//
// Differences vs the original:
//   * boost::json  -> nlohmann::json (already in libs/, no DLL deps)
//   * Texture2D    -> SDK::UI::Icons (existing D3D11 PNG cache)
//   * printf logs  -> SDK::Utils::Logging
//   * Each loader is wrapped in try/catch so a malformed JSON never crashes
//     the host process; it logs the failure and continues with whatever
//     data it managed to parse.
//
// File layout under <dataFolder>/:
//   UnitData.json
//   SpellData.json
//   SpellDataCustom.json   (optional — same schema, merged on top)
//   ItemData.json
//   icons_spells/*.png
//   icons_champs/*.png
//   icons_extra/*.png
//
// The default folder is `<host_exe_directory>\data` so users can keep the
// JSONs and icon directories next to the running League executable.
// ============================================================================

#include "ItemData.h"
#include "ItemInfo.h"
#include "RuneData.h"
#include "SpellInfo.h"
#include "UnitInfo.h"

#include "../UI/Icons.h"
#include "../Utils/AssetInstaller.h"
#include "../Utils/Logging.h"
#include "../../libs/nlohmann/json.hpp"

#include <Windows.h>
#include <imgui.h>

#include <cstring>
#include <fstream>
#include <map>
#include <string>

namespace SDK::Data::GameData {

namespace detail {

    // ────── Internal storage ────────────────────────────────────────────
    inline UnitInfo*&  UnknownUnitSlot()  { static UnitInfo*  u = nullptr; return u; }
    inline SpellInfo*& UnknownSpellSlot() { static SpellInfo* s = nullptr; return s; }
    inline ItemInfo*&  UnknownItemSlot()  { static ItemInfo*  i = nullptr; return i; }

    inline std::map<std::string, UnitInfo*>&  UnitsMap()  { static std::map<std::string, UnitInfo*>  m; return m; }
    inline std::map<std::string, SpellInfo*>& SpellsMap() { static std::map<std::string, SpellInfo*> m; return m; }
    inline std::map<int, ItemInfo*>&          ItemsMap()  { static std::map<int, ItemInfo*>          m; return m; }
    inline std::map<int, ItemInfo>& GeneratedItemsMap() { static std::map<int, ItemInfo> m; return m; }

    inline bool& Initialized() { static bool b = false; return b; }

    // ────── Helpers ─────────────────────────────────────────────────────

    inline std::string ToLower(std::string s) {
        for (auto& c : s) {
            if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + ('a' - 'A'));
        }
        return s;
    }

    // Defensive numeric extraction — handles JSON values that arrive as
    // either integer or float and missing/null entries (returns 0).
    inline float JsonFloat(const nlohmann::json& obj, const char* key) {
        if (!obj.contains(key) || obj[key].is_null()) return 0.0f;
        const auto& v = obj[key];
        if (v.is_number_float())    return static_cast<float>(v.get<double>());
        if (v.is_number_integer())  return static_cast<float>(v.get<int64_t>());
        if (v.is_number_unsigned()) return static_cast<float>(v.get<uint64_t>());
        return 0.0f;
    }

    inline int64_t JsonInt(const nlohmann::json& obj, const char* key) {
        if (!obj.contains(key) || obj[key].is_null()) return 0;
        const auto& v = obj[key];
        if (v.is_number_integer())  return v.get<int64_t>();
        if (v.is_number_unsigned()) return static_cast<int64_t>(v.get<uint64_t>());
        if (v.is_number_float())    return static_cast<int64_t>(v.get<double>());
        return 0;
    }

    inline std::string JsonString(const nlohmann::json& obj, const char* key) {
        if (!obj.contains(key) || obj[key].is_null()) return {};
        const auto& v = obj[key];
        return v.is_string() ? v.get<std::string>() : std::string();
    }

    inline bool JsonBool(const nlohmann::json& obj, const char* key) {
        if (!obj.contains(key) || obj[key].is_null()) return false;
        const auto& v = obj[key];
        return v.is_boolean() ? v.get<bool>() : false;
    }

    inline bool ParseFile(const std::string& path, nlohmann::json& out) {
        std::ifstream stream(path, std::ios::binary);
        if (!stream.is_open()) {
            Utils::Logging::Write(true, true, "GameData::ParseFile")(
                LogLevel::Warn,
                "Cannot open '%s'",
                path.c_str());
            return false;
        }
        try {
            out = nlohmann::json::parse(stream, nullptr, /*allow_exceptions*/ true);
            return true;
        } catch (const std::exception& ex) {
            Utils::Logging::Write(true, true, "GameData::ParseFile")(
                LogLevel::Error,
                "Failed to parse '%s' - %s",
                path.c_str(),
                ex.what());
            return false;
        } catch (...) {
            Utils::Logging::Write(true, true, "GameData::ParseFile")(
                LogLevel::Error,
                "Failed to parse '%s' - unknown error",
                path.c_str());
            return false;
        }
    }

    inline ItemInfo* GetGeneratedItemInfoById(int id) {
        const auto* entry = ItemDatabase::FindById(id);
        if (!entry) {
            return nullptr;
        }

        auto& generated = GeneratedItemsMap();
        auto it = generated.find(id);
        if (it == generated.end()) {
            it = generated.emplace(id, entry->Stats).first;
        }
        return &it->second;
    }

    // ────── Loaders ─────────────────────────────────────────────────────

    inline void LoadUnitData(const std::string& path) {
        nlohmann::json doc;
        if (!ParseFile(path, doc) || !doc.is_array()) return;

        int loaded = 0;
        for (const auto& entry : doc) {
            if (!entry.is_object()) continue;

            UnitInfo* info = new UnitInfo();
            info->name                    = ToLower(JsonString(entry, "name"));
            info->acquisitionRange        = JsonFloat(entry, "acquisitionRange");
            info->attackSpeedRatio        = JsonFloat(entry, "attackSpeedRatio");
            info->baseAttackRange         = JsonFloat(entry, "attackRange");
            info->baseAttackSpeed         = JsonFloat(entry, "attackSpeed");
            info->baseMovementSpeed       = JsonFloat(entry, "baseMoveSpeed");
            info->basicAttackMissileSpeed = JsonFloat(entry, "basicAtkMissileSpeed");
            info->basicAttackWindup       = JsonFloat(entry, "basicAtkWindup");
            info->gameplayRadius          = JsonFloat(entry, "gameplayRadius");
            info->healthBarHeight         = JsonFloat(entry, "healthBarHeight");
            info->pathRadius              = JsonFloat(entry, "pathingRadius");
            info->selectionRadius         = JsonFloat(entry, "selectionRadius");

            if (entry.contains("tags") && entry["tags"].is_array()) {
                for (const auto& tag : entry["tags"]) {
                    if (tag.is_string()) {
                        info->SetTag(tag.get<std::string>().c_str());
                    }
                }
            }

            if (info->name.empty()) {
                delete info;
                continue;
            }

            auto& slot = UnitsMap()[info->name];
            if (slot) delete slot;          // guard against duplicates / re-load
            slot = info;
            ++loaded;
        }

        Utils::Logging::Write()(LogLevel::Info,
            "GameData: loaded %d units from '%s'",
            loaded, path.c_str());
    }

    inline void LoadSpellData(const std::string& path) {
        nlohmann::json doc;
        if (!ParseFile(path, doc) || !doc.is_array()) return;

        int loaded = 0;
        for (const auto& entry : doc) {
            if (!entry.is_object()) continue;

            SpellInfo* info = new SpellInfo();
            info->flags      = static_cast<SpellFlags>(JsonInt(entry, "flags"));
            info->delay      = JsonFloat(entry, "delay");
            info->height     = JsonFloat(entry, "height");
            info->icon       = ToLower(JsonString(entry, "icon"));
            info->name       = ToLower(JsonString(entry, "name"));
            info->width      = JsonFloat(entry, "width");
            info->castRange  = JsonFloat(entry, "castRange");
            info->castRadius = JsonFloat(entry, "castRadius");
            info->speed      = JsonFloat(entry, "speed");
            info->travelTime = JsonFloat(entry, "travelTime");

            if (JsonBool(entry, "projectDestination")) {
                info->AddFlags(ProjectedDestination);
            }

            if (info->name.empty()) {
                delete info;
                continue;
            }

            auto& slot = SpellsMap()[info->name];
            if (slot) delete slot;
            slot = info;
            ++loaded;
        }

        Utils::Logging::Write()(LogLevel::Info,
            "GameData: loaded %d spells from '%s'",
            loaded, path.c_str());
    }

    inline void LoadItemData(const std::string& path) {
        nlohmann::json doc;
        if (!ParseFile(path, doc) || !doc.is_array()) return;

        int loaded = 0;
        for (const auto& entry : doc) {
            if (!entry.is_object()) continue;

            ItemInfo* info = new ItemInfo();
            info->movementSpeed        = JsonFloat(entry, "movementSpeed");
            info->health               = JsonFloat(entry, "health");
            info->crit                 = JsonFloat(entry, "crit");
            info->abilityPower         = JsonFloat(entry, "abilityPower");
            info->mana                 = JsonFloat(entry, "mana");
            info->armour               = JsonFloat(entry, "armour");
            info->magicResist          = JsonFloat(entry, "magicResist");
            info->physicalDamage       = JsonFloat(entry, "physicalDamage");
            info->attackSpeed          = JsonFloat(entry, "attackSpeed");
            info->lifeSteal            = JsonFloat(entry, "lifeSteal");
            info->hpRegen              = JsonFloat(entry, "hpRegen");
            info->movementSpeedPercent = JsonFloat(entry, "movementSpeedPercent");
            info->cost                 = JsonFloat(entry, "cost");
            info->id                   = static_cast<int>(JsonInt(entry, "id"));

            if (info->id == 0) {
                delete info;
                continue;
            }

            auto& slot = ItemsMap()[info->id];
            if (slot) delete slot;
            slot = info;
            ++loaded;
        }

        Utils::Logging::Write()(LogLevel::Info,
            "GameData: loaded %d items from '%s'",
            loaded, path.c_str());
    }

    // ────── Path helpers ────────────────────────────────────────────────

    inline std::string HostExeDirectory() {
        char buf[MAX_PATH] = {};
        const DWORD len = ::GetModuleFileNameA(nullptr, buf, MAX_PATH);
        if (len == 0 || len >= MAX_PATH) return {};
        std::string path(buf, buf + len);
        const auto slash = path.find_last_of("\\/");
        return slash == std::string::npos ? std::string() : path.substr(0, slash);
    }

    // Try the DLL's own directory (only meaningful for LoadLibrary path —
    // manual map returns empty). Used as a secondary search root so the
    // user can drop NightSharp.dll + data/ next to League.exe and have it
    // just work.
    inline std::string SelfModuleDirectory() {
        HMODULE hSelf = nullptr;
        if (!::GetModuleHandleExA(
                GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                reinterpret_cast<LPCSTR>(&SelfModuleDirectory),
                &hSelf) || !hSelf) {
            return {};
        }
        char buf[MAX_PATH] = {};
        const DWORD len = ::GetModuleFileNameA(hSelf, buf, MAX_PATH);
        if (len == 0 || len >= MAX_PATH) return {};
        std::string path(buf, buf + len);
        const auto slash = path.find_last_of("\\/");
        return slash == std::string::npos ? std::string() : path.substr(0, slash);
    }

    inline bool FolderHasUnitData(const std::string& folder) {
        if (folder.empty()) return false;
        const std::string probe = folder + "\\UnitData.json";
        DWORD attr = ::GetFileAttributesA(probe.c_str());
        return attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY);
    }

} // namespace detail

// ────── Public API ──────────────────────────────────────────────────────

// Resolve the data folder. Search order (first hit wins):
//   1. Env var NIGHTSHARP_DATA_DIR
//   2. <host_exe_dir>\data
//   3. <host_exe_dir>\NightSharp\data
//   4. <self_dll_dir>\data            (LoadLibrary path)
//   5. C:\NightSharp\data             (canonical install fallback)
// Falls back to <host_exe_dir>\data even when none exists so the loader
// logs a clear "not found" once instead of silently doing nothing.
inline std::string DefaultDataFolder() {
    char envBuf[MAX_PATH] = {};
    const DWORD envLen = ::GetEnvironmentVariableA("NIGHTSHARP_DATA_DIR", envBuf, MAX_PATH);
    if (envLen > 0 && envLen < MAX_PATH && detail::FolderHasUnitData(envBuf)) {
        return std::string(envBuf, envBuf + envLen);
    }

    const std::string exeDir  = detail::HostExeDirectory();
    const std::string selfDir = detail::SelfModuleDirectory();

    const std::string candidates[] = {
        std::string("C:\\NightSharp\\data"),
    };

    for (const auto& c : candidates) {
        if (!c.empty() && detail::FolderHasUnitData(c)) {
            return c;
        }
    }

    // Nothing found — return the most sensible guess so the failure log
    // points the user at the right install location.
    return exeDir.empty() ? std::string("data") : (exeDir + "\\data");
}

// LView's `GameData::Load(dataFolder)` 1:1.
inline void Load(const std::string& dataFolder) {
    if (dataFolder.empty()) {
        Utils::Logging::Write(true, true, "GameData::Load")(
            LogLevel::Warn, "empty data folder, skipping");
        return;
    }

    // Make sure the unknown sentinels exist before any Get*ByName call could
    // possibly run (e.g. plugins firing off OnLoad concurrently).
    if (!detail::UnknownUnitSlot())  detail::UnknownUnitSlot()  = new UnitInfo();
    if (!detail::UnknownSpellSlot()) detail::UnknownSpellSlot() = new SpellInfo();
    if (!detail::UnknownItemSlot())  detail::UnknownItemSlot()  = new ItemInfo();

    Utils::Logging::Write()(LogLevel::Info,
        "GameData: loading from '%s'", dataFolder.c_str());

    const std::string unitData        = dataFolder + "\\UnitData.json";
    const std::string spellData       = dataFolder + "\\SpellData.json";
    const std::string spellDataCustom = dataFolder + "\\SpellDataCustom.json";
    const std::string itemData        = dataFolder + "\\ItemData.json";

    // Load order matches LView (items → units → spells → custom spells).
    // Icons are NOT loaded here because the D3D11 device might not be ready
    // yet at this point. Icon loading happens separately via LoadIcons()
    // which is called from the overlay after SetDevice().
    detail::LoadItemData(itemData);
    detail::LoadUnitData(unitData);
    detail::LoadSpellData(spellData);
    detail::LoadSpellData(spellDataCustom);

    detail::Initialized() = true;
    Utils::Logging::Write()(LogLevel::Info, "GameData: loading complete");
}

// Load all icons from the data folder. Must be called AFTER
// SDK::UI::Icons::SetDevice() has been called (i.e. D3D11 device is ready).
// Called from Overlay after device creation, not from Bootstrap::Init.
inline void LoadIcons(const std::string& dataFolder) {
    const std::string embeddedImages = SDK::Utils::AssetInstaller::ImagesRoot();
    if (!embeddedImages.empty()) {
        SDK::UI::Icons::LoadIconsFromDirectory(embeddedImages.c_str());
    }

    if (dataFolder.empty()) {
        Utils::Logging::Write()(LogLevel::Info, "GameData: icon loading complete");
        return;
    }

    const std::string iconsFolder     = dataFolder + "/icons";
    const std::string spellIcons      = dataFolder + "/icons_spells";
    const std::string champIcons      = dataFolder + "/icons_champs";
    const std::string extraIcons      = dataFolder + "/icons_extra";

    // Icons go through the existing SDK::UI::Icons cache. The cache is keyed
    // on lowercased filename without extension (LView convention) - keys keep
    // their underscores ("aatrox_q", "aatrox_square") so SpellInfo::icon and
    // <charname>_square lookups work without further normalisation.
    SDK::UI::Icons::LoadIconsFromDirectory(iconsFolder.c_str());
    SDK::UI::Icons::LoadIconsFromDirectory(spellIcons.c_str());
    SDK::UI::Icons::LoadIconsFromDirectory(champIcons.c_str());
    SDK::UI::Icons::LoadIconsFromDirectory(extraIcons.c_str());

    Utils::Logging::Write()(LogLevel::Info, "GameData: icon loading complete");
}

inline void LoadIcons() { LoadIcons(DefaultDataFolder()); }

inline void Load() { Load(DefaultDataFolder()); }

inline bool IsLoaded() { return detail::Initialized(); }

// ────── Lookups (LView API parity) ──────────────────────────────────────

inline UnitInfo* GetUnitInfoByName(const std::string& name) {
    auto& units = detail::UnitsMap();
    const auto key = detail::ToLower(name);
    const auto it = units.find(key);
    if (it != units.end()) return it->second;

    if (!detail::UnknownUnitSlot()) detail::UnknownUnitSlot() = new UnitInfo();
    return detail::UnknownUnitSlot();
}

inline SpellInfo* GetSpellInfoByName(const std::string& name) {
    auto& spells = detail::SpellsMap();
    const auto key = detail::ToLower(name);
    const auto it = spells.find(key);
    if (it != spells.end()) return it->second;

    if (!detail::UnknownSpellSlot()) detail::UnknownSpellSlot() = new SpellInfo();
    return detail::UnknownSpellSlot();
}

inline ItemInfo* GetItemInfoById(int id) {
    auto& items = detail::ItemsMap();
    const auto it = items.find(id);
    if (it != items.end()) return it->second;

    if (ItemInfo* generated = detail::GetGeneratedItemInfoById(id)) {
        return generated;
    }

    if (!detail::UnknownItemSlot()) detail::UnknownItemSlot() = new ItemInfo();
    return detail::UnknownItemSlot();
}

inline const ItemDatabase::ItemDataEntry* GetItemDataById(int id) {
    return ItemDatabase::FindById(id);
}

inline const RuneDatabase::RuneDataEntry* GetRuneDataById(int id) {
    return RuneDatabase::FindById(id);
}

inline const RuneDatabase::RuneStyleDataEntry* GetRuneStyleById(int id) {
    return RuneDatabase::FindStyleById(id);
}

// Icon resolved by spell name. The lookup chain is:
//   1. SpellInfo lookup: spell name -> SpellInfo::icon -> cache.
//   2. Direct cache lookup on the raw input (lowercased).
//   3. Heuristic for summoner spells: try "summoner_<suffix>" pattern.
//   4. ScriptName-style: insert "_" between known segments.
inline ImTextureID GetSpellIcon(const std::string& spellNameOrIcon) {
    if (spellNameOrIcon.empty()) return SDK::UI::Icons::GetPlaceholder();

    const std::string lower = detail::ToLower(spellNameOrIcon);

    // Step 1 - SpellInfo lookup (spell name → icon field from JSON).
    {
        auto& spells = detail::SpellsMap();
        const auto it = spells.find(lower);
        if (it != spells.end() && it->second && !it->second->icon.empty()) {
            ImTextureID tex = SDK::UI::Icons::GetIcon(it->second->icon);
            if (tex && tex != SDK::UI::Icons::GetPlaceholder()) return tex;
        }
    }

    // Step 2 - direct icon-stem lookup (maybe caller passed the icon stem directly).
    {
        ImTextureID tex = SDK::UI::Icons::GetIcon(lower);
        if (tex && tex != SDK::UI::Icons::GetPlaceholder()) return tex;
    }

    // Step 3 - summoner spell heuristics.
    // Game engine returns names like "SummonerFlash", "SummonerDot", "SummonerBoost".
    // Icon files can be "summoner_flash.png" or "summonerignite.png" (no consistent pattern).
    // Try multiple fallback patterns.
    if (lower.size() > 8 && lower.rfind("summoner", 0) == 0) {
        const std::string suffix = lower.substr(8); // e.g. "flash", "dot", "boost"

        // Try "summoner_<suffix>"  (summoner_flash, summoner_boost etc.)
        {
            ImTextureID tex = SDK::UI::Icons::GetIcon("summoner_" + suffix);
            if (tex && tex != SDK::UI::Icons::GetPlaceholder()) return tex;
        }
        // Try "summoner<suffix>" without underscore (summonerignite, summonerbarrier)
        {
            ImTextureID tex = SDK::UI::Icons::GetIcon("summoner" + suffix);
            if (tex && tex != SDK::UI::Icons::GetPlaceholder()) return tex;
        }
    }

    return SDK::UI::Icons::GetPlaceholder();
}

inline ImTextureID GetChampionSquare(const std::string& championName) {
    return SDK::UI::Icons::GetChampionSquare(championName);
}

// Try resolving a champion icon using UnitInfo name as additional fallback.
// UnitInfo stores the "canonical" lowercase name from UnitData.json which
// may differ from what CharacterName() returns at runtime for edge-case champs.
inline ImTextureID GetChampionSquareWithFallback(const std::string& runtimeName) {
    if (runtimeName.empty()) return SDK::UI::Icons::GetPlaceholder();

    // Primary: standard lookup (tries "name_square" and "name" patterns).
    ImTextureID tex = SDK::UI::Icons::GetChampionSquare(runtimeName);
    if (tex && tex != SDK::UI::Icons::GetPlaceholder()) return tex;

    // Secondary: look up the canonical name from UnitInfo data.
    const UnitInfo* info = GetUnitInfoByName(runtimeName);
    if (info && !info->name.empty() && info->name != detail::ToLower(runtimeName)) {
        tex = SDK::UI::Icons::GetChampionSquare(info->name);
        if (tex && tex != SDK::UI::Icons::GetPlaceholder()) return tex;
    }

    return SDK::UI::Icons::GetPlaceholder();
}

// ────── Cleanup ─────────────────────────────────────────────────────────

inline void Reset() {
    auto& units  = detail::UnitsMap();
    auto& spells = detail::SpellsMap();
    auto& items  = detail::ItemsMap();

    for (auto& kv : units)  delete kv.second;
    for (auto& kv : spells) delete kv.second;
    for (auto& kv : items)  delete kv.second;

    units.clear();
    spells.clear();
    items.clear();

    if (detail::UnknownUnitSlot())  { delete detail::UnknownUnitSlot();  detail::UnknownUnitSlot()  = nullptr; }
    if (detail::UnknownSpellSlot()) { delete detail::UnknownSpellSlot(); detail::UnknownSpellSlot() = nullptr; }
    if (detail::UnknownItemSlot())  { delete detail::UnknownItemSlot();  detail::UnknownItemSlot()  = nullptr; }

    detail::Initialized() = false;
}

} // namespace SDK::Data::GameData
