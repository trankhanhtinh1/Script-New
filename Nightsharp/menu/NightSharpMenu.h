#pragma once
#include "imgui/imgui.h"
#include "plugins/PluginManager.h"
#include "menu/MenuConfig.h"
#include "sdk/UI/MenuUI.h"
#include "sdk/Wrappers/Orbwalking/Orbwalker.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

// ============================================================================
// NightSharpMenu - Three-level sliderbar shell
// Layout: Sliderbar 1 -> Sliderbar 2 -> Sliderbar 3
// ============================================================================

namespace NightSharpMenu {

    struct SidebarEntry {
        std::string key;
        std::string label;
        Plugins::IPlugin* plugin = nullptr;
    };

    struct SecondaryEntry {
        std::string key;
        std::string label;
    };

    inline bool showMenu = true;
    inline std::string activePrimaryKey = "";
    inline std::unordered_map<std::string, std::string> activeSecondaryByPrimary;
    inline bool primarySelected = false;   // true once user clicks a primary item
    inline bool secondarySelected = false; // true once user clicks a secondary item
    inline std::string hookBackendLabel = "Unknown";

    inline ImVec2 menuPos = ImVec2(20, 40);
    inline bool isDragging = false;
    inline ImVec2 dragOffset = ImVec2(0, 0);

    inline bool menuSkinChanger = false;

    enum class MenuLanguage {
        EN,
        CN,
        VN
    };

    inline MenuLanguage currentLanguage = MenuLanguage::EN;

    constexpr float PRIMARY_W = 190.0f;
    constexpr float SECONDARY_W = 190.0f;
    constexpr float CONTENT_W = 560.0f;
    constexpr float ITEM_H = 30.0f;
    constexpr float HEADER_H = 32.0f;
    constexpr float PANEL_GAP = 6.0f;
    constexpr float MIN_CONTENT_H = 420.0f;
    constexpr float MAX_CONTENT_H = 620.0f;

    inline ImU32 COL_BG = IM_COL32(8, 10, 18, 214);
    inline ImU32 COL_CONTENT_BG = IM_COL32(8, 10, 18, 128);
    inline ImU32 COL_HEADER = IM_COL32(16, 18, 28, 236);
    inline ImU32 COL_ITEM = IM_COL32(18, 20, 30, 118);
    inline ImU32 COL_ITEM_HOVER = IM_COL32(52, 48, 82, 215);
    inline ImU32 COL_ITEM_ACTIVE = IM_COL32(82, 66, 132, 232);
    inline ImU32 COL_ACCENT = IM_COL32(120, 235, 120, 255);
    inline ImU32 COL_TEXT = IM_COL32(242, 242, 248, 255);
    inline ImU32 COL_TEXT_DIM = IM_COL32(148, 148, 168, 255);
    inline ImU32 COL_BORDER = IM_COL32(88, 100, 148, 180);

    inline const std::array<const char*, 4> kCorePluginNames = {
        "Orbwalker 2.0", "Target Selector", "Activator", "Awareness"
    };

    inline void SetHookBackendLabel(const char* label) {
        hookBackendLabel = (label && *label) ? label : "Unknown";
    }

