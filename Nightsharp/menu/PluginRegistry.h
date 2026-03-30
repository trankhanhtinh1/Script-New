#pragma once
// ============================================================================
// PluginRegistry.h — Plugin Manager cho NightSharp
//
// Quản lý trạng thái Load/Unload + Always Load cho SDK plugins và
// external plugins. Cung cấp thông tin cho NightSharpMenu để:
//   1. Hiện danh sách plugin trong SDK Plugins tab (plugin manager UI)
//   2. Thêm/xóa sidebar categories khi plugin load/unload
//
// ── Phase 1 Limitations ──
//   - Load/Unload = VISIBILITY ONLY. The SDK plugin object and menu subtree
//     are always created by Bootstrap::Init(). "Unload" hides the sidebar
//     category but does NOT teardown the plugin model/logic.
//   - AlwaysLoad is persisted to %APPDATA%/NightSharp/config/plugins.ini
//     using WinAPI only (no CRT file I/O).
//   - Future: Load/Unload should instantiate/teardown plugin behavior+menu
//     through the registry, not just flip a visibility bool.
//
// POD only — no CRT, no STL, manual map safe.
// ============================================================================

#include <Windows.h>
#include <shlobj.h>

namespace SDK {
    namespace MenuUI {
        class Menu;
    }
}

namespace PluginRegistry {

    // ── Plugin entry ──
    enum class PluginKind : int { SDK = 0, Plugin = 1, External = 2 };

    struct PluginEntry {
        const char*  Name        = nullptr;  // Display name (e.g. "Orbwalker")
        const char*  InternalId  = nullptr;  // Internal key (e.g. "orbwalker")
        PluginKind   Kind        = PluginKind::SDK;
        bool         Loaded      = false;    // Currently loaded? (Phase 1: visibility only)
        bool         AlwaysLoad  = true;     // Auto-load on startup? (persisted)
        SDK::MenuUI::Menu* MenuRoot    = nullptr;  // Plugin's own menu subtree
        void*        RuntimeUserData = nullptr;
        bool       (*RuntimeLoad)(void*) = nullptr;
        bool       (*RuntimeUnload)(void*) = nullptr;
        SDK::MenuUI::Menu* (*RuntimeMenuRoot)(void*) = nullptr;
    };

    // ── Fixed-size registry ──
    constexpr int MAX_PLUGINS = 16;
    inline PluginEntry Plugins[MAX_PLUGINS] = {};
    inline int PluginCount = 0;

    // ============================================================================
    // Persistence — WinAPI only (manual map safe)
    // File: %APPDATA%/NightSharp/config/plugins.ini
    // Format: [Plugins]\norbwalker=1\ntargetselector=0\n
    // ============================================================================

    inline void GetConfigPath(char* outPath, int maxLen) {
        // Build: %APPDATA%\NightSharp\config\plugins.ini
        char appData[MAX_PATH] = {};
        SHGetFolderPathA(nullptr, CSIDL_APPDATA, nullptr, 0, appData);

        // Build directory path
        char dirPath[MAX_PATH] = {};
        wsprintfA(dirPath, "%s\\NightSharp\\config", appData);
        CreateDirectoryA(dirPath, nullptr);  // Create if not exists (idempotent)

        // Also create parent
        char parentDir[MAX_PATH] = {};
        wsprintfA(parentDir, "%s\\NightSharp", appData);
        CreateDirectoryA(parentDir, nullptr);
        CreateDirectoryA(dirPath, nullptr);

        wsprintfA(outPath, "%s\\plugins.ini", dirPath);
    }

    // Save AlwaysLoad state for all registered plugins
    inline void SaveConfig() {
        char path[MAX_PATH] = {};
        GetConfigPath(path, MAX_PATH);

        HANDLE hFile = CreateFileA(path, GENERIC_WRITE, 0, nullptr,
                                   CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hFile == INVALID_HANDLE_VALUE) return;

        // Write header
        const char* header = "[Plugins]\r\n";
        DWORD written = 0;
        WriteFile(hFile, header, (DWORD)lstrlenA(header), &written, nullptr);

        // Write each plugin: internalId=0|1
        for (int i = 0; i < PluginCount; i++) {
            if (!Plugins[i].InternalId) continue;
            char line[128] = {};
            wsprintfA(line, "%s=%d\r\n", Plugins[i].InternalId, Plugins[i].AlwaysLoad ? 1 : 0);
            WriteFile(hFile, line, (DWORD)lstrlenA(line), &written, nullptr);
        }

        CloseHandle(hFile);
    }

    // Load AlwaysLoad state — updates existing registered plugins
    inline void LoadConfig() {
        char path[MAX_PATH] = {};
        GetConfigPath(path, MAX_PATH);

        HANDLE hFile = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, nullptr,
                                   OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hFile == INVALID_HANDLE_VALUE) return;  // No config yet — use defaults

        // Read entire file (small — max ~1KB)
        char buf[2048] = {};
        DWORD bytesRead = 0;
        ReadFile(hFile, buf, sizeof(buf) - 1, &bytesRead, nullptr);
        CloseHandle(hFile);
        buf[bytesRead] = '\0';

