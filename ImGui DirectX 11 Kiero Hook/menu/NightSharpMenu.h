#pragma once
#include "imgui/imgui.h"
#include "plugins/PluginManager.h"
#include "menu/MenuConfig.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <string>
#include <vector>

// ============================================================================
// NightSharpMenu - Cascading sidebar menu (NightSharp style)
// Core root + dynamic loaded modules
// ============================================================================

namespace NightSharpMenu {

    struct SidebarEntry {
        std::string key;
        std::string label;
        Plugins::IPlugin* plugin = nullptr;
    };

    inline bool showMenu = true;
    inline std::string activeKey = "core";

    // Dragging state
    inline ImVec2 menuPos = ImVec2(20, 40);
    inline bool isDragging = false;
    inline ImVec2 dragOffset = ImVec2(0, 0);

    // Core settings
    inline int languageIndex = 0; // 0=CN, 1=EN
    inline int pluginFilterIndex = 0; // All/Champion/Utility/Core/Misc
    inline bool menuSkinChanger = false; // Placeholder for future module

    // Style
    constexpr float SIDEBAR_W = 190.0f;
    constexpr float PANEL_W = 420.0f;
    constexpr float ITEM_H = 28.0f;

    inline ImU32 COL_BG = IM_COL32(18, 18, 24, 240);
    inline ImU32 COL_HEADER = IM_COL32(30, 30, 42, 255);
    inline ImU32 COL_ITEM = IM_COL32(25, 25, 35, 255);
    inline ImU32 COL_ITEM_HOVER = IM_COL32(45, 40, 70, 255);
    inline ImU32 COL_ITEM_ACTIVE = IM_COL32(60, 50, 95, 255);
    inline ImU32 COL_ACCENT = IM_COL32(100, 200, 100, 255);
    inline ImU32 COL_TEXT = IM_COL32(220, 220, 230, 255);
    inline ImU32 COL_TEXT_DIM = IM_COL32(120, 120, 140, 255);
    inline ImU32 COL_BORDER = IM_COL32(50, 45, 75, 150);

    inline const std::array<const char*, 5> kCorePluginNames = {
        "Orbwalker", "Target Selector", "Prediction", "Activator", "Awareness"
    };

    inline bool IsCorePluginName(const char* name) {
        if (!name) return false;
        for (const char* n : kCorePluginNames) {
            if (strcmp(name, n) == 0) return true;
        }
        return false;
    }

    inline const char* CategoryLabel(Plugins::PluginCategory c) {
        switch (c) {
        case Plugins::PluginCategory::CorePlugin: return "Core Plugin";
        case Plugins::PluginCategory::Champion: return "Champion";
        case Plugins::PluginCategory::Utility: return "Utility";
        case Plugins::PluginCategory::Misc: return "Misc";
        default: return "Misc";
        }
    }

    inline bool MatchPluginFilter(const Plugins::IPlugin* p) {
        if (!p) return false;
        if (pluginFilterIndex == 0) return true; // All
        if (pluginFilterIndex == 1) return p->GetCategory() == Plugins::PluginCategory::Champion;
        if (pluginFilterIndex == 2) return p->GetCategory() == Plugins::PluginCategory::Utility;
        if (pluginFilterIndex == 3) return p->GetCategory() == Plugins::PluginCategory::CorePlugin;
        if (pluginFilterIndex == 4) return p->GetCategory() == Plugins::PluginCategory::Misc;
        return true;
    }

    inline int CategoryRank(Plugins::PluginCategory c) {
        switch (c) {
        case Plugins::PluginCategory::CorePlugin: return 0;
        case Plugins::PluginCategory::Champion: return 1;
        case Plugins::PluginCategory::Utility: return 2;
        case Plugins::PluginCategory::Misc: return 3;
        default: return 3;
        }
    }

