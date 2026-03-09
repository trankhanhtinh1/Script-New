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
            if (!plugin || plugin->m_loaded || !plugin->CanLoad()) return false;
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
            DrawMenuInternal(nullptr);
        }

        void OnMenu(PluginCategory category) {
            DrawMenuInternal(&category);
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
        static const char* GetCategoryName(PluginCategory category) {
            switch (category) {
            case PluginCategory::CorePlugin: return "Core Plugin";
            case PluginCategory::Champion:   return "Champion";
            case PluginCategory::Utility:    return "Utility";
            case PluginCategory::Misc:       return "Misc";
            default:                         return "Unknown";
            }
        }

        void DrawPluginEntry(IPlugin* plugin) {
            ImGui::PushID(plugin);

            bool loaded = plugin->m_loaded;
            if (ImGui::Checkbox("##load", &loaded)) {
                if (loaded) Load(plugin);
                else Unload(plugin);
            }
            ImGui::SameLine();

            if (plugin->m_loaded) {
                bool enabled = plugin->m_enabled;
                ImGui::Checkbox(plugin->GetName(), &enabled);
                plugin->m_enabled = enabled;

                ImGui::SameLine();
                ImGui::TextDisabled("(%s)", plugin->GetAuthor());
                if (const char* requiredChampion = plugin->GetRequiredChampion()) {
                    ImGui::SameLine();
                    ImGui::TextDisabled("[%s]", requiredChampion);
                }

                if (plugin->m_enabled) {
                    ImGui::Indent(20.0f);
                    plugin->OnMenu();
                    ImGui::Unindent(20.0f);
                }
            } else {
                ImGui::TextDisabled("%s", plugin->GetName());
                ImGui::SameLine();
                ImGui::TextDisabled("(%s) [not loaded]", plugin->GetAuthor());
                if (const char* requiredChampion = plugin->GetRequiredChampion()) {
                    ImGui::SameLine();
                    ImGui::TextDisabled("[%s]", requiredChampion);
                }
            }

            ImGui::PopID();
        }

        void DrawMenuInternal(const PluginCategory* filter) {
            ImGui::Text("Loaded Plugins: %d / %d",
                CountLoaded(), (int)m_plugins.size());
            ImGui::Separator();

            if (filter != nullptr) {
                ImGui::TextDisabled("%s", GetCategoryName(*filter));
                ImGui::Separator();

                bool hasAny = false;
                for (auto& p : m_plugins) {
                    if (!p->ShouldAppearInMenu() || p->GetCategory() != *filter) {
                        continue;
                    }

                    hasAny = true;
                    DrawPluginEntry(p.get());
                    ImGui::Spacing();
                    ImGui::Separator();
                }

                if (!hasAny) {
                    ImGui::TextDisabled("No plugins registered in this category.");
                }
                return;
            }

            // Group by category
            const char* catNames[] = { "Core Plugin", "Champion", "Utility", "Misc" };

            for (int cat = 0; cat < 4; cat++) {
                auto category = (PluginCategory)cat;
                bool hasAny = false;
                for (auto& p : m_plugins)
                    if (p->ShouldAppearInMenu() && p->GetCategory() == category) { hasAny = true; break; }
                if (!hasAny) continue;

                if (ImGui::CollapsingHeader(catNames[cat], ImGuiTreeNodeFlags_DefaultOpen)) {
                    for (auto& p : m_plugins) {
                        if (!p->ShouldAppearInMenu()) continue;
                        if (p->GetCategory() != category) continue;
                        DrawPluginEntry(p.get());
                    }
                }
            }
        }
        PluginManager() = default;
        std::vector<std::unique_ptr<IPlugin>> m_plugins;
        std::unordered_set<std::string> m_autoLoadNames;
    };

} // namespace Plugins
