#pragma once
#include "../imgui/imgui.h"
#include "../plugins/PluginManager.h"
#include "../sdk/MenuUI.h"
#include "MenuConfig.h"
#include <string>
#include <vector>
#include <functional>

// ============================================================================
// BGXMenu — Cascading sidebar menu (BGX scripting platform style)
// Draggable, toggleable with CapsLock (toggle) or Shift (hold)
// ============================================================================

namespace BGXMenu {

    // State
    inline bool showMenu = true;
    inline int  activeCategory = -1;
    inline int  activeSubCat = -1;

    // Dragging state
    inline ImVec2 menuPos = ImVec2(20, 40);
    inline bool   isDragging = false;
    inline ImVec2 dragOffset = ImVec2(0, 0);

    // Style
    constexpr float SIDEBAR_W = 170.0f;
    constexpr float PANEL_W   = 300.0f;
    constexpr float ITEM_H    = 28.0f;

    inline ImU32 COL_BG         = IM_COL32(18, 18, 24, 240);
    inline ImU32 COL_HEADER     = IM_COL32(30, 30, 42, 255);
    inline ImU32 COL_ITEM       = IM_COL32(25, 25, 35, 255);
    inline ImU32 COL_ITEM_HOVER = IM_COL32(45, 40, 70, 255);
    inline ImU32 COL_ITEM_ACTIVE= IM_COL32(60, 50, 95, 255);
    inline ImU32 COL_ACCENT     = IM_COL32(100, 200, 100, 255);
    inline ImU32 COL_TEXT       = IM_COL32(220, 220, 230, 255);
    inline ImU32 COL_TEXT_DIM   = IM_COL32(120, 120, 140, 255);
    inline ImU32 COL_BORDER     = IM_COL32(50, 45, 75, 150);

    enum CatID {
        CAT_CORE = 0, CAT_ORBWALKER, CAT_TARGET,
        CAT_AWARENESS, CAT_EVADE, CAT_MISC, CAT_DEBUG, CAT_COUNT
    };

    // ====================================================================
    // Sidebar item drawing
    // ====================================================================
    inline bool DrawSidebarItem(ImDrawList* dl, ImVec2 pos, float w,
                                 const char* text, bool isActive, bool hasArrow = false) {
        ImVec2 mn = pos, mx = ImVec2(pos.x + w, pos.y + ITEM_H);
        bool hovered = ImGui::IsMouseHoveringRect(mn, mx);
        bool clicked = hovered && ImGui::IsMouseClicked(0);
        dl->AddRectFilled(mn, mx, isActive ? COL_ITEM_ACTIVE : (hovered ? COL_ITEM_HOVER : COL_ITEM));
        if (isActive) dl->AddRectFilled(mn, ImVec2(mn.x + 3, mx.y), COL_ACCENT);
        dl->AddText(ImVec2(pos.x + 12, pos.y + 7), COL_TEXT, text);
        if (hasArrow) dl->AddText(ImVec2(pos.x + w - 18, pos.y + 7), COL_TEXT_DIM, ">");
        return clicked;
    }

    // ====================================================================
    // Content functions
    // ====================================================================

