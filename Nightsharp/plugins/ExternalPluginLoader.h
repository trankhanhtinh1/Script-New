#pragma once
// ============================================================================
// ExternalPluginLoader.h - External plugin loader.
//
// Release packages are loaded from:
//   %AppData%\NightSharp\Plugins\*.NS
//
// Developer mode loads unpacked DLLs from:
//   %AppData%\NightSharp\Plugins\Dev
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

#include <NightSharp.SDK.Package.h>
#include <NightSharp.SDK.Plugin.h>

#include <Windows.h>
#include <ShlObj.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

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
        char enabled[16] = {};
        const DWORD enabledLen = GetEnvironmentVariableA(
            "NIGHTSHARP_ENABLE_DEV_PLUGINS",
            enabled,
            static_cast<DWORD>(sizeof(enabled)));

#if defined(_DEBUG)
        bool enabledByDefault = true;
#else
        bool enabledByDefault = false;
#endif

        if (enabledLen > 0 && enabled[0] != '\0') {
            enabledByDefault = enabled[0] != '0';
        }

        char disabled[16] = {};
        const DWORD len = GetEnvironmentVariableA(
            "NIGHTSHARP_DISABLE_DEV_PLUGINS",
            disabled,
            static_cast<DWORD>(sizeof(disabled)));
        if (len > 0 && disabled[0] != '\0' && disabled[0] != '0') {
            return false;
        }
        return enabledByDefault;
#else
        return false;
