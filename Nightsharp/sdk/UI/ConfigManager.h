#pragma once
#include "MenuUI.h"
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <filesystem>
#include <cstdint>
#include <Windows.h>

// ============================================================================
// ConfigManager — Save/Load menu settings to INI file
// Reference: EnsoulSharp config auto-save system
//
// Usage:
//   SDK::ConfigManager::SetConfigPath("C:\\NightSharp\\config");
//   SDK::ConfigManager::LoadAll();   // Load on startup
//   SDK::ConfigManager::SaveAll();   // Save on change or exit
//   SDK::ConfigManager::AutoSave();  // Call each frame — saves every 5s if dirty
// ============================================================================

namespace SDK {

    class ConfigManager {
    public:
        // ====================================================================
        // Set base config directory (default: %APPDATA%\NightSharp\config)
        // ====================================================================
        static void SetConfigPath(const std::string& path) {
            configDir = path;
        }

        static std::string GetConfigPath() {
            if (configDir.empty()) {
                // Default to the current module directory (same folder as hid.dll).
                char modulePath[MAX_PATH] = {};
                HMODULE hm = nullptr;
                if (GetModuleHandleExA(
                        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                        reinterpret_cast<LPCSTR>(&GetConfigPath),
                        &hm) && hm) {
                    if (GetModuleFileNameA(hm, modulePath, MAX_PATH) > 0) {
                        std::filesystem::path p(modulePath);
                        configDir = p.parent_path().string();
                    }
                }

                if (configDir.empty()) {
                    char appdata[MAX_PATH] = {};
                    if (GetEnvironmentVariableA("APPDATA", appdata, MAX_PATH)) {
                        configDir = std::string(appdata) + "\\NightSharp\\config";
                    } else {
                        configDir = "C:\\NightSharp\\config";
                    }
                }
            }
            return configDir;
        }

        // ====================================================================
        // Save all menus to INI files
        // Each root menu gets its own [section] in a global config.ini
        // ====================================================================
        static void SaveAll() {
            std::string dir = GetConfigPath();
            EnsureDirectoryExists(dir);

            std::string filepath = dir + "\\config.ini";
            std::ofstream file(filepath);
            if (!file.is_open()) return;

            file << "; NightSharp SDK Config — Auto-generated\n";
            file << "; Do not edit manually unless you know what you're doing\n\n";

            auto& menus = MenuUI::Menu::GetGlobalMenus();
            for (auto& menu : menus) {
                SaveMenu(file, menu.get(), menu->InternalName);
            }

            file.close();
            isDirty = false;
            lastSaveTime = GetTickCount64();
        }

        // ====================================================================
        // Load all menus from INI file
        // ====================================================================
        static void LoadAll() {
            std::string filepath = GetConfigPath() + "\\config.ini";
            std::ifstream file(filepath);
            if (!file.is_open()) {
                BindMenuEvents();
                SaveAll();
                return;
            }

            // Parse INI into flat map: section.key = value
            std::unordered_map<std::string, std::string> data;
            std::string currentSection;
            std::string line;

            while (std::getline(file, line)) {
                // Skip comments and empty lines
                if (line.empty() || line[0] == ';' || line[0] == '#') continue;

                // Section header
                if (line[0] == '[') {
                    size_t end = line.find(']');
                    if (end != std::string::npos) {
                        currentSection = line.substr(1, end - 1);
                    }
                    continue;
                }

                // Key = Value
                size_t eq = line.find('=');
                if (eq != std::string::npos) {
                    std::string key = Trim(line.substr(0, eq));
                    std::string value = Trim(line.substr(eq + 1));
                    data[currentSection + "." + key] = value;
                }
            }
            file.close();

            // Apply values to menus
            auto& menus = MenuUI::Menu::GetGlobalMenus();
            for (auto& menu : menus) {
                LoadMenu(data, menu.get(), menu->InternalName);
            }

            isDirty = false;
            lastSaveTime = GetTickCount64();
            BindMenuEvents();
        }

        // ====================================================================
        // Auto-save: call each frame, saves every 5 seconds if dirty
        // ====================================================================
        static void AutoSave() {
            BindMenuEvents();
            if (!isDirty) return;
            ULONGLONG now = GetTickCount64();
            if (now - lastDirtyTime < 250) return; // settle burst changes
            if (now - lastSaveTime < 250) return;  // throttle writes
            SaveAll();
        }

        // Mark config as dirty (needs saving)
        static void MarkDirty() {
            isDirty = true;
            lastDirtyTime = GetTickCount64();

            // Save quickly so settings persist even if process exits unexpectedly.
            if (lastDirtyTime - lastSaveTime >= 250) {
                SaveAll();
            }
        }

