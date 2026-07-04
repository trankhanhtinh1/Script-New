#pragma once
// ============================================================================
// PluginManager.h — Plugin lifecycle manager cho NightSharp
//
// Ported from Old/plugins/PluginManager.h.
// Simplified: no SDK::MenuUI dependency.
// ============================================================================

#include "IPlugin.h"
#include "PluginRegistry.h"
#include "../CrashReporter.h"
#include "../DebugLog.h"
#include "../FpsDropDebug.h"

#include <cstdio>
#include <memory>
#include <utility>
#include <vector>

namespace Plugins {

    class PluginManager {
    public:
        static PluginManager& Get() {
            static PluginManager instance;
            return instance;
        }

        template<typename T, typename... Args>
        T* Register(Args&&... args) {
            NightSharpDebug::Logf("[PluginManager] Register begin");
            auto plugin = std::make_unique<T>(std::forward<Args>(args)...);
            T* raw = plugin.get();
            IPlugin* registered = RegisterOwned(std::move(plugin), PluginRegistry::PluginKind::Plugin);
            return registered ? raw : nullptr;
        }

        IPlugin* RegisterExternal(std::unique_ptr<IPlugin> plugin) {
            return RegisterOwned(std::move(plugin), PluginRegistry::PluginKind::External);
        }

        bool Load(IPlugin* plugin) {
            if (!plugin) return false;
            if (plugin->m_loaded) return true;
            NightSharpDebug::Logf("[PluginManager] Load begin name=%s id=%s",
                                  plugin->GetName(), plugin->GetInternalId());
            if (!SafeCanLoad(plugin, "PluginManager::Load/CanLoad")) {
                NightSharpDebug::Logf("[PluginManager] Load blocked by CanLoad name=%s",
                                      plugin->GetName());
                plugin->m_loaded = false;
                SyncRegistry(plugin);
                return false;
            }

            __try {
                plugin->OnLoad();
            }
            __except (NightSharpDebug::CrashReporter::LogAndDumpException(
                          "PluginManager::Load/OnLoad",
                          GetExceptionInformation())) {
                NightSharpDebug::Logf("[PluginManager] Load crashed name=%s id=%s",
                                      plugin->GetName(), plugin->GetInternalId());
                DisableAfterCrash(plugin, "PluginManager::Load/OnLoad");
                plugin->m_loaded = false;
                SyncRegistry(plugin);
                return false;
            }
            plugin->m_loaded = true;
            plugin->m_enabled = true;
            ClearRuntimeError(plugin);
            ResetCanLoadCache(plugin);
            SyncRegistry(plugin);
            NightSharpDebug::Logf("[PluginManager] Load complete name=%s id=%s",
                                  plugin->GetName(), plugin->GetInternalId());
            return true;
        }

        bool Unload(IPlugin* plugin) {
            if (!plugin) return false;
            if (!plugin->m_loaded) return true;

            NightSharpDebug::Logf("[PluginManager] Unload begin name=%s id=%s",
                                  plugin->GetName(), plugin->GetInternalId());
            bool crashed = false;
            __try {
                plugin->OnUnload();
            }
            __except (NightSharpDebug::CrashReporter::LogAndDumpException(
                          "PluginManager::Unload/OnUnload",
                          GetExceptionInformation())) {
                NightSharpDebug::Logf("[PluginManager] Unload crashed name=%s id=%s",
                                      plugin->GetName(), plugin->GetInternalId());
                MarkRuntimeCrash(plugin, "PluginManager::Unload/OnUnload");
                crashed = true;
            }

            RuntimeCleanup(plugin, "PluginManager::Unload/Cleanup");
            plugin->m_loaded = false;
            SyncRegistry(plugin);
            NightSharpDebug::Logf("[PluginManager] Unload complete name=%s id=%s",
                                  plugin->GetName(), plugin->GetInternalId());
            return !crashed;
        }

