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
            NightSharpDebug::Logf("[PluginManager] Register created name=%s id=%s",
                                  raw->GetName(), raw->GetInternalId());

            int idx = PluginRegistry::FindByInternalId(raw->GetInternalId());
            if (idx < 0) {
                idx = PluginRegistry::Register(
                    raw->GetName(),
                    raw->GetInternalId(),
                    ToRegistryKind(raw->GetCategory()),
                    raw->AutoLoadByDefault(),
                    ToRegistryCategory(raw->GetCategory()),
                    raw->GetChampionName());
            } else {
                auto& entry = PluginRegistry::Plugins[idx];
                entry.Name = raw->GetName();
                entry.InternalId = raw->GetInternalId();
                entry.Category = ToRegistryCategory(raw->GetCategory());
                entry.ChampionName = raw->GetChampionName();
                if (!entry.AlwaysLoadConfigured) {
                    entry.AlwaysLoad = raw->AutoLoadByDefault();
                }
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

        bool Load(IPlugin* plugin) {
            if (!plugin) return false;
            if (plugin->m_loaded) return true;
            NightSharpDebug::Logf("[PluginManager] Load begin name=%s id=%s",
                                  plugin->GetName(), plugin->GetInternalId());
            if (!plugin->CanLoad()) {
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
                plugin->m_loaded = false;
                SyncRegistry(plugin);
                return false;
            }
            plugin->m_loaded = true;
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
            __try {
                plugin->OnUnload();
            }
            __except (NightSharpDebug::CrashReporter::LogAndDumpException(
                          "PluginManager::Unload/OnUnload",
                          GetExceptionInformation())) {
                NightSharpDebug::Logf("[PluginManager] Unload crashed name=%s id=%s",
                                      plugin->GetName(), plugin->GetInternalId());
                plugin->m_loaded = false;
                SyncRegistry(plugin);
                return false;
            }
            plugin->m_loaded = false;
            SyncRegistry(plugin);
            NightSharpDebug::Logf("[PluginManager] Unload complete name=%s id=%s",
                                  plugin->GetName(), plugin->GetInternalId());
            return true;
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
                    char stage[160] = {};
                    _snprintf_s(stage,
                                sizeof(stage),
                                _TRUNCATE,
                                "PluginManager::OnUpdate/%s",
                                plugin->GetInternalId());
                    __try {
                        plugin->OnUpdate();
                    }
                    __except (NightSharpDebug::CrashReporter::LogAndDumpException(
                                  stage,
                                  GetExceptionInformation())) {
                        NightSharpDebug::Logf("[PluginManager] OnUpdate crashed; disabling name=%s id=%s",
                                              plugin->GetName(),
                                              plugin->GetInternalId());
                        plugin->m_enabled = false;
                        SyncRegistry(plugin.get());
                    }
                }
            }
        }

        void OnRender() {
            for (auto& plugin : m_plugins) {
                if (plugin->m_loaded && plugin->m_enabled) {
                    char stage[160] = {};
                    _snprintf_s(stage,
                                sizeof(stage),
                                _TRUNCATE,
                                "PluginManager::OnRender/%s",
                                plugin->GetInternalId());
                    __try {
                        plugin->OnRender();
                    }
                    __except (NightSharpDebug::CrashReporter::LogAndDumpException(
                                  stage,
                                  GetExceptionInformation())) {
                        NightSharpDebug::Logf("[PluginManager] OnRender crashed; disabling name=%s id=%s",
                                              plugin->GetName(),
                                              plugin->GetInternalId());
                        plugin->m_enabled = false;
                        SyncRegistry(plugin.get());
                    }
                }
            }
        }

    private:
        static PluginRegistry::PluginCategory ToRegistryCategory(PluginCategory category) {
            switch (category) {
            case PluginCategory::Champion: return PluginRegistry::PluginCategory::Champion;
            case PluginCategory::Utility:  return PluginRegistry::PluginCategory::Utility;
            case PluginCategory::Misc:     return PluginRegistry::PluginCategory::Misc;
            case PluginCategory::Core:
            default:                       return PluginRegistry::PluginCategory::Core;
            }
        }

        static PluginRegistry::PluginKind ToRegistryKind(PluginCategory category) {
            (void)category;
            return PluginRegistry::PluginKind::Plugin;
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
                    plugin->m_enabled = false;
                    Get().SyncRegistry(plugin);
                }
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
            __try {
                return plugin->CanLoad();
            }
            __except (NightSharpDebug::CrashReporter::LogAndDumpException(
                          stage,
                          GetExceptionInformation())) {
                NightSharpDebug::Logf("[PluginManager] CanLoad crashed name=%s id=%s",
                                      plugin->GetName(),
                                      plugin->GetInternalId());
                return false;
            }
        }

        void SyncRegistry(IPlugin* plugin) {
            if (!plugin) return;
            const int idx = plugin->m_registryIndex;
            if (idx < 0 || idx >= PluginRegistry::PluginCount) return;

            auto& entry = PluginRegistry::Plugins[idx];
            entry.Loaded = plugin->m_loaded;
            entry.Category = ToRegistryCategory(plugin->GetCategory());
            entry.ChampionName = plugin->GetChampionName();
        }

        PluginManager() = default;
        std::vector<std::unique_ptr<IPlugin>> m_plugins;
    };

} // namespace Plugins