    inline bool DrawSidebarItem(ImDrawList* dl, ImVec2 pos, float w,
        const char* text, bool isActive, bool hasArrow) {
        ImVec2 mn = pos, mx = ImVec2(pos.x + w, pos.y + ITEM_H);
        bool hovered = ImGui::IsMouseHoveringRect(mn, mx);
        bool clicked = hovered && ImGui::IsMouseClicked(0);
        dl->AddRectFilled(mn, mx, isActive ? COL_ITEM_ACTIVE : (hovered ? COL_ITEM_HOVER : COL_ITEM));
        if (isActive) dl->AddRectFilled(mn, ImVec2(mn.x + 3, mx.y), COL_ACCENT);
        dl->AddText(ImVec2(pos.x + 12, pos.y + 7), COL_TEXT, text);
        if (hasArrow) dl->AddText(ImVec2(pos.x + w - 18, pos.y + 7), COL_TEXT_DIM, ">");
        return clicked;
    }

    inline std::vector<SidebarEntry> BuildSidebarEntries() {
        std::vector<SidebarEntry> entries;
        entries.push_back({ "core", "Core", nullptr });

        auto& pm = Plugins::PluginManager::Get();

        // Core plugins (ordered)
        for (const char* name : kCorePluginNames) {
            auto* p = pm.Find(name);
            if (p && p->IsLoaded()) {
                entries.push_back({ std::string("plugin:") + name, name, p });
            }
        }

        // Other loaded plugins
        std::vector<Plugins::IPlugin*> others;
        for (auto& up : pm.GetPlugins()) {
            Plugins::IPlugin* p = up.get();
            if (!p || !p->IsLoaded()) continue;
            if (IsCorePluginName(p->GetName())) continue;
            others.push_back(p);
        }

        std::sort(others.begin(), others.end(), [](const Plugins::IPlugin* a, const Plugins::IPlugin* b) {
            int ra = CategoryRank(a->GetCategory());
            int rb = CategoryRank(b->GetCategory());
            if (ra != rb) return ra < rb;
            return strcmp(a->GetName(), b->GetName()) < 0;
            });

        for (auto* p : others) {
            entries.push_back({ std::string("plugin:") + p->GetName(), p->GetName(), p });
        }

        return entries;
    }

    inline void DrawSectionTitle(const char* title) {
        ImGui::TextColored(ImVec4(0.45f, 0.85f, 0.45f, 1.0f), "%s", title);
        ImGui::Separator();
    }

    inline void DrawCorePluginRow(const char* pluginName, bool defaultLoad) {
        auto& pm = Plugins::PluginManager::Get();
        auto* plugin = pm.Find(pluginName);

        ImGui::PushID(pluginName);
        ImGui::TextUnformatted(pluginName);
        ImGui::SameLine(190.0f);

        if (!plugin) {
            ImGui::TextDisabled("Not installed");
            ImGui::PopID();
            return;
        }

        bool loaded = plugin->IsLoaded();
        if (ImGui::Checkbox("##loaded", &loaded)) {
            if (loaded) pm.Load(plugin);
            else pm.Unload(plugin);
        }
        ImGui::SameLine();
        ImGui::TextColored(loaded ? ImVec4(0.35f, 0.9f, 0.35f, 1.0f) : ImVec4(0.8f, 0.6f, 0.25f, 1.0f),
            loaded ? "Loaded" : "Unloaded");

        bool autoLoad = pm.IsAutoLoad(pluginName) || defaultLoad;
        if (defaultLoad && !pm.IsAutoLoad(pluginName)) {
            pm.SetAutoLoad(pluginName, true);
        }

        ImGui::SameLine(320.0f);
        if (defaultLoad) {
            bool dummy = true;
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
            ImGui::Checkbox("Load always", &dummy);
            ImGui::PopStyleVar();
        }
        else {
            if (ImGui::Checkbox("Load always", &autoLoad)) {
                pm.SetAutoLoad(pluginName, autoLoad);
            }
        }

        ImGui::PopID();
    }