        void LoadAuto() {
            NightSharpDebug::Logf("[PluginManager] LoadAuto begin count=%d", PluginRegistry::PluginCount);
            for (auto& plugin : m_plugins) {
                const int idx = plugin->m_registryIndex;
                if (idx < 0) continue;

                if (PluginRegistry::Plugins[idx].AlwaysLoad) {
                    NightSharpDebug::Logf("[PluginManager] LoadAuto loading name=%s idx=%d",
                                          plugin->GetName(), idx);
                    if (!Load(plugin.get())) {
                        SyncRegistry(plugin.get());
                    }
                } else {
                    NightSharpDebug::Logf("[PluginManager] LoadAuto unloading name=%s idx=%d",
                                          plugin->GetName(), idx);
                    Unload(plugin.get());
                }
            }
            NightSharpDebug::Logf("[PluginManager] LoadAuto complete");
        }

        void UnloadAll() {
            NightSharpDebug::Logf("[PluginManager] UnloadAll begin");
            for (auto it = m_plugins.rbegin(); it != m_plugins.rend(); ++it) {
                Unload(it->get());
            }
            NightSharpDebug::Logf("[PluginManager] UnloadAll complete");
        }

        void Shutdown() {
            UnloadAll();
            m_plugins.clear();
        }

        void OnUpdate() {
            for (auto& plugin : m_plugins) {
                if (plugin->m_loaded && plugin->m_enabled) {
                    const auto perfStart = NightSharpPerf::Now();
                    const auto sectionStart = NightSharpPerf::SectionNow();
                    __try {
                        plugin->OnUpdate();
                    }
                    __except (NightSharpDebug::CrashReporter::LogAndDumpException(
                                  plugin->GetInternalId(),
                                  GetExceptionInformation())) {
                        NightSharpDebug::Logf("[PluginManager] OnUpdate crashed; disabling name=%s id=%s",
                                              plugin->GetName(),
                                              plugin->GetInternalId());
                        DisableAfterCrash(plugin.get(), "PluginManager::OnUpdate");
                        SyncRegistry(plugin.get());
                    }
                    NightSharpPerf::AddPluginTiming(
                        "update",
                        plugin->GetInternalId(),
                        plugin->GetName(),
                        NightSharpPerf::MsSince(perfStart));
                    NightSharpPerf::RecordSection(
                        plugin->GetInternalId(),
                        NightSharpPerf::SectionMsSince(sectionStart));
                }
            }
        }

        void OnRender() {
            for (auto& plugin : m_plugins) {
                if (plugin->m_loaded && plugin->m_enabled) {
                    const auto perfStart = NightSharpPerf::Now();
                    const auto sectionStart = NightSharpPerf::SectionNow();
                    __try {
                        plugin->OnRender();
                    }
                    __except (NightSharpDebug::CrashReporter::LogAndDumpException(
                                  plugin->GetInternalId(),
                                  GetExceptionInformation())) {
                        NightSharpDebug::Logf("[PluginManager] OnRender crashed; disabling name=%s id=%s",
                                              plugin->GetName(),
                                              plugin->GetInternalId());
                        DisableAfterCrash(plugin.get(), "PluginManager::OnRender");
                        SyncRegistry(plugin.get());
                    }
                    NightSharpPerf::AddPluginTiming(
                        "render",
                        plugin->GetInternalId(),
                        plugin->GetName(),
                        NightSharpPerf::MsSince(perfStart));
                    // Optional: could add another section prefix for render
                    // NightSharpPerf::RecordSection(...) 
                }
            }
        }

    private:
        void ClearRuntimeError(IPlugin* plugin) {
            if (!plugin) {
                return;
            }
            plugin->m_lastRuntimeError[0] = '\0';
        }

