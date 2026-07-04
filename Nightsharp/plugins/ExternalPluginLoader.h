#pragma once
// ============================================================================
// ExternalPluginLoader.h - Raw DLL plugin loader for developer mode.
//
// Phase 3 only loads unpacked DLLs from:
//   %AppData%\NightSharp\Plugins\Dev
// Release .NS package loading/encryption belongs to a later phase.
// ============================================================================

#include "IPlugin.h"
#include "PluginManager.h"
#include "PluginRegistry.h"
#include "../CrashReporter.h"
#include "../DebugLog.h"
#include "../SDK/Core/Game.h"
#include "../SDK/Events/Events.h"
#include "../SDK/UI/Drawing.h"
#include "../SDK/UI/UI.h"
#include "../SDK/Wrappers/Orbwalking/OrbwalkerBase.h"

#include <NightSharp.SDK.Plugin.h>

#include <Windows.h>
#include <ShlObj.h>

#include <cstdio>
#include <cstring>
#include <memory>
#include <string>

#ifndef NIGHTSHARP_EXTERNAL_PLUGIN_DEV_LOADER
#define NIGHTSHARP_EXTERNAL_PLUGIN_DEV_LOADER 1
#endif

namespace Plugins::ExternalPluginLoader {

namespace Detail {

    inline bool CStrEquals(const char* a, const char* b) {
        if (!a || !b) {
            return false;
        }
        return lstrcmpA(a, b) == 0;
    }

    inline const char* SafeText(const char* value, const char* fallback = "") {
        return value ? value : fallback;
    }

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

    inline bool IsDevLoaderEnabled() {
#if NIGHTSHARP_EXTERNAL_PLUGIN_DEV_LOADER
        char disabled[16] = {};
        const DWORD len = GetEnvironmentVariableA(
            "NIGHTSHARP_DISABLE_DEV_PLUGINS",
            disabled,
            static_cast<DWORD>(sizeof(disabled)));
        if (len > 0 && disabled[0] != '\0' && disabled[0] != '0') {
            return false;
        }
        return true;
#else
        return false;
#endif
    }

    inline bool BuildDevFolder(char* out, DWORD outSize) {
        if (!out || outSize == 0) {
            return false;
        }
        out[0] = '\0';

        char appData[MAX_PATH] = {};
        if (FAILED(SHGetFolderPathA(nullptr, CSIDL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, appData))) {
            NightSharpDebug::Logf("[ExternalPluginLoader] SHGetFolderPathA failed gle=%lu",
                                  static_cast<unsigned long>(GetLastError()));
            return false;
        }

        char nightSharpDir[MAX_PATH] = {};
        _snprintf_s(nightSharpDir, sizeof(nightSharpDir), _TRUNCATE, "%s\\NightSharp", appData);
        if (!EnsureDirectory(nightSharpDir)) {
            NightSharpDebug::Logf("[ExternalPluginLoader] Create dir failed path=%s gle=%lu",
                                  nightSharpDir,
                                  static_cast<unsigned long>(GetLastError()));
            return false;
        }

        char pluginsDir[MAX_PATH] = {};
        _snprintf_s(pluginsDir, sizeof(pluginsDir), _TRUNCATE, "%s\\Plugins", nightSharpDir);
        if (!EnsureDirectory(pluginsDir)) {
            NightSharpDebug::Logf("[ExternalPluginLoader] Create dir failed path=%s gle=%lu",
                                  pluginsDir,
                                  static_cast<unsigned long>(GetLastError()));
            return false;
        }

        _snprintf_s(out, outSize, _TRUNCATE, "%s\\Dev", pluginsDir);
        if (!EnsureDirectory(out)) {
            NightSharpDebug::Logf("[ExternalPluginLoader] Create dir failed path=%s gle=%lu",
                                  out,
                                  static_cast<unsigned long>(GetLastError()));
            return false;
        }

        return out[0] != '\0';
    }

    inline PluginCategory ToPluginCategory(NightSharp::Plugin::Category category) {
        switch (category) {
        case NightSharp::Plugin::Category::Champion:
            return PluginCategory::Champion;
        case NightSharp::Plugin::Category::Utility:
            return PluginCategory::Utility;
        case NightSharp::Plugin::Category::Misc:
            return PluginCategory::Misc;
        case NightSharp::Plugin::Category::Core:
        default:
            return PluginCategory::Core;
        }
    }