        // ====================================================================
        // Save/Load single menu by name
        // ====================================================================
        static void SaveMenu(const std::string& menuName) {
            auto& menus = MenuUI::Menu::GetGlobalMenus();
            for (auto& menu : menus) {
                if (menu->InternalName == menuName) {
                    std::string dir = GetConfigPath();
                    EnsureDirectoryExists(dir);
                    std::string filepath = dir + "\\" + menuName + ".ini";
                    std::ofstream file(filepath);
                    if (!file.is_open()) return;
                    SaveMenu(file, menu.get(), menu->InternalName);
                    file.close();
                    return;
                }
            }
        }

        static void LoadMenu(const std::string& menuName) {
            std::string filepath = GetConfigPath() + "\\" + menuName + ".ini";
            std::ifstream file(filepath);
            if (!file.is_open()) return;

            std::unordered_map<std::string, std::string> data;
            std::string currentSection;
            std::string line;
            while (std::getline(file, line)) {
                if (line.empty() || line[0] == ';' || line[0] == '#') continue;
                if (line[0] == '[') {
                    size_t end = line.find(']');
                    if (end != std::string::npos) currentSection = line.substr(1, end - 1);
                    continue;
                }
                size_t eq = line.find('=');
                if (eq != std::string::npos) {
                    std::string key = Trim(line.substr(0, eq));
                    std::string value = Trim(line.substr(eq + 1));
                    data[currentSection + "." + key] = value;
                }
            }
            file.close();

            auto& menus = MenuUI::Menu::GetGlobalMenus();
            for (auto& menu : menus) {
                if (menu->InternalName == menuName) {
                    LoadMenu(data, menu.get(), menu->InternalName);
                    return;
                }
            }
        }

    private:
        static inline std::string configDir;
        static inline bool isDirty = false;
        static inline ULONGLONG lastSaveTime = 0;
        static inline ULONGLONG lastDirtyTime = 0;
        static inline std::unordered_set<uintptr_t> boundMenuCallbacks;

        static void BindMenuEvents() {
            auto& menus = MenuUI::Menu::GetGlobalMenus();
            std::unordered_set<uintptr_t> alive;
            alive.reserve(menus.size());

            for (auto& menu : menus) {
                if (!menu) continue;
                const uintptr_t key = reinterpret_cast<uintptr_t>(menu.get());
                alive.insert(key);

                if (boundMenuCallbacks.find(key) != boundMenuCallbacks.end()) {
                    continue;
                }

                menu->OnMenuValueChanged([](const MenuUI::MenuValueChangedEventArgs&) {
                    MarkDirty();
                });
                boundMenuCallbacks.insert(key);
            }

            for (auto it = boundMenuCallbacks.begin(); it != boundMenuCallbacks.end();) {
                if (alive.find(*it) == alive.end()) {
                    it = boundMenuCallbacks.erase(it);
                } else {
                    ++it;
                }
            }
        }

        // ====================================================================
        // Recursive save: Menu → items → sub-menus
        // ====================================================================
        static void SaveMenu(std::ofstream& file, MenuUI::MenuItem* item, const std::string& section) {
            auto* menu = dynamic_cast<MenuUI::Menu*>(item);
            if (!menu) return;

            file << "[" << section << "]\n";

            for (auto& child : menu->GetItems()) {
                auto* boolItem = dynamic_cast<MenuUI::MenuBool*>(child.get());
                if (boolItem) {
                    file << boolItem->InternalName << "=" << (boolItem->Enabled ? "1" : "0") << "\n";
                    continue;
                }

                auto* sliderItem = dynamic_cast<MenuUI::MenuSlider*>(child.get());
                if (sliderItem) {
                    file << sliderItem->InternalName << "=" << sliderItem->Value << "\n";
                    continue;
                }

                auto* sliderF = dynamic_cast<MenuUI::MenuSliderF*>(child.get());
                if (sliderF) {
                    file << sliderF->InternalName << "=" << sliderF->Value << "\n";
                    continue;
                }

                auto* listItem = dynamic_cast<MenuUI::MenuList*>(child.get());
                if (listItem) {
                    file << listItem->InternalName << "=" << listItem->Index << "\n";
                    continue;
                }

                auto* keyBind = dynamic_cast<MenuUI::MenuKeyBind*>(child.get());
                if (keyBind) {
                    file << keyBind->InternalName << ".key=" << keyBind->Key << "\n";
                    file << keyBind->InternalName << ".type="
                         << (keyBind->Type == MenuUI::KeyBindType::Toggle ? "toggle" : "press") << "\n";
                    file << keyBind->InternalName << ".active=" << (keyBind->Active ? "1" : "0") << "\n";
                    continue;
                }

                auto* color = dynamic_cast<MenuUI::MenuColor*>(child.get());
                if (color) {
                    file << color->InternalName << "="
                         << color->Color[0] << "," << color->Color[1] << ","
                         << color->Color[2] << "," << color->Color[3] << "\n";
                    continue;
                }

                auto* subMenu = dynamic_cast<MenuUI::Menu*>(child.get());
                if (subMenu) {
                    SaveMenu(file, subMenu, section + "." + subMenu->InternalName);
                    continue;
                }
            }
            file << "\n";
        }

