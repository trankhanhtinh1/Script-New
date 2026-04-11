#pragma once

#include "IPlugin.h"
#include "../menu/PluginRegistry.h"

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
            auto plugin = std::make_unique<T>(std::forward<Args>(args)...);
            T* raw = plugin.get();

            int idx = PluginRegistry::FindByInternalId(raw->GetInternalId());
            if (idx < 0) {
                idx = PluginRegistry::Register(
                    raw->GetName(),
                    raw->GetInternalId(),
                    PluginRegistry::PluginKind::Plugin,
                    raw->GetMenuRoot(),
                    raw->AutoLoadByDefault());
            } else {
                auto& entry = PluginRegistry::Plugins[idx];
                entry.Name = raw->GetName();
                entry.InternalId = raw->GetInternalId();
                entry.Kind = PluginRegistry::PluginKind::Plugin;
                entry.MenuRoot = raw->GetMenuRoot();
                entry.AlwaysLoad = raw->AutoLoadByDefault();
            }

            raw->m_registryIndex = idx;
            PluginRegistry::BindRuntime(idx, raw, &PluginManager::LoadThunk,
                &PluginManager::UnloadThunk, &PluginManager::MenuRootThunk,
                &PluginManager::CanLoadThunk);

            m_plugins.push_back(std::move(plugin));
            SyncRegistry(raw);
            return raw;
        }

        bool Load(IPlugin* plugin) {
            if (!plugin) return false;
            if (plugin->m_loaded) return true;
            if (!plugin->CanLoad()) return false;

            plugin->OnLoad();
            plugin->RegisterEvents();  // Auto-register virtual event callbacks
            plugin->m_loaded = true;
            SyncRegistry(plugin);
            return true;
        }

        bool Unload(IPlugin* plugin) {
            if (!plugin) return false;
            if (!plugin->m_loaded) return true;

            plugin->OnUnload();
            plugin->UnregisterEvents();
            plugin->m_loaded = false;
            SyncRegistry(plugin);
            return true;
        }

        void LoadAuto() {
            for (auto& plugin : m_plugins) {
                const int idx = plugin->m_registryIndex;
                if (idx < 0) continue;

                if (PluginRegistry::Plugins[idx].AlwaysLoad) {
                    if (!Load(plugin.get())) {
                        SyncRegistry(plugin.get());
                    }
                } else {
                    Unload(plugin.get());
                }
            }
        }

        void UnloadAll() {
            for (auto& plugin : m_plugins) {
                Unload(plugin.get());
            }
        }

        void RefreshRegistryViews() {
            for (auto& plugin : m_plugins) {
                SyncRegistry(plugin.get());
            }
        }

        void OnUpdate() {
            RefreshRegistryViews();
            for (auto& plugin : m_plugins) {
                if (plugin->m_loaded && plugin->m_enabled) {
                    plugin->OnUpdate();
                }
            }
        }

        void OnRender() {
            RefreshRegistryViews();
            for (auto& plugin : m_plugins) {
                if (plugin->m_loaded && plugin->m_enabled) {
                    plugin->OnRender();
                }
            }
        }

    private:
        static bool LoadThunk(void* userData) {
            return Get().Load(static_cast<IPlugin*>(userData));
        }

        static bool UnloadThunk(void* userData) {
            return Get().Unload(static_cast<IPlugin*>(userData));
        }

        static SDK::MenuUI::Menu* MenuRootThunk(void* userData) {
            auto* plugin = static_cast<IPlugin*>(userData);
            return plugin ? plugin->GetMenuRoot() : nullptr;
        }

        static bool CanLoadThunk(void* userData) {
            auto* plugin = static_cast<IPlugin*>(userData);
            return plugin ? plugin->CanLoad() : false;
        }

        void SyncRegistry(IPlugin* plugin) {
            if (!plugin) return;
            const int idx = plugin->m_registryIndex;
            if (idx < 0 || idx >= PluginRegistry::PluginCount) return;

            auto& entry = PluginRegistry::Plugins[idx];
            entry.Loaded = plugin->m_loaded;
            entry.MenuRoot = plugin->GetMenuRoot();
        }

        PluginManager() = default;
        std::vector<std::unique_ptr<IPlugin>> m_plugins;
    };

} // namespace Plugins
