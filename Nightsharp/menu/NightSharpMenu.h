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
#include "MenuTheme.h"
#include "MenuUI.h"
#include "MenuItemRenderers.h"
#include "MenuConfig.h"
#include "Translations.h"

#include "PluginRegistry.h"
#include "SDKDiagnostics.h"
#include "../plugins/core/DebugLog.h"

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

    struct PanelRect { float x, y, w, h; };
    inline PanelRect menuPanels[3] = {};
    inline int menuPanelCount = 0;

    using namespace MenuTheme;

    inline float PRIMARY_W      = 190.0f;
    inline float SECONDARY_W    = 190.0f;
    inline float CONTENT_W      = 560.0f;

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
        bool hovered = g_inputEnabled && ImGui::IsMouseHoveringRect(mn, mx, false);
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

    inline bool DrawOnOffEditor(const char* label, bool& value, const char* id)
    {
        bool changed = false;
        ImDrawList* dl = ImGui::GetForegroundDrawList();
        ImVec2 pos = ImGui::GetCursorScreenPos();
        float panelW = ImGui::GetContentRegionAvail().x;
        ImVec2 winPos = ImGui::GetWindowPos();
        float panelX = winPos.x;

        ImVec2 mn = ImVec2(panelX, pos.y);
        ImVec2 mx = ImVec2(panelX + panelW, pos.y + ITEM_H);
        bool hovered = g_inputEnabled && ImGui::IsMouseHoveringRect(mn, mx, false);

        if (hovered)
            dl->AddRectFilled(mn, mx, COL_ITEM_HOVER, 3.0f);
        dl->AddLine(ImVec2(panelX, mx.y), ImVec2(panelX + panelW, mx.y), COL_BORDER, 1.0f);

        dl->AddText(ImVec2(pos.x + 2, pos.y + 7), COL_TEXT, Translations::T(label));

        const char* stateText = value ? Translations::T("On") : Translations::T("Off");
        ImVec2 stateSize = ImGui::CalcTextSize(stateText);
        float padX = ImGui::GetStyle().FramePadding.x;
        float btnW = stateSize.x + padX * 2.0f;
        float btnX = panelX + panelW - btnW - 8.0f;
        ImU32 btnCol = value ? IM_COL32(46, 140, 71, 250) : IM_COL32(166, 56, 61, 250);
        dl->AddRectFilled(ImVec2(btnX, pos.y + 4.0f), ImVec2(btnX + btnW, pos.y + ITEM_H - 4.0f), btnCol, 7.0f);
        dl->AddText(ImVec2(btnX + padX, pos.y + (ITEM_H - stateSize.y) * 0.5f),
                    IM_COL32(245, 247, 255, 255), stateText);

        if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            value = !value;
            changed = true;
        }

        ImGui::SetCursorScreenPos(ImVec2(pos.x, pos.y + ITEM_H));
        return changed;
    }

    inline void DrawSectionTitle(const char* title) {
        ImGui::TextColored(ImVec4(0.47f,0.92f,0.47f,1.0f), "%s", Translations::T(title));
        ImGui::Separator();
    }

    // ============================================================================
    // Content sections — Core
    // ============================================================================
    inline bool debugWindowEnabled = false;

    inline void DrawMenuSection() {
        DrawSectionTitle("Menu Settings");
        static bool skinChanger = false;
        DrawOnOffEditor("Skin Changer", skinChanger, "menu_skin");
        DrawOnOffEditor("Zoom Hack", Config::ZoomHack::enabled, "zoom_hack");
        DrawOnOffEditor("Bypass OBS", Config::StreamProtection::bypassObs, "bypass_obs");
        DrawOnOffEditor("Debug Window", debugWindowEnabled, "debug_window_enabled");
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.5f,0.5f,0.6f,1.0f), "%s", Translations::T("Bypass OBS: overlay hidden from screen capture"));
        ImGui::TextColored(ImVec4(0.5f,0.5f,0.6f,1.0f), "%s", Translations::T("(requires Win10 2004+)"));
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


    inline void DrawPluginManagerRows(PluginRegistry::PluginKind kind, const char* emptyText, int idBase = 0) {
        int pluginCount = PluginRegistry::GetCountByKind(kind);
        if (pluginCount == 0) {
            if (emptyText && emptyText[0]) {
                ImGui::TextColored(ImVec4(0.5f,0.5f,0.6f,1.0f), "%s", Translations::T(emptyText));
            }
            return;
        }

        for (int i = 0; i < PluginRegistry::PluginCount; i++) {
            auto& p = PluginRegistry::Plugins[i];
            if (p.Kind != kind) continue;
            if (!p.Name) continue;
            const bool hasRuntime = PluginRegistry::HasRuntime(i);

            // Check champion compatibility
            const bool canLoad = PluginRegistry::CanPluginLoad(i);

            // Hide incompatible champion scripts — don't clutter the list
            if (!canLoad) continue;

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
                    hasRuntime ? Translations::T("(no menu)") : Translations::T("(menu init failed)"));
            }

            ImGui::SameLine();
            float rightEdge = ImGui::GetContentRegionAvail().x;
            float buttonW = 70.0f;
            float checkW = 120.0f;
            float targetX = ImGui::GetCursorPosX() + rightEdge - buttonW - checkW - 16.0f;
            if (targetX > ImGui::GetCursorPosX())
                ImGui::SetCursorPosX(targetX);

            if (PluginRegistry::IsBuiltInSDKPlugin(i)) {
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 7.0f);
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f,0.45f,0.22f,0.95f));
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f,0.97f,1.0f,1.0f));
                ImGui::Button(Translations::T("Built-in"), ImVec2(buttonW, 0));
                ImGui::PopStyleColor(2);
                ImGui::PopStyleVar();
            } else if (canLoad && (p.MenuRoot || hasRuntime)) {
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
                ImGui::Button(Translations::T("Error"), ImVec2(buttonW, 0));
                ImGui::PopStyleColor(2);
                ImGui::PopStyleVar();
            }

            ImGui::SameLine(0, 8);
            bool al = p.AlwaysLoad;
            if (ImGui::Checkbox(Translations::T("Always Load"), &al)) {
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
        DrawSectionTitle("SDK Built-ins");
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.5f,0.75f,0.5f,1.0f), "%s", Translations::T("Orbwalker and Target Selector are always on."));
        ImGui::TextColored(ImVec4(0.5f,0.5f,0.6f,1.0f), "%s", Translations::T("This section is read-only for built-in SDK modules."));
        ImGui::Separator();
        int sdkCount = PluginRegistry::GetCountByKind(PluginRegistry::PluginKind::SDK);
        if (sdkCount == 0) {
            ImGui::TextColored(ImVec4(0.5f,0.5f,0.6f,1.0f), "%s", Translations::T("No SDK plugins registered."));
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
            const bool canLoad = PluginRegistry::CanPluginLoad(i);

            ImVec4 statusColor;
            const char* statusText;
            if (!canLoad) {
                statusColor = ImVec4(0.6f, 0.4f, 0.1f, 1.0f);  // Dark orange = incompatible
                statusText = "[NC]";
            } else if (!p.MenuRoot) {
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
            ImGui::TextColored(canLoad ? ImVec4(1.0f, 1.0f, 1.0f, 1.0f) : ImVec4(0.6f, 0.6f, 0.6f, 0.7f), "%s", p.Name);

            // Show warning if incompatible or menu creation failed
            if (!canLoad) {
                ImGui::SameLine(0, 8);
                ImGui::TextColored(ImVec4(0.8f, 0.5f, 0.1f, 0.9f), "%s", Translations::T("(wrong champion)"));
            } else if (!p.MenuRoot) {
                ImGui::SameLine(0, 8);
                ImGui::TextColored(ImVec4(0.9f, 0.6f, 0.1f, 0.9f), "%s",
                    hasRuntime ? Translations::T("(no menu)") : Translations::T("(menu init failed)"));
            }

            ImGui::SameLine();

            float rightEdge = ImGui::GetContentRegionAvail().x;
            float buttonW = 70.0f;
            float checkW = 120.0f;
            float targetX = ImGui::GetCursorPosX() + rightEdge - buttonW - checkW - 16.0f;
            if (targetX > ImGui::GetCursorPosX())
                ImGui::SetCursorPosX(targetX);

            // Load/Unload button (disabled for incompatible champions)
            if (canLoad && (p.MenuRoot || hasRuntime)) {
                if (p.Loaded) {
                    if (DrawStateButton("unload", "Unload", true, false, buttonW))
                        PluginRegistry::UnloadPlugin(i);
                } else {
                    if (DrawStateButton("load", "Load", false, true, buttonW))
                        PluginRegistry::LoadPlugin(i);
                }
            } else {
                // Disabled button for incompatible or plugins without menu
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 7.0f);
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f,0.2f,0.2f,0.5f));
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f,0.5f,0.5f,0.8f));
                ImGui::Button(canLoad ? Translations::T("Error") : Translations::T("N/A"), ImVec2(buttonW, 0));
                ImGui::PopStyleColor(2);
                ImGui::PopStyleVar();
            }

            ImGui::SameLine(0, 8);

            // Always Load checkbox — persisted to plugins.ini
            if (PluginRegistry::IsBuiltInSDKPlugin(i)) {
                ImGui::TextColored(ImVec4(0.5f,0.85f,0.5f,1.0f), "%s", Translations::T("Built-in"));
            } else {
                bool al = p.AlwaysLoad;
                if (ImGui::Checkbox(Translations::T("Always Load"), &al)) {
                    PluginRegistry::SetAlwaysLoad(i, al);
                }
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
            ImGui::TextColored(ImVec4(0.5f,0.5f,0.6f,1.0f), "%s", Translations::T("No external plugins loaded."));
            ImGui::TextColored(ImVec4(0.5f,0.5f,0.6f,1.0f), "%s", Translations::T("Drop plugin DLLs into /plugins/ folder."));
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
        if (secondaryIdx == 1)      DrawMenuSection();
        else if (secondaryIdx == 2) DrawDebugSection();
        else if (secondaryIdx == 3) SDKDiagnostics::Render();
        else if (secondaryIdx == 4) DrawSDKPluginsSection();
        else if (secondaryIdx == 5) DrawPluginsSection();
        else                        {}
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
    inline void DrawPluginContentPanel(int pluginIdx, int secIdx, float panelX, float panelW) {
        if (pluginIdx < 0 || pluginIdx >= PluginRegistry::PluginCount) return;
        auto& p = PluginRegistry::Plugins[pluginIdx];
        if (!p.MenuRoot || !p.Loaded) return;

        auto sections = p.MenuRoot->GetRootSections();
        if (secIdx < 0 || secIdx >= (int)sections.size()) return;

        const std::string& sectionKey = sections[secIdx].first;
        ImDrawList* dl = ImGui::GetForegroundDrawList();
        ImVec2 cursor = ImGui::GetCursorScreenPos();
        float y = cursor.y;

        auto drawItems = [&](const auto& items) {
            int count = 0;
            for (auto& item : items) { if (item) count++; }
            int idx = 0;
            for (auto& item : items) {
                if (!item) continue;
                idx++;
                bool isLast = (idx == count);
                y += MenuRenderers::DrawItem(dl, ImVec2(cursor.x, y), panelX, panelW, item, !isLast);
            }
            ImGui::SetCursorScreenPos(ImVec2(cursor.x, y));
        };

        if (sectionKey == "__root_items") {
            std::vector<SDK::MenuItem*> standalone;
            p.MenuRoot->GetStandaloneItems(standalone);
            drawItems(standalone);
            return;
        }

        auto* sub = p.MenuRoot->FindSection(sectionKey);
        if (!sub) return;
        drawItems(sub->GetItems());
    }

    // ============================================================================
    // Utility
    // ============================================================================
    inline float MaxF(float a, float b) { return a > b ? a : b; }
    inline int MaxI(int a, int b) { return a > b ? a : b; }
    inline bool IsPrimaryPluginEntry(const PluginRegistry::PluginEntry& p, int registryIdx = -1) {
        if (!p.Name || !p.Loaded || !p.MenuRoot) {
            return false;
        }
        // Hide champion plugins that don't match current champion
        if (p.CanLoadFn && !p.CanLoadFn(p.RuntimeUserData)) {
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
        Translations::InitTranslations();

        static int flushDelay = 0;
        if (Translations::g_missingDirty) {
            if (++flushDelay >= 60) {
                flushDelay = 0;
                Translations::FlushMissTranslations();
            }
        } else {
            flushDelay = 0;
        }

        if (!showMenu) return;

        {
            bool isFg = SDK::MenuUI::MenuKeyBind::IsCurrentProcessForeground();
            bool mouseInMenu = false;
            ImVec2 mp = ImGui::GetIO().MousePos;
            for (int i = 0; i < menuPanelCount; i++) {
                auto& r = menuPanels[i];
                if (mp.x >= r.x && mp.x <= r.x + r.w && mp.y >= r.y && mp.y <= r.y + r.h) {
                    mouseInMenu = true;
                    break;
                }
            }
            MenuTheme::g_inputEnabled = isFg || mouseInMenu;
        }

        // ── Build dynamic primary entries ──
        // [0] = Core, [1..N] = loaded SDK plugins
        constexpr int MAX_PRIMARY = 17;  // 1 Core + 16 plugins max
        const char* primaryLabels[MAX_PRIMARY];
        int primaryPluginMap[MAX_PRIMARY];  // maps primary index → plugin registry index (-1 for Core)
        int primaryCount = 0;

        primaryLabels[0] = Translations::T("Core");
        primaryPluginMap[0] = -1;  // Core
        primaryCount = 1;

        for (int i = 0; i < PluginRegistry::PluginCount; i++) {
            auto& p = PluginRegistry::Plugins[i];
            if (IsPrimaryPluginEntry(p, i)) {
                if (primaryCount < MAX_PRIMARY) {
                    primaryLabels[primaryCount] = Translations::T(p.Name);
                    primaryPluginMap[primaryCount] = i;
                    primaryCount++;
                }
            }
        }

        {
            float maxTextW = 0.0f;
            ImVec2 arrowSize = ImGui::CalcTextSize(">");
            ImVec2 spaceSize = ImGui::CalcTextSize("    ");
            for (int i = 0; i < primaryCount; i++) {
                ImVec2 ts = ImGui::CalcTextSize(primaryLabels[i]);
                if (ts.x > maxTextW) maxTextW = ts.x;
            }
            float computed = 12.0f + maxTextW + spaceSize.x + arrowSize.x + 12.0f;
            float headerW = 10.0f + ImGui::CalcTextSize("NightSharp").x + 10.0f;
            if (headerW > computed) computed = headerW;
            PRIMARY_W = computed;
        }

        bool showSecondary = primarySelected && activePrimaryIdx >= 0;
        int activePlugMapEarly = (activePrimaryIdx >= 0 && activePrimaryIdx < primaryCount)
                                 ? primaryPluginMap[activePrimaryIdx] : -1;
        bool isLangSelected = showSecondary && secondarySelected && activePlugMapEarly < 0 && activeSecondaryIdx == 0;
        bool showContent   = showSecondary && secondarySelected && !isLangSelected;

        int secCount = 0;
        if (showSecondary && activePrimaryIdx >= 0 && activePrimaryIdx < primaryCount) {
            int plugIdx = primaryPluginMap[activePrimaryIdx];
            if (plugIdx < 0) {
                secCount = CORE_SECONDARY_COUNT;
            } else {
                auto& p = PluginRegistry::Plugins[plugIdx];
                secCount = (p.MenuRoot && p.Loaded) ? GetPluginSecondaryCount(plugIdx) : 0;
            }
        }

        if (showSecondary && secCount > 0) {
            float maxTextW = 0.0f;
            ImVec2 arrowSize = ImGui::CalcTextSize(">");
            ImVec2 spaceSize = ImGui::CalcTextSize("    ");
            for (int i = 0; i < secCount; i++) {
                const char* lbl = nullptr;
                if (activePlugMapEarly < 0) {
                    lbl = Translations::T(CORE_SECONDARY[i].label);
                } else {
                    lbl = GetPluginSecondaryLabel(activePlugMapEarly, i);
                }
                if (!lbl) lbl = "?";
                ImVec2 ts = ImGui::CalcTextSize(lbl);
                if (ts.x > maxTextW) maxTextW = ts.x;
            }
            float computed = 12.0f + maxTextW + spaceSize.x + arrowSize.x + 12.0f;

            const char* headerLabel = (activePrimaryIdx >= 0 && activePrimaryIdx < primaryCount)
                                      ? primaryLabels[activePrimaryIdx] : "?";
            float headerW = 10.0f + ImGui::CalcTextSize(headerLabel).x + 10.0f;
            if (headerW > computed) computed = headerW;

            if (activePlugMapEarly < 0) {
                const char* const langItemsEarly[] = { "EN", "CN", "VN" };
                float maxLangW = 0.0f;
                for (int li = 0; li < 3; li++) {
                    float lw = ImGui::CalcTextSize(langItemsEarly[li]).x;
                    if (lw > maxLangW) maxLangW = lw;
                }
                float langDropMinW = maxLangW + 12.0f + 6.0f;
                float langRow = 12.0f + maxTextW + langDropMinW + 12.0f;
                if (computed < langRow) computed = langRow;
            }
            SECONDARY_W = computed;
        }

        if (showContent) {
            CONTENT_W = 560.0f;
            if (activePlugMapEarly >= 0 && activeSecondaryIdx >= 0) {
                auto& p = PluginRegistry::Plugins[activePlugMapEarly];
                if (p.MenuRoot && p.Loaded) {
                    auto sections = p.MenuRoot->GetRootSections();
                    if (activeSecondaryIdx < (int)sections.size()) {
                        float estimated = p.MenuRoot->EstimateRootSectionWidth(sections[activeSecondaryIdx].first);
                        CONTENT_W = estimated + 40.0f;
                    }
                }
            }
        }

        float totalW = PRIMARY_W;
        if (showSecondary) totalW += PANEL_GAP + SECONDARY_W;
        if (showContent)   totalW += PANEL_GAP + CONTENT_W;

        // ---- Drag handling ----
        ImVec2 mouse = ImGui::GetIO().MousePos;
        float titleMaxX = menuPosX + totalW;
        float titleMaxY = menuPosY + HEADER_H;
        bool inTitle = mouse.x >= menuPosX && mouse.x <= titleMaxX
                    && mouse.y >= menuPosY && mouse.y <= titleMaxY;

        if (g_inputEnabled && ImGui::IsMouseClicked(0) && inTitle && !ImGui::IsAnyItemHovered() && !ImGui::IsAnyItemActive()) {
            isDragging = true;
            dragOffX = mouse.x - menuPosX;
            dragOffY = mouse.y - menuPosY;
        }
        if (!ImGui::IsMouseDown(0)) isDragging = false;
        if (isDragging) {
            menuPosX = mouse.x - dragOffX;
            menuPosY = mouse.y - dragOffY;
        }

        float primaryH   = HEADER_H + ITEM_H * (float)MaxI(1, primaryCount) + 4;
        float secondaryH = showSecondary ? HEADER_H + ITEM_H * (float)MaxI(1, secCount) + 4 : 0;
        float contentH   = showContent ? MaxF(secondaryH, MAX_CONTENT_H) : 0;
        float actualH    = MaxF(primaryH, MaxF(secondaryH, contentH));

        const ImVec2 display = ImGui::GetIO().DisplaySize;
        if (display.x > 0.0f && display.y > 0.0f) {
            if (menuPosX < 0.0f) menuPosX = 0.0f;
            if (menuPosY < 0.0f) menuPosY = 0.0f;
            if (menuPosX + totalW > display.x) menuPosX = display.x - totalW;
            if (menuPosY + actualH > display.y) menuPosY = display.y - actualH;
        }

        ImDrawList* dl = ImGui::GetForegroundDrawList();
        if (!dl) return;

        ImVec2 primaryPos   = ImVec2(menuPosX, menuPosY);
        ImVec2 secondaryPos = ImVec2(menuPosX + PRIMARY_W + PANEL_GAP, menuPosY);
        ImVec2 contentPos   = ImVec2(menuPosX + PRIMARY_W + SECONDARY_W + PANEL_GAP * 2, menuPosY);

        // ==== PRIMARY PANEL ====
        {
            dl->AddRectFilled(primaryPos, ImVec2(primaryPos.x + PRIMARY_W, primaryPos.y + primaryH), COL_BG, 4.0f);
            dl->AddRectFilled(primaryPos, ImVec2(primaryPos.x + PRIMARY_W, primaryPos.y + HEADER_H), COL_HEADER, 4.0f);
            dl->AddRect(primaryPos, ImVec2(primaryPos.x + PRIMARY_W, primaryPos.y + primaryH), COL_BORDER, 4.0f);
            dl->AddText(ImVec2(primaryPos.x + 10, primaryPos.y + 8), COL_ACCENT, "NightSharp");
            dl->AddLine(ImVec2(primaryPos.x, primaryPos.y + HEADER_H),
                        ImVec2(primaryPos.x + PRIMARY_W, primaryPos.y + HEADER_H), COL_BORDER);

            float y = primaryPos.y + HEADER_H + 2;
            for (int i = 0; i < primaryCount; i++) {
                if (DrawSidebarItem(dl, ImVec2(primaryPos.x, y), PRIMARY_W,
                                    primaryLabels[i], primarySelected && activePrimaryIdx == i, true)) {
                    if (activePrimaryIdx == i) {
                        primarySelected = !primarySelected;
                        if (!primarySelected) {
                            activeSecondaryIdx = -1;
                            activePluginSecIdx = -1;
                            secondarySelected = false;
                        }
                    } else {
                        activePrimaryIdx = i;
                        activeSecondaryIdx = -1;
                        activePluginSecIdx = -1;
                        secondarySelected = false;
                        primarySelected = true;
                    }
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

            int& langIndex = Translations::langIndex;
            static bool langDropOpen = false;
            const char* const langItems[] = { "EN", "CN", "VN" };
            constexpr int langItemCount = 3;
            int langExtraRows = (activePlugMap < 0 && langDropOpen) ? langItemCount : 0;

            secondaryH = HEADER_H + ITEM_H * (float)MaxI(1, secCount + langExtraRows) + 4;

            dl->AddRectFilled(secondaryPos, ImVec2(secondaryPos.x + SECONDARY_W, secondaryPos.y + secondaryH), COL_BG, 4.0f);
            dl->AddRectFilled(secondaryPos, ImVec2(secondaryPos.x + SECONDARY_W, secondaryPos.y + HEADER_H), COL_HEADER, 4.0f);
            dl->AddRect(secondaryPos, ImVec2(secondaryPos.x + SECONDARY_W, secondaryPos.y + secondaryH), COL_BORDER, 4.0f);

            const char* headerLabel = (activePrimaryIdx >= 0 && activePrimaryIdx < primaryCount)
                                      ? primaryLabels[activePrimaryIdx] : "?";
            dl->AddText(ImVec2(secondaryPos.x + 10, secondaryPos.y + 8), COL_ACCENT, headerLabel);
            dl->AddLine(ImVec2(secondaryPos.x, secondaryPos.y + HEADER_H),
                        ImVec2(secondaryPos.x + SECONDARY_W, secondaryPos.y + HEADER_H), COL_BORDER);

            float y = secondaryPos.y + HEADER_H + 2;
            for (int i = 0; i < secCount; i++) {
                const char* secLabel = nullptr;
                if (activePlugMap < 0) {
                    secLabel = Translations::T(CORE_SECONDARY[i].label);
                } else {
                    secLabel = GetPluginSecondaryLabel(activePlugMap, i);
                }
                if (!secLabel) secLabel = "?";

                if (activePlugMap < 0 && i == 0) {
                    float maxLangW = 0.0f;
                    for (int li = 0; li < langItemCount; li++) {
                        float lw = ImGui::CalcTextSize(langItems[li]).x;
                        if (lw > maxLangW) maxLangW = lw;
                    }
                    float dropW = maxLangW + 12.0f;
                    float dropX = secondaryPos.x + SECONDARY_W - dropW - 6.0f;
                    ImVec2 mn = ImVec2(secondaryPos.x, y);
                    ImVec2 mx = ImVec2(secondaryPos.x + SECONDARY_W, y + ITEM_H);

                    bool rowHovered = g_inputEnabled && ImGui::IsMouseHoveringRect(mn, mx, false);
                    ImVec2 dropMin = ImVec2(dropX, y + 3.0f);
                    ImVec2 dropMax = ImVec2(dropX + dropW, y + ITEM_H - 3.0f);
                    bool dropHovered = g_inputEnabled && ImGui::IsMouseHoveringRect(dropMin, dropMax, false);

                    bool langRowActive = secondarySelected && activeSecondaryIdx == i;
                    if (rowHovered || langRowActive)
                        dl->AddRectFilled(mn, mx, langRowActive ? COL_ITEM_ACTIVE : COL_ITEM_HOVER, 3.0f);
                    if (langRowActive)
                        dl->AddLine(ImVec2(mn.x + 1, mn.y + 2), ImVec2(mn.x + 1, mx.y - 2), COL_ACCENT, 2.0f);
                    dl->AddLine(ImVec2(mn.x, mx.y), ImVec2(mx.x, mx.y), COL_BORDER, 1.0f);
                    dl->AddText(ImVec2(secondaryPos.x + 12, y + 7), COL_TEXT, secLabel);

                    dl->AddRectFilled(dropMin, dropMax, dropHovered ? COL_ITEM_HOVER : COL_ITEM, 3.0f);
                    dl->AddRect(dropMin, dropMax, COL_BORDER, 3.0f);
                    const char* cur = langItems[langIndex];
                    ImVec2 ts = ImGui::CalcTextSize(cur);
                    dl->AddText(ImVec2(dropX + (dropW - ts.x) * 0.5f, y + (ITEM_H - ts.y) * 0.5f), COL_TEXT, cur);

                    if (rowHovered && ImGui::IsMouseClicked(0)) {
                        langDropOpen = !langDropOpen;
                        activeSecondaryIdx = i;
                        secondarySelected = true;
                    }

                    y += ITEM_H;

                    if (langDropOpen) {
                        for (int li = 0; li < langItemCount; li++) {
                            ImVec2 rMin = ImVec2(secondaryPos.x, y);
                            ImVec2 rMax = ImVec2(secondaryPos.x + SECONDARY_W, y + ITEM_H);
                            bool isSel = (li == langIndex);
                            bool rHov = g_inputEnabled && ImGui::IsMouseHoveringRect(rMin, rMax, false);

                            dl->AddRectFilled(rMin, rMax, isSel ? COL_ITEM_ACTIVE : (rHov ? COL_ITEM_HOVER : COL_ITEM), 0.0f);
                            dl->AddLine(ImVec2(rMin.x, rMax.y), ImVec2(rMax.x, rMax.y), COL_BORDER, 1.0f);

                            ImVec2 its = ImGui::CalcTextSize(langItems[li]);
                            dl->AddText(ImVec2(rMin.x + 24, rMin.y + (ITEM_H - its.y) * 0.5f),
                                        isSel ? COL_ACCENT : COL_TEXT, langItems[li]);

                            if (rHov && ImGui::IsMouseClicked(0)) {
                                langIndex = li;
                                Translations::SaveLangIndex();
                                Translations::FlushMissTranslations();
                            }
                            y += ITEM_H;
                        }
                    }
                } else {
                    if (DrawSidebarItem(dl, ImVec2(secondaryPos.x, y), SECONDARY_W,
                                        secLabel, secondarySelected && activeSecondaryIdx == i, true)) {
                        activeSecondaryIdx = i;
                        secondarySelected = true;
                        langDropOpen = false;
                    }
                    y += ITEM_H;
                }
            }
        }

        // ==== CONTENT PANEL ====
        bool isLangActive = activePlugMap < 0 && activeSecondaryIdx == 0;
        if (secondarySelected && activeSecondaryIdx >= 0 && !isLangActive) {
            ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0,0,0,0));
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0,0,0,0));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f,1.0f,1.0f,1.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10,4));

            float estimatedContentH = HEADER_H + 4.0f;
            if (activePlugMap >= 0 && activeSecondaryIdx >= 0) {
                auto& pe = PluginRegistry::Plugins[activePlugMap];
                if (pe.MenuRoot && pe.Loaded) {
                    auto secs = pe.MenuRoot->GetRootSections();
                    if (activeSecondaryIdx < (int)secs.size()) {
                        float totalItemH = 0.0f;
                        const auto& sectionKey = secs[activeSecondaryIdx].first;
                        if (sectionKey == "__root_items") {
                            std::vector<SDK::MenuItem*> standalone;
                            pe.MenuRoot->GetStandaloneItems(standalone);
                            for (auto* item : standalone)
                                totalItemH += MenuRenderers::EstimateItemHeight(item);
                        } else {
                            auto* sub = pe.MenuRoot->FindSection(sectionKey);
                            if (sub) {
                                for (auto& item : sub->GetItems())
                                    totalItemH += MenuRenderers::EstimateItemHeight(item);
                            }
                        }
                        estimatedContentH = HEADER_H + MaxF(totalItemH, ITEM_H) + 8.0f;
                    }
                }
            } else if (activePlugMap < 0) {
                estimatedContentH = MAX_CONTENT_H;
            }
            contentH = estimatedContentH;

            dl->AddRectFilled(contentPos, ImVec2(contentPos.x + CONTENT_W, contentPos.y + contentH), COL_CONTENT_BG, 4.0f);
            dl->AddRectFilled(contentPos, ImVec2(contentPos.x + CONTENT_W, contentPos.y + HEADER_H), COL_HEADER, 4.0f);
            dl->AddRect(contentPos, ImVec2(contentPos.x + CONTENT_W, contentPos.y + contentH), COL_BORDER, 4.0f);

            // Section label
            const char* sectionLabel = "?";
            if (activePlugMap < 0 && activeSecondaryIdx < CORE_SECONDARY_COUNT)
                sectionLabel = Translations::T(CORE_SECONDARY[activeSecondaryIdx].label);
            else if (activePlugMap >= 0) {
                sectionLabel = GetPluginSecondaryLabel(activePlugMap, activeSecondaryIdx);
            }

            dl->AddText(ImVec2(contentPos.x + 10, contentPos.y + 8), COL_ACCENT, sectionLabel);
            dl->AddLine(ImVec2(contentPos.x, contentPos.y + HEADER_H),
                        ImVec2(contentPos.x + CONTENT_W, contentPos.y + HEADER_H), COL_BORDER);

            ImGui::SetNextWindowPos(ImVec2(contentPos.x, contentPos.y + HEADER_H), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(CONTENT_W, contentH - HEADER_H), ImGuiCond_Always);
            ImGuiWindowFlags contentFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBackground;
            if (!g_inputEnabled)
                contentFlags |= ImGuiWindowFlags_NoInputs;
            ImGui::Begin("##ns_content", nullptr, contentFlags);

            if (activePlugMap < 0) {
                // Core content
                DrawCoreContentPanel(activeSecondaryIdx);
            } else {
                // Plugin content — render SDK menu subtree
                DrawPluginContentPanel(activePlugMap, activeSecondaryIdx, contentPos.x, CONTENT_W);
            }

            ImGui::End();
            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor(3);
        }

        menuPanelCount = 0;
        menuPanels[menuPanelCount++] = { menuPosX, menuPosY, PRIMARY_W, primaryH };
        if (showSecondary)
            menuPanels[menuPanelCount++] = { menuPosX + PRIMARY_W + PANEL_GAP, menuPosY, SECONDARY_W, secondaryH };
        if (showContent)
            menuPanels[menuPanelCount++] = { menuPosX + PRIMARY_W + SECONDARY_W + PANEL_GAP * 2, menuPosY, CONTENT_W, contentH };

    }

    inline char g_debugTextBuf[DebugLogState::kMaxLines * DebugLogState::kMaxLineLen] = {};
    inline int  g_debugLastGen = 0;

    inline void RenderDebugWindow() {
        if (!debugWindowEnabled) return;

        auto& s = DebugLogState::Get();

        ImGui::SetNextWindowSize(ImVec2(500, 300), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowBgAlpha(0.65f);

        if (!ImGui::Begin("Debug Log", nullptr,
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoFocusOnAppearing)) {
            ImGui::End();
            return;
        }

        if (ImGui::Button("Clear")) {
            printClear();
        }

        ImGui::Separator();

        g_debugTextBuf[0] = '\0';
        int pos = 0;
        for (int i = 0; i < s.Count; ++i) {
            const char* line;
            if (s.Count < DebugLogState::kMaxLines)
                line = s.Lines[i];
            else
                line = s.Lines[(s.WriteIndex + i) % DebugLogState::kMaxLines];
            int len = (int)strlen(line);
            if (pos + len + 1 < (int)sizeof(g_debugTextBuf)) {
                memcpy(g_debugTextBuf + pos, line, len);
                pos += len;
                g_debugTextBuf[pos++] = '\n';
            }
        }
        g_debugTextBuf[pos] = '\0';

        ImVec2 avail = ImGui::GetContentRegionAvail();
        ImGui::InputTextMultiline("##debuglog", g_debugTextBuf, sizeof(g_debugTextBuf), avail,
            ImGuiInputTextFlags_ReadOnly);

        if (s.Generation != g_debugLastGen) {
            ImGui::SetScrollHereY(1.0f);
            g_debugLastGen = s.Generation;
        }

        ImGui::End();
    }

} // namespace NightSharpMenu