    inline Plugins::IPlugin* GetLoadedOrbwalkerPlugin() {
        auto* plugin = Plugins::PluginManager::Get().Find("Orbwalker 2.0");
        if (plugin && plugin->IsLoaded()) {
            return plugin;
        }
        return nullptr;
    }

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
        const char* text, bool isActive, bool hasArrow = false) {
        ImVec2 mn = pos;
        ImVec2 mx = ImVec2(pos.x + w, pos.y + ITEM_H);
        bool hovered = ImGui::IsMouseHoveringRect(mn, mx, false); // false = don't clip to current window
        bool clicked = hovered && ImGui::IsMouseClicked(0);
        if (hovered || isActive) {
            dl->AddRectFilled(mn, mx, isActive ? COL_ITEM_ACTIVE : COL_ITEM_HOVER, 3.0f);
        }
        if (isActive) {
            dl->AddLine(ImVec2(mn.x + 1.0f, mn.y + 2.0f), ImVec2(mn.x + 1.0f, mx.y - 2.0f), COL_ACCENT, 2.0f);
        }
        dl->AddLine(ImVec2(mn.x, mx.y), ImVec2(mx.x, mx.y), COL_BORDER, 1.0f);
        dl->AddText(ImVec2(pos.x + 12.0f, pos.y + 7.0f), COL_TEXT, text);
        if (hasArrow) {
            dl->AddText(ImVec2(pos.x + w - 18.0f, pos.y + 7.0f), COL_TEXT_DIM, ">");
        }
        return clicked;
    }

    inline bool DrawStateButton(const char* id, const char* label, bool active, bool positive, float width = 44.0f) {
        ImVec4 activeBase = positive ? ImVec4(0.18f, 0.55f, 0.28f, 0.98f) : ImVec4(0.65f, 0.22f, 0.24f, 0.98f);
        ImVec4 activeHover = positive ? ImVec4(0.22f, 0.64f, 0.32f, 1.0f) : ImVec4(0.75f, 0.27f, 0.29f, 1.0f);
        ImVec4 activePressed = positive ? ImVec4(0.15f, 0.48f, 0.24f, 1.0f) : ImVec4(0.58f, 0.18f, 0.20f, 1.0f);
        ImVec4 inactiveBase = ImVec4(0.14f, 0.16f, 0.24f, 0.98f);
        ImVec4 inactiveHover = ImVec4(0.20f, 0.24f, 0.34f, 1.0f);
        ImVec4 inactivePressed = ImVec4(0.24f, 0.28f, 0.40f, 1.0f);
        ImVec4 text = active ? ImVec4(0.96f, 0.97f, 1.0f, 1.0f) : ImVec4(0.78f, 0.81f, 0.90f, 1.0f);

        ImGui::PushID(id);
        ImGui::PushStyleColor(ImGuiCol_Button, active ? activeBase : inactiveBase);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, active ? activeHover : inactiveHover);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, active ? activePressed : inactivePressed);
        ImGui::PushStyleColor(ImGuiCol_Text, text);
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 7.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
        bool clicked = ImGui::Button(label, ImVec2(width, 0.0f));
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(5);
        ImGui::PopID();
        return clicked;
    }

    inline bool DrawOnOffEditor(const char* label, bool& value, const char* id) {
        bool changed = false;
        ImGui::PushID(id);
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(label);
        ImGui::SameLine();

        float targetX = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - 96.0f;
        if (targetX > ImGui::GetCursorPosX()) {
            ImGui::SetCursorPosX(targetX);
        }

        if (DrawStateButton("on", "On", value, true) && !value) {
            value = true;
            changed = true;
        }
        ImGui::SameLine(0.0f, 6.0f);
        if (DrawStateButton("off", "Off", !value, false) && value) {
            value = false;
            changed = true;
        }
        ImGui::PopID();
        return changed;
    }

    inline std::vector<SidebarEntry> BuildPrimaryEntries() {
        std::vector<SidebarEntry> entries;
        entries.push_back({ "core", "Core", nullptr });

        auto& pm = Plugins::PluginManager::Get();
        entries.push_back({ "orbwalker", "Orbwalker 2.0", GetLoadedOrbwalkerPlugin() });

        for (const char* name : kCorePluginNames) {
            if (strcmp(name, "Orbwalker 2.0") == 0) {
                continue;
            }
            auto* p = pm.Find(name);
            if (p && p->IsLoaded()) {
                entries.push_back({ std::string("plugin:") + name, name, p });
            }
        }

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

    inline std::vector<SecondaryEntry> BuildCoreSections() {
        return {
            { "language", "Language" },
            { "core_plugins", "Core Plugin" },
            { "plugins", "Plugin" },
            { "menu", "Menu" }
        };
    }

    inline std::vector<SecondaryEntry> BuildSecondaryEntries(const SidebarEntry& primary) {
        if (primary.key == "core") {
            return BuildCoreSections();
        }
        if (primary.key == "orbwalker") {
            std::vector<SecondaryEntry> sections;
            if (auto* plugin = GetLoadedOrbwalkerPlugin()) {
                if (auto* menuRoot = plugin->GetMenuRoot()) {
                    for (const auto& s : menuRoot->GetRootSections()) {
                        sections.push_back({ s.first, s.second });
                    }
                }
            }
            if (sections.empty()) {
                sections.push_back({ "__sdk_orbwalker", "Orbwalker" });
            }
            return sections;
        }

        std::vector<SecondaryEntry> sections;
        if (primary.plugin) {
            if (auto* menuRoot = primary.plugin->GetMenuRoot()) {
                for (const auto& s : menuRoot->GetRootSections()) {
                    sections.push_back({ s.first, s.second });
                }
            }
        }

        if (sections.empty()) {
            sections.push_back({ "__plugin_general", "General" });
        }
        return sections;
    }

    inline const SidebarEntry* FindPrimaryEntry(const std::vector<SidebarEntry>& entries, const std::string& key) {
        for (const auto& e : entries) {
            if (e.key == key) {
                return &e;
            }
        }
        return nullptr;
    }

    inline const SecondaryEntry* FindSecondaryEntry(const std::vector<SecondaryEntry>& entries, const std::string& key) {
        for (const auto& e : entries) {
            if (e.key == key) {
                return &e;
            }
        }
        return nullptr;
    }

    inline const char* EnsureActiveSecondary(const std::string& primaryKey,
        const std::vector<SecondaryEntry>& sections) {
        if (sections.empty()) {
            activeSecondaryByPrimary.erase(primaryKey);
            return "";
        }

        std::string& active = activeSecondaryByPrimary[primaryKey];
        bool found = false;
        for (const auto& s : sections) {
            if (s.key == active) {
                found = true;
                break;
            }
        }
        if (!found) {
            active = sections.front().key;
        }
        return active.c_str();
    }

    inline void DrawSectionTitle(const char* title) {
        ImGui::TextColored(ImVec4(0.45f, 0.85f, 0.45f, 1.0f), "%s", title);
        ImGui::Separator();
    }

    inline bool SetPluginLoaded(Plugins::IPlugin* plugin, bool shouldLoad) {
        if (!plugin) {
            return false;
        }

        auto& pm = Plugins::PluginManager::Get();
        pm.SetAutoLoad(plugin->GetName(), shouldLoad);

        bool changed = shouldLoad ? pm.Load(plugin) : pm.Unload(plugin);
        if (!shouldLoad && activePrimaryKey == std::string("plugin:") + plugin->GetName()) {
            activePrimaryKey = "core";
        }
        return changed;
    }

    inline void DrawPluginLoadRow(Plugins::IPlugin* plugin) {
        if (!plugin) {
            return;
        }

        bool loaded = plugin->IsLoaded();
        bool changed = DrawOnOffEditor(plugin->GetName(), loaded, plugin->GetName());
        if (changed) {
            SetPluginLoaded(plugin, loaded);
        }

        // "Always Load" checkbox
        ImGui::SameLine(0.0f, 12.0f);
        bool autoLoad = Plugins::PluginManager::Get().IsAutoLoad(plugin->GetName());
        std::string alId = std::string("##autoload_") + plugin->GetName();
        if (ImGui::Checkbox(("Always Load" + alId).c_str(), &autoLoad)) {
            Plugins::PluginManager::Get().SetAutoLoad(plugin->GetName(), autoLoad);
        }
    }

    inline void DrawLanguageSelector(const char* label, MenuLanguage value) {
        ImGui::PushID(label);
        if (DrawStateButton(label, label, currentLanguage == value, true, 56.0f)) {
            currentLanguage = value;
        }
        ImGui::PopID();
    }

    inline void DrawLanguageSection() {
        DrawSectionTitle("Language");
        DrawLanguageSelector("EN", MenuLanguage::EN);
        ImGui::SameLine(0.0f, 8.0f);
        DrawLanguageSelector("CN", MenuLanguage::CN);
        ImGui::SameLine(0.0f, 8.0f);
        DrawLanguageSelector("VN", MenuLanguage::VN);
    }

    inline void DrawOrbwalkerSection() {
        DrawSectionTitle("Orbwalker");
        if (auto* menu = SDK::Orbwalker::GetMenuRoot()) {
            for (const auto& item : menu->GetItems()) {
                if (item) {
                    item->Draw();
                }
            }
            return;
        }
        ImGui::TextDisabled("Orbwalker menu is not initialized yet.");
    }

    inline void DrawCorePluginsSection() {
        DrawSectionTitle("Core Plugin");
        auto& pm = Plugins::PluginManager::Get();
        for (const char* name : kCorePluginNames) {
            if (auto* plugin = pm.Find(name)) {
                DrawPluginLoadRow(plugin);
                ImGui::Separator();
            }
        }
    }

    inline void DrawPluginCategorySection(Plugins::PluginCategory category, const char* title) {
        auto& pm = Plugins::PluginManager::Get();
        bool hasAny = false;

        for (auto& up : pm.GetPlugins()) {
            auto* plugin = up.get();
            if (!plugin || plugin->GetCategory() != category || IsCorePluginName(plugin->GetName())) {
                continue;
            }
            hasAny = true;
        }

        if (!hasAny) {
            return;
        }

        ImGui::TextColored(ImVec4(0.94f, 0.95f, 0.98f, 1.0f), "%s", title);
        ImGui::Separator();

        for (auto& up : pm.GetPlugins()) {
            auto* plugin = up.get();
            if (!plugin || plugin->GetCategory() != category || IsCorePluginName(plugin->GetName())) {
                continue;
            }
            DrawPluginLoadRow(plugin);
            ImGui::Separator();
        }
        ImGui::Spacing();
    }

    inline void DrawPluginsSection() {
        DrawSectionTitle("Plugin");
        DrawPluginCategorySection(Plugins::PluginCategory::Champion, "Champion");
        DrawPluginCategorySection(Plugins::PluginCategory::Utility, "Utility");
        DrawPluginCategorySection(Plugins::PluginCategory::Misc, "Misc");
    }

    inline void DrawMenuSection() {
        DrawSectionTitle("Menu");
        DrawOnOffEditor("Skin Changer", menuSkinChanger, "menu_skin");
        DrawOnOffEditor("Zoom Hack", Config::ZoomHack::enabled, "zoom_hack");
        DrawOnOffEditor("Bypass OBS", Config::StreamProtection::bypassObs, "bypass_obs");
        ImGui::Separator();
        ImGui::TextDisabled("Bypass OBS: overlay hidden from screen capture (Win10 2004+)");
    }

    inline void DrawCoreSectionContent(const std::string& sectionKey) {
        if (sectionKey == "language") {
            DrawLanguageSection();
        } else if (sectionKey == "core_plugins") {
            DrawCorePluginsSection();
        } else if (sectionKey == "plugins") {
            DrawPluginsSection();
        } else if (sectionKey == "menu") {
            DrawMenuSection();
        } else {
            DrawLanguageSection();
        }
    }

    inline int CountPluginsInCategory(Plugins::PluginCategory category) {
        int count = 0;
        for (const auto& up : Plugins::PluginManager::Get().GetPlugins()) {
            auto* plugin = up.get();
            if (!plugin || plugin->GetCategory() != category || IsCorePluginName(plugin->GetName())) {
                continue;
            }
            ++count;
        }
        return count;
    }

    inline int EstimateCoreSectionRows(const std::string& sectionKey) {
        if (sectionKey == "language") {
            return 2;
        }

        if (sectionKey == "core_plugins") {
            int rows = 1;
            auto& pm = Plugins::PluginManager::Get();
            for (const char* name : kCorePluginNames) {
                if (pm.Find(name)) {
                    ++rows;
                }
            }
            return std::max(rows, 2);
        }

        if (sectionKey == "plugins") {
            int rows = 1;
            for (auto category : { Plugins::PluginCategory::Champion, Plugins::PluginCategory::Utility, Plugins::PluginCategory::Misc }) {
                int count = CountPluginsInCategory(category);
                if (count > 0) {
                    rows += 1 + count;
                }
            }
            return std::max(rows, 2);
        }

        if (sectionKey == "menu") {
            return 5;
        }

        return 3;
    }

    inline int EstimatePluginSectionRows(Plugins::IPlugin* plugin, const std::string& sectionKey) {
        if (!plugin) {
            return 2;
        }

        if (auto* menuRoot = plugin->GetMenuRoot()) {
            return menuRoot->EstimateRootSectionRowCount(sectionKey);
        }

        return 6;
    }

    inline int EstimateOrbwalkerSectionRows(const std::string& sectionKey) {
        if (auto* plugin = GetLoadedOrbwalkerPlugin()) {
            return EstimatePluginSectionRows(plugin, sectionKey);
        }
        if (auto* menu = SDK::Orbwalker::GetMenuRoot()) {
            return std::max(menu->EstimateRowCount(), 10);
        }
        return 10;
    }

    inline void DrawPluginSectionContent(Plugins::IPlugin* plugin, const std::string& sectionKey) {
        if (!plugin) {
            ImGui::TextDisabled("Plugin not found.");
            return;
        }

        if (auto* menuRoot = plugin->GetMenuRoot()) {
            menuRoot->DrawRootSection(sectionKey);
            return;
        }

        plugin->OnMenu();
    }

    inline void DrawOrbwalkerPrimaryContent(const std::string& sectionKey) {
        if (auto* plugin = GetLoadedOrbwalkerPlugin()) {
            DrawPluginSectionContent(plugin, sectionKey);
            return;
        }
        DrawOrbwalkerSection();
    }

    inline void Render() {
        bool gameFocused = false;
        HWND fg = GetForegroundWindow();
        if (fg) {
            DWORD pid = 0;
            GetWindowThreadProcessId(fg, &pid);
            gameFocused = (pid == GetCurrentProcessId());
        }

        static bool f1WasDown = false;
        bool f1IsDown = gameFocused && (GetAsyncKeyState(VK_F1) & 0x8000) != 0;
        if (f1IsDown && !f1WasDown) {
            showMenu = !showMenu;
        }
        f1WasDown = f1IsDown;

        bool shiftHeld = gameFocused && (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
        bool visible = showMenu || shiftHeld;
        if (!visible) {
            return;
        }

        auto primaryEntries = BuildPrimaryEntries();
        if (!activePrimaryKey.empty() && !FindPrimaryEntry(primaryEntries, activePrimaryKey)) {
            activePrimaryKey = "";
            primarySelected = false;
            secondarySelected = false;
        }

        // Determine which panels are visible
        bool showSecondary = primarySelected && !activePrimaryKey.empty();
        bool showContent = showSecondary && secondarySelected;

        const SidebarEntry* activePrimary = nullptr;
        std::vector<SecondaryEntry> secondaryEntries;
        const char* activeSecondary = "";
        const SecondaryEntry* activeSecondaryEntry = nullptr;

        if (showSecondary) {
            activePrimary = FindPrimaryEntry(primaryEntries, activePrimaryKey);
            if (!activePrimary) {
                showSecondary = false;
                showContent = false;
            }
        }

        if (showSecondary && activePrimary) {
            secondaryEntries = BuildSecondaryEntries(*activePrimary);
            activeSecondary = EnsureActiveSecondary(activePrimaryKey, secondaryEntries);
            activeSecondaryEntry = FindSecondaryEntry(secondaryEntries, activeSecondary);
        }

        const float primaryHeight = HEADER_H + ITEM_H * static_cast<float>(std::max<size_t>(1, primaryEntries.size())) + 4.0f;
        const float secondaryHeight = showSecondary ? (HEADER_H + ITEM_H * static_cast<float>(std::max<size_t>(1, secondaryEntries.size())) + 4.0f) : 0.0f;
        const float sidebarHeight = std::max(primaryHeight, secondaryHeight > 0 ? secondaryHeight : primaryHeight);

        // Content panel: use generous fixed height with scrollbar for overflow
        const float contentHeight = std::max(sidebarHeight, MAX_CONTENT_H);

        // Calculate title bar width based on visible panels
        float totalMenuWidth = PRIMARY_W;
        if (showSecondary) totalMenuWidth += PANEL_GAP + SECONDARY_W;
        if (showContent) totalMenuWidth += PANEL_GAP + CONTENT_W;

        ImVec2 mousePos = ImGui::GetIO().MousePos;
        ImVec2 titleMin = menuPos;
        ImVec2 titleMax = ImVec2(menuPos.x + totalMenuWidth, menuPos.y + HEADER_H);
        bool mouseInTitle = (mousePos.x >= titleMin.x && mousePos.x <= titleMax.x &&
            mousePos.y >= titleMin.y && mousePos.y <= titleMax.y);

        // Only start dragging if no ImGui window is hovered/active (fixes click-through issues)
        if (ImGui::IsMouseClicked(0) && mouseInTitle && !ImGui::IsAnyItemHovered() && !ImGui::IsAnyItemActive()) {
            isDragging = true;
            dragOffset = ImVec2(mousePos.x - menuPos.x, mousePos.y - menuPos.y);
        }
        if (!ImGui::IsMouseDown(0)) {
            isDragging = false;
        }
        if (isDragging) {
            menuPos.x = mousePos.x - dragOffset.x;
            menuPos.y = mousePos.y - dragOffset.y;
        }

        ImDrawList* dl = ImGui::GetForegroundDrawList();
        if (!dl) {
            return;
        }

        const ImVec2 primaryPos = menuPos;
        const ImVec2 secondaryPos(menuPos.x + PRIMARY_W + PANEL_GAP, menuPos.y);
        const ImVec2 contentPos(menuPos.x + PRIMARY_W + SECONDARY_W + PANEL_GAP * 2.0f, menuPos.y);

        // Draw Primary sidebar (always visible)
        {
            dl->AddRectFilled(primaryPos, ImVec2(primaryPos.x + PRIMARY_W, primaryPos.y + sidebarHeight), COL_BG, 4.0f);
            dl->AddRectFilled(primaryPos, ImVec2(primaryPos.x + PRIMARY_W, primaryPos.y + HEADER_H), COL_HEADER, 4.0f);
            dl->AddRect(primaryPos, ImVec2(primaryPos.x + PRIMARY_W, primaryPos.y + sidebarHeight), COL_BORDER, 4.0f);
            dl->AddText(ImVec2(primaryPos.x + 10.0f, primaryPos.y + 8.0f), IM_COL32(120, 235, 120, 255), "NightSharp");
            dl->AddLine(ImVec2(primaryPos.x, primaryPos.y + HEADER_H), ImVec2(primaryPos.x + PRIMARY_W, primaryPos.y + HEADER_H), COL_BORDER, 1.0f);

            float y = primaryPos.y + HEADER_H + 2.0f;
            for (const auto& entry : primaryEntries) {
                if (DrawSidebarItem(dl, ImVec2(primaryPos.x, y), PRIMARY_W, entry.label.c_str(), activePrimaryKey == entry.key, true)) {
                    if (activePrimaryKey != entry.key) {
                        activePrimaryKey = entry.key;
                        secondarySelected = false; // Reset secondary when primary changes
                    }
                    primarySelected = true;
                }
                y += ITEM_H;
            }
        }

        // Recalculate after potential click changes
        if (primarySelected && !activePrimaryKey.empty()) {
            activePrimary = FindPrimaryEntry(primaryEntries, activePrimaryKey);
            if (activePrimary) {
                secondaryEntries = BuildSecondaryEntries(*activePrimary);
                activeSecondary = EnsureActiveSecondary(activePrimaryKey, secondaryEntries);
                activeSecondaryEntry = FindSecondaryEntry(secondaryEntries, activeSecondary);
                showSecondary = true;
            }
        }

        if (!showSecondary || !activePrimary) {
            return;
        }

        // Determine if we should collapse secondary+content into one panel
        const bool collapseOrbwalkerToSecondary =
            (activePrimary->key == "orbwalker" && secondaryEntries.size() <= 1);
        const bool collapseToTwoPanel = (!collapseOrbwalkerToSecondary && secondaryEntries.size() <= 1);

        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.96f, 0.99f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 4.0f));

        if (collapseOrbwalkerToSecondary || collapseToTwoPanel) {
            // 2-panel layout: Primary + merged content
            const float mergedWidth = SECONDARY_W + PANEL_GAP + CONTENT_W;
            const float mergedHeight = std::max(sidebarHeight, contentHeight);

            dl->AddRectFilled(secondaryPos, ImVec2(secondaryPos.x + mergedWidth, secondaryPos.y + mergedHeight), COL_CONTENT_BG, 4.0f);
            dl->AddRectFilled(secondaryPos, ImVec2(secondaryPos.x + mergedWidth, secondaryPos.y + HEADER_H), COL_HEADER, 4.0f);
            dl->AddRect(secondaryPos, ImVec2(secondaryPos.x + mergedWidth, secondaryPos.y + mergedHeight), COL_BORDER, 4.0f);
            dl->AddText(ImVec2(secondaryPos.x + 10.0f, secondaryPos.y + 8.0f), COL_ACCENT,
                activeSecondaryEntry ? activeSecondaryEntry->label.c_str() : activePrimary->label.c_str());
            dl->AddLine(ImVec2(secondaryPos.x, secondaryPos.y + HEADER_H), ImVec2(secondaryPos.x + mergedWidth, secondaryPos.y + HEADER_H), COL_BORDER, 1.0f);

            ImGui::SetNextWindowPos(ImVec2(secondaryPos.x, secondaryPos.y + HEADER_H), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(mergedWidth, mergedHeight - HEADER_H), ImGuiCond_Always);
            ImGui::Begin("##nightsharp_secondary_content", nullptr,
                ImGuiWindowFlags_NoTitleBar |
                ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoMove |
                ImGuiWindowFlags_NoSavedSettings |
                ImGuiWindowFlags_NoCollapse |
                ImGuiWindowFlags_NoBackground);
            ImGui::BeginChild("##nightsharp_secondary_scroll", ImVec2(0.0f, 0.0f), false, ImGuiWindowFlags_AlwaysVerticalScrollbar);
            if (activePrimary->key == "orbwalker") {
                DrawOrbwalkerPrimaryContent(activeSecondary);
            } else if (activePrimary->key == "core") {
                DrawCoreSectionContent(activeSecondary);
            } else {
                DrawPluginSectionContent(activePrimary->plugin, activeSecondary);
            }
            ImGui::EndChild();
            ImGui::End();
        } else {
            // 3-panel layout: Primary + Secondary + Content
            // Draw Secondary sidebar
            {
                dl->AddRectFilled(secondaryPos, ImVec2(secondaryPos.x + SECONDARY_W, secondaryPos.y + sidebarHeight), COL_BG, 4.0f);
                dl->AddRectFilled(secondaryPos, ImVec2(secondaryPos.x + SECONDARY_W, secondaryPos.y + HEADER_H), COL_HEADER, 4.0f);
                dl->AddRect(secondaryPos, ImVec2(secondaryPos.x + SECONDARY_W, secondaryPos.y + sidebarHeight), COL_BORDER, 4.0f);
                dl->AddText(ImVec2(secondaryPos.x + 10.0f, secondaryPos.y + 8.0f), IM_COL32(120, 235, 120, 255), activePrimary->label.c_str());
                dl->AddLine(ImVec2(secondaryPos.x, secondaryPos.y + HEADER_H), ImVec2(secondaryPos.x + SECONDARY_W, secondaryPos.y + HEADER_H), COL_BORDER, 1.0f);

                float y = secondaryPos.y + HEADER_H + 2.0f;
                for (const auto& entry : secondaryEntries) {
                    if (DrawSidebarItem(dl, ImVec2(secondaryPos.x, y), SECONDARY_W, entry.label.c_str(), activeSecondaryByPrimary[activePrimaryKey] == entry.key, true)) {
                        activeSecondaryByPrimary[activePrimaryKey] = entry.key;
                        secondarySelected = true;
                    }
                    y += ITEM_H;
                }
            }

            // Draw Content panel (only if secondary is selected)
            if (secondarySelected) {
                // Refresh active secondary after potential click
                activeSecondary = EnsureActiveSecondary(activePrimaryKey, secondaryEntries);

                dl->AddRectFilled(contentPos, ImVec2(contentPos.x + CONTENT_W, contentPos.y + contentHeight), COL_CONTENT_BG, 4.0f);
                dl->AddRectFilled(contentPos, ImVec2(contentPos.x + CONTENT_W, contentPos.y + HEADER_H), COL_HEADER, 4.0f);
                dl->AddRect(contentPos, ImVec2(contentPos.x + CONTENT_W, contentPos.y + contentHeight), COL_BORDER, 4.0f);
                activeSecondaryEntry = FindSecondaryEntry(secondaryEntries, activeSecondary);
                dl->AddText(ImVec2(contentPos.x + 10.0f, contentPos.y + 8.0f), COL_ACCENT,
                    activeSecondaryEntry ? activeSecondaryEntry->label.c_str() : activeSecondary);
                dl->AddLine(ImVec2(contentPos.x, contentPos.y + HEADER_H), ImVec2(contentPos.x + CONTENT_W, contentPos.y + HEADER_H), COL_BORDER, 1.0f);

                ImGui::SetNextWindowPos(ImVec2(contentPos.x, contentPos.y + HEADER_H), ImGuiCond_Always);
                ImGui::SetNextWindowSize(ImVec2(CONTENT_W, contentHeight - HEADER_H), ImGuiCond_Always);
                ImGui::Begin("##nightsharp_content", nullptr,
                    ImGuiWindowFlags_NoTitleBar |
                    ImGuiWindowFlags_NoResize |
                    ImGuiWindowFlags_NoMove |
                    ImGuiWindowFlags_NoSavedSettings |
                    ImGuiWindowFlags_NoCollapse |
                    ImGuiWindowFlags_NoBackground);
                ImGui::BeginChild("##nightsharp_content_scroll", ImVec2(0.0f, 0.0f), false, ImGuiWindowFlags_AlwaysVerticalScrollbar);

                if (activePrimary->key == "core") {
                    DrawCoreSectionContent(activeSecondary);
                } else if (activePrimary->key == "orbwalker") {
                    DrawOrbwalkerPrimaryContent(activeSecondary);
                } else {
                    DrawPluginSectionContent(activePrimary->plugin, activeSecondary);
                }

                ImGui::EndChild();
                ImGui::End();
            }
        }

        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(3);
    }

} // namespace NightSharpMenu