        void MarkRuntimeCrash(IPlugin* plugin, const char* stage) {
            if (!plugin) {
                return;
            }

            plugin->m_enabled = false;
            ++plugin->m_crashCount;
            lstrcpynA(plugin->m_lastRuntimeError,
                      stage && stage[0] ? stage : "runtime-crash",
                      static_cast<int>(sizeof(plugin->m_lastRuntimeError)));
            NightSharpDebug::Logf("[PluginManager] Disabled crashed plugin name=%s id=%s crashes=%d stage=%s",
                                  plugin->GetName(),
                                  plugin->GetInternalId(),
                                  plugin->m_crashCount,
                                  plugin->m_lastRuntimeError);
            SyncRegistry(plugin);
        }

        void DisableAfterCrash(IPlugin* plugin, const char* stage) {
            if (!plugin) {
                return;
            }

            MarkRuntimeCrash(plugin, stage);
            RuntimeCleanup(plugin, stage && stage[0] ? stage : "PluginManager::CrashCleanup");
            plugin->m_loaded = false;
            SyncRegistry(plugin);
        }

        void ResetCanLoadCache(IPlugin* plugin) {
            if (!plugin) {
                return;
            }
            const int idx = plugin->m_registryIndex;
            if (idx < 0 || idx >= PluginRegistry::PluginCount) {
                return;
            }
            auto& entry = PluginRegistry::Plugins[idx];
            entry.CanLoadCached = true;
            entry.CanLoadChecked = false;
        }

        bool SafeCanLoad(IPlugin* plugin, const char* stage) {
            if (!plugin) {
                return false;
            }

            __try {
                return plugin->CanLoad();
            }
            __except (NightSharpDebug::CrashReporter::LogAndDumpException(
                          stage ? stage : "PluginManager::CanLoad",
                          GetExceptionInformation())) {
                NightSharpDebug::Logf("[PluginManager] CanLoad crashed name=%s id=%s",
                                      plugin->GetName(),
                                      plugin->GetInternalId());
                DisableAfterCrash(plugin, stage ? stage : "PluginManager::CanLoad");
                return false;
            }
        }

        void RuntimeCleanup(IPlugin* plugin, const char* stage) {
            if (!plugin) {
                return;
            }

            __try {
                plugin->OnRuntimeCleanup();
            }
            __except (NightSharpDebug::CrashReporter::LogAndDumpException(
                          stage ? stage : "PluginManager::RuntimeCleanup",
                          GetExceptionInformation())) {
                NightSharpDebug::Logf("[PluginManager] Runtime cleanup crashed name=%s id=%s",
                                      plugin->GetName(),
                                      plugin->GetInternalId());
                MarkRuntimeCrash(plugin, stage ? stage : "PluginManager::RuntimeCleanup");
            }
        }

        IPlugin* RegisterOwned(std::unique_ptr<IPlugin> plugin, PluginRegistry::PluginKind kind) {
            if (!plugin) {
                NightSharpDebug::Logf("[PluginManager] RegisterOwned skipped: null plugin");
                return nullptr;
            }

            IPlugin* raw = plugin.get();
            NightSharpDebug::Logf("[PluginManager] Register created name=%s id=%s kind=%d",
                                  raw->GetName(), raw->GetInternalId(), static_cast<int>(kind));

            int idx = PluginRegistry::FindByInternalId(raw->GetInternalId());
            if (idx < 0) {
                idx = PluginRegistry::Register(
                    raw->GetName(),
                    raw->GetInternalId(),
                    kind,
                    raw->AutoLoadByDefault(),
                    ToRegistryCategory(raw->GetCategory()),
                    raw->GetChampionName(),
                    raw->GetConfigFileName());
            } else {
                auto& entry = PluginRegistry::Plugins[idx];
                entry.Name = raw->GetName();
                entry.InternalId = raw->GetInternalId();
                entry.Kind = kind;
                entry.Category = ToRegistryCategory(raw->GetCategory());
                entry.ChampionName = raw->GetChampionName();
                entry.ConfigFileName = raw->GetConfigFileName();
                if (!entry.AlwaysLoadConfigured) {
                    entry.AlwaysLoad = raw->AutoLoadByDefault();
                }
            }

            if (idx < 0) {
                NightSharpDebug::Logf("[PluginManager] Register failed name=%s id=%s kind=%d",
                                      raw->GetName(), raw->GetInternalId(), static_cast<int>(kind));
                return nullptr;
            }

            raw->m_registryIndex = idx;
            PluginRegistry::BindRuntime(idx, raw, &PluginManager::LoadThunk,
                &PluginManager::UnloadThunk, &PluginManager::MenuThunk,
                &PluginManager::CanLoadThunk);

            m_plugins.push_back(std::move(plugin));
            SyncRegistry(raw);
            NightSharpDebug::Logf("[PluginManager] Register complete name=%s idx=%d",
                                  raw->GetName(), raw->m_registryIndex);
            return raw;
        }