        // ====================================================================
        // Recursive load: apply INI values to menu items
        // ====================================================================
        static void LoadMenu(const std::unordered_map<std::string, std::string>& data,
                             MenuUI::MenuItem* item, const std::string& section) {
            auto* menu = dynamic_cast<MenuUI::Menu*>(item);
            if (!menu) return;

            for (auto& child : menu->GetItems()) {
                auto* boolItem = dynamic_cast<MenuUI::MenuBool*>(child.get());
                if (boolItem) {
                    auto it = data.find(section + "." + boolItem->InternalName);
                    if (it != data.end()) {
                        boolItem->Enabled = (it->second == "1" || it->second == "true");
                    }
                    continue;
                }

                auto* sliderItem = dynamic_cast<MenuUI::MenuSlider*>(child.get());
                if (sliderItem) {
                    auto it = data.find(section + "." + sliderItem->InternalName);
                    if (it != data.end()) {
                        try { sliderItem->Value = std::stoi(it->second); } catch (...) {}
                        if (sliderItem->Value < sliderItem->MinValue) sliderItem->Value = sliderItem->MinValue;
                        if (sliderItem->Value > sliderItem->MaxValue) sliderItem->Value = sliderItem->MaxValue;
                    }
                    continue;
                }

                auto* sliderF = dynamic_cast<MenuUI::MenuSliderF*>(child.get());
                if (sliderF) {
                    auto it = data.find(section + "." + sliderF->InternalName);
                    if (it != data.end()) {
                        try { sliderF->Value = std::stof(it->second); } catch (...) {}
                        if (sliderF->Value < sliderF->MinValue) sliderF->Value = sliderF->MinValue;
                        if (sliderF->Value > sliderF->MaxValue) sliderF->Value = sliderF->MaxValue;
                    }
                    continue;
                }

                auto* listItem = dynamic_cast<MenuUI::MenuList*>(child.get());
                if (listItem) {
                    auto it = data.find(section + "." + listItem->InternalName);
                    if (it != data.end()) {
                        try { listItem->Index = std::stoi(it->second); } catch (...) {}
                        if (listItem->Index < 0) listItem->Index = 0;
                        if (listItem->Index >= (int)listItem->Items.size())
                            listItem->Index = (int)listItem->Items.size() - 1;
                    }
                    continue;
                }

                auto* keyBind = dynamic_cast<MenuUI::MenuKeyBind*>(child.get());
                if (keyBind) {
                    auto itKey = data.find(section + "." + keyBind->InternalName + ".key");
                    if (itKey != data.end()) {
                        try { keyBind->Key = std::stoi(itKey->second); } catch (...) {}
                    }
                    auto itType = data.find(section + "." + keyBind->InternalName + ".type");
                    if (itType != data.end()) {
                        std::string type = itType->second;
                        for (char& ch : type) {
                            if (ch >= 'A' && ch <= 'Z') {
                                ch = (char)(ch - 'A' + 'a');
                            }
                        }
                        keyBind->Type = (type == "toggle")
                            ? MenuUI::KeyBindType::Toggle
                            : MenuUI::KeyBindType::Press;
                    }
                    auto itActive = data.find(section + "." + keyBind->InternalName + ".active");
                    if (itActive != data.end()) {
                        keyBind->Active = (itActive->second == "1" || itActive->second == "true");
                    }
                    continue;
                }

                auto* color = dynamic_cast<MenuUI::MenuColor*>(child.get());
                if (color) {
                    auto it = data.find(section + "." + color->InternalName);
                    if (it != data.end()) {
                        float r, g, b, a;
                        if (sscanf_s(it->second.c_str(), "%f,%f,%f,%f", &r, &g, &b, &a) == 4) {
                            color->Color[0] = r; color->Color[1] = g;
                            color->Color[2] = b; color->Color[3] = a;
                        }
                    }
                    continue;
                }

                auto* subMenu = dynamic_cast<MenuUI::Menu*>(child.get());
                if (subMenu) {
                    LoadMenu(data, subMenu, section + "." + subMenu->InternalName);
                    continue;
                }
            }
        }

        // ====================================================================
        // Utility functions
        // ====================================================================
        static void EnsureDirectoryExists(const std::string& path) {
            std::filesystem::create_directories(path);
        }

        static std::string Trim(const std::string& s) {
            size_t start = s.find_first_not_of(" \t\r\n");
            size_t end = s.find_last_not_of(" \t\r\n");
            if (start == std::string::npos) return "";
            return s.substr(start, end - start + 1);
        }
    };

} // namespace SDK