    inline void BuildConfigFileNameFromPath(const char* path, char* out, size_t outSize) {
        if (!out || outSize == 0) {
            return;
        }
        out[0] = '\0';

        const char* base = NightSharpDebug::BaseName(path);
        if (!base || !base[0]) {
            base = "ExternalPlugin";
        }

        char stem[160] = {};
        size_t used = 0;
        for (const char* p = base; *p && used + 1 < sizeof(stem); ++p) {
            if (*p == '.') {
                const char* next = p + 1;
                if ((next[0] == 'd' || next[0] == 'D' || next[0] == 'n' || next[0] == 'N') &&
                    (next[1] == 'l' || next[1] == 'L' || next[1] == 's' || next[1] == 'S') &&
                    (next[2] == 'l' || next[2] == 'L' || next[2] == '\0')) {
                    break;
                }
            }

            const unsigned char ch = static_cast<unsigned char>(*p);
            if ((ch >= 'a' && ch <= 'z') ||
                (ch >= 'A' && ch <= 'Z') ||
                (ch >= '0' && ch <= '9') ||
                ch == '-' || ch == '_' || ch == '.') {
                stem[used++] = static_cast<char>(ch);
            } else {
                stem[used++] = '_';
            }
        }

        if (used == 0) {
            lstrcpynA(stem, "ExternalPlugin", static_cast<int>(sizeof(stem)));
        } else {
            stem[used] = '\0';
        }

        _snprintf_s(out, outSize, _TRUNCATE, "%s.ini", stem);
    }

    inline bool ValidateExportsImpl(const NightSharp::Plugin::Exports* exports,
                                    char* reason,
                                    size_t reasonSize) {
        if (!exports) {
            _snprintf_s(reason, reasonSize, _TRUNCATE, "missing exports");
            return false;
        }

        if (exports->Size < sizeof(NightSharp::Plugin::Exports)) {
            _snprintf_s(reason,
                        reasonSize,
                        _TRUNCATE,
                        "exports size mismatch plugin=%u expected=%llu",
                        static_cast<unsigned>(exports->Size),
                        static_cast<unsigned long long>(sizeof(NightSharp::Plugin::Exports)));
            return false;
        }

        const auto& descriptor = exports->Descriptor;
        if (descriptor.Size < sizeof(NightSharp::Plugin::Descriptor)) {
            _snprintf_s(reason,
                        reasonSize,
                        _TRUNCATE,
                        "descriptor size mismatch plugin=%u expected=%llu",
                        static_cast<unsigned>(descriptor.Size),
                        static_cast<unsigned long long>(sizeof(NightSharp::Plugin::Descriptor)));
            return false;
        }

        if (descriptor.AbiRevision != NIGHTSHARP_SDK_ABI_REVISION) {
            _snprintf_s(reason,
                        reasonSize,
                        _TRUNCATE,
                        "ABI revision mismatch plugin=%u expected=%u",
                        static_cast<unsigned>(descriptor.AbiRevision),
                        static_cast<unsigned>(NIGHTSHARP_SDK_ABI_REVISION));
            return false;
        }

        if (!CStrEquals(descriptor.SdkAbiId, NIGHTSHARP_SDK_ABI_ID)) {
            _snprintf_s(reason,
                        reasonSize,
                        _TRUNCATE,
                        "ABI id mismatch plugin=%s expected=%s",
                        SafeText(descriptor.SdkAbiId, "<null>"),
                        NIGHTSHARP_SDK_ABI_ID);
            return false;
        }

        if (!descriptor.Name || !descriptor.Name[0]) {
            _snprintf_s(reason, reasonSize, _TRUNCATE, "missing plugin name");
            return false;
        }

        if (!descriptor.InternalId || !descriptor.InternalId[0]) {
            _snprintf_s(reason, reasonSize, _TRUNCATE, "missing plugin internal id");
            return false;
        }

        reason[0] = '\0';
        return true;
    }

    inline bool ValidateExportsNoThrow(const NightSharp::Plugin::Exports* exports,
                                       const char* path,
                                       char* reason,
                                       size_t reasonSize) {
        char stage[192] = {};
        _snprintf_s(stage,
                    sizeof(stage),
                    _TRUNCATE,
                    "ExternalPluginLoader::Validate/%s",
                    NightSharpDebug::BaseName(path));

        __try {
            return ValidateExportsImpl(exports, reason, reasonSize);
        }
        __except (NightSharpDebug::CrashReporter::LogAndDumpException(
                      stage,
                      GetExceptionInformation())) {
            _snprintf_s(reason, reasonSize, _TRUNCATE, "exception while validating exports");
            return false;
        }
    }