    inline void DrawCoreContent() {
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1), "Core");
        ImGui::Separator();

        // Language
        if (ImGui::CollapsingHeader("Language")) {
            ImGui::Text("English (default)");
        }

        // Plugins
        if (ImGui::CollapsingHeader("Plugins", ImGuiTreeNodeFlags_DefaultOpen)) {
            auto& pm = Plugins::PluginManager::Get();
            ImGui::Text("Loaded: %d / %d", pm.CountLoaded(), (int)pm.GetPlugins().size());

            for (auto& p : pm.GetPlugins()) {
                ImGui::PushID(p.get());
                if (p->IsLoaded() && p->IsEnabled())
                    ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.3f, 1), "*");
                else if (p->IsLoaded())
                    ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.3f, 1), "*");
                else
                    ImGui::TextColored(ImVec4(0.4f, 0.4f, 0.5f, 1), "o");
                ImGui::SameLine();
                if (p->IsLoaded()) {
                    bool en = p->IsEnabled();
                    ImGui::Checkbox(p->GetName(), &en);
                    p->SetEnabled(en);
                } else {
                    ImGui::TextDisabled("%s", p->GetName());
                }
                ImGui::SameLine(ImGui::GetContentRegionAvail().x - 50);
                if (p->IsLoaded()) {
                    if (ImGui::SmallButton("Unload")) pm.Unload(p.get());
                } else {
                    if (ImGui::SmallButton("Load")) pm.Load(p.get());
                }
                ImGui::PopID();
            }
        }

        // Core Plugins info
        if (ImGui::CollapsingHeader("Core Plugins")) {
            ImGui::TextDisabled("Orbwalker -> 'Orbwalker' tab");
            ImGui::TextDisabled("Target Selector -> 'Target' tab");
            ImGui::TextDisabled("Awareness -> 'Awareness' tab");
        }

        // Script Menus (SDK::MenuUI menus registered by scripts)
        auto& globalMenus = SDK::MenuUI::Menu::GetGlobalMenus();
        if (!globalMenus.empty()) {
            if (ImGui::CollapsingHeader("Script Menus")) {
                for (auto& menu : globalMenus) {
                    ImGui::PushID(menu.get());
                    if (ImGui::TreeNode(menu->DisplayName.c_str())) {
                        menu->Draw();
                        ImGui::TreePop();
                    }
                    ImGui::PopID();
                }
            }
        }
    }

    inline void DrawOrbwalkerContent() {
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1), "Orbwalker");
        ImGui::Separator();
        auto* plugin = Plugins::PluginManager::Get().Find("Orbwalker");
        if (plugin && plugin->IsLoaded()) {
            bool en = plugin->IsEnabled();
            ImGui::Checkbox("Enable Orbwalker", &en);
            plugin->SetEnabled(en);
            if (en) { ImGui::Separator(); plugin->OnMenu(); }
        } else {
            ImGui::TextDisabled("Not loaded");
            if (plugin && ImGui::Button("Load")) Plugins::PluginManager::Get().Load(plugin);
        }
    }

    inline void DrawTargetContent() {
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1), "Target Selector");
        ImGui::Separator();
        auto* plugin = Plugins::PluginManager::Get().Find("Target Selector");
        if (plugin && plugin->IsLoaded()) {
            bool en = plugin->IsEnabled();
            ImGui::Checkbox("Enable Target Selector", &en);
            plugin->SetEnabled(en);
            if (en) { ImGui::Separator(); plugin->OnMenu(); }
        } else {
            ImGui::TextDisabled("Not loaded");
            if (plugin && ImGui::Button("Load")) Plugins::PluginManager::Get().Load(plugin);
        }
    }

    inline void DrawAwarenessContent() {
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1), "Awareness");
        ImGui::Separator();
        auto* plugin = Plugins::PluginManager::Get().Find("Awareness");
        if (plugin && plugin->IsLoaded()) {
            bool en = plugin->IsEnabled();
            ImGui::Checkbox("Enable Awareness", &en);
            plugin->SetEnabled(en);
            if (en) { ImGui::Separator(); plugin->OnMenu(); }
        } else {
            ImGui::TextDisabled("Not loaded");
            if (plugin && ImGui::Button("Load")) Plugins::PluginManager::Get().Load(plugin);
        }
    }

    inline void DrawDebugContent() {
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1), "Debug");
        ImGui::Separator();
        ImGui::Text("Base: 0x%llX", (unsigned long long)Globals::base);
        uintptr_t lp = Globals::Read<uintptr_t>(Globals::base + Offset::Global::LocalPlayer);
        ImGui::Text("Player: 0x%llX", (unsigned long long)lp);
        if (Globals::IsValidPtr(lp)) {
            Vec3 pos = Globals::Read<Vec3>(lp + Offset::GameObject::Position);
            ImGui::Text("Pos: (%.0f, %.0f, %.0f)", pos.x, pos.y, pos.z);
            float hp = Globals::Read<float>(lp + Offset::Health::HP);
            float mhp = Globals::Read<float>(lp + Offset::Health::MaxHP);
            ImGui::Text("HP: %.0f / %.0f", hp, mhp);
            float ms = Globals::Read<float>(lp + Offset::HeroStats::MoveSpeed);
            ImGui::Text("MS: %.0f", ms);
        }
        float gt = Globals::Read<float>(Globals::base + Offset::Global::GameTime);
        if (gt > 0 && gt < 100000) ImGui::Text("Time: %d:%02d", (int)gt/60, (int)gt%60);
        ImGui::Separator();
        ImGui::Text("E:%d A:%d T:%d J:%d M:%d",
            (int)SDK::GameObjects::EnemyHeroes.size(),
            (int)SDK::GameObjects::AllyHeroes.size(),
            (int)SDK::GameObjects::Turrets.size(),
            (int)SDK::GameObjects::JungleMinions.size(),
            (int)SDK::GameObjects::EnemyMinions.size());

        ImGui::Separator();
        ImGui::Text("SDK MenuUI Menus: %d", (int)SDK::MenuUI::Menu::GetGlobalMenus().size());
    }

    // ====================================================================
    // Main Render
    // ====================================================================
    inline void Render() {
        // ---- Toggle: CapsLock (toggle) or Shift (hold) ----
        static bool capsWasDown = false;
        bool capsIsDown = (GetAsyncKeyState(VK_CAPITAL) & 0x8000) != 0;
        if (capsIsDown && !capsWasDown) showMenu = !showMenu;
        capsWasDown = capsIsDown;

        // Shift hold — show while held
        bool shiftHeld = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
        bool visible = showMenu || shiftHeld;

        if (!visible) return;

        ImDrawList* dl = ImGui::GetForegroundDrawList();
        if (!dl) return;

        // ---- Handle dragging (title bar area) ----
        ImVec2 sidebarPos = menuPos;
        float sidebarH = CAT_COUNT * ITEM_H + ITEM_H + 4;
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

        // ---- Sidebar ----
        dl->AddRectFilled(sidebarPos,
            ImVec2(sidebarPos.x + SIDEBAR_W, sidebarPos.y + sidebarH), COL_BG, 4.0f);
        dl->AddRect(sidebarPos,
            ImVec2(sidebarPos.x + SIDEBAR_W, sidebarPos.y + sidebarH), COL_BORDER, 4.0f);

        // Title (EnsoulSharp heritage: L# -> E# -> NS#)
        dl->AddRectFilled(sidebarPos,
            ImVec2(sidebarPos.x + SIDEBAR_W, sidebarPos.y + ITEM_H), COL_HEADER, 4.0f);
        dl->AddText(ImVec2(sidebarPos.x + 10, sidebarPos.y + 7), COL_ACCENT, "NightSharp v1.0");

        // Drag hint
        dl->AddText(ImVec2(sidebarPos.x + SIDEBAR_W - 30, sidebarPos.y + 7), COL_TEXT_DIM, "::");

        float y = sidebarPos.y + ITEM_H + 2;
        const char* names[] = { "Core","Orbwalker","Target","Awareness","Evade","Misc","Debug" };
        bool arrows[] = { true, true, true, true, false, false, false };

        for (int i = 0; i < CAT_COUNT; i++) {
            if (DrawSidebarItem(dl, ImVec2(sidebarPos.x, y), SIDEBAR_W,
                    names[i], activeCategory == i, arrows[i])) {
                activeCategory = (activeCategory == i) ? -1 : i;
            }
            y += ITEM_H;
        }

        // ---- Content Panel ----
        if (activeCategory < 0) return;

        ImVec2 panelPos(sidebarPos.x + SIDEBAR_W + 4,
                        sidebarPos.y);

        ImGui::SetNextWindowPos(panelPos, ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(PANEL_W, 0)); // auto-height
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.07f, 0.07f, 0.10f, 0.95f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.20f, 0.18f, 0.30f, 0.60f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 4.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 8));

        char id[32]; snprintf(id, sizeof(id), "##p%d", activeCategory);
        ImGui::Begin(id, nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize);

        switch (activeCategory) {
        case CAT_CORE:      DrawCoreContent(); break;
        case CAT_ORBWALKER: DrawOrbwalkerContent(); break;
        case CAT_TARGET:    DrawTargetContent(); break;
        case CAT_AWARENESS: DrawAwarenessContent(); break;
        case CAT_EVADE:
            ImGui::TextColored(ImVec4(0.4f,0.8f,0.4f,1), "Evade");
            ImGui::Separator();
            ImGui::Checkbox("Enable", &Config::Evade::enabled);
            ImGui::Checkbox("Dodge Skillshots", &Config::Evade::dodgeSkillshots);
            break;
        case CAT_MISC:
            ImGui::TextColored(ImVec4(0.4f,0.8f,0.4f,1), "Misc");
            ImGui::Separator();
            ImGui::Checkbox("Show FPS", &Config::Misc::showFPS);
            ImGui::Checkbox("Show Game Time", &Config::Misc::showGameTime);
            ImGui::Checkbox("Anti-AFK", &Config::Misc::antiAFK);
            break;
        case CAT_DEBUG: DrawDebugContent(); break;
        }

        ImGui::End();
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(2);
    }

} // namespace BGXMenu
