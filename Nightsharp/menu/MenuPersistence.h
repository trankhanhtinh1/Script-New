#pragma once

#include <Windows.h>
#include <cstdio>
#include <cstdlib>

#include "MenuUI.h"

namespace MenuPersistence {

    inline bool GetConfigDir(char* out, int maxLen)
    {
        char appdata[MAX_PATH] = {};
        DWORD len = GetEnvironmentVariableA("APPDATA", appdata, MAX_PATH);
        if (len == 0 || len >= MAX_PATH) return false;

        _snprintf_s(out, maxLen, _TRUNCATE, "%s\\NightSharp\\config", appdata);
        return true;
    }

    inline bool EnsureDirectory(const char* path)
    {
        char tmp[MAX_PATH] = {};
        _snprintf_s(tmp, sizeof(tmp), _TRUNCATE, "%s", path);

        for (char* p = tmp + 1; *p; ++p) {
            if (*p == '\\' || *p == '/') {
                *p = '\0';
                CreateDirectoryA(tmp, nullptr);
                *p = '\\';
            }
        }

        CreateDirectoryA(tmp, nullptr);
        return true;
    }

    inline void GetMenuFilePath(const char* menuName, char* out, int maxLen)
    {
        char dir[MAX_PATH] = {};
        if (!GetConfigDir(dir, MAX_PATH)) {
            out[0] = '\0';
            return;
        }

        EnsureDirectory(dir);
        _snprintf_s(out, maxLen, _TRUNCATE, "%s\\%s.ini", dir, menuName);
    }

    inline bool ParseLine(const char* line, char* outKey, int keyMax, char* outVal, int valMax)
    {
        if (!line || line[0] == '#' || line[0] == '\n' || line[0] == '\r' || line[0] == '\0') {
            return false;
        }

        const char* eq = nullptr;
        for (const char* p = line; *p; ++p) {
            if (*p == '=') {
                eq = p;
                break;
            }
        }
        if (!eq) return false;

        int kLen = (int)(eq - line);
        if (kLen >= keyMax) kLen = keyMax - 1;
        for (int i = 0; i < kLen; ++i) outKey[i] = line[i];
        outKey[kLen] = '\0';

        const char* v = eq + 1;
        int vLen = 0;
        while (v[vLen] && v[vLen] != '\n' && v[vLen] != '\r' && vLen < valMax - 1) ++vLen;
        for (int i = 0; i < vLen; ++i) outVal[i] = v[i];
        outVal[vLen] = '\0';

        return true;
    }

    inline bool StrEq(const char* a, const char* b)
    {
        while (*a && *b) {
            if (*a++ != *b++) return false;
        }
        return *a == *b;
    }

    inline void WriteInt(FILE* f, const char* key, int value)
    {
        fprintf(f, "%s=%d\n", key, value);
    }

    inline void WriteBool(FILE* f, const char* key, bool value)
    {
        fprintf(f, "%s=%d\n", key, value ? 1 : 0);
    }

    inline void WriteUint(FILE* f, const char* key, uint32_t value)
    {
        fprintf(f, "%s=%u\n", key, value);
    }

    inline void WriteColor(FILE* f, const char* key, const float* rgba)
    {
        int r = (int)(rgba[0] * 255.0f + 0.5f);
        int g = (int)(rgba[1] * 255.0f + 0.5f);
        int b = (int)(rgba[2] * 255.0f + 0.5f);
        int a = (int)(rgba[3] * 255.0f + 0.5f);
        fprintf(f, "%s=%d,%d,%d,%d\n", key, r, g, b, a);
    }

    inline void BuildPath(const SDK::AMenuComponent* comp, const char* parentPath, char* path, int pathMax)
    {
        if (parentPath && parentPath[0] != '\0') {
            _snprintf_s(path, pathMax, _TRUNCATE, "%s.%s", parentPath, comp->InternalName.c_str());
        } else {
            _snprintf_s(path, pathMax, _TRUNCATE, "%s", comp->InternalName.c_str());
        }
    }

