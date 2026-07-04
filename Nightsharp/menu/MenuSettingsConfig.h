#pragma once
// ============================================================================
// MenuSettingsConfig.h - Persistent menu settings for NightSharp.
//
// Stores runtime menu values under:
//   %AppData%\NightSharp\Config
//
// File ownership:
//   NightSharp.ini      - core NightSharp menu settings
//   NightSharp.SDK.ini  - built-in SDK menu tree (Orbwalker, TargetSelector, ...)
//   <plugin>.ini        - source plugins or external package/plugin menus
// ============================================================================

#include "MenuConfig.h"
#include "../DebugLog.h"
#include "../Plugins/PluginRegistry.h"
#include "../SDK/UI/UI.h"

#include <Windows.h>
#include <ShlObj.h>

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace NightSharpMenu::MenuSettingsConfig {

inline constexpr DWORD kSaveDebounceMs = 500;
inline bool g_coreLoaded = false;
inline DWORD g_lastSaveTick = 0;
inline std::unordered_set<const SDK::UI::MenuItem*> g_loadedItems;
inline std::unordered_map<std::string, std::string> g_lastSnapshots;

inline bool EnsureDirectory(const char* path) {
    if (!path || !path[0]) {
        return false;
    }

    const DWORD attrs = GetFileAttributesA(path);
    if (attrs != INVALID_FILE_ATTRIBUTES) {
        return (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0;
    }

    if (CreateDirectoryA(path, nullptr)) {
        return true;
    }

    return GetLastError() == ERROR_ALREADY_EXISTS;
}

inline bool GetConfigDirectory(char* out, DWORD outSize) {
    if (!out || outSize == 0) {
        return false;
    }
    out[0] = '\0';

    char appData[MAX_PATH] = {};
    if (FAILED(SHGetFolderPathA(nullptr, CSIDL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, appData))) {
        NightSharpDebug::Logf("[MenuSettingsConfig] SHGetFolderPathA failed gle=%lu",
                              static_cast<unsigned long>(GetLastError()));
        return false;
    }

    char nightSharpDir[MAX_PATH] = {};
    _snprintf_s(nightSharpDir, sizeof(nightSharpDir), _TRUNCATE, "%s\\NightSharp", appData);
    if (!EnsureDirectory(nightSharpDir)) {
        NightSharpDebug::Logf("[MenuSettingsConfig] Create dir failed path=%s gle=%lu",
                              nightSharpDir,
                              static_cast<unsigned long>(GetLastError()));
        return false;
    }

    _snprintf_s(out, outSize, _TRUNCATE, "%s\\Config", nightSharpDir);
    if (!EnsureDirectory(out)) {
        NightSharpDebug::Logf("[MenuSettingsConfig] Create dir failed path=%s gle=%lu",
                              out,
                              static_cast<unsigned long>(GetLastError()));
        return false;
    }

    return true;
}

inline std::string SanitizeFileName(const char* value) {
    std::string out;
    if (value) {
        for (const unsigned char ch : std::string(value)) {
            if (std::isalnum(ch) || ch == '-' || ch == '_' || ch == '.') {
                out.push_back(static_cast<char>(ch));
            } else {
                out.push_back('_');
            }
        }
    }
    if (out.empty()) {
        out = "NightSharp";
    }
    if (out.size() < 4 || _stricmp(out.c_str() + out.size() - 4, ".ini") != 0) {
        out += ".ini";
    }
    return out;
}

inline bool BuildConfigPath(const char* fileName, char* out, DWORD outSize) {
    if (!out || outSize == 0) {
        return false;
    }
    out[0] = '\0';

    char dir[MAX_PATH] = {};
    if (!GetConfigDirectory(dir, MAX_PATH)) {
        return false;
    }

    const std::string safeFile = SanitizeFileName(fileName);
    _snprintf_s(out, outSize, _TRUNCATE, "%s\\%s", dir, safeFile.c_str());
    return out[0] != '\0';
}

inline std::string IniSafeValue(const char* value) {
    std::string out;
    if (value) {
        for (const char ch : std::string(value)) {
            if (ch == '\r' || ch == '\n') {
                out.push_back(' ');
            } else {
                out.push_back(ch);
            }
        }
    }
    return out;
}

inline std::string IniSafeName(const char* value, const char* fallback) {
    std::string out;
    if (value) {
        for (const unsigned char ch : std::string(value)) {
            if (std::isalnum(ch) || ch == '-' || ch == '_' || ch == '.') {
                out.push_back(static_cast<char>(ch));
            } else {
                out.push_back('_');
            }
        }
    }
    if (out.empty() && fallback) {
        return IniSafeName(fallback, nullptr);
    }
    return out.empty() ? "unnamed" : out;
}

inline bool ParseBool(const char* value, bool fallback) {
    if (!value || !value[0]) {
        return fallback;
    }
    if (value[0] == '1') return true;
    if (value[0] == '0') return false;
    if (_stricmp(value, "true") == 0 || _stricmp(value, "on") == 0 ||
        _stricmp(value, "yes") == 0) {
        return true;
    }
    if (_stricmp(value, "false") == 0 || _stricmp(value, "off") == 0 ||
        _stricmp(value, "no") == 0) {
        return false;
    }
    return fallback;
}

inline bool ReadIniValue(const char* path,
                         const char* section,
                         const char* key,
                         char* out,
                         DWORD outSize) {
    if (!path || !section || !key || !out || outSize == 0) {
        return false;
    }
    out[0] = '\0';
    const DWORD read = GetPrivateProfileStringA(section, key, "", out, outSize, path);
    return read > 0;
}

inline bool ReadBool(const char* path, const char* section, const char* key, bool fallback) {
    char value[64] = {};
    if (!ReadIniValue(path, section, key, value, sizeof(value))) {
        return fallback;
    }
    return ParseBool(value, fallback);
}

inline int ReadInt(const char* path, const char* section, const char* key, int fallback) {
    char value[64] = {};
    if (!ReadIniValue(path, section, key, value, sizeof(value))) {
        return fallback;
    }
    char* end = nullptr;
    const long parsed = std::strtol(value, &end, 0);
    return end && *end == '\0' ? static_cast<int>(parsed) : fallback;
}

inline float ReadFloat(const char* path, const char* section, const char* key, float fallback) {
    char value[64] = {};
    if (!ReadIniValue(path, section, key, value, sizeof(value))) {
        return fallback;
    }
    char* end = nullptr;
    const float parsed = std::strtof(value, &end);
    return end && *end == '\0' ? parsed : fallback;
}

inline unsigned int ReadUInt(const char* path,
                             const char* section,
                             const char* key,
                             unsigned int fallback) {
    char value[64] = {};
    if (!ReadIniValue(path, section, key, value, sizeof(value))) {
        return fallback;
    }
    char* end = nullptr;
    const unsigned long parsed = std::strtoul(value, &end, 0);
    return end && *end == '\0' ? static_cast<unsigned int>(parsed) : fallback;
}

inline const char* ValueTypeName(SDK::UI::MenuValueType type) {
    switch (type) {
    case SDK::UI::MenuValueType::Boolean:   return "Boolean";
    case SDK::UI::MenuValueType::Slider:    return "Slider";
    case SDK::UI::MenuValueType::SliderF:   return "SliderF";
    case SDK::UI::MenuValueType::KeyBind:   return "KeyBind";
    case SDK::UI::MenuValueType::List:      return "List";
    case SDK::UI::MenuValueType::Button:    return "Button";
    case SDK::UI::MenuValueType::Color:     return "Color";
    case SDK::UI::MenuValueType::Separator: return "Separator";
    case SDK::UI::MenuValueType::SliderBtn: return "SliderButton";
    case SDK::UI::MenuValueType::None:
    default:                                return "None";
    }
}

inline bool IsPersistable(const SDK::UI::MenuItem* item) {
    if (!item) {
        return false;
    }

    switch (item->Kind()) {
    case SDK::UI::MenuValueType::Boolean:
    case SDK::UI::MenuValueType::Slider:
    case SDK::UI::MenuValueType::SliderF:
    case SDK::UI::MenuValueType::KeyBind:
    case SDK::UI::MenuValueType::List:
    case SDK::UI::MenuValueType::Color:
    case SDK::UI::MenuValueType::SliderBtn:
        return true;
    case SDK::UI::MenuValueType::Button:
    case SDK::UI::MenuValueType::Separator:
    case SDK::UI::MenuValueType::None:
    default:
        return false;
    }
}

inline void ApplyItemValue(SDK::UI::MenuItem* item, const char* path, const std::string& section) {
    if (!item || !IsPersistable(item)) {
        return;
    }

    const std::string key = IniSafeName(item->Name.c_str(), item->DisplayName.c_str());
    switch (item->Kind()) {
    case SDK::UI::MenuValueType::Boolean: {
        auto* value = static_cast<SDK::UI::MenuBool*>(item);
        char raw[64] = {};
        if (ReadIniValue(path, section.c_str(), key.c_str(), raw, sizeof(raw))) {
            value->Set(ParseBool(raw, value->Value));
        }
        break;
    }
    case SDK::UI::MenuValueType::Slider: {
        auto* value = static_cast<SDK::UI::MenuSlider*>(item);
        char raw[64] = {};
        if (ReadIniValue(path, section.c_str(), key.c_str(), raw, sizeof(raw))) {
            char* end = nullptr;
            const long parsed = std::strtol(raw, &end, 0);
            if (end && *end == '\0') {
                value->Set(static_cast<int>(parsed));
            }
        }
        break;
    }
    case SDK::UI::MenuValueType::SliderF: {
        auto* value = static_cast<SDK::UI::MenuSliderF*>(item);
        char raw[64] = {};
        if (ReadIniValue(path, section.c_str(), key.c_str(), raw, sizeof(raw))) {
            char* end = nullptr;
            const float parsed = std::strtof(raw, &end);
            if (end && *end == '\0') {
                value->Set(parsed);
            }
        }
        break;
    }
    case SDK::UI::MenuValueType::KeyBind: {
        auto* value = static_cast<SDK::UI::MenuKeyBind*>(item);
        const std::string keyCode = key + ".key";
        const std::string active = key + ".active";
        const int loadedKey = ReadInt(path, section.c_str(), keyCode.c_str(), value->Key);
        if (loadedKey != value->Key) {
            value->Key = loadedKey;
            value->FireValueChanged();
        }
        value->SetActive(ReadBool(path, section.c_str(), active.c_str(), value->Active));
        break;
    }
    case SDK::UI::MenuValueType::List: {
        auto* value = static_cast<SDK::UI::MenuList*>(item);
        char raw[64] = {};
        if (ReadIniValue(path, section.c_str(), key.c_str(), raw, sizeof(raw))) {
            char* end = nullptr;
            const long parsed = std::strtol(raw, &end, 0);
            if (end && *end == '\0') {
                value->Set(static_cast<int>(parsed));
            }
        }
        break;
    }
    case SDK::UI::MenuValueType::Color: {
        auto* value = static_cast<SDK::UI::MenuColor*>(item);
        char raw[64] = {};
        if (ReadIniValue(path, section.c_str(), key.c_str(), raw, sizeof(raw))) {
            char* end = nullptr;
            const unsigned long parsed = std::strtoul(raw, &end, 0);
            if (end && *end == '\0') {
                value->Set(static_cast<unsigned int>(parsed));
            }
        }
        break;
    }
    case SDK::UI::MenuValueType::SliderBtn: {
        auto* value = static_cast<SDK::UI::MenuSliderButton*>(item);
        const std::string slider = key + ".value";
        const std::string enabled = key + ".enabled";
        value->SetValue(ReadInt(path, section.c_str(), slider.c_str(), value->Value));
        value->SetEnabled(ReadBool(path, section.c_str(), enabled.c_str(), value->Enabled));
        break;
    }
    default:
        break;
    }
}

inline bool MenuMatchesPlugin(const SDK::UI::Menu* menu, const PluginRegistry::PluginEntry& plugin) {
    if (!menu || !plugin.InternalId) {
        return false;
    }

    if (menu->Name.equals(plugin.InternalId) || menu->DisplayName.equals(plugin.InternalId)) {
        return true;
    }

    if (plugin.Name &&
        (menu->Name.equals(plugin.Name) || menu->DisplayName.equals(plugin.Name))) {
        return true;
    }

    return false;
}

inline const char* PluginKindName(PluginRegistry::PluginKind kind) {
    switch (kind) {
    case PluginRegistry::PluginKind::SDK:      return "SDK";
    case PluginRegistry::PluginKind::Plugin:   return "Plugin";
    case PluginRegistry::PluginKind::External: return "External";
    default:                                   return "Unknown";
    }
}

inline std::string ConfigFileForPlugin(const PluginRegistry::PluginEntry& plugin) {
    if (plugin.ConfigFileName && plugin.ConfigFileName[0]) {
        return SanitizeFileName(plugin.ConfigFileName);
    }
    if (plugin.InternalId && plugin.InternalId[0]) {
        return SanitizeFileName(plugin.InternalId);
    }
    if (plugin.Name && plugin.Name[0]) {
        return SanitizeFileName(plugin.Name);
    }
    return "NightSharp.Plugin.ini";
}

inline std::string ConfigFileForRootMenu(const SDK::UI::Menu* root) {
    if (!root) {
        return "NightSharp.ini";
    }

    if (root->Name.equals("EnsoulSharp")) {
        return "NightSharp.SDK.ini";
    }

    for (int i = 0; i < PluginRegistry::PluginCount; ++i) {
        const auto& plugin = PluginRegistry::Plugins[i];
        if (!MenuMatchesPlugin(root, plugin)) {
            continue;
        }

        if (plugin.ConfigFileName && plugin.ConfigFileName[0]) {
            return SanitizeFileName(plugin.ConfigFileName);
        }
        if (plugin.InternalId && plugin.InternalId[0]) {
            return SanitizeFileName(plugin.InternalId);
        }
        if (plugin.Name && plugin.Name[0]) {
            return SanitizeFileName(plugin.Name);
        }
    }

    return SanitizeFileName(root->Name.empty() ? root->DisplayName.c_str() : root->Name.c_str());
}

inline void AppendLine(std::string& output, const std::string& key, const std::string& value) {
    output += key;
    output += '=';
    output += value;
    output += "\r\n";
}

inline void AppendLine(std::string& output, const std::string& key, const char* value) {
    AppendLine(output, key, std::string(value ? value : ""));
}

inline void AppendLine(std::string& output, const std::string& key, int value) {
    char buffer[64] = {};
    std::snprintf(buffer, sizeof(buffer), "%d", value);
    AppendLine(output, key, buffer);
}

inline void AppendLine(std::string& output, const std::string& key, unsigned int value) {
    char buffer[64] = {};
    std::snprintf(buffer, sizeof(buffer), "0x%08X", value);
    AppendLine(output, key, buffer);
}

inline void AppendLine(std::string& output, const std::string& key, float value) {
    char buffer[64] = {};
    std::snprintf(buffer, sizeof(buffer), "%.6f", static_cast<double>(value));
    AppendLine(output, key, buffer);
}

inline void AppendLine(std::string& output, const std::string& key, bool value) {
    AppendLine(output, key, value ? "1" : "0");
}

inline void AppendItem(std::string& output, const SDK::UI::MenuItem* item) {
    if (!item || !IsPersistable(item)) {
        return;
    }

    const std::string key = IniSafeName(item->Name.c_str(), item->DisplayName.c_str());
    AppendLine(output, "__" + key + ".display", IniSafeValue(item->DisplayName.c_str()));
    AppendLine(output, "__" + key + ".type", ValueTypeName(item->Kind()));

    switch (item->Kind()) {
    case SDK::UI::MenuValueType::Boolean: {
        const auto* value = static_cast<const SDK::UI::MenuBool*>(item);
        AppendLine(output, key, value->Value);
        break;
    }
    case SDK::UI::MenuValueType::Slider: {
        const auto* value = static_cast<const SDK::UI::MenuSlider*>(item);
        AppendLine(output, key, value->Value);
        AppendLine(output, "__" + key + ".min", value->MinValue);
        AppendLine(output, "__" + key + ".max", value->MaxValue);
        break;
    }
    case SDK::UI::MenuValueType::SliderF: {
        const auto* value = static_cast<const SDK::UI::MenuSliderF*>(item);
        AppendLine(output, key, value->Value);
        AppendLine(output, "__" + key + ".min", value->MinValue);
        AppendLine(output, "__" + key + ".max", value->MaxValue);
        break;
    }
    case SDK::UI::MenuValueType::KeyBind: {
        const auto* value = static_cast<const SDK::UI::MenuKeyBind*>(item);
        AppendLine(output, key + ".key", value->Key);
        AppendLine(output, key + ".active", value->Active);
        AppendLine(output, key + ".mode", value->Type == SDK::KeyBindType::Toggle ? "Toggle" : "Press");
        break;
    }
    case SDK::UI::MenuValueType::List: {
        const auto* value = static_cast<const SDK::UI::MenuList*>(item);
        AppendLine(output, key, value->Index);
        AppendLine(output, key + ".selected", IniSafeValue(value->SelectedValue()));
        break;
    }
    case SDK::UI::MenuValueType::Color: {
        const auto* value = static_cast<const SDK::UI::MenuColor*>(item);
        AppendLine(output, key, value->Value);
        break;
    }
    case SDK::UI::MenuValueType::SliderBtn: {
        const auto* value = static_cast<const SDK::UI::MenuSliderButton*>(item);
        AppendLine(output, key + ".value", value->Value);
        AppendLine(output, key + ".enabled", value->Enabled);
        AppendLine(output, "__" + key + ".min", value->MinValue);
        AppendLine(output, "__" + key + ".max", value->MaxValue);
        break;
    }
    default:
        break;
    }
}

inline void LoadMenuItems(SDK::UI::Menu* menu,
                          const char* path,
                          const std::string& section) {
    if (!menu || !path) {
        return;
    }

    for (int i = 0; i < menu->Components.size(); ++i) {
        auto* component = menu->Components[i];
        if (!component || !component->Visible) {
            continue;
        }

        if (component->IsMenu()) {
            const std::string childSection =
                section + "." + IniSafeName(component->Name.c_str(), component->DisplayName.c_str());
            LoadMenuItems(static_cast<SDK::UI::Menu*>(component), path, childSection);
            continue;
        }

        auto* item = dynamic_cast<SDK::UI::MenuItem*>(component);
        if (!item || g_loadedItems.find(item) != g_loadedItems.end()) {
            continue;
        }

        ApplyItemValue(item, path, section);
        g_loadedItems.insert(item);
    }
}

inline void AppendMenu(std::string& output,
                       const SDK::UI::Menu* menu,
                       const std::string& section) {
    if (!menu) {
        return;
    }

    std::string items;
    for (int i = 0; i < menu->Components.size(); ++i) {
        auto* component = menu->Components[i];
        if (!component || !component->Visible || component->IsMenu()) {
            continue;
        }

        if (const auto* item = dynamic_cast<const SDK::UI::MenuItem*>(component)) {
            AppendItem(items, item);
        }
    }

    if (!items.empty()) {
        output += "[";
        output += section;
        output += "]\r\n";
        output += items;
        output += "\r\n";
    }

    for (int i = 0; i < menu->Components.size(); ++i) {
        auto* component = menu->Components[i];
        if (!component || !component->Visible || !component->IsMenu()) {
            continue;
        }

        const std::string childSection =
            section + "." + IniSafeName(component->Name.c_str(), component->DisplayName.c_str());
        AppendMenu(output, static_cast<const SDK::UI::Menu*>(component), childSection);
    }
}

inline void LoadCoreConfig() {
    if (g_coreLoaded) {
        return;
    }
    g_coreLoaded = true;

    char path[MAX_PATH] = {};
    if (!BuildConfigPath("NightSharp.ini", path, MAX_PATH)) {
        return;
    }

    Config::Language::index = ReadInt(path, "Core.Language", "index", Config::Language::index);
    if (Config::Language::index < 0) Config::Language::index = 0;
    if (Config::Language::index > 2) Config::Language::index = 2;

    Config::SkinChanger::enabled =
        ReadBool(path, "Core.Menu", "SkinChanger.enabled", Config::SkinChanger::enabled);
    Config::SkinChanger::skinId =
        ReadInt(path, "Core.Menu", "SkinChanger.skinId", Config::SkinChanger::skinId);
    Config::ZoomHack::enabled =
        ReadBool(path, "Core.Menu", "ZoomHack.enabled", Config::ZoomHack::enabled);
    Config::ZoomHack::maxZoom =
        ReadFloat(path, "Core.Menu", "ZoomHack.maxZoom", Config::ZoomHack::maxZoom);
    Config::StreamProtection::bypassObs =
        ReadBool(path, "Core.Menu", "StreamProtection.bypassObs", Config::StreamProtection::bypassObs);
    Config::OverlayInput::clickThrough =
        ReadBool(path, "Core.Menu", "OverlayInput.clickThrough", Config::OverlayInput::clickThrough);

    Config::PermaShow::enabled =
        ReadBool(path, "Core.PermaShow", "enabled", Config::PermaShow::enabled);
    Config::PermaShow::width =
        ReadInt(path, "Core.PermaShow", "width", Config::PermaShow::width);
    Config::PermaShow::indicatorWidth =
        ReadInt(path, "Core.PermaShow", "indicatorWidth", Config::PermaShow::indicatorWidth);
    Config::PermaShow::x =
        ReadInt(path, "Core.PermaShow", "x", Config::PermaShow::x);
    Config::PermaShow::y =
        ReadInt(path, "Core.PermaShow", "y", Config::PermaShow::y);
    Config::PermaShow::positionInitialized =
        ReadBool(path,
                 "Core.PermaShow",
                 "positionInitialized",
                 Config::PermaShow::positionInitialized);
}

inline void AppendCoreConfig(std::unordered_map<std::string, std::string>& files) {
    std::string& output = files["NightSharp.ini"];
    output += "[Core.Language]\r\n";
    AppendLine(output, "index", Config::Language::index);
    output += "\r\n";

    output += "[Core.Menu]\r\n";
    AppendLine(output, "SkinChanger.enabled", Config::SkinChanger::enabled);
    AppendLine(output, "SkinChanger.skinId", Config::SkinChanger::skinId);
    AppendLine(output, "ZoomHack.enabled", Config::ZoomHack::enabled);
    AppendLine(output, "ZoomHack.maxZoom", Config::ZoomHack::maxZoom);
    AppendLine(output, "StreamProtection.bypassObs", Config::StreamProtection::bypassObs);
    AppendLine(output, "OverlayInput.clickThrough", Config::OverlayInput::clickThrough);
    output += "\r\n";

    output += "[Core.PermaShow]\r\n";
    AppendLine(output, "enabled", Config::PermaShow::enabled);
    AppendLine(output, "width", Config::PermaShow::width);
    AppendLine(output, "indicatorWidth", Config::PermaShow::indicatorWidth);
    AppendLine(output, "x", Config::PermaShow::x);
    AppendLine(output, "y", Config::PermaShow::y);
    AppendLine(output, "positionInitialized", Config::PermaShow::positionInitialized);
    output += "\r\n";
}

inline void AppendPluginMetadata(std::unordered_map<std::string, std::string>& files) {
    for (int i = 0; i < PluginRegistry::PluginCount; ++i) {
        const auto& plugin = PluginRegistry::Plugins[i];
        if (plugin.Kind == PluginRegistry::PluginKind::SDK || !plugin.InternalId) {
            continue;
        }

        std::string& output = files[ConfigFileForPlugin(plugin)];
        output += "[Plugin.";
        output += IniSafeName(plugin.InternalId, plugin.Name);
        output += "]\r\n";
        AppendLine(output, "Name", IniSafeValue(plugin.Name ? plugin.Name : ""));
        AppendLine(output, "InternalId", IniSafeValue(plugin.InternalId));
        AppendLine(output, "Kind", PluginKindName(plugin.Kind));
        AppendLine(output, "Category", PluginRegistry::CategoryName(plugin.Category));
        AppendLine(output, "ChampionName", IniSafeValue(plugin.ChampionName ? plugin.ChampionName : ""));
        AppendLine(output, "AlwaysLoad", plugin.AlwaysLoad);
        AppendLine(output, "Loaded", plugin.Loaded);
        output += "\r\n";
    }
}

inline void ApplyNewMenuValues() {
    LoadCoreConfig();

    auto& manager = SDK::UI::MenuManager::Instance();
    for (int i = 0; i < manager.Menus.size(); ++i) {
        SDK::UI::Menu* root = manager.Menus[i];
        if (!root) {
            continue;
        }

        char path[MAX_PATH] = {};
        const std::string configFile = ConfigFileForRootMenu(root);
        if (!BuildConfigPath(configFile.c_str(), path, MAX_PATH)) {
            continue;
        }

        const std::string rootSection =
            IniSafeName(root->Name.c_str(), root->DisplayName.c_str());
        LoadMenuItems(root, path, rootSection);
    }
}

inline void BuildSnapshots(std::unordered_map<std::string, std::string>& files) {
    files.clear();
    AppendCoreConfig(files);
    AppendPluginMetadata(files);

    auto& manager = SDK::UI::MenuManager::Instance();
    for (int i = 0; i < manager.Menus.size(); ++i) {
        const SDK::UI::Menu* root = manager.Menus[i];
        if (!root) {
            continue;
        }

        const std::string configFile = ConfigFileForRootMenu(root);
        std::string& output = files[configFile];
        const std::string rootSection =
            IniSafeName(root->Name.c_str(), root->DisplayName.c_str());
        AppendMenu(output, root, rootSection);
    }

    const char* header =
        "; NightSharp menu config\r\n"
        "; Auto-generated from in-game menu values.\r\n\r\n";
    for (auto& item : files) {
        if (item.second.rfind(header, 0) != 0) {
            item.second.insert(0, header);
        }
    }
}

inline bool WriteTextFile(const char* path, const std::string& content) {
    HANDLE file = CreateFileA(
        path,
        GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        NightSharpDebug::Logf("[MenuSettingsConfig] CreateFile failed gle=%lu path=%s",
                              static_cast<unsigned long>(GetLastError()),
                              path ? path : "");
        return false;
    }

    DWORD written = 0;
    const BOOL ok = WriteFile(
        file,
        content.data(),
        static_cast<DWORD>(content.size()),
        &written,
        nullptr);
    CloseHandle(file);

    if (!ok || written != content.size()) {
        NightSharpDebug::Logf("[MenuSettingsConfig] WriteFile failed gle=%lu path=%s written=%lu size=%llu",
                              static_cast<unsigned long>(GetLastError()),
                              path ? path : "",
                              static_cast<unsigned long>(written),
                              static_cast<unsigned long long>(content.size()));
        return false;
    }

    return true;
}

inline void SaveAll(bool force) {
    const DWORD now = GetTickCount();
    if (!force && g_lastSaveTick != 0 && now - g_lastSaveTick < kSaveDebounceMs) {
        return;
    }

    std::unordered_map<std::string, std::string> files;
    BuildSnapshots(files);

    bool wroteAny = false;
    for (const auto& item : files) {
        const std::string fileName = SanitizeFileName(item.first.c_str());
        const auto existing = g_lastSnapshots.find(fileName);
        if (!force && existing != g_lastSnapshots.end() && existing->second == item.second) {
            continue;
        }

        char path[MAX_PATH] = {};
        if (!BuildConfigPath(fileName.c_str(), path, MAX_PATH)) {
            continue;
        }

        if (WriteTextFile(path, item.second)) {
            g_lastSnapshots[fileName] = item.second;
            wroteAny = true;
        }
    }

    if (wroteAny) {
        g_lastSaveTick = now;
    }
}

inline void SaveAllIfChanged() {
    SaveAll(false);
}

inline void SaveAllNow() {
    SaveAll(true);
}

struct AutoSaveScope {
    ~AutoSaveScope() {
        SaveAllIfChanged();
    }
};

} // namespace NightSharpMenu::MenuSettingsConfig
