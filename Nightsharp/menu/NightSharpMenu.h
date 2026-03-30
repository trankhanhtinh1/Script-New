#pragma once
/*
 * NightSharp v2.0 — Three-level Sidebar Menu (CRT-Free)
 *
 * Rewritten from old NightSharp menu to eliminate all CRT dependencies:
 *   - No std::string, std::vector, std::array
 *   - Fixed C arrays with const char* labels
 *   - Integer indices instead of string keys
 *   - All state as POD types (zero-initialized, no dynamic init)
 *
 * Visual layout preserved: 3-panel sidebar with dark theme.
 * Text made bolder per user request.
 *
 * Plugin Manager: SDK Plugins tab = plugin manager UI (Load/Unload + Always Load).
 * Loaded plugins appear as primary sidebar categories below Core.
 */

#include "../imgui/imgui.h"
#include "MenuUI.h"
#include "MenuConfig.h"

#include "PluginRegistry.h"
#include "SDKDiagnostics.h"

namespace NightSharpMenu {

    // ============================================================================
    // State — POD only, zero-initialized (safe without _initterm)
    // ============================================================================
    inline bool showMenu          = true;
    inline int  activePrimaryIdx  = -1;
    inline int  activeSecondaryIdx = -1;
    inline bool primarySelected   = false;
    inline bool secondarySelected = false;

    // Plugin sidebar state — which plugin's secondary panel is active
    inline int  activePluginIdx   = -1;     // index into PluginRegistry::Plugins[]
    inline int  activePluginSecIdx = -1;    // secondary item within plugin menu

    inline float menuPosX   = 20.0f;
    inline float menuPosY   = 40.0f;
    inline bool  isDragging = false;
    inline float dragOffX   = 0.0f;
    inline float dragOffY   = 0.0f;

    // Explicit menu bounding rect for WM_NCHITTEST (updated each frame)
    inline float menuBoundsRight  = 0.0f;
    inline float menuBoundsBottom = 0.0f;

    // ============================================================================
    // Layout constants
    // ============================================================================
    constexpr float PRIMARY_W   = 190.0f;
    constexpr float SECONDARY_W = 190.0f;
    constexpr float CONTENT_W   = 560.0f;
    constexpr float ITEM_H      = 30.0f;
    constexpr float HEADER_H    = 32.0f;
    constexpr float PANEL_GAP   = 6.0f;
    constexpr float MAX_CONTENT_H = 620.0f;

    // ============================================================================
    // Color scheme (dark theme with green accent)
    // ============================================================================
    inline ImU32 COL_BG          = IM_COL32(8, 10, 18, 214);
    inline ImU32 COL_CONTENT_BG  = IM_COL32(8, 10, 18, 128);
    inline ImU32 COL_HEADER      = IM_COL32(16, 18, 28, 236);
    inline ImU32 COL_ITEM        = IM_COL32(18, 20, 30, 118);
    inline ImU32 COL_ITEM_HOVER  = IM_COL32(52, 48, 82, 215);
    inline ImU32 COL_ITEM_ACTIVE = IM_COL32(82, 66, 132, 232);
    inline ImU32 COL_ACCENT      = IM_COL32(120, 235, 120, 255);
    inline ImU32 COL_TEXT        = IM_COL32(255, 255, 255, 255);
    inline ImU32 COL_TEXT_DIM    = IM_COL32(185, 185, 205, 255);
    inline ImU32 COL_BORDER      = IM_COL32(88, 100, 148, 180);

    // ============================================================================
    // Entry structures — POD, const char* only
    // ============================================================================
    struct SidebarEntry { const char* label; };

    // ============================================================================
    // Core secondary entries (fixed)
    // ============================================================================
    constexpr int CORE_SECONDARY_COUNT = 6;
    inline const SidebarEntry CORE_SECONDARY[CORE_SECONDARY_COUNT] = {
        { "Language" }, { "Menu" }, { "Debug Info" }, { "SDK Diagnostics" }, { "SDK Plugins" }, { "Plugins" },
    };