    inline void SaveComponent(FILE* f, SDK::AMenuComponent* comp, const char* parentPath)
    {
        if (!comp) return;

        char path[512] = {};
        BuildPath(comp, parentPath, path, (int)sizeof(path));

        if (auto* asMenu = dynamic_cast<SDK::Menu*>(comp)) {
            for (const auto& child : asMenu->GetItems()) {
                SaveComponent(f, child, path);
            }
            return;
        }

        if (auto* p = dynamic_cast<SDK::MenuBool*>(comp)) {
            WriteBool(f, path, p->Enabled);
        } else if (auto* p = dynamic_cast<SDK::MenuSliderButton*>(comp)) {
            char keySlider[512] = {};
            char keyBool[512] = {};
            _snprintf_s(keySlider, sizeof(keySlider), _TRUNCATE, "%s.slider", path);
            _snprintf_s(keyBool, sizeof(keyBool), _TRUNCATE, "%s.enabled", path);
            WriteInt(f, keySlider, p->SValue);
            WriteBool(f, keyBool, p->BValue);
        } else if (auto* p = dynamic_cast<SDK::MenuSlider*>(comp)) {
            WriteInt(f, path, p->Value);
        } else if (auto* p = dynamic_cast<SDK::MenuKeyBind*>(comp)) {
            char keyKey[512] = {};
            char keyActive[512] = {};
            char keyType[512] = {};
            _snprintf_s(keyKey, sizeof(keyKey), _TRUNCATE, "%s.key", path);
            _snprintf_s(keyActive, sizeof(keyActive), _TRUNCATE, "%s.active", path);
            _snprintf_s(keyType, sizeof(keyType), _TRUNCATE, "%s.type", path);
            WriteUint(f, keyKey, (uint32_t)p->Key);
            WriteBool(f, keyActive, p->Active);
            WriteInt(f, keyType, static_cast<int>(p->Type));
        } else if (auto* p = dynamic_cast<SDK::MenuList*>(comp)) {
            WriteInt(f, path, p->Index);
        } else if (auto* p = dynamic_cast<SDK::MenuColor*>(comp)) {
            WriteColor(f, path, p->Color);
        }
    }

    inline void SaveMenu(SDK::Menu* menu)
    {
        if (!menu) return;

        char filepath[MAX_PATH] = {};
        GetMenuFilePath(menu->InternalName.c_str(), filepath, MAX_PATH);
        if (filepath[0] == '\0') return;

        FILE* f = nullptr;
        fopen_s(&f, filepath, "w");
        if (!f) return;

        fprintf(f, "# NightSharp Menu Config\n");
        fprintf(f, "# Menu: %s\n\n", menu->DisplayName.c_str());

        for (const auto& child : menu->GetItems()) {
            SaveComponent(f, child, menu->InternalName.c_str());
        }

        fclose(f);
    }