        static PluginRegistry::PluginCategory ToRegistryCategory(PluginCategory category) {
            switch (category) {
            case PluginCategory::Champion: return PluginRegistry::PluginCategory::Champion;
            case PluginCategory::Utility:  return PluginRegistry::PluginCategory::Utility;
            case PluginCategory::Misc:     return PluginRegistry::PluginCategory::Misc;
            case PluginCategory::Core:
            default:                       return PluginRegistry::PluginCategory::Core;
            }
        }

        static bool LoadThunk(void* userData) {
            return Get().Load(static_cast<IPlugin*>(userData));
        }

        static bool UnloadThunk(void* userData) {
            return Get().Unload(static_cast<IPlugin*>(userData));
        }

        static void MenuThunk(void* userData) {
            auto* plugin = static_cast<IPlugin*>(userData);
            if (plugin && plugin->m_loaded && plugin->m_enabled) {
                const auto perfStart = NightSharpPerf::Now();
                char stage[160] = {};
                _snprintf_s(stage,
                            sizeof(stage),
                            _TRUNCATE,
                            "PluginManager::OnMenu/%s",
                            plugin->GetInternalId());
                __try {
                    plugin->OnMenu();
                }
                __except (NightSharpDebug::CrashReporter::LogAndDumpException(
                              stage,
                              GetExceptionInformation())) {
                    NightSharpDebug::Logf("[PluginManager] OnMenu crashed; disabling name=%s id=%s",
                                          plugin->GetName(),
                                          plugin->GetInternalId());
                    Get().DisableAfterCrash(plugin, stage);
                    Get().SyncRegistry(plugin);
                }
                NightSharpPerf::AddPluginTiming(
                    "menu",
                    plugin->GetInternalId(),
                    plugin->GetName(),
                    NightSharpPerf::MsSince(perfStart));
            }
        }

        static bool CanLoadThunk(void* userData) {
            auto* plugin = static_cast<IPlugin*>(userData);
            if (!plugin) {
                return false;
            }

            char stage[160] = {};
            _snprintf_s(stage,
                        sizeof(stage),
                        _TRUNCATE,
                        "PluginManager::CanLoad/%s",
                        plugin->GetInternalId());
            return Get().SafeCanLoad(plugin, stage);
        }

        void SyncRegistry(IPlugin* plugin) {
            if (!plugin) return;
            const int idx = plugin->m_registryIndex;
            if (idx < 0 || idx >= PluginRegistry::PluginCount) return;

            auto& entry = PluginRegistry::Plugins[idx];
            entry.Loaded = plugin->m_loaded;
            entry.Enabled = plugin->m_enabled;
            entry.Category = ToRegistryCategory(plugin->GetCategory());
            entry.ChampionName = plugin->GetChampionName();
            entry.CrashCount = plugin->m_crashCount;
            entry.LastRuntimeError = plugin->m_lastRuntimeError[0] ? plugin->m_lastRuntimeError : nullptr;
            if (!plugin->m_enabled && plugin->m_crashCount > 0) {
                entry.CanLoadCached = false;
                entry.CanLoadChecked = true;
            }
        }

        PluginManager() = default;
        std::vector<std::unique_ptr<IPlugin>> m_plugins;
    };

} // namespace Plugins