    // ============================================================================
    // Drawing helpers
    // ============================================================================
    inline bool DrawSidebarItem(ImDrawList* dl, ImVec2 pos, float w,
                                const char* text, bool isActive, bool hasArrow = false)
    {
        ImVec2 mn = pos;
        ImVec2 mx = ImVec2(pos.x + w, pos.y + ITEM_H);
        bool hovered = ImGui::IsMouseHoveringRect(mn, mx, false);
        bool clicked = hovered && ImGui::IsMouseClicked(0);

        if (hovered || isActive)
            dl->AddRectFilled(mn, mx, isActive ? COL_ITEM_ACTIVE : COL_ITEM_HOVER, 3.0f);
        if (isActive)
            dl->AddLine(ImVec2(mn.x + 1, mn.y + 2), ImVec2(mn.x + 1, mx.y - 2), COL_ACCENT, 2.0f);
        dl->AddLine(ImVec2(mn.x, mx.y), ImVec2(mx.x, mx.y), COL_BORDER, 1.0f);
        dl->AddText(ImVec2(pos.x + 12, pos.y + 7), COL_TEXT, text);
        if (hasArrow)
            dl->AddText(ImVec2(pos.x + w - 18, pos.y + 7), COL_TEXT_DIM, ">");

        return clicked;
    }