    inline const NightSharp::Plugin::Exports* FetchExportsNoThrow(HMODULE module, const char* path) {
        if (!module) {
            return nullptr;
        }

        auto* proc = reinterpret_cast<NightSharp::Plugin::GetExportsFn>(
            GetProcAddress(module, "NightSharpGetPluginExports"));
        if (!proc) {
            NightSharpDebug::Logf("[ExternalPluginLoader] Missing NightSharpGetPluginExports path=%s",
                                  SafeText(path));
            return nullptr;
        }

        char stage[192] = {};
        _snprintf_s(stage,
                    sizeof(stage),
                    _TRUNCATE,
                    "ExternalPluginLoader::GetExports/%s",
                    NightSharpDebug::BaseName(path));

        __try {
            return proc();
        }
        __except (NightSharpDebug::CrashReporter::LogAndDumpException(
                      stage,
                      GetExceptionInformation())) {
            NightSharpDebug::Logf("[ExternalPluginLoader] GetExports crashed path=%s",
                                  SafeText(path));
            return nullptr;
        }
    }

    class ExternalPlugin final : public IPlugin {
    public:
        ExternalPlugin(HMODULE module,
                       const NightSharp::Plugin::Exports* exports,
                       const char* path)
            : m_module(module),
              m_exports(exports),
              m_name(SafeText(exports->Descriptor.Name, "External Plugin")),
              m_internalId(SafeText(exports->Descriptor.InternalId, "external.unknown")),
              m_author(SafeText(exports->Descriptor.Author, "External")),
              m_championName(SafeText(exports->Descriptor.ChampionName, "")),
              m_path(SafeText(path)),
              m_category(ToPluginCategory(exports->Descriptor.PluginCategory)),
              m_autoLoad(exports->Descriptor.AutoLoad) {
            char configFileName[192] = {};
            BuildConfigFileNameFromPath(path, configFileName, sizeof(configFileName));
            m_configFileName = configFileName;
        }

        ~ExternalPlugin() override {
            if (m_module) {
                CleanupModuleCallbacks();
                DetachOwnedRuntimeMenus();
                char modulePath[MAX_PATH] = {};
                GetModuleFileNameA(m_module, modulePath, MAX_PATH);
                NightSharpDebug::Logf("[ExternalPluginLoader] FreeLibrary name=%s path=%s",
                                      m_name.c_str(),
                                      modulePath[0] ? modulePath : m_path.c_str());
                FreeLibrary(m_module);
                m_module = nullptr;
                m_exports = nullptr;
            }
        }

        const char* GetName() const override {
            return m_name.c_str();
        }

        const char* GetInternalId() const override {
            return m_internalId.c_str();
        }

        const char* GetAuthor() const override {
            return m_author.c_str();
        }

        PluginCategory GetCategory() const override {
            return m_category;
        }

        const char* GetChampionName() const override {
            return m_championName.empty() ? nullptr : m_championName.c_str();
        }

        const char* GetConfigFileName() const override {
            return m_configFileName.c_str();
        }

        bool AutoLoadByDefault() const override {
            return m_autoLoad;
        }

        bool CanLoad() const override {
            if (!m_exports || !m_exports->CanLoad) {
                return true;
            }
            return m_exports->CanLoad();
        }

        void OnLoad() override {
            if (m_exports && m_exports->OnLoad) {
                m_exports->OnLoad();
            }
        }

        void OnUnload() override {
            if (m_exports && m_exports->OnUnload) {
                m_exports->OnUnload();
            }
        }

        void OnUpdate() override {
            if (m_exports && m_exports->OnUpdate) {
                m_exports->OnUpdate();
            }
        }

        void OnRender() override {
            if (m_exports && m_exports->OnRender) {
                m_exports->OnRender();
            }
        }

        void OnMenu() override {
            if (m_exports && m_exports->OnMenu) {
                m_exports->OnMenu();
            }
        }

        void OnRuntimeCleanup() override {
            CleanupModuleCallbacks();
            DetachOwnedRuntimeMenus();
        }

    private:
        void CleanupModuleCallbacks() {
            if (!m_module) {
                return;
            }

            int removed = 0;
            removed += SDK::Events::RemoveHandlersFromModule(m_module);
            removed += SDK::Game::RemoveHandlersFromModule(m_module);
            removed += SDK::Drawing::RemoveHandlersFromModule(m_module);
            removed += SDK::OrbwalkingDetail::RemoveHandlersFromModule(m_module);
            if (removed > 0) {
                NightSharpDebug::Logf("[ExternalPluginLoader] Removed %d callbacks for name=%s module=%p",
                                      removed,
                                      m_name.c_str(),
                                      m_module);
            }
        }

        bool IsOwnedRuntimeMenu(const SDK::UI::Menu* menu) const {
            if (!menu) {
                return false;
            }

            return menu->Name.equals(m_internalId.c_str()) ||
                   menu->DisplayName.equals(m_internalId.c_str()) ||
                   menu->Name.equals(m_name.c_str()) ||
                   menu->DisplayName.equals(m_name.c_str());
        }