#endif
    }

    inline bool BuildPluginsFolder(char* out, DWORD outSize) {
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

        lstrcpynA(out, pluginsDir, static_cast<int>(outSize));
        return out[0] != '\0';
    }

    inline bool BuildDevFolder(char* out, DWORD outSize) {
        char pluginsDir[MAX_PATH] = {};
        if (!BuildPluginsFolder(pluginsDir, MAX_PATH)) {
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

    inline bool BuildPackageCacheFolder(char* out, DWORD outSize) {
        char pluginsDir[MAX_PATH] = {};
        if (!BuildPluginsFolder(pluginsDir, MAX_PATH)) {
            return false;
        }

        _snprintf_s(out, outSize, _TRUNCATE, "%s\\Cache", pluginsDir);
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

    inline bool FixedStringTerminated(const char* value, size_t size) {
        return value && size > 0 && std::memchr(value, '\0', size) != nullptr;
    }

    inline bool ReadAllBytes(const char* path, std::vector<std::uint8_t>& out) {
        out.clear();
        if (!path || !path[0]) {
            return false;
        }

        HANDLE file = CreateFileA(path,
                                  GENERIC_READ,
                                  FILE_SHARE_READ,
                                  nullptr,
                                  OPEN_EXISTING,
                                  FILE_ATTRIBUTE_NORMAL,
                                  nullptr);
        if (file == INVALID_HANDLE_VALUE) {
            NightSharpDebug::Logf("[ExternalPluginLoader] Read open failed gle=%lu path=%s",
                                  static_cast<unsigned long>(GetLastError()),
                                  path);
            return false;
        }

        LARGE_INTEGER size = {};
        if (!GetFileSizeEx(file, &size) || size.QuadPart <= 0 || size.QuadPart > 512ll * 1024ll * 1024ll) {
            NightSharpDebug::Logf("[ExternalPluginLoader] Invalid file size path=%s size=%lld",
                                  path,
                                  static_cast<long long>(size.QuadPart));
            CloseHandle(file);
            return false;
        }

        out.resize(static_cast<size_t>(size.QuadPart));
        DWORD read = 0;
        const BOOL ok = ReadFile(file,
                                 out.data(),
                                 static_cast<DWORD>(out.size()),
                                 &read,
                                 nullptr);
        CloseHandle(file);

        if (!ok || read != out.size()) {
            NightSharpDebug::Logf("[ExternalPluginLoader] Read failed path=%s read=%lu expected=%llu gle=%lu",
                                  path,
                                  static_cast<unsigned long>(read),
                                  static_cast<unsigned long long>(out.size()),
                                  static_cast<unsigned long>(GetLastError()));
            out.clear();
            return false;
        }

        return true;
    }

    inline bool WriteAllBytes(const char* path, const std::uint8_t* data, size_t size) {
        if (!path || !path[0] || !data || size == 0 || size > 512ull * 1024ull * 1024ull) {
            return false;
        }

        HANDLE file = CreateFileA(path,
                                  GENERIC_WRITE,
                                  0,
                                  nullptr,
                                  CREATE_ALWAYS,
                                  FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_NOT_CONTENT_INDEXED,
                                  nullptr);
        if (file == INVALID_HANDLE_VALUE) {
            NightSharpDebug::Logf("[ExternalPluginLoader] Write open failed gle=%lu path=%s",
                                  static_cast<unsigned long>(GetLastError()),
                                  path);
            return false;
        }

        DWORD written = 0;
        const BOOL ok = WriteFile(file,
                                  data,
                                  static_cast<DWORD>(size),
                                  &written,
                                  nullptr);
        CloseHandle(file);

        if (!ok || written != size) {
            NightSharpDebug::Logf("[ExternalPluginLoader] Write failed path=%s written=%lu expected=%llu gle=%lu",
                                  path,
                                  static_cast<unsigned long>(written),
                                  static_cast<unsigned long long>(size),
                                  static_cast<unsigned long>(GetLastError()));
            DeleteFileA(path);
            return false;
        }

        return true;
    }

    inline void BuildSafeStem(const char* value, char* out, size_t outSize) {
        if (!out || outSize == 0) {
            return;
        }
        out[0] = '\0';

        const char* base = value && value[0] ? value : "plugin";
        size_t used = 0;
        for (const char* p = base; *p && used + 1 < outSize; ++p) {
            if (*p == '\\' || *p == '/') {
                used = 0;
                continue;
            }
            if (*p == '.') {
                const char* next = p + 1;
                if ((next[0] == 'n' || next[0] == 'N' || next[0] == 'd' || next[0] == 'D') &&
                    (next[1] == 's' || next[1] == 'S' || next[1] == 'l' || next[1] == 'L') &&
                    (next[2] == '\0' || next[2] == 'l' || next[2] == 'L')) {
                    break;
                }
            }

            const unsigned char ch = static_cast<unsigned char>(*p);
            if ((ch >= 'a' && ch <= 'z') ||
                (ch >= 'A' && ch <= 'Z') ||
                (ch >= '0' && ch <= '9') ||
                ch == '-' || ch == '_' || ch == '.') {
                out[used++] = static_cast<char>(ch);
            } else {
                out[used++] = '_';
            }
        }

        if (used == 0) {
            lstrcpynA(out, "plugin", static_cast<int>(outSize));
        } else {
            out[used] = '\0';
        }
    }

    inline bool BuildSiblingPath(const char* packagePath,
                                 const char* dependency,
                                 char* out,
                                 size_t outSize) {
        if (!packagePath || !dependency || !out || outSize == 0) {
            return false;
        }

        if ((dependency[0] && dependency[1] == ':') || dependency[0] == '\\' || dependency[0] == '/') {
            lstrcpynA(out, dependency, static_cast<int>(outSize));
            return out[0] != '\0';
        }

        const char* slash = packagePath;
        for (const char* p = packagePath; *p; ++p) {
            if (*p == '\\' || *p == '/') {
                slash = p + 1;
            }
        }

        const size_t dirLen = static_cast<size_t>(slash - packagePath);
        if (dirLen + lstrlenA(dependency) + 1 >= outSize) {
            return false;
        }

        std::memcpy(out, packagePath, dirLen);
        out[dirLen] = '\0';
        lstrcpynA(out + dirLen, dependency, static_cast<int>(outSize - dirLen));
        return out[0] != '\0';
    }

    inline bool CheckPackageDependencies(const char* packagePath,
                                         const char* dependencies,
                                         char* reason,
                                         size_t reasonSize) {
        if (!dependencies || !dependencies[0]) {
            return true;
        }

        const char* p = dependencies;
        while (*p) {
            while (*p == ' ' || *p == '\t' || *p == ';' || *p == ',') {
                ++p;
            }
            if (!*p) {
                break;
            }

            char token[MAX_PATH] = {};
            size_t used = 0;
            while (*p && *p != ';' && *p != ',' && used + 1 < sizeof(token)) {
                token[used++] = *p++;
            }
            while (used > 0 && (token[used - 1] == ' ' || token[used - 1] == '\t')) {
                token[--used] = '\0';
            }
            if (used == 0) {
                continue;
            }

            char depPath[MAX_PATH] = {};
            if (!BuildSiblingPath(packagePath, token, depPath, sizeof(depPath)) ||
                GetFileAttributesA(depPath) == INVALID_FILE_ATTRIBUTES) {
                _snprintf_s(reason,
                            reasonSize,
                            _TRUNCATE,
                            "missing dependency %s",
                            token);
                return false;
            }
        }

        reason[0] = '\0';
        return true;
    }

    inline void CleanPackageCache() {
        char folder[MAX_PATH] = {};
        if (!BuildPackageCacheFolder(folder, MAX_PATH)) {
            return;
        }

        char pattern[MAX_PATH] = {};
        _snprintf_s(pattern, sizeof(pattern), _TRUNCATE, "%s\\*.dll", folder);

        WIN32_FIND_DATAA data = {};
        HANDLE find = FindFirstFileA(pattern, &data);
        if (find == INVALID_HANDLE_VALUE) {
            return;
        }

        do {
            if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
                continue;
            }

            char path[MAX_PATH] = {};
            _snprintf_s(path, sizeof(path), _TRUNCATE, "%s\\%s", folder, data.cFileName);
            DeleteFileA(path);
        } while (FindNextFileA(find, &data));

        FindClose(find);
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
                       const char* path,
                       const char* ownedFile = nullptr)
            : m_module(module),
              m_exports(exports),
              m_name(SafeText(exports->Descriptor.Name, "External Plugin")),
              m_internalId(SafeText(exports->Descriptor.InternalId, "external.unknown")),
              m_author(SafeText(exports->Descriptor.Author, "External")),
              m_championName(SafeText(exports->Descriptor.ChampionName, "")),
              m_path(SafeText(path)),
              m_ownedFile(SafeText(ownedFile)),
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
                if (!m_ownedFile.empty()) {
                    if (DeleteFileA(m_ownedFile.c_str())) {
                        NightSharpDebug::Logf("[ExternalPluginLoader] Deleted package cache path=%s",
                                              m_ownedFile.c_str());
                    }
                }
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
        std::string m_ownedFile;
        PluginCategory m_category = PluginCategory::Core;
        bool m_autoLoad = true;
    };

    inline void DeleteOwnedFile(const char* ownedFile) {
        if (ownedFile && ownedFile[0]) {
            DeleteFileA(ownedFile);
        }
    }

    inline bool RegisterDllFromPath(const char* dllPath,
                                    const char* sourcePath,
                                    const char* ownedFile = nullptr) {
        if (!dllPath || !dllPath[0]) {
            return false;
        }

        const char* displayPath = sourcePath && sourcePath[0] ? sourcePath : dllPath;
        NightSharpDebug::Logf("[ExternalPluginLoader] Loading path=%s modulePath=%s",
                              displayPath,
                              dllPath);
        HMODULE module = LoadLibraryExA(dllPath, nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
        if (!module) {
            NightSharpDebug::Logf("[ExternalPluginLoader] LoadLibraryExA failed gle=%lu path=%s",
                                  static_cast<unsigned long>(GetLastError()),
                                  dllPath);
            DeleteOwnedFile(ownedFile);
            return false;
        }

        const NightSharp::Plugin::Exports* exports = FetchExportsNoThrow(module, displayPath);
        if (!exports) {
            FreeLibrary(module);
            DeleteOwnedFile(ownedFile);
            return false;
        }

        char reason[256] = {};
        if (!ValidateExportsNoThrow(exports, displayPath, reason, sizeof(reason))) {
            NightSharpDebug::Logf("[ExternalPluginLoader] Reject path=%s reason=%s",
                                  displayPath,
                                  reason[0] ? reason : "unknown");
            FreeLibrary(module);
            DeleteOwnedFile(ownedFile);
            return false;
        }

        const char* internalId = exports->Descriptor.InternalId;
        const int duplicate = PluginRegistry::FindByInternalId(internalId);
        if (duplicate >= 0) {
            NightSharpDebug::Logf("[ExternalPluginLoader] Reject duplicate id=%s existingIdx=%d path=%s",
                                  internalId,
                                  duplicate,
                                  displayPath);
            FreeLibrary(module);
            DeleteOwnedFile(ownedFile);
            return false;
        }

        auto plugin = std::make_unique<ExternalPlugin>(module, exports, displayPath, ownedFile);
        const std::string name = plugin->GetName();
        const std::string id = plugin->GetInternalId();

        IPlugin* registered = PluginManager::Get().RegisterExternal(std::move(plugin));
        if (!registered) {
            NightSharpDebug::Logf("[ExternalPluginLoader] RegisterExternal failed name=%s id=%s path=%s",
                                  name.c_str(),
                                  id.c_str(),
                                  displayPath);
            return false;
        }

        NightSharpDebug::Logf("[ExternalPluginLoader] Registered external plugin name=%s id=%s path=%s",
                              registered->GetName(),
                              registered->GetInternalId(),
                              displayPath);
        return true;
    }

    inline bool RegisterDll(const char* path) {
        return RegisterDllFromPath(path, path, nullptr);
    }

    inline bool ValidatePackageHeader(const NightSharp::Package::Header& header,
                                      const std::uint8_t* payload,
                                      size_t payloadSize,
                                      size_t fileSize,
                                      char* reason,
                                      size_t reasonSize) {
        if (header.Magic != NightSharp::Package::kMagic) {
            _snprintf_s(reason, reasonSize, _TRUNCATE, "bad package magic");
            return false;
        }
        if (header.HeaderSize != sizeof(NightSharp::Package::Header)) {
            _snprintf_s(reason,
                        reasonSize,
                        _TRUNCATE,
                        "header size mismatch plugin=%u expected=%llu",
                        static_cast<unsigned>(header.HeaderSize),
                        static_cast<unsigned long long>(sizeof(NightSharp::Package::Header)));
            return false;
        }
        if (header.FormatVersion != NightSharp::Package::kFormatVersion) {
            _snprintf_s(reason,
                        reasonSize,
                        _TRUNCATE,
                        "format mismatch plugin=%u expected=%u",
                        static_cast<unsigned>(header.FormatVersion),
                        static_cast<unsigned>(NightSharp::Package::kFormatVersion));
            return false;
        }
        if ((header.Flags & NightSharp::Package::kFlagEncrypted) == 0) {
            _snprintf_s(reason, reasonSize, _TRUNCATE, "package payload is not encrypted");
            return false;
        }
        if (header.AbiRevision != NIGHTSHARP_SDK_ABI_REVISION) {
            _snprintf_s(reason,
                        reasonSize,
                        _TRUNCATE,
                        "ABI revision mismatch plugin=%u expected=%u",
                        static_cast<unsigned>(header.AbiRevision),
                        static_cast<unsigned>(NIGHTSHARP_SDK_ABI_REVISION));
            return false;
        }
        if (header.PayloadSize == 0 ||
            header.PayloadSize != header.OriginalSize ||
            header.PayloadSize != payloadSize ||
            header.PayloadSize > 512ull * 1024ull * 1024ull ||
            sizeof(NightSharp::Package::Header) + payloadSize != fileSize) {
            _snprintf_s(reason,
                        reasonSize,
                        _TRUNCATE,
                        "payload size mismatch payload=%llu file=%llu",
                        static_cast<unsigned long long>(header.PayloadSize),
                        static_cast<unsigned long long>(fileSize));
            return false;
        }
        if (!FixedStringTerminated(header.SdkAbiId, sizeof(header.SdkAbiId)) ||
            !FixedStringTerminated(header.Name, sizeof(header.Name)) ||
            !FixedStringTerminated(header.InternalId, sizeof(header.InternalId)) ||
            !FixedStringTerminated(header.Author, sizeof(header.Author)) ||
            !FixedStringTerminated(header.ChampionName, sizeof(header.ChampionName)) ||
            !FixedStringTerminated(header.PluginVersion, sizeof(header.PluginVersion)) ||
            !FixedStringTerminated(header.Dependencies, sizeof(header.Dependencies))) {
            _snprintf_s(reason, reasonSize, _TRUNCATE, "package metadata is not terminated");
            return false;
        }
        if (!header.Name[0] || !header.InternalId[0]) {
            _snprintf_s(reason, reasonSize, _TRUNCATE, "package metadata missing name or id");
            return false;
        }
        if (!CStrEquals(header.SdkAbiId, NIGHTSHARP_SDK_ABI_ID)) {
            _snprintf_s(reason,
                        reasonSize,
                        _TRUNCATE,
                        "ABI id mismatch plugin=%s expected=%s",
                        header.SdkAbiId,
                        NIGHTSHARP_SDK_ABI_ID);
            return false;
        }
        const std::uint64_t cipherHash = NightSharp::Package::Fnv1a64(payload, payloadSize);
        if (cipherHash != header.CipherHash) {
            _snprintf_s(reason, reasonSize, _TRUNCATE, "cipher hash mismatch");
            return false;
        }
        if (!NightSharp::Package::VerifySignature(header, payload, payloadSize)) {
            _snprintf_s(reason, reasonSize, _TRUNCATE, "package signature mismatch");
            return false;
        }

        reason[0] = '\0';
        return true;
    }

    inline bool BuildPackageCachePath(const char* packagePath,
                                      const NightSharp::Package::Header& header,
                                      char* out,
                                      size_t outSize) {
        if (!out || outSize == 0) {
            return false;
        }
        out[0] = '\0';

        char folder[MAX_PATH] = {};
        if (!BuildPackageCacheFolder(folder, MAX_PATH)) {
            return false;
        }

        char stem[80] = {};
        BuildSafeStem(header.InternalId[0] ? header.InternalId : NightSharpDebug::BaseName(packagePath),
                      stem,
                      sizeof(stem));
        _snprintf_s(out,
                    outSize,
                    _TRUNCATE,
                    "%s\\%s.%016llX.%016llX.dll",
                    folder,
                    stem,
                    static_cast<unsigned long long>(header.Signature0),
                    static_cast<unsigned long long>(header.PlainHash));
        return out[0] != '\0';
    }

    inline bool ExtractPackageToCache(const char* packagePath,
                                      char* outCachePath,
                                      size_t outCachePathSize,
                                      char* reason,
                                      size_t reasonSize) {
        if (!packagePath || !packagePath[0] || !outCachePath || outCachePathSize == 0) {
            _snprintf_s(reason, reasonSize, _TRUNCATE, "invalid package path");
            return false;
        }
        outCachePath[0] = '\0';

        std::vector<std::uint8_t> bytes;
        if (!ReadAllBytes(packagePath, bytes)) {
            _snprintf_s(reason, reasonSize, _TRUNCATE, "read package failed");
            return false;
        }

        if (bytes.size() <= sizeof(NightSharp::Package::Header)) {
            _snprintf_s(reason, reasonSize, _TRUNCATE, "package too small");
            return false;
        }

        NightSharp::Package::Header header = {};
        std::memcpy(&header, bytes.data(), sizeof(header));

        const auto* payload = bytes.data() + sizeof(NightSharp::Package::Header);
        const size_t payloadSize = bytes.size() - sizeof(NightSharp::Package::Header);
        if (!ValidatePackageHeader(header,
                                   payload,
                                   payloadSize,
                                   bytes.size(),
                                   reason,
                                   reasonSize)) {
            return false;
        }

        if (!CheckPackageDependencies(packagePath, header.Dependencies, reason, reasonSize)) {
            return false;
        }

        std::vector<std::uint8_t> plain(payload, payload + payloadSize);
        NightSharp::Package::XorCrypt(header, plain.data(), plain.size());
        const std::uint64_t plainHash = NightSharp::Package::Fnv1a64(plain.data(), plain.size());
        if (plainHash != header.PlainHash) {
            _snprintf_s(reason, reasonSize, _TRUNCATE, "plain hash mismatch");
            return false;
        }

        if (!BuildPackageCachePath(packagePath, header, outCachePath, outCachePathSize)) {
            _snprintf_s(reason, reasonSize, _TRUNCATE, "cache path unavailable");
            return false;
        }

        DeleteFileA(outCachePath);
        if (!WriteAllBytes(outCachePath, plain.data(), plain.size())) {
            _snprintf_s(reason, reasonSize, _TRUNCATE, "write cache dll failed");
            return false;
        }

        NightSharpDebug::Logf("[ExternalPluginLoader] Extracted .NS name=%s id=%s version=%s cache=%s",
                              header.Name,
                              header.InternalId,
                              header.PluginVersion,
                              outCachePath);
        reason[0] = '\0';
        return true;
    }

    inline bool RegisterPackage(const char* packagePath) {
        char cachePath[MAX_PATH] = {};
        char reason[256] = {};
        if (!ExtractPackageToCache(packagePath, cachePath, sizeof(cachePath), reason, sizeof(reason))) {
            NightSharpDebug::Logf("[ExternalPluginLoader] Reject .NS path=%s reason=%s",
                                  SafeText(packagePath),
                                  reason[0] ? reason : "unknown");
            return false;
        }

        return RegisterDllFromPath(cachePath, packagePath, cachePath);
    }

} // namespace Detail

inline void RegisterReleasePlugins() {
    Detail::CleanPackageCache();

    char folder[MAX_PATH] = {};
    if (!Detail::BuildPluginsFolder(folder, MAX_PATH)) {
        NightSharpDebug::Logf("[ExternalPluginLoader] Release folder unavailable");
        return;
    }

    NightSharpDebug::Logf("[ExternalPluginLoader] Release folder=%s", folder);

    char pattern[MAX_PATH] = {};
    _snprintf_s(pattern, sizeof(pattern), _TRUNCATE, "%s\\*.NS", folder);

    WIN32_FIND_DATAA data = {};
    HANDLE find = FindFirstFileA(pattern, &data);
    if (find == INVALID_HANDLE_VALUE) {
        NightSharpDebug::Logf("[ExternalPluginLoader] No release .NS plugins found gle=%lu",
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
        if (Detail::RegisterPackage(path)) {
            ++registered;
        }
    } while (FindNextFileA(find, &data));

    FindClose(find);
    NightSharpDebug::Logf("[ExternalPluginLoader] Release scan complete found=%d registered=%d",
                          found,
                          registered);
}

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