    inline bool DrawStateButton(const char* id, const char* label, bool active, bool positive, float width = 44.0f)
    {
        ImVec4 activeBase   = positive ? ImVec4(0.18f,0.55f,0.28f,0.98f) : ImVec4(0.65f,0.22f,0.24f,0.98f);
        ImVec4 activeHover  = positive ? ImVec4(0.22f,0.64f,0.32f,1.0f)  : ImVec4(0.75f,0.27f,0.29f,1.0f);
        ImVec4 activePress  = positive ? ImVec4(0.15f,0.48f,0.24f,1.0f)  : ImVec4(0.58f,0.18f,0.20f,1.0f);
        ImVec4 inactiveBase = ImVec4(0.14f,0.16f,0.24f,0.98f);
        ImVec4 inactiveHov  = ImVec4(0.20f,0.24f,0.34f,1.0f);
        ImVec4 inactivePr   = ImVec4(0.24f,0.28f,0.40f,1.0f);
        ImVec4 text = active ? ImVec4(0.96f,0.97f,1.0f,1.0f) : ImVec4(0.78f,0.81f,0.90f,1.0f);

        ImGui::PushID(id);
        ImGui::PushStyleColor(ImGuiCol_Button,        active ? activeBase  : inactiveBase);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,  active ? activeHover : inactiveHov);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,   active ? activePress : inactivePr);
        ImGui::PushStyleColor(ImGuiCol_Text, text);
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0,0,0,0));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 7.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
        bool clicked = ImGui::Button(label, ImVec2(width, 0));
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(5);
        ImGui::PopID();
        return clicked;
    }

    inline bool DrawOnOffEditor(const char* label, bool& value, const char* id)
    {
        bool changed = false;
        ImGui::PushID(id);
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(label);
        ImGui::SameLine();
        float targetX = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - 96.0f;
        if (targetX > ImGui::GetCursorPosX())
            ImGui::SetCursorPosX(targetX);
        if (DrawStateButton("on", "On", value, true) && !value)   { value = true;  changed = true; }
        ImGui::SameLine(0, 6);
        if (DrawStateButton("off", "Off", !value, false) && value) { value = false; changed = true; }
        ImGui::PopID();
        return changed;
    }

    inline void DrawSectionTitle(const char* title) {
        ImGui::TextColored(ImVec4(0.47f,0.92f,0.47f,1.0f), "%s", title);
        ImGui::Separator();
    }

    // ============================================================================
    // Content sections — Core
    // ============================================================================
    inline void DrawMenuSection() {
        DrawSectionTitle("Menu Settings");
        static bool skinChanger = false;
        DrawOnOffEditor("Skin Changer", skinChanger, "menu_skin");
        DrawOnOffEditor("Zoom Hack", Config::ZoomHack::enabled, "zoom_hack");
        DrawOnOffEditor("Bypass OBS", Config::StreamProtection::bypassObs, "bypass_obs");
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.5f,0.5f,0.6f,1.0f), "Bypass OBS: overlay hidden from screen capture");
        ImGui::TextColored(ImVec4(0.5f,0.5f,0.6f,1.0f), "(requires Win10 2004+)");
    }

    inline void DrawDebugSection() {
        DrawSectionTitle("Debug Info");
        ImGui::Text("NightSharp v2.0");
        ImGui::Text("Overlay: D3D11 External");
        ImGui::Text("Injection: ManualMap APC");
        ImGui::Text("CRT: Bypassed (HeapAlloc)");
        ImGui::Separator();
        ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
        ImGui::Text("Frame: %.3f ms", 1000.0f / ImGui::GetIO().Framerate);
    }

    inline void DrawLanguageSection() {
        DrawSectionTitle("Language");
        static int lang = 0;
        const char* langs[] = { "EN", "CN", "VN" };
        for (int i = 0; i < 3; i++) {
            ImGui::PushID(langs[i]);
            if (DrawStateButton(langs[i], langs[i], lang == i, true, 56.0f))
                lang = i;
            ImGui::PopID();
            if (i < 2) ImGui::SameLine(0, 8);
        }
    }

    inline void DrawPluginManagerRows(PluginRegistry::PluginKind kind, const char* emptyText, int idBase = 0) {
        int pluginCount = PluginRegistry::GetCountByKind(kind);
        if (pluginCount == 0) {
            if (emptyText && emptyText[0]) {
                ImGui::TextColored(ImVec4(0.5f,0.5f,0.6f,1.0f), "%s", emptyText);
            }
            return;
        }

        for (int i = 0; i < PluginRegistry::PluginCount; i++) {
            auto& p = PluginRegistry::Plugins[i];
            if (p.Kind != kind) continue;
            if (!p.Name) continue;
            const bool hasRuntime = PluginRegistry::HasRuntime(i);

            ImGui::PushID(i + idBase);
            ImGui::BeginGroup();

            ImVec4 statusColor;
            const char* statusText;
            if (!p.MenuRoot) {
                statusColor = ImVec4(0.9f, 0.6f, 0.1f, 1.0f);
                statusText = "[!!]";
            } else if (p.Loaded) {
                statusColor = ImVec4(0.3f, 0.9f, 0.3f, 1.0f);
                statusText = "[ON]";
            } else {
                statusColor = ImVec4(0.6f, 0.6f, 0.6f, 0.8f);
                statusText = "[--]";
            }
            ImGui::TextColored(statusColor, "%s", statusText);
            ImGui::SameLine(0, 8);
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "%s", p.Name);

            if (!p.MenuRoot) {
                ImGui::SameLine(0, 8);
                ImGui::TextColored(ImVec4(0.9f, 0.6f, 0.1f, 0.9f),
                    hasRuntime ? "(no menu)" : "(menu init failed)");
            }

            ImGui::SameLine();
            float rightEdge = ImGui::GetContentRegionAvail().x;
            float buttonW = 70.0f;
            float checkW = 120.0f;
            float targetX = ImGui::GetCursorPosX() + rightEdge - buttonW - checkW - 16.0f;
            if (targetX > ImGui::GetCursorPosX())
                ImGui::SetCursorPosX(targetX);

            if (p.MenuRoot || hasRuntime) {
                if (p.Loaded) {
                    if (DrawStateButton("unload", "Unload", true, false, buttonW))
                        PluginRegistry::UnloadPlugin(i);
                } else {
                    if (DrawStateButton("load", "Load", false, true, buttonW))
                        PluginRegistry::LoadPlugin(i);
                }
            } else {
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 7.0f);
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f,0.2f,0.2f,0.5f));
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f,0.5f,0.5f,0.8f));
                ImGui::Button("Error", ImVec2(buttonW, 0));
                ImGui::PopStyleColor(2);
                ImGui::PopStyleVar();
            }

            ImGui::SameLine(0, 8);
            bool al = p.AlwaysLoad;
            if (ImGui::Checkbox("Always Load", &al)) {
                PluginRegistry::SetAlwaysLoad(i, al);
            }

            ImGui::EndGroup();
            ImGui::Separator();
            ImGui::PopID();
        }
    }

    // ============================================================================
    // SDK Plugins tab — PLUGIN MANAGER UI
    // Hiện danh sách plugin với Load/Unload + Always Load
    // ============================================================================
    inline void DrawSDKPluginsSection() {
        DrawSectionTitle("SDK Plugin Manager");
        ImGui::Spacing();
        int sdkCount = PluginRegistry::GetCountByKind(PluginRegistry::PluginKind::SDK);
        if (sdkCount == 0) {
            ImGui::TextColored(ImVec4(0.5f,0.5f,0.6f,1.0f), "No SDK plugins registered.");
            return;
        }

        for (int i = 0; i < PluginRegistry::PluginCount; i++) {
            auto& p = PluginRegistry::Plugins[i];
            if (p.Kind != PluginRegistry::PluginKind::SDK) continue;
            if (!p.Name) continue;
            const bool hasRuntime = PluginRegistry::HasRuntime(i);

            ImGui::PushID(i);

            // Plugin row: [icon] Name    [Load/Unload] [☑ Always Load]
            ImGui::BeginGroup();

            // Status indicator
            ImVec4 statusColor;
            const char* statusText;
            if (!p.MenuRoot) {
                statusColor = ImVec4(0.9f, 0.6f, 0.1f, 1.0f);  // Orange = no menu
                statusText = "[!!]";
            } else if (p.Loaded) {
                statusColor = ImVec4(0.3f, 0.9f, 0.3f, 1.0f);  // Green = loaded
                statusText = "[ON]";
            } else {
                statusColor = ImVec4(0.6f, 0.6f, 0.6f, 0.8f);  // Gray = unloaded
                statusText = "[--]";
            }
            ImGui::TextColored(statusColor, "%s", statusText);
            ImGui::SameLine(0, 8);

            // Plugin name
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "%s", p.Name);

            // Show warning if menu creation failed
            if (!p.MenuRoot) {
                ImGui::SameLine(0, 8);
                ImGui::TextColored(ImVec4(0.9f, 0.6f, 0.1f, 0.9f),
                    hasRuntime ? "(no menu)" : "(menu init failed)");
            }

            ImGui::SameLine();

            // Push to right side
            float rightEdge = ImGui::GetContentRegionAvail().x;
            float buttonW = 70.0f;
            float checkW = 120.0f;
            float targetX = ImGui::GetCursorPosX() + rightEdge - buttonW - checkW - 16.0f;
            if (targetX > ImGui::GetCursorPosX())
                ImGui::SetCursorPosX(targetX);

            // Load/Unload button (works even without MenuRoot — just controls visibility)
            if (p.MenuRoot || hasRuntime) {
                if (p.Loaded) {
                    if (DrawStateButton("unload", "Unload", true, false, buttonW))
                        PluginRegistry::UnloadPlugin(i);
                } else {
                    if (DrawStateButton("load", "Load", false, true, buttonW))
                        PluginRegistry::LoadPlugin(i);
                }
            } else {
                // Disabled button for plugins without menu
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 7.0f);
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f,0.2f,0.2f,0.5f));
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f,0.5f,0.5f,0.8f));
                ImGui::Button("Error", ImVec2(buttonW, 0));
                ImGui::PopStyleColor(2);
                ImGui::PopStyleVar();
            }

            ImGui::SameLine(0, 8);

            // Always Load checkbox — persisted to plugins.ini
            bool al = p.AlwaysLoad;
            if (ImGui::Checkbox("Always Load", &al)) {
                PluginRegistry::SetAlwaysLoad(i, al);
            }

            ImGui::EndGroup();

            ImGui::Separator();
            ImGui::PopID();
        }
    }

    inline void DrawPluginsSection() {
        DrawSectionTitle("Plugin Manager");
        ImGui::Spacing();
        DrawPluginManagerRows(PluginRegistry::PluginKind::Plugin, "No source plugins registered.");

        int extCount = PluginRegistry::GetCountByKind(PluginRegistry::PluginKind::External);
        if (extCount == 0) {
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.5f,0.5f,0.6f,1.0f), "No external plugins loaded.");
            ImGui::TextColored(ImVec4(0.5f,0.5f,0.6f,1.0f), "Drop plugin DLLs into /plugins/ folder.");
            return;
        }

        // Tương tự SDK plugins - sẽ implement sau
        for (int i = 0; i < PluginRegistry::PluginCount; i++) {
            auto& p = PluginRegistry::Plugins[i];
            if (p.Kind != PluginRegistry::PluginKind::External) continue;
            if (!p.Name) continue;

            ImGui::PushID(i + 100);
            ImGui::TextColored(ImVec4(1,1,1,1), "%s", p.Name);
            ImGui::PopID();
        }
    }

    // ============================================================================
    // Core content panel dispatch
    // ============================================================================
    inline void DrawCoreContentPanel(int secondaryIdx) {
        if (secondaryIdx == 0)      DrawLanguageSection();
        else if (secondaryIdx == 1) DrawMenuSection();
        else if (secondaryIdx == 2) DrawDebugSection();
        else if (secondaryIdx == 3) SDKDiagnostics::Render();
        else if (secondaryIdx == 4) DrawSDKPluginsSection();
        else if (secondaryIdx == 5) DrawPluginsSection();
        else                        DrawLanguageSection();
    }

    // ============================================================================
    // Plugin menu helpers — build virtual secondary entries
    //
    // Orbwalker menu structure:
    //   orbwalker (Menu_)
    //   ├── drawings (Menu)        → secondary "Drawings"
    //   ├── advanced (Menu)        → secondary "Advanced"
    //   ├── separatorKeys (Sep)    ┐
    //   ├── lasthitKey (KeyBind)   │→ grouped into synthetic "General" entry
    //   ├── laneclearKey (KeyBind) │
    //   ├── comboKey (KeyBind)     │
    //   └── enabledOption (Bool)   ┘
    //
    // Secondary sidebar shows: Drawings, Advanced, General
    // "General" contains all non-Menu root children
    // ============================================================================

    // Get virtual secondary count using new MenuUI API
    inline int GetPluginSecondaryCount(int pluginIdx) {
        auto& p = PluginRegistry::Plugins[pluginIdx];
        if (!p.MenuRoot) return 0;
        return (int)p.MenuRoot->GetRootSections().size();
    }

    // Get label for virtual secondary entry idx
    inline const char* GetPluginSecondaryLabel(int pluginIdx, int virtualIdx) {
        auto& p = PluginRegistry::Plugins[pluginIdx];
        if (!p.MenuRoot) return "?";
        auto sections = p.MenuRoot->GetRootSections();
        if (virtualIdx >= 0 && virtualIdx < sections.size()) {
            // Need a static/thread-local buffer to return const char* safely without allocations
            static char s_labelBuffer[256];
            strncpy_s(s_labelBuffer, sizeof(s_labelBuffer), sections[virtualIdx].second.c_str(), _TRUNCATE);
            return s_labelBuffer;
        }
        return "?";
    }

    // ============================================================================
    // Plugin content panel — renders plugin's SDK menu subtree
    // ============================================================================
    inline void DrawPluginContentPanel(int pluginIdx, int secIdx) {
        if (pluginIdx < 0 || pluginIdx >= PluginRegistry::PluginCount) return;
        auto& p = PluginRegistry::Plugins[pluginIdx];
        if (!p.MenuRoot || !p.Loaded) return;

        auto sections = p.MenuRoot->GetRootSections();
        if (secIdx >= 0 && secIdx < sections.size()) {
            p.MenuRoot->DrawRootSection(sections[secIdx].first);
        } else {
            // Fallback - draw whole root
            p.MenuRoot->Draw();
        }
    }

    // ============================================================================
    // Utility
    // ============================================================================
    inline float MaxF(float a, float b) { return a > b ? a : b; }
    inline int MaxI(int a, int b) { return a > b ? a : b; }
    inline bool IsPrimaryPluginEntry(const PluginRegistry::PluginEntry& p) {
        if (!p.Name || !p.Loaded || !p.MenuRoot) {
            return false;
        }
        return p.Kind == PluginRegistry::PluginKind::SDK ||
               p.Kind == PluginRegistry::PluginKind::Plugin ||
               p.Kind == PluginRegistry::PluginKind::External;
    }

    // ============================================================================
    // Primary panel entry count: "Core" + loaded plugins
    // ============================================================================
    inline int GetPrimaryCount() {
        int count = 1;
        for (int i = 0; i < PluginRegistry::PluginCount; i++) {
            if (IsPrimaryPluginEntry(PluginRegistry::Plugins[i])) {
                count++;
            }
        }
        return count;
    }

    // ============================================================================
    // Main render — called each ImGui frame
    // ============================================================================
    inline void Render() {
        if (!showMenu) return;

        // ── Build dynamic primary entries ──
        // [0] = Core, [1..N] = loaded SDK plugins
        constexpr int MAX_PRIMARY = 17;  // 1 Core + 16 plugins max
        const char* primaryLabels[MAX_PRIMARY];
        int primaryPluginMap[MAX_PRIMARY];  // maps primary index → plugin registry index (-1 for Core)
        int primaryCount = 0;

        primaryLabels[0] = "Core";
        primaryPluginMap[0] = -1;  // Core
        primaryCount = 1;

        for (int i = 0; i < PluginRegistry::PluginCount; i++) {
            auto& p = PluginRegistry::Plugins[i];
            if (IsPrimaryPluginEntry(p)) {
                if (primaryCount < MAX_PRIMARY) {
                    primaryLabels[primaryCount] = p.Name;
                    primaryPluginMap[primaryCount] = i;
                    primaryCount++;
                }
            }
        }

        bool showSecondary = primarySelected && activePrimaryIdx >= 0;
        bool showContent   = showSecondary && secondarySelected;

        float totalW = PRIMARY_W;
        if (showSecondary) totalW += PANEL_GAP + SECONDARY_W;
        if (showContent)   totalW += PANEL_GAP + CONTENT_W;

        // ---- Drag handling ----
        ImVec2 mouse = ImGui::GetIO().MousePos;
        float titleMaxX = menuPosX + totalW;
        float titleMaxY = menuPosY + HEADER_H;
        bool inTitle = mouse.x >= menuPosX && mouse.x <= titleMaxX
                    && mouse.y >= menuPosY && mouse.y <= titleMaxY;

        if (ImGui::IsMouseClicked(0) && inTitle && !ImGui::IsAnyItemHovered() && !ImGui::IsAnyItemActive()) {
            isDragging = true;
            dragOffX = mouse.x - menuPosX;
            dragOffY = mouse.y - menuPosY;
        }
        if (!ImGui::IsMouseDown(0)) isDragging = false;
        if (isDragging) {
            menuPosX = mouse.x - dragOffX;
            menuPosY = mouse.y - dragOffY;
        }

        ImDrawList* dl = ImGui::GetForegroundDrawList();
        if (!dl) return;

        // ---- Positions ----
        ImVec2 primaryPos   = ImVec2(menuPosX, menuPosY);
        ImVec2 secondaryPos = ImVec2(menuPosX + PRIMARY_W + PANEL_GAP, menuPosY);
        ImVec2 contentPos   = ImVec2(menuPosX + PRIMARY_W + SECONDARY_W + PANEL_GAP * 2, menuPosY);

        // ── Secondary count depends on active primary ──
        int secCount = 0;
        if (showSecondary && activePrimaryIdx >= 0 && activePrimaryIdx < primaryCount) {
            int plugIdx = primaryPluginMap[activePrimaryIdx];
            if (plugIdx < 0) {
                // Core
                secCount = CORE_SECONDARY_COUNT;
            } else {
                // Plugin — virtual secondary: submenus + "General" for leaf items
                auto& p = PluginRegistry::Plugins[plugIdx];
                secCount = (p.MenuRoot && p.Loaded) ? GetPluginSecondaryCount(plugIdx) : 0;
            }
        }

        float primaryH   = HEADER_H + ITEM_H * (float)MaxI(1, primaryCount) + 4;
        float secondaryH = showSecondary ? HEADER_H + ITEM_H * (float)MaxI(1, secCount) + 4 : 0;
        float sidebarH   = MaxF(primaryH, secondaryH > 0 ? secondaryH : primaryH);
        float contentH   = MaxF(sidebarH, MAX_CONTENT_H);

        // ==== PRIMARY PANEL ====
        {
            dl->AddRectFilled(primaryPos, ImVec2(primaryPos.x + PRIMARY_W, primaryPos.y + sidebarH), COL_BG, 4.0f);
            dl->AddRectFilled(primaryPos, ImVec2(primaryPos.x + PRIMARY_W, primaryPos.y + HEADER_H), COL_HEADER, 4.0f);
            dl->AddRect(primaryPos, ImVec2(primaryPos.x + PRIMARY_W, primaryPos.y + sidebarH), COL_BORDER, 4.0f);
            dl->AddText(ImVec2(primaryPos.x + 10, primaryPos.y + 8), COL_ACCENT, "NightSharp");
            dl->AddLine(ImVec2(primaryPos.x, primaryPos.y + HEADER_H),
                        ImVec2(primaryPos.x + PRIMARY_W, primaryPos.y + HEADER_H), COL_BORDER);

            float y = primaryPos.y + HEADER_H + 2;
            for (int i = 0; i < primaryCount; i++) {
                if (DrawSidebarItem(dl, ImVec2(primaryPos.x, y), PRIMARY_W,
                                    primaryLabels[i], activePrimaryIdx == i, true)) {
                    if (activePrimaryIdx != i) {
                        activePrimaryIdx = i;
                        activeSecondaryIdx = -1;
                        activePluginSecIdx = -1;
                        secondarySelected = false;
                    }
                    primarySelected = true;
                }
                y += ITEM_H;
            }
        }

        // Recompute
        showSecondary = primarySelected && activePrimaryIdx >= 0;
        if (!showSecondary) return;

        int activePlugMap = (activePrimaryIdx >= 0 && activePrimaryIdx < primaryCount)
                            ? primaryPluginMap[activePrimaryIdx] : -1;

        if (activePlugMap < 0) {
            // Core
            secCount = CORE_SECONDARY_COUNT;
        } else {
            auto& p = PluginRegistry::Plugins[activePlugMap];
            secCount = (p.MenuRoot && p.Loaded) ? GetPluginSecondaryCount(activePlugMap) : 0;
        }

        // ==== SECONDARY PANEL ====
        {
            if (activeSecondaryIdx < 0 && secCount > 0)
                activeSecondaryIdx = 0;

            secondaryH = HEADER_H + ITEM_H * (float)MaxI(1, secCount) + 4;
            sidebarH = MaxF(primaryH, secondaryH);

            dl->AddRectFilled(secondaryPos, ImVec2(secondaryPos.x + SECONDARY_W, secondaryPos.y + sidebarH), COL_BG, 4.0f);
            dl->AddRectFilled(secondaryPos, ImVec2(secondaryPos.x + SECONDARY_W, secondaryPos.y + HEADER_H), COL_HEADER, 4.0f);
            dl->AddRect(secondaryPos, ImVec2(secondaryPos.x + SECONDARY_W, secondaryPos.y + sidebarH), COL_BORDER, 4.0f);

            // Header label
            const char* headerLabel = (activePrimaryIdx >= 0 && activePrimaryIdx < primaryCount)
                                      ? primaryLabels[activePrimaryIdx] : "?";
            dl->AddText(ImVec2(secondaryPos.x + 10, secondaryPos.y + 8), COL_ACCENT, headerLabel);
            dl->AddLine(ImVec2(secondaryPos.x, secondaryPos.y + HEADER_H),
                        ImVec2(secondaryPos.x + SECONDARY_W, secondaryPos.y + HEADER_H), COL_BORDER);

            float y = secondaryPos.y + HEADER_H + 2;
            for (int i = 0; i < secCount; i++) {
                const char* secLabel = nullptr;
                if (activePlugMap < 0) {
                    // Core secondary entries
                    secLabel = CORE_SECONDARY[i].label;
                } else {
                    // Plugin secondary entries = virtual (submenus + General)
                    secLabel = GetPluginSecondaryLabel(activePlugMap, i);
                }
                if (!secLabel) secLabel = "?";

                if (DrawSidebarItem(dl, ImVec2(secondaryPos.x, y), SECONDARY_W,
                                    secLabel, activeSecondaryIdx == i, true)) {
                    activeSecondaryIdx = i;
                    secondarySelected = true;
                }
                y += ITEM_H;
            }
        }

        // ==== CONTENT PANEL ====
        if (secondarySelected && activeSecondaryIdx >= 0) {
            ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0,0,0,0));
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0,0,0,0));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f,1.0f,1.0f,1.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10,4));

            contentH = MaxF(sidebarH, MAX_CONTENT_H);

            dl->AddRectFilled(contentPos, ImVec2(contentPos.x + CONTENT_W, contentPos.y + contentH), COL_CONTENT_BG, 4.0f);
            dl->AddRectFilled(contentPos, ImVec2(contentPos.x + CONTENT_W, contentPos.y + HEADER_H), COL_HEADER, 4.0f);
            dl->AddRect(contentPos, ImVec2(contentPos.x + CONTENT_W, contentPos.y + contentH), COL_BORDER, 4.0f);

            // Section label
            const char* sectionLabel = "?";
            if (activePlugMap < 0 && activeSecondaryIdx < CORE_SECONDARY_COUNT)
                sectionLabel = CORE_SECONDARY[activeSecondaryIdx].label;
            else if (activePlugMap >= 0) {
                sectionLabel = GetPluginSecondaryLabel(activePlugMap, activeSecondaryIdx);
            }

            dl->AddText(ImVec2(contentPos.x + 10, contentPos.y + 8), COL_ACCENT, sectionLabel);
            dl->AddLine(ImVec2(contentPos.x, contentPos.y + HEADER_H),
                        ImVec2(contentPos.x + CONTENT_W, contentPos.y + HEADER_H), COL_BORDER);

            ImGui::SetNextWindowPos(ImVec2(contentPos.x, contentPos.y + HEADER_H), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(CONTENT_W, contentH - HEADER_H), ImGuiCond_Always);
            ImGui::Begin("##ns_content", nullptr,
                ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBackground);

            if (activePlugMap < 0) {
                // Core content
                DrawCoreContentPanel(activeSecondaryIdx);
            } else {
                // Plugin content — render SDK menu subtree
                DrawPluginContentPanel(activePlugMap, activeSecondaryIdx);
            }

            ImGui::End();
            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor(3);
        }

        // Update explicit menu bounds for WM_NCHITTEST
        menuBoundsRight  = menuPosX + totalW;
        menuBoundsBottom = menuPosY + contentH;
    }

} // namespace NightSharpMenu
