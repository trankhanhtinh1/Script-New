#pragma once

#include "IPlugin.h"
#include "../menu/PluginRegistry.h"
#include "../core/CrashTelemetry.h"

#include <cstdio>
#include <memory>
#include <utility>
#include <vector>

namespace Plugins {

    inline char g_pluginStageBuf[256] = {};

    inline void SetPluginStage(const char* phase, const char* name, int idx = -1) {
        const char* p = phase ? phase : "?";
        const char* n = (name && *name) ? name : "(null)";
        if (idx >= 0) {
            std::snprintf(g_pluginStageBuf, sizeof(g_pluginStageBuf),
                "PluginManager::%s[%d]::%s", p, idx, n);
        } else {
            std::snprintf(g_pluginStageBuf, sizeof(g_pluginStageBuf),
                "PluginManager::%s::%s", p, n);
        }
        CrashTelemetry::SetStage(g_pluginStageBuf);
    }

    inline void LogPluginStage(const char* phase, const char* name, int idx) {
        char line[320];
        std::snprintf(line, sizeof(line),
            "[NightSharp][Plugin] %s idx=%d name=%s\r\n",
            phase ? phase : "?", idx,
            (name && *name) ? name : "(null)");
        CrashTelemetry::AppendStageLine(line);
    }

    class PluginManager;

    inline PluginManager* g_pluginManagerInstance = nullptr;
    inline alignas(16) unsigned char g_pluginManagerStorage[2048] = {};

    class PluginManager {
    public:
        static PluginManager& Get() {
            if (!g_pluginManagerInstance) {
                g_pluginManagerInstance = ::new(static_cast<void*>(&g_pluginManagerStorage[0])) PluginManager();
            }
            return *g_pluginManagerInstance;
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
            const char* nm = plugin->GetName();
            const int idx = plugin->m_registryIndex;
            SetPluginStage("Load::Enter", nm, idx);
            if (plugin->m_loaded) { SetPluginStage("Load::AlreadyLoaded", nm, idx); return true; }
            SetPluginStage("Load::CanLoad", nm, idx);
            if (!plugin->CanLoad()) { SetPluginStage("Load::CanLoadFalse", nm, idx); return false; }

            SetPluginStage("Load::OnLoad", nm, idx);
            plugin->OnLoad();
            SetPluginStage("Load::RegisterEvents", nm, idx);
            plugin->RegisterEvents();
            plugin->m_loaded = true;
            SetPluginStage("Load::SyncRegistry", nm, idx);
            SyncRegistry(plugin);
            SetPluginStage("Load::Done", nm, idx);
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
            CrashTelemetry::AppendStageLine("[NightSharp][Plugin] LoadAuto::Begin\r\n");
            int loopIdx = 0;
            for (auto& plugin : m_plugins) {
                const char* nm = plugin ? plugin->GetName() : "(null-plugin)";
                const int idx = plugin ? plugin->m_registryIndex : -1;
                SetPluginStage("LoadAuto::Iter", nm, loopIdx);
                LogPluginStage("LoadAuto::Iter", nm, loopIdx);
                if (!plugin) { LogPluginStage("LoadAuto::Skip(null)", nm, loopIdx); ++loopIdx; continue; }
                if (idx < 0) { LogPluginStage("LoadAuto::Skip(idx<0)", nm, loopIdx); ++loopIdx; continue; }
                if (idx >= PluginRegistry::PluginCount) {
                    LogPluginStage("LoadAuto::Skip(idx>=count)", nm, loopIdx);
                    ++loopIdx; continue;
                }

                if (PluginRegistry::Plugins[idx].AlwaysLoad) {
                    LogPluginStage("LoadAuto::Load", nm, idx);
                    if (!Load(plugin.get())) {
                        LogPluginStage("LoadAuto::LoadFailed", nm, idx);
                        SyncRegistry(plugin.get());
                    }
                } else {
                    LogPluginStage("LoadAuto::Unload", nm, idx);
                    Unload(plugin.get());
                }
                LogPluginStage("LoadAuto::IterEnd", nm, loopIdx);
                ++loopIdx;
            }
            CrashTelemetry::AppendStageLine("[NightSharp][Plugin] LoadAuto::End\r\n");
        }

        void ClearAll() {
            CrashTelemetry::AppendStageLine("[NightSharp][Plugin] ClearAll::Begin\r\n");
            m_plugins.clear();
            IPlugin::ResetAll();
            CrashTelemetry::AppendStageLine("[NightSharp][Plugin] ClearAll::End\r\n");
        }

        void UnloadAll() {
            CrashTelemetry::AppendStageLine("[NightSharp][Plugin] UnloadAll::Begin\r\n");
            int loopIdx = 0;
            for (auto& plugin : m_plugins) {
                const char* nm = plugin ? plugin->GetName() : "(null-plugin)";
                const int idx = plugin ? plugin->m_registryIndex : -1;
                if (!plugin) {
                    LogPluginStage("UnloadAll::Skip(null)", nm, loopIdx);
                    ++loopIdx; continue;
                }
                if (!plugin->m_loaded) {
                    LogPluginStage("UnloadAll::Skip(notLoaded)", nm, idx);
                    ++loopIdx; continue;
                }
                LogPluginStage("UnloadAll::Unload::Enter", nm, idx);
                SetPluginStage("UnloadAll::Unload", nm, idx);
                __try {
                    Unload(plugin.get());
                    LogPluginStage("UnloadAll::Unload::OK", nm, idx);
                } __except (EXCEPTION_EXECUTE_HANDLER) {
                    LogPluginStage("UnloadAll::Unload::SEH_CAUGHT", nm, idx);
                    if (plugin) {
                        plugin->m_loaded = false;
                        SyncRegistry(plugin.get());
                    }
                }
                ++loopIdx;
            }
            CrashTelemetry::AppendStageLine("[NightSharp][Plugin] UnloadAll::End\r\n");
        }

        void RefreshRegistryViews() {
            for (auto& plugin : m_plugins) {
                SyncRegistry(plugin.get());
            }
        }

        void OnUpdate() {
            static int s_updateFrame = 0;
            const bool traceFile = (s_updateFrame < 2);
            if (traceFile) CrashTelemetry::AppendStageLine("[NightSharp][Plugin] OnUpdate::Begin\r\n");

            SetPluginStage("OnUpdate::RefreshRegistryViews", "(all)");
            RefreshRegistryViews();

            int loopIdx = 0;
            for (auto& plugin : m_plugins) {
                const char* nm = plugin ? plugin->GetName() : "(null-plugin)";
                SetPluginStage("OnUpdate::Iter", nm, loopIdx);
                if (traceFile) LogPluginStage("OnUpdate::Iter", nm, loopIdx);
                if (!plugin) { ++loopIdx; continue; }

                if (!plugin->m_loaded) {
                    const int idx = plugin->m_registryIndex;
                    if (idx >= 0 && idx < PluginRegistry::PluginCount && PluginRegistry::Plugins[idx].AlwaysLoad) {
                        SetPluginStage("OnUpdate::AutoLoad", nm, idx);
                        if (Load(plugin.get())) { ++loopIdx; continue; }
                    }
                }
                if (plugin->m_loaded && plugin->m_enabled) {
                    SetPluginStage("OnUpdate::Tick", nm, loopIdx);
                    plugin->OnUpdate();
                }
                ++loopIdx;
            }

            if (traceFile) {
                CrashTelemetry::AppendStageLine("[NightSharp][Plugin] OnUpdate::End\r\n");
                ++s_updateFrame;
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