        // Parse line by line — find "key=value" pairs
        const char* p = buf;
        while (*p) {
            // Skip header lines starting with [
            if (*p == '[') {
                while (*p && *p != '\n') p++;
                if (*p == '\n') p++;
                continue;
            }

            // Skip whitespace/newlines
            if (*p == '\r' || *p == '\n' || *p == ' ' || *p == '\t') {
                p++;
                continue;
            }

            // Read key (until '=')
            char key[64] = {};
            int ki = 0;
            while (*p && *p != '=' && *p != '\r' && *p != '\n' && ki < 63) {
                key[ki++] = *p++;
            }
            key[ki] = '\0';

            if (*p != '=') {
                // Malformed line — skip
                while (*p && *p != '\n') p++;
                if (*p == '\n') p++;
                continue;
            }
            p++;  // skip '='

            // Read value
            int value = 0;
            if (*p == '1') value = 1;
            while (*p && *p != '\r' && *p != '\n') p++;
            if (*p == '\r') p++;
            if (*p == '\n') p++;

            // Find matching plugin by InternalId
            for (int i = 0; i < PluginCount; i++) {
                if (!Plugins[i].InternalId) continue;
                // strcmp without CRT
                const char* a = Plugins[i].InternalId;
                const char* b = key;
                bool match = true;
                while (*a && *b) {
                    if (*a != *b) { match = false; break; }
                    a++; b++;
                }
                if (*a != *b) match = false;

                if (match) {
                    Plugins[i].AlwaysLoad = (value == 1);
                    Plugins[i].Loaded = Plugins[i].AlwaysLoad;
                    break;
                }
            }
        }
    }

    // ============================================================================
    // Register / Load / Unload
    // ============================================================================

    inline int Register(const char* name, const char* internalId,
                        PluginKind kind, SDK::MenuUI::Menu* menuRoot, bool autoLoad = true)
    {
        if (PluginCount >= MAX_PLUGINS) return -1;
        int idx = PluginCount++;
        Plugins[idx].Name       = name;
        Plugins[idx].InternalId = internalId;
        Plugins[idx].Kind       = kind;
        Plugins[idx].MenuRoot   = menuRoot;
        Plugins[idx].AlwaysLoad = autoLoad;
        Plugins[idx].Loaded     = autoLoad;
        return idx;
    }

    inline void BindRuntime(int idx,
                            void* userData,
                            bool (*loadFn)(void*),
                            bool (*unloadFn)(void*),
                            SDK::MenuUI::Menu* (*menuRootFn)(void*)) {
        if (idx < 0 || idx >= PluginCount) return;
        Plugins[idx].RuntimeUserData = userData;
        Plugins[idx].RuntimeLoad = loadFn;
        Plugins[idx].RuntimeUnload = unloadFn;
        Plugins[idx].RuntimeMenuRoot = menuRootFn;
        if (menuRootFn) {
            Plugins[idx].MenuRoot = menuRootFn(userData);
        }
    }

    inline bool HasRuntime(int idx) {
        return idx >= 0 && idx < PluginCount &&
               (Plugins[idx].RuntimeLoad != nullptr || Plugins[idx].RuntimeUnload != nullptr);
    }

    inline void RefreshMenuRoot(int idx) {
        if (idx < 0 || idx >= PluginCount) return;
        if (Plugins[idx].RuntimeMenuRoot) {
            Plugins[idx].MenuRoot = Plugins[idx].RuntimeMenuRoot(Plugins[idx].RuntimeUserData);
        }
    }

    inline int FindByInternalId(const char* internalId) {
        if (!internalId || !internalId[0]) return -1;

        for (int i = 0; i < PluginCount; i++) {
            if (!Plugins[i].InternalId) continue;

            const char* a = Plugins[i].InternalId;
            const char* b = internalId;
            bool match = true;
            while (*a && *b) {
                if (*a != *b) { match = false; break; }
                a++; b++;
            }
            if (*a != *b) match = false;

            if (match) {
                return i;
            }
        }

        return -1;
    }

    inline bool IsLoaded(const char* internalId, bool defaultValue = true) {
        const int idx = FindByInternalId(internalId);
        if (idx < 0) {
            return defaultValue;
        }
        return Plugins[idx].Loaded;
    }

    // Phase 1: Load/Unload = visibility only.
    // The SDK plugin object and menu subtree remain alive.
    // Future: should instantiate/teardown plugin behavior+menu.
    inline void LoadPlugin(int idx) {
        if (idx >= 0 && idx < PluginCount) {
            auto& p = Plugins[idx];
            bool ok = true;
            if (p.RuntimeLoad) {
                ok = p.RuntimeLoad(p.RuntimeUserData);
            }
            if (ok) {
                p.Loaded = true;
                RefreshMenuRoot(idx);
            }
        }
    }

    inline void UnloadPlugin(int idx) {
        if (idx >= 0 && idx < PluginCount) {
            auto& p = Plugins[idx];
            bool ok = true;
            if (p.RuntimeUnload) {
                ok = p.RuntimeUnload(p.RuntimeUserData);
            }
            if (ok) {
                p.Loaded = false;
                RefreshMenuRoot(idx);
            }
        }
    }

    inline void SetAlwaysLoad(int idx, bool value) {
        if (idx >= 0 && idx < PluginCount) {
            Plugins[idx].AlwaysLoad = value;
            SaveConfig();  // Persist immediately
        }
    }

    // ── Query helpers ──
    inline int GetLoadedCount(PluginKind kind) {
        int count = 0;
        for (int i = 0; i < PluginCount; i++)
            if (Plugins[i].Kind == kind && Plugins[i].Loaded)
                count++;
        return count;
    }

    inline int GetCountByKind(PluginKind kind) {
        int count = 0;
        for (int i = 0; i < PluginCount; i++)
            if (Plugins[i].Kind == kind)
                count++;
        return count;
    }

} // namespace PluginRegistry