        void DetachOwnedRuntimeMenus() {
            auto& manager = SDK::UI::MenuManager::Instance();
            int removed = 0;
            for (int i = manager.Menus.size() - 1; i >= 0; --i) {
                SDK::UI::Menu* root = manager.Menus[i];
                if (!IsOwnedRuntimeMenu(root)) {
                    continue;
                }

                manager.Remove(root);
                ++removed;
            }

            if (removed > 0) {
                NightSharpDebug::Logf("[ExternalPluginLoader] Detached %d SDK menus for name=%s id=%s",
                                      removed,
                                      m_name.c_str(),
                                      m_internalId.c_str());
            }
        }

        HMODULE m_module = nullptr;
        const NightSharp::Plugin::Exports* m_exports = nullptr;
        std::string m_name;
        std::string m_internalId;
        std::string m_author;
        std::string m_championName;
        std::string m_configFileName;
        std::string m_path;
        PluginCategory m_category = PluginCategory::Core;
        bool m_autoLoad = true;
    };

    inline bool RegisterDll(const char* path) {
        if (!path || !path[0]) {
            return false;
        }

        NightSharpDebug::Logf("[ExternalPluginLoader] Loading path=%s", path);
        HMODULE module = LoadLibraryExA(path, nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
        if (!module) {
            NightSharpDebug::Logf("[ExternalPluginLoader] LoadLibraryExA failed gle=%lu path=%s",
                                  static_cast<unsigned long>(GetLastError()),
                                  path);
            return false;
        }

        const NightSharp::Plugin::Exports* exports = FetchExportsNoThrow(module, path);
        if (!exports) {
            FreeLibrary(module);
            return false;
        }

        char reason[256] = {};
        if (!ValidateExportsNoThrow(exports, path, reason, sizeof(reason))) {
            NightSharpDebug::Logf("[ExternalPluginLoader] Reject path=%s reason=%s",
                                  path,
                                  reason[0] ? reason : "unknown");
            FreeLibrary(module);
            return false;
        }

        const char* internalId = exports->Descriptor.InternalId;
        const int duplicate = PluginRegistry::FindByInternalId(internalId);
        if (duplicate >= 0) {
            NightSharpDebug::Logf("[ExternalPluginLoader] Reject duplicate id=%s existingIdx=%d path=%s",
                                  internalId,
                                  duplicate,
                                  path);
            FreeLibrary(module);
            return false;
        }

        auto plugin = std::make_unique<ExternalPlugin>(module, exports, path);
        const std::string name = plugin->GetName();
        const std::string id = plugin->GetInternalId();

        IPlugin* registered = PluginManager::Get().RegisterExternal(std::move(plugin));
        if (!registered) {
            NightSharpDebug::Logf("[ExternalPluginLoader] RegisterExternal failed name=%s id=%s path=%s",
                                  name.c_str(),
                                  id.c_str(),
                                  path);
            return false;
        }

        NightSharpDebug::Logf("[ExternalPluginLoader] Registered external plugin name=%s id=%s path=%s",
                              registered->GetName(),
                              registered->GetInternalId(),
                              path);
        return true;
    }

} // namespace Detail

inline void RegisterDevPlugins() {
    if (!Detail::IsDevLoaderEnabled()) {
        NightSharpDebug::Logf("[ExternalPluginLoader] Dev loader disabled");
        return;
    }

    char folder[MAX_PATH] = {};
    if (!Detail::BuildDevFolder(folder, MAX_PATH)) {
        NightSharpDebug::Logf("[ExternalPluginLoader] Dev folder unavailable");
        return;
    }

    NightSharpDebug::Logf("[ExternalPluginLoader] Dev folder=%s", folder);

    char pattern[MAX_PATH] = {};
    _snprintf_s(pattern, sizeof(pattern), _TRUNCATE, "%s\\*.dll", folder);

    WIN32_FIND_DATAA data = {};
    HANDLE find = FindFirstFileA(pattern, &data);
    if (find == INVALID_HANDLE_VALUE) {
        NightSharpDebug::Logf("[ExternalPluginLoader] No dev plugin DLLs found gle=%lu",
                              static_cast<unsigned long>(GetLastError()));
        return;
    }

    int found = 0;
    int registered = 0;

    do {
        if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            continue;
        }

        char path[MAX_PATH] = {};
        _snprintf_s(path, sizeof(path), _TRUNCATE, "%s\\%s", folder, data.cFileName);
        ++found;
        if (Detail::RegisterDll(path)) {
            ++registered;
        }
    } while (FindNextFileA(find, &data));

    FindClose(find);
    NightSharpDebug::Logf("[ExternalPluginLoader] Scan complete found=%d registered=%d",
                          found,
                          registered);
}

inline void Shutdown() {
    NightSharpDebug::Logf("[ExternalPluginLoader] Shutdown");
}

} // namespace Plugins::ExternalPluginLoader