    inline void LoadValue(SDK::AMenuComponent* comp, const char* parentPath, const char* key, const char* value)
    {
        if (!comp) return;

        char path[512] = {};
        BuildPath(comp, parentPath, path, (int)sizeof(path));

        if (auto* asMenu = dynamic_cast<SDK::Menu*>(comp)) {
            for (const auto& child : asMenu->GetItems()) {
                LoadValue(child, path, key, value);
            }
            return;
        }

        if (auto* p = dynamic_cast<SDK::MenuBool*>(comp)) {
            if (StrEq(key, path)) {
                p->Enabled = (atoi(value) != 0);
            }
        } else if (auto* p = dynamic_cast<SDK::MenuSliderButton*>(comp)) {
            char keySlider[512] = {};
            char keyBool[512] = {};
            _snprintf_s(keySlider, sizeof(keySlider), _TRUNCATE, "%s.slider", path);
            _snprintf_s(keyBool, sizeof(keyBool), _TRUNCATE, "%s.enabled", path);
            if (StrEq(key, keySlider)) p->SetValue(atoi(value));
            if (StrEq(key, keyBool)) p->BValue = (atoi(value) != 0);
        } else if (auto* p = dynamic_cast<SDK::MenuSlider*>(comp)) {
            if (StrEq(key, path)) {
                p->SetValue(atoi(value));
            }
        } else if (auto* p = dynamic_cast<SDK::MenuKeyBind*>(comp)) {
            char keyKey[512] = {};
            char keyActive[512] = {};
            char keyType[512] = {};
            _snprintf_s(keyKey, sizeof(keyKey), _TRUNCATE, "%s.key", path);
            _snprintf_s(keyActive, sizeof(keyActive), _TRUNCATE, "%s.active", path);
            _snprintf_s(keyType, sizeof(keyType), _TRUNCATE, "%s.type", path);
            if (StrEq(key, keyKey)) p->Key = (int)strtoul(value, nullptr, 10);
            if (StrEq(key, keyActive)) p->Active = (atoi(value) != 0);
            if (StrEq(key, keyType)) {
                int t = atoi(value);
                if (t >= 0 && t <= 2) {
                    p->Type = static_cast<SDK::KeyBindType>(t);
                }
            }
        } else if (auto* p = dynamic_cast<SDK::MenuList*>(comp)) {
            if (StrEq(key, path)) {
                p->SetIndex(atoi(value));
            }
        } else if (auto* p = dynamic_cast<SDK::MenuColor*>(comp)) {
            if (StrEq(key, path)) {
                int r = 255, g = 255, b = 255, a = 255;
                sscanf_s(value, "%d,%d,%d,%d", &r, &g, &b, &a);
                p->Color[0] = r / 255.0f;
                p->Color[1] = g / 255.0f;
                p->Color[2] = b / 255.0f;
                p->Color[3] = a / 255.0f;
            }
        }
    }

    inline void LoadMenu(SDK::Menu* menu)
    {
        if (!menu) return;

        char filepath[MAX_PATH] = {};
        GetMenuFilePath(menu->InternalName.c_str(), filepath, MAX_PATH);
        if (filepath[0] == '\0') return;

        FILE* f = nullptr;
        fopen_s(&f, filepath, "r");
        if (!f) return;

        char line[1024] = {};
        while (fgets(line, sizeof(line), f)) {
            char key[512] = {};
            char val[512] = {};
            if (!ParseLine(line, key, (int)sizeof(key), val, (int)sizeof(val))) continue;

            for (const auto& child : menu->GetItems()) {
                LoadValue(child, menu->InternalName.c_str(), key, val);
            }
        }

        fclose(f);
    }

    inline void LoadSubtree(SDK::Menu* root, SDK::AMenuComponent* subtree)
    {
        if (!root || !subtree) return;

        char filepath[MAX_PATH] = {};
        GetMenuFilePath(root->InternalName.c_str(), filepath, MAX_PATH);
        if (filepath[0] == '\0') return;

        FILE* f = nullptr;
        fopen_s(&f, filepath, "r");
        if (!f) return;

        char line[1024] = {};
        while (fgets(line, sizeof(line), f)) {
            char key[512] = {};
            char val[512] = {};
            if (!ParseLine(line, key, (int)sizeof(key), val, (int)sizeof(val))) continue;
            LoadValue(subtree, root->InternalName.c_str(), key, val);
        }

        fclose(f);
    }

    inline void OnChildAddedHook(SDK::Menu* root, SDK::AMenuComponent* newChild)
    {
        LoadSubtree(root, newChild);
    }

    inline void WireChildPersistence(SDK::Menu* rootMenu)
    {
        if (rootMenu) {
            rootMenu->OnChildAdded = &OnChildAddedHook;
        }
    }

    inline void SaveAll()
    {
        int count = SDK::MenuManager::GetMenuCount();
        for (int i = 0; i < count; ++i) {
            SaveMenu(SDK::MenuManager::GetMenu(i));
        }
    }

    inline void LoadAll()
    {
        int count = SDK::MenuManager::GetMenuCount();
        for (int i = 0; i < count; ++i) {
            auto* m = SDK::MenuManager::GetMenu(i);
            LoadMenu(m);
            WireChildPersistence(m);
        }
    }

} // namespace MenuPersistence