    inline void DrawCoreContent() {
        auto& pm = Plugins::PluginManager::Get();

        DrawSectionTitle("Core");
        ImGui::Text("Loaded modules: %d / %d", pm.CountLoaded(), (int)pm.GetPlugins().size());

        if (ImGui::CollapsingHeader("Language", ImGuiTreeNodeFlags_DefaultOpen)) {
            const char* langs[] = { "CN", "EN" };
            ImGui::SetNextItemWidth(120.0f);
            ImGui::Combo("##lang", &languageIndex, langs, IM_ARRAYSIZE(langs));
            ImGui::SameLine();
            ImGui::TextDisabled("(UI language scaffold)");
        }

        if (ImGui::CollapsingHeader("Plugin", ImGuiTreeNodeFlags_DefaultOpen)) {
            const char* filters[] = { "All", "Champion", "Utility", "Core Plugin", "Misc" };
            ImGui::SetNextItemWidth(180.0f);
            ImGui::Combo("Category", &pluginFilterIndex, filters, IM_ARRAYSIZE(filters));

            ImGui::Separator();

            for (auto& up : pm.GetPlugins()) {
                Plugins::IPlugin* p = up.get();
                if (!p) continue;
                if (IsCorePluginName(p->GetName())) continue; // built-in core handled below
                if (!MatchPluginFilter(p)) continue;

                ImGui::PushID(p);

                bool loaded = p->IsLoaded();
                if (ImGui::Checkbox("##load", &loaded)) {
                    if (loaded) pm.Load(p);
                    else pm.Unload(p);
                }
                ImGui::SameLine();

                ImGui::Text("%s", p->GetName());
                ImGui::SameLine(220.0f);
                ImGui::TextDisabled("[%s]", CategoryLabel(p->GetCategory()));

                bool autoLoad = pm.IsAutoLoad(p->GetName());
                ImGui::SameLine(320.0f);
                if (ImGui::Checkbox("Load always", &autoLoad)) {
                    pm.SetAutoLoad(p->GetName(), autoLoad);
                }

                ImGui::PopID();
            }
        }

        if (ImGui::CollapsingHeader("Plugin Core", ImGuiTreeNodeFlags_DefaultOpen)) {
            DrawCorePluginRow("Orbwalker", true);
            DrawCorePluginRow("Target Selector", true);
            DrawCorePluginRow("Prediction", true);
            DrawCorePluginRow("Activator", false);
            DrawCorePluginRow("Awareness", false);
        }

        if (ImGui::CollapsingHeader("Menu", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Checkbox("Skin Changer", &menuSkinChanger);
            ImGui::Checkbox("Zoom Hack", &Config::ZoomHack::enabled);
            ImGui::TextDisabled("Menu modules are scaffolded for future expansion.");
        }
    }

    inline void DrawPluginContent(Plugins::IPlugin* plugin) {
        if (!plugin) {
            ImGui::TextDisabled("Plugin not found.");
            return;
        }

        DrawSectionTitle(plugin->GetName());
        ImGui::Text("Author: %s", plugin->GetAuthor());
        ImGui::Text("Category: %s", CategoryLabel(plugin->GetCategory()));

        auto& pm = Plugins::PluginManager::Get();
        bool loaded = plugin->IsLoaded();
        if (!loaded) {
            ImGui::TextDisabled("This plugin is not loaded.");
            if (ImGui::Button("Load")) pm.Load(plugin);
            return;
        }

        bool enabled = plugin->IsEnabled();
        if (ImGui::Checkbox("Enable", &enabled)) {
            plugin->SetEnabled(enabled);
        }

        ImGui::SameLine();
        if (ImGui::Button("Unload")) {
            pm.Unload(plugin);
            activeKey = "core";
            return;
        }

        if (!enabled) {
            ImGui::TextDisabled("Plugin is loaded but disabled.");
            return;
        }

        ImGui::Separator();
        ImGui::BeginChild("plugin_menu", ImVec2(0, 0), true, ImGuiWindowFlags_None);
        plugin->OnMenu();
        ImGui::EndChild();
    }

    inline void Render() {
        // Only process menu keys when game window is focused
        bool gameFocused = false;
        {
            HWND fg = GetForegroundWindow();
            if (fg) {
                DWORD pid = 0;
                GetWindowThreadProcessId(fg, &pid);
                gameFocused = (pid == GetCurrentProcessId());
            }
        }

        // Toggle: F1 (toggle) or Shift (hold)
        static bool capsWasDown = false;
        bool capsIsDown = gameFocused && (GetAsyncKeyState(VK_F1) & 0x8000) != 0;
        if (capsIsDown && !capsWasDown) showMenu = !showMenu;
        capsWasDown = capsIsDown;

        bool shiftHeld = gameFocused && (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
        bool visible = showMenu || shiftHeld;
        if (!visible) return;

        auto entries = BuildSidebarEntries();
        bool keyFound = false;
        for (auto& e : entries) {
            if (e.key == activeKey) {
                keyFound = true;
                break;
            }
        }
        if (!keyFound) activeKey = "core";

        ImDrawList* dl = ImGui::GetForegroundDrawList();
        if (!dl) return;

        // Handle dragging (title area)
        ImVec2 sidebarPos = menuPos;
        float sidebarH = (float)entries.size() * ITEM_H + ITEM_H + 4;
        ImVec2 titleMin = sidebarPos;
        ImVec2 titleMax = ImVec2(sidebarPos.x + SIDEBAR_W, sidebarPos.y + ITEM_H);

        ImVec2 mousePos = ImGui::GetIO().MousePos;
        bool mouseInTitle = (mousePos.x >= titleMin.x && mousePos.x <= titleMax.x &&
            mousePos.y >= titleMin.y && mousePos.y <= titleMax.y);

        if (ImGui::IsMouseClicked(0) && mouseInTitle) {
            isDragging = true;
            dragOffset = ImVec2(mousePos.x - sidebarPos.x, mousePos.y - sidebarPos.y);
        }
        if (!ImGui::IsMouseDown(0)) {
            isDragging = false;
        }
        if (isDragging) {
            menuPos.x = mousePos.x - dragOffset.x;
            menuPos.y = mousePos.y - dragOffset.y;
            sidebarPos = menuPos;
        }

        // Sidebar
        dl->AddRectFilled(sidebarPos,
            ImVec2(sidebarPos.x + SIDEBAR_W, sidebarPos.y + sidebarH), COL_BG, 4.0f);
        dl->AddRect(sidebarPos,
            ImVec2(sidebarPos.x + SIDEBAR_W, sidebarPos.y + sidebarH), COL_BORDER, 4.0f);

        dl->AddRectFilled(sidebarPos,
            ImVec2(sidebarPos.x + SIDEBAR_W, sidebarPos.y + ITEM_H), COL_HEADER, 4.0f);
        dl->AddText(ImVec2(sidebarPos.x + 10, sidebarPos.y + 7), COL_ACCENT, "NightSharp BGX");
        dl->AddText(ImVec2(sidebarPos.x + SIDEBAR_W - 30, sidebarPos.y + 7), COL_TEXT_DIM, "::");

        float y = sidebarPos.y + ITEM_H + 2;
        for (auto& e : entries) {
            bool isActive = (activeKey == e.key);
            bool hasArrow = true;
            if (DrawSidebarItem(dl, ImVec2(sidebarPos.x, y), SIDEBAR_W, e.label.c_str(), isActive, hasArrow)) {
                activeKey = e.key;
            }
            y += ITEM_H;
        }

        // Content panel
        SidebarEntry* activeEntry = nullptr;
        for (auto& e : entries) {
            if (e.key == activeKey) {
                activeEntry = &e;
                break;
            }
        }
        if (!activeEntry) return;

        ImVec2 panelPos(sidebarPos.x + SIDEBAR_W + 4, sidebarPos.y);

        ImGui::SetNextWindowPos(panelPos, ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(PANEL_W, 0));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.07f, 0.07f, 0.10f, 0.95f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.20f, 0.18f, 0.30f, 0.60f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 4.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 8));

        ImGui::Begin("##nightsharp_content", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize);

        if (activeEntry->key == "core") {
            DrawCoreContent();
        }
        else {
            DrawPluginContent(activeEntry->plugin);
        }

        ImGui::End();
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(2);
    }

} // namespace NightSharpMenu
