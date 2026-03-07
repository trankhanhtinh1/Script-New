#pragma once
#include "IPlugin.h"
#include "imgui/imgui.h"
#include <vector>
#include <memory>
#include <algorithm>
#include <unordered_set>
#include <string>

// ============================================================================
// PluginManager — Manages plugin lifecycle, loading, and rendering
// Plugins are registered at startup, user chooses which to load via menu
// ============================================================================

namespace Plugins {

    class PluginManager {
    public:
        // ====================================================================
        // Singleton
        // ====================================================================
        static PluginManager& Get() {
            static PluginManager instance;
            return instance;
        }

        // ====================================================================
        // Registration — call at startup to register available plugins
        // ====================================================================
        template<typename T, typename... Args>
        T* Register(Args&&... args) {
            auto plugin = std::make_unique<T>(std::forward<Args>(args)...);
            T* ptr = plugin.get();
            m_plugins.push_back(std::move(plugin));
            return ptr;
        }

        // ====================================================================
        // Load / Unload
        // ====================================================================
        bool Load(IPlugin* plugin) {
            if (!plugin || plugin->m_loaded) return false;
            plugin->OnLoad();
            plugin->m_loaded = true;
            return true;
        }

        bool Unload(IPlugin* plugin) {
            if (!plugin || !plugin->m_loaded) return false;
            plugin->OnUnload();
            plugin->m_loaded = false;
            return true;
        }

        void LoadAll() {
            for (auto& p : m_plugins) Load(p.get());
        }

        void LoadAuto() {
            for (auto& p : m_plugins) {
                if (IsAutoLoad(p->GetName()))
                    Load(p.get());
            }
        }

        void UnloadAll() {
            for (auto& p : m_plugins) Unload(p.get());
        }

        bool LoadByName(const char* name) {
            IPlugin* p = Find(name);
            return p ? Load(p) : false;
        }

        bool UnloadByName(const char* name) {
            IPlugin* p = Find(name);
            return p ? Unload(p) : false;
        }

        // ====================================================================
        // Per-frame — call from main render loop
        // ====================================================================
        void OnUpdate() {
            for (auto& p : m_plugins) {
                if (p->m_loaded && p->m_enabled)
                    p->OnUpdate();
            }
        }

        void OnRender() {
            for (auto& p : m_plugins) {
                if (p->m_loaded && p->m_enabled)
                    p->OnRender();
            }
        }

        // ====================================================================
        // Menu — Draw plugin management UI
        // ====================================================================
        void OnMenu() {
            ImGui::Text("Loaded Plugins: %d / %d",
                CountLoaded(), (int)m_plugins.size());
            ImGui::Separator();

            // Group by category
            const char* catNames[] = { "Core Plugin", "Champion", "Utility", "Misc" };

            for (int cat = 0; cat < 4; cat++) {
                auto category = (PluginCategory)cat;
                bool hasAny = false;
                for (auto& p : m_plugins)
                    if (p->GetCategory() == category) { hasAny = true; break; }
                if (!hasAny) continue;

                if (ImGui::CollapsingHeader(catNames[cat], ImGuiTreeNodeFlags_DefaultOpen)) {
                    for (auto& p : m_plugins) {
                        if (p->GetCategory() != category) continue;

                        ImGui::PushID(p.get());

                        // Load/Unload toggle
                        bool loaded = p->m_loaded;
                        if (ImGui::Checkbox("##load", &loaded)) {
                            if (loaded) Load(p.get());
                            else Unload(p.get());
                        }
                        ImGui::SameLine();

                        // Enable/Disable toggle (only if loaded)
                        if (p->m_loaded) {
                            bool enabled = p->m_enabled;
                            ImGui::Checkbox(p->GetName(), &enabled);
                            p->m_enabled = enabled;

                            // Show author
                            ImGui::SameLine();
                            ImGui::TextDisabled("(%s)", p->GetAuthor());

                            // Plugin-specific menu (collapsible)
                            if (p->m_enabled) {
                                ImGui::Indent(20.0f);
                                p->OnMenu();
                                ImGui::Unindent(20.0f);
                            }
                        } else {
                            ImGui::TextDisabled("%s", p->GetName());
                            ImGui::SameLine();
                            ImGui::TextDisabled("(%s) [not loaded]", p->GetAuthor());
                        }

                        ImGui::PopID();
                    }
                }
            }
        }

        // ====================================================================
        // Accessors
        // ====================================================================
        const std::vector<std::unique_ptr<IPlugin>>& GetPlugins() const { return m_plugins; }

        int CountLoaded() const {
            int n = 0;
            for (auto& p : m_plugins) if (p->m_loaded) n++;
            return n;
        }

        void SetAutoLoad(const std::string& pluginName, bool enabled) {
            if (enabled) m_autoLoadNames.insert(pluginName);
            else m_autoLoadNames.erase(pluginName);
        }

        bool IsAutoLoad(const std::string& pluginName) const {
            return m_autoLoadNames.find(pluginName) != m_autoLoadNames.end();
        }

        // Find plugin by name
        IPlugin* Find(const char* name) {
            for (auto& p : m_plugins)
                if (strcmp(p->GetName(), name) == 0) return p.get();
            return nullptr;
        }

    private:
        PluginManager() = default;
        std::vector<std::unique_ptr<IPlugin>> m_plugins;
        std::unordered_set<std::string> m_autoLoadNames;
    };

} // namespace Plugins
