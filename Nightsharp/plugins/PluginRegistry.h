#pragma once
// ============================================================================
// PluginRegistry.h — Plugin Manager cho NightSharp
//
// Ported from Old/menu/PluginRegistry.h.
// Simplified: no SDK::MenuUI dependency (MenuRoot removed).
// POD only — no CRT, no STL, manual map safe.
// ============================================================================

#include <Windows.h>
#include <shlobj.h>
#include "../CrashReporter.h"
#include "../DebugLog.h"

namespace PluginRegistry {

    // ── Plugin entry ──
    enum class PluginKind : int { SDK = 0, Plugin = 1, External = 2 };
    enum class PluginCategory : int { Core = 0, Champion = 1, Utility = 2, Misc = 3 };

    struct PluginEntry {
        const char*  Name        = nullptr;
        const char*  InternalId  = nullptr;
        PluginKind   Kind        = PluginKind::SDK;
        PluginCategory Category  = PluginCategory::Core;
        const char*  ChampionName = nullptr;
        bool         Loaded      = false;
        bool         AlwaysLoad  = true;
        bool         AlwaysLoadConfigured = false;
        void*        RuntimeUserData = nullptr;
        bool       (*RuntimeLoad)(void*) = nullptr;
        bool       (*RuntimeUnload)(void*) = nullptr;
        void       (*RuntimeMenu)(void*) = nullptr;
        bool       (*CanLoadFn)(void*) = nullptr;
        bool         CanLoadCached = true;
        bool         CanLoadChecked = false;
    };

    // ── Fixed-size registry ──
    constexpr int MAX_PLUGINS = 128;
    inline PluginEntry Plugins[MAX_PLUGINS] = {};
    inline int PluginCount = 0;

    // ============================================================================
    // Persistence — WinAPI only (manual map safe)
    // ============================================================================

    inline void GetConfigPath(char* outPath, int maxLen) {
        char appData[MAX_PATH] = {};
        SHGetFolderPathA(nullptr, CSIDL_APPDATA, nullptr, 0, appData);

        char parentDir[MAX_PATH] = {};
        wsprintfA(parentDir, "%s\\NightSharp", appData);
        CreateDirectoryA(parentDir, nullptr);

        char dirPath[MAX_PATH] = {};
        wsprintfA(dirPath, "%s\\config", parentDir);
        CreateDirectoryA(dirPath, nullptr);

        wsprintfA(outPath, "%s\\plugins.ini", dirPath);
        NightSharpDebug::Logf("[PluginRegistry] Config path=%s", outPath ? outPath : "");
    }

    inline void SaveConfig() {
        char path[MAX_PATH] = {};
        GetConfigPath(path, MAX_PATH);

        HANDLE hFile = CreateFileA(path, GENERIC_WRITE, 0, nullptr,
                                   CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hFile == INVALID_HANDLE_VALUE) return;

        const char* header = "[Plugins]\r\n";
        DWORD written = 0;
        WriteFile(hFile, header, (DWORD)lstrlenA(header), &written, nullptr);

        for (int i = 0; i < PluginCount; i++) {
            if (!Plugins[i].InternalId) continue;
            if (Plugins[i].Kind == PluginKind::SDK) continue;
            char line[128] = {};
            wsprintfA(line, "%s=%d\r\n", Plugins[i].InternalId, Plugins[i].AlwaysLoad ? 1 : 0);
            WriteFile(hFile, line, (DWORD)lstrlenA(line), &written, nullptr);
        }

        CloseHandle(hFile);
    }

    inline void LoadConfig() {
        char path[MAX_PATH] = {};
        GetConfigPath(path, MAX_PATH);
        NightSharpDebug::Logf("[PluginRegistry] LoadConfig opening path=%s", path);

        HANDLE hFile = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, nullptr,
                                   OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hFile == INVALID_HANDLE_VALUE) {
            NightSharpDebug::Logf("[PluginRegistry] LoadConfig no file gle=%lu",
                                  static_cast<unsigned long>(GetLastError()));
            return;
        }

