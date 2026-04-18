#pragma once

#include <Windows.h>

#include "MenuUI.h"
#include "../core/CrashTelemetry.h"

namespace MenuPersistence {

    inline bool GetConfigDir(char* out, int maxLen)
    {
        char appdata[MAX_PATH] = {};
        DWORD len = GetEnvironmentVariableA("APPDATA", appdata, MAX_PATH);
        if (len == 0 || len >= (DWORD)MAX_PATH) return false;
        wsprintfA(out, "%s\\NightSharp\\config", appdata);
        return true;
    }

    inline bool EnsureDirectory(const char* path)
    {
        char tmp[MAX_PATH] = {};
        for (int i = 0; path[i] && i < MAX_PATH - 1; ++i) tmp[i] = path[i];

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
        wsprintfA(out, "%s\\%s.ini", dir, menuName);
    }

    inline bool ParseLine(const char* line, char* outKey, int keyMax, char* outVal, int valMax)
    {
        if (!line || line[0] == '#' || line[0] == '\n' || line[0] == '\r' || line[0] == '\0')
            return false;

        const char* eq = nullptr;
        for (const char* p = line; *p; ++p) {
            if (*p == '=') { eq = p; break; }
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

    inline bool HasSafeVTable(const void* obj) {
        if (!obj) return false;
        uintptr_t vtbl = *reinterpret_cast<const uintptr_t*>(obj);
        return (vtbl & 7) == 0 && vtbl > 0x10000ULL && vtbl < 0x7FFFFFFFFFFF0000ULL;
    }

    inline bool StrEq(const char* a, const char* b)
    {
        while (*a && *b) {
            if (*a++ != *b++) return false;
        }
        return *a == *b;
    }

    struct WriteCtx {
        HANDLE hFile = INVALID_HANDLE_VALUE;

        void Write(const char* s) {
            if (!s || !*s || hFile == INVALID_HANDLE_VALUE) return;
            DWORD written = 0;
            DWORD len = 0;
            for (; s[len]; ++len) {}
            WriteFile(hFile, s, len, &written, nullptr);
        }

        void WriteInt(const char* key, int value) {
            char buf[640] = {};
            wsprintfA(buf, "%s=%d\n", key, value);
            Write(buf);
        }

        void WriteBool(const char* key, bool value) {
            char buf[640] = {};
            wsprintfA(buf, "%s=%d\n", key, value ? 1 : 0);
            Write(buf);
        }

        void WriteUint(const char* key, unsigned int value) {
            char buf[640] = {};
            wsprintfA(buf, "%s=%u\n", key, value);
            Write(buf);
        }

        void WriteColor(const char* key, const float* rgba) {
            int r = (int)(rgba[0] * 255.0f + 0.5f);
            int g = (int)(rgba[1] * 255.0f + 0.5f);
            int b = (int)(rgba[2] * 255.0f + 0.5f);
            int a = (int)(rgba[3] * 255.0f + 0.5f);
            char buf[640] = {};
            wsprintfA(buf, "%s=%d,%d,%d,%d\n", key, r, g, b, a);
            Write(buf);
        }
    };

    inline void BuildPath(const SDK::AMenuComponent* comp, const char* parentPath, char* path, int pathMax)
    {
        if (parentPath && parentPath[0] != '\0') {
            wsprintfA(path, "%s.%s", parentPath, comp->InternalName.c_str());
        } else {
            wsprintfA(path, "%s", comp->InternalName.c_str());
        }
    }

    inline void SaveComponent(WriteCtx& ctx, SDK::AMenuComponent* comp, const char* parentPath)
    {
        if (!comp || !HasSafeVTable(comp)) return;

        char path[512] = {};
        BuildPath(comp, parentPath, path, (int)sizeof(path));

        if (auto* asMenu = dynamic_cast<SDK::Menu*>(comp)) {
            for (const auto& child : asMenu->GetItems()) {
                SaveComponent(ctx, child, path);
            }
            return;
        }

        if (auto* p = dynamic_cast<SDK::MenuBool*>(comp)) {
            ctx.WriteBool(path, p->Enabled);
        } else if (auto* p = dynamic_cast<SDK::MenuSliderButton*>(comp)) {
            char keySlider[512] = {};
            char keyBool[512] = {};
            wsprintfA(keySlider, "%s.slider", path);
            wsprintfA(keyBool, "%s.enabled", path);
            ctx.WriteInt(keySlider, p->SValue);
            ctx.WriteBool(keyBool, p->BValue);
        } else if (auto* p = dynamic_cast<SDK::MenuSlider*>(comp)) {
            ctx.WriteInt(path, p->Value);
        } else if (auto* p = dynamic_cast<SDK::MenuKeyBind*>(comp)) {
            char keyKey[512] = {};
            char keyActive[512] = {};
            char keyType[512] = {};
            wsprintfA(keyKey, "%s.key", path);
            wsprintfA(keyActive, "%s.active", path);
            wsprintfA(keyType, "%s.type", path);
            ctx.WriteUint(keyKey, (unsigned int)p->Key);
            ctx.WriteBool(keyActive, p->Active);
            ctx.WriteInt(keyType, static_cast<int>(p->Type));
        } else if (auto* p = dynamic_cast<SDK::MenuList*>(comp)) {
            ctx.WriteInt(path, p->Index);
        } else if (auto* p = dynamic_cast<SDK::MenuColor*>(comp)) {
            ctx.WriteColor(path, p->Color);
        }
    }

    inline void SaveMenu(SDK::Menu* menu)
    {
        if (!menu) return;

        char filepath[MAX_PATH] = {};
        GetMenuFilePath(menu->InternalName.c_str(), filepath, MAX_PATH);
        if (filepath[0] == '\0') return;

        HANDLE hFile = CreateFileA(filepath, GENERIC_WRITE, 0, nullptr,
                                   CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hFile == INVALID_HANDLE_VALUE) return;

        WriteCtx ctx;
        ctx.hFile = hFile;

        ctx.Write("# NightSharp Menu Config\n");

        char header[512] = {};
        wsprintfA(header, "# Menu: %s\n\n", menu->DisplayName.c_str());
        ctx.Write(header);

        for (const auto& child : menu->GetItems()) {
            SaveComponent(ctx, child, menu->InternalName.c_str());
        }

        CloseHandle(hFile);
    }

    inline void ParseColorStr(const char* value, float* rgba)
    {
        const char* p = value;
        int components[4] = { 255, 255, 255, 255 };
        for (int i = 0; i < 4 && *p; ++i) {
            while (*p == ' ') ++p;
            bool neg = (*p == '-');
            if (neg) ++p;
            int v = 0;
            while (*p >= '0' && *p <= '9') v = v * 10 + (*p++ - '0');
            components[i] = neg ? -v : v;
            while (*p == ' ' || *p == ',') ++p;
        }
        rgba[0] = components[0] / 255.0f;
        rgba[1] = components[1] / 255.0f;
        rgba[2] = components[2] / 255.0f;
        rgba[3] = components[3] / 255.0f;
    }

    inline void LoadValue(SDK::AMenuComponent* comp, const char* parentPath, const char* key, const char* value)
    {
        if (!comp || !HasSafeVTable(comp)) return;

        char path[512] = {};
        BuildPath(comp, parentPath, path, (int)sizeof(path));

        __try {
            if (auto* asMenu = dynamic_cast<SDK::Menu*>(comp)) {
                for (const auto& child : asMenu->GetItems()) {
                    LoadValue(child, path, key, value);
                }
                return;
            }

            if (auto* p = dynamic_cast<SDK::MenuBool*>(comp)) {
                if (StrEq(key, path)) p->Enabled = (atoi(value) != 0);
            } else if (auto* p = dynamic_cast<SDK::MenuSliderButton*>(comp)) {
                char keySlider[512] = {};
                char keyBool[512] = {};
                wsprintfA(keySlider, "%s.slider", path);
                wsprintfA(keyBool, "%s.enabled", path);
                if (StrEq(key, keySlider)) p->SetValue(atoi(value));
                if (StrEq(key, keyBool)) p->BValue = (atoi(value) != 0);
            } else if (auto* p = dynamic_cast<SDK::MenuSlider*>(comp)) {
                if (StrEq(key, path)) p->SetValue(atoi(value));
            } else if (auto* p = dynamic_cast<SDK::MenuKeyBind*>(comp)) {
                char keyKey[512] = {};
                char keyActive[512] = {};
                char keyType[512] = {};
                wsprintfA(keyKey, "%s.key", path);
                wsprintfA(keyActive, "%s.active", path);
                wsprintfA(keyType, "%s.type", path);
                if (StrEq(key, keyKey)) p->Key = (int)strtoul(value, nullptr, 10);
                if (StrEq(key, keyActive)) p->Active = (atoi(value) != 0);
                if (StrEq(key, keyType)) {
                    int t = atoi(value);
                    if (t >= 0 && t <= 2) p->Type = static_cast<SDK::KeyBindType>(t);
                }
            } else if (auto* p = dynamic_cast<SDK::MenuList*>(comp)) {
                if (StrEq(key, path)) p->SetIndex(atoi(value));
            } else if (auto* p = dynamic_cast<SDK::MenuColor*>(comp)) {
                if (StrEq(key, path)) ParseColorStr(value, p->Color);
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
    }

    inline void LoadMenu(SDK::Menu* menu)
    {
        CrashTelemetry::SetStage("MenuPersistence::LoadMenu::Enter");
        if (!menu) return;

        CrashTelemetry::SetStage("MenuPersistence::LoadMenu::GetName");
        const char* rawName = menu->InternalName.c_str();

        char dbg[256] = {};
        wsprintfA(dbg, "MenuPersistence::LoadMenu::name=%s", rawName ? rawName : "(null)");
        CrashTelemetry::SetStage(dbg);

        char filepath[MAX_PATH] = {};
        CrashTelemetry::SetStage("MenuPersistence::LoadMenu::GetPath");
        GetMenuFilePath(rawName, filepath, MAX_PATH);
        if (filepath[0] == '\0') return;

        CrashTelemetry::SetStage("MenuPersistence::LoadMenu::OpenFile");
        HANDLE hFile = CreateFileA(filepath, GENERIC_READ, FILE_SHARE_READ, nullptr,
                                   OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hFile == INVALID_HANDLE_VALUE) return;

        CrashTelemetry::SetStage("MenuPersistence::LoadMenu::GetSize");
        LARGE_INTEGER fileSize = {};
        GetFileSizeEx(hFile, &fileSize);
        if (fileSize.QuadPart <= 0 || fileSize.QuadPart > 65535) {
            CloseHandle(hFile);
            return;
        }

        CrashTelemetry::SetStage("MenuPersistence::LoadMenu::HeapAlloc");
        char* buf = static_cast<char*>(HeapAlloc(GetProcessHeap(), 0, (SIZE_T)fileSize.QuadPart + 1));
        if (!buf) { CloseHandle(hFile); return; }

        CrashTelemetry::SetStage("MenuPersistence::LoadMenu::ReadFile");
        DWORD bytesRead = 0;
        ReadFile(hFile, buf, (DWORD)fileSize.QuadPart, &bytesRead, nullptr);
        CloseHandle(hFile);
        buf[bytesRead] = '\0';

        CrashTelemetry::SetStage("MenuPersistence::LoadMenu::ParseLines");
        const char* p = buf;
        while (*p) {
            const char* lineStart = p;
            while (*p && *p != '\n') ++p;
            const char* lineEnd = p;
            if (*p == '\n') ++p;

            int lineLen = (int)(lineEnd - lineStart);
            if (lineLen <= 0 || lineLen >= 1023) continue;

            char line[1024] = {};
            for (int i = 0; i < lineLen; ++i) line[i] = lineStart[i];
            line[lineLen] = '\0';

            char key[512] = {};
            char val[512] = {};
            if (!ParseLine(line, key, (int)sizeof(key), val, (int)sizeof(val))) continue;

            for (const auto& child : menu->GetItems()) {
                LoadValue(child, rawName, key, val);
            }
        }

        CrashTelemetry::SetStage("MenuPersistence::LoadMenu::HeapFree");
        HeapFree(GetProcessHeap(), 0, buf);
        CrashTelemetry::SetStage("MenuPersistence::LoadMenu::Done");
    }

    inline void LoadSubtree(SDK::Menu* root, SDK::AMenuComponent* subtree)
    {
        if (!root || !subtree) return;

        char filepath[MAX_PATH] = {};
        GetMenuFilePath(root->InternalName.c_str(), filepath, MAX_PATH);
        if (filepath[0] == '\0') return;

        HANDLE hFile = CreateFileA(filepath, GENERIC_READ, FILE_SHARE_READ, nullptr,
                                   OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hFile == INVALID_HANDLE_VALUE) return;

        LARGE_INTEGER fileSize = {};
        GetFileSizeEx(hFile, &fileSize);
        if (fileSize.QuadPart <= 0 || fileSize.QuadPart > 65535) {
            CloseHandle(hFile);
            return;
        }

        char* buf = static_cast<char*>(HeapAlloc(GetProcessHeap(), 0, (SIZE_T)fileSize.QuadPart + 1));
        if (!buf) { CloseHandle(hFile); return; }

        DWORD bytesRead = 0;
        ReadFile(hFile, buf, (DWORD)fileSize.QuadPart, &bytesRead, nullptr);
        CloseHandle(hFile);
        buf[bytesRead] = '\0';

        const char* p = buf;
        while (*p) {
            const char* lineStart = p;
            while (*p && *p != '\n') ++p;
            const char* lineEnd = p;
            if (*p == '\n') ++p;

            int lineLen = (int)(lineEnd - lineStart);
            if (lineLen <= 0 || lineLen >= 1023) continue;

            char line[1024] = {};
            for (int i = 0; i < lineLen; ++i) line[i] = lineStart[i];
            line[lineLen] = '\0';

            char key[512] = {};
            char val[512] = {};
            if (!ParseLine(line, key, (int)sizeof(key), val, (int)sizeof(val))) continue;
            LoadValue(subtree, root->InternalName.c_str(), key, val);
        }

        HeapFree(GetProcessHeap(), 0, buf);
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
        __try {
            int count = SDK::MenuManager::GetMenuCount();
            for (int i = 0; i < count; ++i) {
                SaveMenu(SDK::MenuManager::GetMenu(i));
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
    }

    inline void LoadAll()
    {
        __try {
            CrashTelemetry::SetStage("MenuPersistence::LoadAll::GetCount");
            int count = SDK::MenuManager::GetMenuCount();

            char dbg[128] = {};
            wsprintfA(dbg, "MenuPersistence::LoadAll::count=%d", count);
            CrashTelemetry::SetStage(dbg);

            for (int i = 0; i < count; ++i) {
                char s[64] = {};
                wsprintfA(s, "MenuPersistence::LoadAll::GetMenu[%d]", i);
                CrashTelemetry::SetStage(s);
                auto* m = SDK::MenuManager::GetMenu(i);

                wsprintfA(s, "MenuPersistence::LoadAll::LoadMenu[%d]", i);
                CrashTelemetry::SetStage(s);
                LoadMenu(m);

                wsprintfA(s, "MenuPersistence::LoadAll::Wire[%d]", i);
                CrashTelemetry::SetStage(s);
                WireChildPersistence(m);
            }
            CrashTelemetry::SetStage("MenuPersistence::LoadAll::Done");
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            CrashTelemetry::SetStage("MenuPersistence::LoadAll::SEH_CAUGHT");
        }
    }

} // namespace MenuPersistence