        char buf[2048] = {};
        DWORD bytesRead = 0;
        ReadFile(hFile, buf, sizeof(buf) - 1, &bytesRead, nullptr);
        CloseHandle(hFile);
        buf[bytesRead] = '\0';
        NightSharpDebug::Logf("[PluginRegistry] LoadConfig read bytes=%lu",
                              static_cast<unsigned long>(bytesRead));

        const char* p = buf;
        while (*p) {
            if (*p == '[') {
                while (*p && *p != '\n') p++;
                if (*p == '\n') p++;
                continue;
            }
            if (*p == '\r' || *p == '\n' || *p == ' ' || *p == '\t') {
                p++;
                continue;
            }

            char key[64] = {};
            int ki = 0;
            while (*p && *p != '=' && *p != '\r' && *p != '\n' && ki < 63) {
                key[ki++] = *p++;
            }
            key[ki] = '\0';

            if (*p != '=') {
                while (*p && *p != '\n') p++;
                if (*p == '\n') p++;
                continue;
            }
            p++;

            int value = 0;
            if (*p == '1') value = 1;
            while (*p && *p != '\r' && *p != '\n') p++;
            if (*p == '\r') p++;
            if (*p == '\n') p++;

            for (int i = 0; i < PluginCount; i++) {
                if (!Plugins[i].InternalId) continue;
                if (Plugins[i].Kind == PluginKind::SDK) continue;
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
                    Plugins[i].AlwaysLoadConfigured = true;
                    Plugins[i].Loaded = Plugins[i].AlwaysLoad;
                    NightSharpDebug::Logf("[PluginRegistry] LoadConfig entry id=%s always=%d",
                                          Plugins[i].InternalId,
                                          Plugins[i].AlwaysLoad ? 1 : 0);
                    break;
                }
            }
        }
        NightSharpDebug::Logf("[PluginRegistry] LoadConfig done");
    }

    // ============================================================================
    // Register / Load / Unload
    // ============================================================================

    inline int Register(const char* name, const char* internalId,
                        PluginKind kind, bool autoLoad = true,
                        PluginCategory category = PluginCategory::Core,
                        const char* championName = nullptr)
    {
        if (PluginCount >= MAX_PLUGINS) return -1;
        int idx = PluginCount++;
        Plugins[idx].Name       = name;
        Plugins[idx].InternalId = internalId;
        Plugins[idx].Kind       = kind;
        Plugins[idx].Category   = category;
        Plugins[idx].ChampionName = championName;
        Plugins[idx].AlwaysLoad = autoLoad;
        Plugins[idx].AlwaysLoadConfigured = false;
        Plugins[idx].Loaded     = autoLoad;
        NightSharpDebug::Logf("[PluginRegistry] Register idx=%d name=%s id=%s auto=%d",
                              idx,
                              name ? name : "",
                              internalId ? internalId : "",
                              autoLoad ? 1 : 0);
        return idx;
    }

    inline void BindRuntime(int idx,
                            void* userData,
                            bool (*loadFn)(void*),
                            bool (*unloadFn)(void*),
                            void (*menuFn)(void*) = nullptr,
                            bool (*canLoadFn)(void*) = nullptr) {
        if (idx < 0 || idx >= PluginCount) return;
        Plugins[idx].RuntimeUserData = userData;
        Plugins[idx].RuntimeLoad = loadFn;
        Plugins[idx].RuntimeUnload = unloadFn;
        Plugins[idx].RuntimeMenu = menuFn;
        Plugins[idx].CanLoadFn = canLoadFn;
        Plugins[idx].CanLoadCached = true;
        Plugins[idx].CanLoadChecked = false;
    }

    inline bool HasRuntime(int idx) {
        return idx >= 0 && idx < PluginCount &&
            (Plugins[idx].RuntimeLoad || Plugins[idx].RuntimeUnload || Plugins[idx].RuntimeMenu);
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

            if (match) return i;
        }

        return -1;
    }

    inline void LoadPlugin(int idx) {
        if (idx >= 0 && idx < PluginCount) {
            auto& p = Plugins[idx];
            bool ok = true;
            if (p.RuntimeLoad) {
                __try {
                    ok = p.RuntimeLoad(p.RuntimeUserData);
                }
                __except (NightSharpDebug::CrashReporter::LogAndDumpException(
                              "PluginRegistry::LoadPlugin",
                              GetExceptionInformation())) {
                    NightSharpDebug::Logf("[PluginRegistry] RuntimeLoad crashed id=%s",
                                          p.InternalId ? p.InternalId : "");
                    ok = false;
                    p.Loaded = false;
                }
            }
            if (ok) {
                p.Loaded = true;
            }
        }
    }

    inline void UnloadPlugin(int idx) {
        if (idx >= 0 && idx < PluginCount) {
            auto& p = Plugins[idx];
            bool ok = true;
            if (p.RuntimeUnload) {
                __try {
                    ok = p.RuntimeUnload(p.RuntimeUserData);
                }
                __except (NightSharpDebug::CrashReporter::LogAndDumpException(
                              "PluginRegistry::UnloadPlugin",
                              GetExceptionInformation())) {
                    NightSharpDebug::Logf("[PluginRegistry] RuntimeUnload crashed id=%s",
                                          p.InternalId ? p.InternalId : "");
                    ok = false;
                    p.Loaded = false;
                }
            }
            if (ok) {
                p.Loaded = false;
            }
        }
    }

    inline void SetAlwaysLoad(int idx, bool value) {
        if (idx >= 0 && idx < PluginCount) {
            Plugins[idx].AlwaysLoad = value;
            Plugins[idx].AlwaysLoadConfigured = true;
            SaveConfig();
        }
    }

    inline bool IsLoaded(const char* internalId, bool defaultValue = true) {
        const int idx = FindByInternalId(internalId);
        return idx >= 0 ? Plugins[idx].Loaded : defaultValue;
    }

    inline int GetCountByKind(PluginKind kind) {
        int count = 0;
        for (int i = 0; i < PluginCount; ++i) {
            if (Plugins[i].Kind == kind) {
                ++count;
            }
        }
        return count;
    }

    inline int GetLoadedCount(PluginKind kind) {
        int count = 0;
        for (int i = 0; i < PluginCount; ++i) {
            if (Plugins[i].Kind == kind && Plugins[i].Loaded) {
                ++count;
            }
        }
        return count;
    }

    inline bool CanPluginLoad(int idx) {
        if (idx < 0 || idx >= PluginCount) return false;
        auto& p = Plugins[idx];
        if (!p.CanLoadFn) {
            p.CanLoadCached = true;
            p.CanLoadChecked = true;
            return true;
        }

        if (p.CanLoadChecked) {
            return p.CanLoadCached;
        }

        __try {
            p.CanLoadCached = p.CanLoadFn(p.RuntimeUserData);
        }
        __except (NightSharpDebug::CrashReporter::LogAndDumpException(
                      "PluginRegistry::CanPluginLoad",
                      GetExceptionInformation())) {
            NightSharpDebug::Logf("[PluginRegistry] CanLoad crashed id=%s",
                                  p.InternalId ? p.InternalId : "");
            p.CanLoadCached = false;
        }
        p.CanLoadChecked = true;
        return p.CanLoadCached;
    }

    inline void DrawPluginMenu(int idx) {
        if (idx < 0 || idx >= PluginCount) {
            return;
        }

        auto& p = Plugins[idx];
        if (p.RuntimeMenu) {
            __try {
                p.RuntimeMenu(p.RuntimeUserData);
            }
            __except (NightSharpDebug::CrashReporter::LogAndDumpException(
                          "PluginRegistry::DrawPluginMenu",
                          GetExceptionInformation())) {
                NightSharpDebug::Logf("[PluginRegistry] RuntimeMenu crashed; disabling id=%s",
                                      p.InternalId ? p.InternalId : "");
                p.Loaded = false;
                p.RuntimeMenu = nullptr;
            }
        }
    }

    inline const char* CategoryName(PluginCategory category) {
        switch (category) {
        case PluginCategory::Core:     return "Core Plugin";
        case PluginCategory::Champion: return "Champion Plugin";
        case PluginCategory::Utility:  return "Utility Plugin";
        case PluginCategory::Misc:     return "Misc Plugin";
        default:                       return "Plugin";
        }
    }

    inline void Reset() {
        for (int i = 0; i < MAX_PLUGINS; ++i) {
            Plugins[i] = {};
        }
        PluginCount = 0;
    }

} // namespace PluginRegistry
