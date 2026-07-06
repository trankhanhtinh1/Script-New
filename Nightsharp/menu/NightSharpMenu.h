#pragma once
/*
 * NightSharp - Standalone ImGui sidebar menu
 *
 * Menu integrates the SDK::UI menu system (Menu / MenuItem / PermaShow)
 * so plugins can register components in the EnsoulSharp style and have
 * them automatically rendered inside DrawPluginContentPanel + the
 * PermaShow overlay.
 */

#include "../Core/CoreEvents.h"
#include "../imgui/imgui.h"
#include "../Plugins/PluginRegistry.h"
#include "../SDK/UI/UI.h"
#include "../SDK/UI/PermaShow.h"
#include "MenuConfig.h"

#include <cstdio>

namespace NightSharpMenu {

    inline bool showMenu = true;
    inline int activePrimaryIdx = -1;
    inline int activeSecondaryIdx = -1;
    inline bool primarySelected = false;
    inline bool secondarySelected = false;
    inline int activePluginIdx = -1;
    inline int pluginManagerFilter = 0;

    inline float menuPosX = 20.0f;
    inline float menuPosY = 40.0f;
    inline bool isDragging = false;
    inline float dragOffX = 0.0f;
    inline float dragOffY = 0.0f;
    inline bool permaShowDragging = false;
    inline float permaShowDragOffX = 0.0f;
    inline float permaShowDragOffY = 0.0f;

    inline float menuBoundsRight = 0.0f;
    inline float menuBoundsBottom = 0.0f;
    inline float permaShowBoundsRight = 0.0f;
    inline float permaShowBoundsBottom = 0.0f;

    constexpr float PRIMARY_W = 190.0f;
    constexpr float SECONDARY_W = 190.0f;
    constexpr float CONTENT_W = 560.0f;
    constexpr float ITEM_H = 30.0f;
    constexpr float HEADER_H = 32.0f;
    constexpr float PANEL_GAP = 6.0f;
    constexpr float MAX_CONTENT_H = 620.0f;

    // Margin from the screen edges when auto-positioning PermaShow.
    constexpr float PERMASHOW_EDGE_MARGIN = 18.0f;

    inline ImU32 COL_BG = IM_COL32(8, 10, 18, 214);
    inline ImU32 COL_CONTENT_BG = IM_COL32(8, 10, 18, 128);
    inline ImU32 COL_HEADER = IM_COL32(16, 18, 28, 236);
    inline ImU32 COL_ITEM = IM_COL32(18, 20, 30, 118);
    inline ImU32 COL_ITEM_HOVER = IM_COL32(52, 48, 82, 215);
    inline ImU32 COL_ITEM_ACTIVE = IM_COL32(82, 66, 132, 232);
    inline ImU32 COL_ACCENT = IM_COL32(120, 235, 120, 255);
    inline ImU32 COL_TEXT = IM_COL32(255, 255, 255, 255);
    inline ImU32 COL_TEXT_DIM = IM_COL32(185, 185, 205, 255);
    inline ImU32 COL_BORDER = IM_COL32(88, 100, 148, 180);

    struct SidebarEntry {
        const char* label;
    };

    constexpr int PRIMARY_COUNT = 1;
    inline const SidebarEntry PRIMARY[PRIMARY_COUNT] = {
        { "Core" },
    };

    constexpr int CORE_SECONDARY_COUNT = 4;
    inline const SidebarEntry CORE_SECONDARY[CORE_SECONDARY_COUNT] = {
        { "Language" },
        { "Menu" },
        { "Debug Info" },
        { "Plugins" },
    };

    inline float MaxF(float a, float b) {
        return a > b ? a : b;
    }

    inline int MaxI(int a, int b) {
        return a > b ? a : b;
    }

    inline int ClampI(int value, int minValue, int maxValue) {
        if (maxValue < minValue) {
            maxValue = minValue;
        }
        if (value < minValue) {
            return minValue;
        }
        if (value > maxValue) {
            return maxValue;
        }
        return value;
    }

    inline ImVec2 GetOverlayDisplaySize() {
        ImVec2 size = ImGui::GetIO().DisplaySize;
        if (ImGuiViewport* vp = ImGui::GetMainViewport()) {
            if (vp->Size.x > 0.0f && vp->Size.y > 0.0f) {
                size = vp->Size;
            }
        }

        if (size.x <= 0.0f) {
            size.x = 1920.0f;
        }
        if (size.y <= 0.0f) {
            size.y = 1080.0f;
        }
        return size;
    }

    inline float GetPermaShowTotalHeight(int rows) {
        const float padding = 8.0f;
        const float lineHeight = ImGui::GetFontSize() * 1.35f;
        const float titleHeight = lineHeight + 4.0f;
        const int visibleRows = rows > 0 ? rows : 1;
        return padding * 2.0f + titleHeight + static_cast<float>(visibleRows) * lineHeight;
    }

    inline void ClampPermaShowPosition(float width, float totalHeight) {
        const ImVec2 display = GetOverlayDisplaySize();
        const int maxX = MaxI(0, static_cast<int>(display.x - width));
        const int maxY = MaxI(0, static_cast<int>(display.y - totalHeight));
        Config::PermaShow::x = ClampI(Config::PermaShow::x, 0, maxX);
        Config::PermaShow::y = ClampI(Config::PermaShow::y, 0, maxY);
    }

    inline void EnsurePermaShowPositionInitialized(float width, float totalHeight) {
        if (!Config::PermaShow::positionInitialized) {
            const ImVec2 anchor = GetOverlayDisplaySize();
            Config::PermaShow::x = static_cast<int>(anchor.x - width - PERMASHOW_EDGE_MARGIN);
            Config::PermaShow::y = static_cast<int>(anchor.y - totalHeight - PERMASHOW_EDGE_MARGIN);
            Config::PermaShow::positionInitialized = true;
        }

        ClampPermaShowPosition(width, totalHeight);
    }

    inline bool IsPointInside(float x, float y) {
        if (!showMenu) {
            return false;
        }

        const bool insideMenu = x >= menuPosX &&
            x <= menuBoundsRight &&
            y >= menuPosY &&
            y <= menuBoundsBottom;

        const bool insidePermaShow =
            Config::PermaShow::enabled &&
            x >= static_cast<float>(Config::PermaShow::x) &&
            x <= permaShowBoundsRight &&
            y >= static_cast<float>(Config::PermaShow::y) &&
            y <= permaShowBoundsBottom;

        return insideMenu || insidePermaShow;
    }

    inline bool DrawSidebarItem(ImDrawList* dl, ImVec2 pos, float w, const char* text, bool isActive, bool hasArrow = false) {
        ImVec2 mn = pos;
        ImVec2 mx = ImVec2(pos.x + w, pos.y + ITEM_H);
        bool hovered = ImGui::IsMouseHoveringRect(mn, mx, false);
        bool clicked = hovered && ImGui::IsMouseClicked(0);

        if (hovered || isActive) {
            dl->AddRectFilled(mn, mx, isActive ? COL_ITEM_ACTIVE : COL_ITEM_HOVER, 3.0f);
        }
        if (isActive) {
            dl->AddLine(ImVec2(mn.x + 1, mn.y + 2), ImVec2(mn.x + 1, mx.y - 2), COL_ACCENT, 2.0f);
        }

        dl->AddLine(ImVec2(mn.x, mx.y), ImVec2(mx.x, mx.y), COL_BORDER, 1.0f);
        dl->AddText(ImVec2(pos.x + 12, pos.y + 7), COL_TEXT, text);
        if (hasArrow) {
            dl->AddText(ImVec2(pos.x + w - 18, pos.y + 7), COL_TEXT_DIM, ">");
        }

        return clicked;
    }

    inline bool DrawStateButton(const char* id, const char* label, bool active, bool positive, float width = 44.0f) {
        ImVec4 activeBase = positive ? ImVec4(0.18f, 0.55f, 0.28f, 0.98f) : ImVec4(0.65f, 0.22f, 0.24f, 0.98f);
        ImVec4 activeHover = positive ? ImVec4(0.22f, 0.64f, 0.32f, 1.0f) : ImVec4(0.75f, 0.27f, 0.29f, 1.0f);
        ImVec4 activePress = positive ? ImVec4(0.15f, 0.48f, 0.24f, 1.0f) : ImVec4(0.58f, 0.18f, 0.20f, 1.0f);
        ImVec4 inactiveBase = ImVec4(0.14f, 0.16f, 0.24f, 0.98f);
        ImVec4 inactiveHov = ImVec4(0.20f, 0.24f, 0.34f, 1.0f);
        ImVec4 inactivePr = ImVec4(0.24f, 0.28f, 0.40f, 1.0f);
        ImVec4 text = active ? ImVec4(0.96f, 0.97f, 1.0f, 1.0f) : ImVec4(0.78f, 0.81f, 0.90f, 1.0f);

        ImGui::PushID(id);
        ImGui::PushStyleColor(ImGuiCol_Button, active ? activeBase : inactiveBase);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, active ? activeHover : inactiveHov);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, active ? activePress : inactivePr);
        ImGui::PushStyleColor(ImGuiCol_Text, text);
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 7.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
        bool clicked = ImGui::Button(label, ImVec2(width, 0));
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
        ImGui::SameLine(0, 6);
        if (DrawStateButton("off", "Off", !value, false) && value) {
            value = false;
            changed = true;
        }
        ImGui::PopID();
        return changed;
    }

    inline void DrawSectionTitle(const char* title) {
        ImGui::TextColored(ImVec4(0.47f, 0.92f, 0.47f, 1.0f), "%s", title);
        ImGui::Separator();
    }

    inline void DrawLanguageSection() {
        DrawSectionTitle("Language");
        static int lang = 0;
        const char* langs[] = { "EN", "CN", "VN" };

        for (int i = 0; i < 3; ++i) {
            ImGui::PushID(langs[i]);
            if (DrawStateButton(langs[i], langs[i], lang == i, true, 56.0f)) {
                lang = i;
            }
            ImGui::PopID();
            if (i < 2) {
                ImGui::SameLine(0, 8);
            }
        }
    }

    inline void DrawMenuSection() {
        DrawSectionTitle("Menu Settings");
        DrawOnOffEditor("Skin Changer", Config::SkinChanger::enabled, "menu_skin");
        if (Config::SkinChanger::enabled) {
            ImGui::SliderInt("##skin_id", &Config::SkinChanger::skinId, 0, 100, "Skin ID: %d");
        }

        DrawOnOffEditor("Zoom Hack", Config::ZoomHack::enabled, "zoom_hack");
        if (Config::ZoomHack::enabled) {
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.6f, 1.0f), "Lan chuot de zoom xa tuy y");
        }

        DrawOnOffEditor("PermaShow", Config::PermaShow::enabled, "perma_show");
        DrawOnOffEditor("Bypass OBS", Config::StreamProtection::bypassObs, "bypass_obs");
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.6f, 1.0f), "Bypass OBS: overlay hidden from screen capture");
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.6f, 1.0f), "(requires Win10 2004+)");
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.6f, 1.0f), "Click-through always on: clicks pass through the overlay.");
    }

    inline void DrawDebugSection() {
        DrawSectionTitle("Debug Info");
        ImGui::Text("NightSharp");
        ImGui::Text("Overlay: D3D11 External");
        ImGui::Text("Menu: Old sidebar style");
        ImGui::Text("Input: %s", Config::OverlayInput::clickThrough ? "Click-through" : "Menu capture");
        ImGui::Separator();
        ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
        if (ImGui::GetIO().Framerate > 0.0f) {
            ImGui::Text("Frame: %.3f ms", 1000.0f / ImGui::GetIO().Framerate);
        }
    }

    inline PluginRegistry::PluginCategory FilterCategoryFromIndex(int filterIdx) {
        switch (filterIdx) {
        case 1:  return PluginRegistry::PluginCategory::Champion;
        case 2:  return PluginRegistry::PluginCategory::Utility;
        case 3:  return PluginRegistry::PluginCategory::Core;
        case 4:  return PluginRegistry::PluginCategory::Misc;
        default: return PluginRegistry::PluginCategory::Core;
        }
    }

    inline bool PluginManagerRowVisible(int idx, PluginRegistry::PluginKind kind) {
        if (idx < 0 || idx >= PluginRegistry::PluginCount) return false;

        auto& p = PluginRegistry::Plugins[idx];
        if (p.Kind != kind || !p.Name || !p.InternalId) return false;

        if (p.Category == PluginRegistry::PluginCategory::Champion &&
            !PluginRegistry::CanPluginLoad(idx)) {
            return false;
        }

        if (pluginManagerFilter <= 0) {
            return true;
        }

        return p.Category == FilterCategoryFromIndex(pluginManagerFilter);
    }

    inline int CountPluginManagerRows(PluginRegistry::PluginKind kind) {
        int count = 0;
        for (int i = 0; i < PluginRegistry::PluginCount; ++i) {
            if (PluginManagerRowVisible(i, kind)) {
                ++count;
            }
        }
        return count;
    }

    inline void DrawPluginManagerFilters() {
        const char* labels[] = { "All", "Champion", "Utility", "Core", "Misc" };
        for (int i = 0; i < 5; ++i) {
            ImGui::PushID(i);
            if (DrawStateButton(labels[i], labels[i], pluginManagerFilter == i, true, i == 1 ? 88.0f : 64.0f)) {
                pluginManagerFilter = i;
            }
            ImGui::PopID();
            if (i < 4) {
                ImGui::SameLine(0, 6);
            }
        }
        ImGui::Spacing();
    }

    inline void DrawPluginManagerRows(PluginRegistry::PluginKind kind, const char* emptyText, int idBase = 0) {
        if (CountPluginManagerRows(kind) == 0) {
            if (emptyText && emptyText[0]) {
                ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.6f, 1.0f), "%s", emptyText);
            }
            return;
        }

        for (int i = 0; i < PluginRegistry::PluginCount; ++i) {
            auto& p = PluginRegistry::Plugins[i];
            if (!PluginManagerRowVisible(i, kind)) {
                continue;
            }

            const bool canLoad = PluginRegistry::CanPluginLoad(i);
            ImGui::PushID(i + idBase);

            ImVec4 statusColor = !canLoad
                ? ImVec4(0.75f, 0.55f, 0.18f, 1.0f)
                : (p.Loaded ? ImVec4(0.30f, 0.86f, 0.34f, 1.0f) : ImVec4(0.62f, 0.64f, 0.70f, 1.0f));
            ImGui::TextColored(statusColor, "%s", p.Loaded ? "[ON]" : (canLoad ? "[--]" : "[NC]"));
            ImGui::SameLine(0, 8);
            ImGui::Text("%s", p.Name);
            ImGui::SameLine(0, 8);
            ImGui::TextColored(ImVec4(0.48f, 0.62f, 0.78f, 0.95f), "[%s]", PluginRegistry::CategoryName(p.Category));
            if (p.ChampionName && p.ChampionName[0]) {
                ImGui::SameLine(0, 6);
                ImGui::TextColored(ImVec4(0.68f, 0.78f, 1.0f, 0.95f), "%s", p.ChampionName);
            }

            float rightEdge = ImGui::GetContentRegionAvail().x;
            float targetX = ImGui::GetCursorPosX() + rightEdge - 210.0f;
            if (targetX > ImGui::GetCursorPosX()) {
                ImGui::SetCursorPosX(targetX);
            }

            if (canLoad) {
                if (p.Loaded) {
                    if (DrawStateButton("unload", "Unload", true, false, 74.0f)) {
                        PluginRegistry::UnloadPlugin(i);
                    }
                } else {
                    if (DrawStateButton("load", "Load", false, true, 74.0f)) {
                        PluginRegistry::LoadPlugin(i);
                    }
                }
            } else {
                ImGui::BeginDisabled(true);
                ImGui::Button("N/A", ImVec2(74.0f, 0));
                ImGui::EndDisabled();
            }

            ImGui::SameLine(0, 8);
            bool alwaysLoad = p.AlwaysLoad;
            if (ImGui::Checkbox("Always Load", &alwaysLoad)) {
                PluginRegistry::SetAlwaysLoad(i, alwaysLoad);
            }

            if (!p.RuntimeMenu) {
                ImGui::TextColored(ImVec4(0.55f, 0.55f, 0.64f, 1.0f), "No custom ImGui menu callback.");
            }

            ImGui::Separator();
            ImGui::PopID();
        }
    }

    inline void DrawPluginsSection() {
        DrawSectionTitle("Plugin Manager");
        DrawPluginManagerFilters();
        DrawPluginManagerRows(PluginRegistry::PluginKind::Plugin, "No source plugins for this filter.");

        const int extCount = CountPluginManagerRows(PluginRegistry::PluginKind::External);
        if (extCount <= 0) {
            return;
        }

        ImGui::Spacing();
        DrawSectionTitle("External Plugins");
        DrawPluginManagerRows(PluginRegistry::PluginKind::External, "No external plugins for this filter.", 1000);
    }

    inline int LoadedPluginCount() {
        int count = 0;
        for (int i = 0; i < PluginRegistry::PluginCount; ++i) {
            if (PluginRegistry::Plugins[i].Loaded) {
                ++count;
            }
        }
        return count;
    }

    // ----------------------------------------------------------------
    // PermaShow row helpers — driven by the SDK::UI::PermaShow registry.
    // ----------------------------------------------------------------

    inline void DrawPermaShowRow(ImDrawList* dl,
                                 float x,
                                 float width,
                                 float indicatorWidth,
                                 float padding,
                                 float& currentY,
                                 float lineHeight,
                                 const char* label,
                                 const char* value,
                                 bool hasState,
                                 bool stateOn,
                                 ImU32 color) {
        if (!dl || !label || !value) {
            return;
        }

        const ImVec2 valueSize = ImGui::CalcTextSize(value);
        const float valueX = x + width - padding - valueSize.x;

        dl->AddText(ImVec2(x + padding, currentY), color, label);

        if (hasState) {
            const float boxRight = valueX - 8.0f;
            const float boxLeft = boxRight - indicatorWidth;
            if (boxLeft > x + padding) {
                dl->AddRectFilled(
                    ImVec2(boxLeft, currentY + 1.0f),
                    ImVec2(boxRight, currentY + lineHeight - 3.0f),
                    stateOn ? IM_COL32(0, 110, 50, 180) : IM_COL32(120, 35, 35, 180),
                    2.0f);
            }
        }

        dl->AddText(ImVec2(valueX, currentY), color, value);
        currentY += lineHeight;
    }

    // Render one SDK::UI::PermaShow entry. Mirrors EnsoulSharp PermaShow's
    // per-MenuValueType branching.
    inline void DrawPermaShowEntry(ImDrawList* dl,
                                   const SDK::UI::PermaShow::Entry& entry,
                                   float x,
                                   float width,
                                   float indicatorWidth,
                                   float padding,
                                   float& currentY,
                                   float lineHeight) {
        SDK::UI::MenuItem* item = entry.Item;
        if (!item) return;

        ImU32 color = entry.Color;
        const char* name = entry.DisplayName;

        switch (item->Kind()) {
        case SDK::UI::MenuValueType::Boolean: {
            auto* b = static_cast<SDK::UI::MenuBool*>(item);
            DrawPermaShowRow(dl, x, width, indicatorWidth, padding,
                             currentY, lineHeight,
                             name, b->Value ? "True" : "False",
                             true, b->Value, color);
            break;
        }
        case SDK::UI::MenuValueType::Slider: {
            auto* s = static_cast<SDK::UI::MenuSlider*>(item);
            char val[24] = {};
            std::snprintf(val, sizeof(val), "%d", s->Value);
            DrawPermaShowRow(dl, x, width, indicatorWidth, padding,
                             currentY, lineHeight,
                             name, val, false, false, color);
            break;
        }
        case SDK::UI::MenuValueType::SliderF: {
            auto* s = static_cast<SDK::UI::MenuSliderF*>(item);
            char val[32] = {};
            std::snprintf(val, sizeof(val), "%.2f", s->Value);
            DrawPermaShowRow(dl, x, width, indicatorWidth, padding,
                             currentY, lineHeight,
                             name, val, false, false, color);
            break;
        }
        case SDK::UI::MenuValueType::KeyBind: {
            auto* k = static_cast<SDK::UI::MenuKeyBind*>(item);
            char label[160] = {};
            std::snprintf(label, sizeof(label), "%s [%s]",
                          name, SDK::UI::MenuKeyBind::VkToText(k->Key));
            DrawPermaShowRow(dl, x, width, indicatorWidth, padding,
                             currentY, lineHeight,
                             label, k->Active ? "True" : "False",
                             true, k->Active, color);
            break;
        }
        case SDK::UI::MenuValueType::List: {
            auto* l = static_cast<SDK::UI::MenuList*>(item);
            DrawPermaShowRow(dl, x, width, indicatorWidth, padding,
                             currentY, lineHeight,
                             name, l->SelectedValue(),
                             false, false, color);
            break;
        }
        case SDK::UI::MenuValueType::Color: {
            auto* c = static_cast<SDK::UI::MenuColor*>(item);
            char val[16] = {};
            std::snprintf(val, sizeof(val), "0x%08X", c->Value);
            DrawPermaShowRow(dl, x, width, indicatorWidth, padding,
                             currentY, lineHeight,
                             name, val, false, false, color);
            break;
        }
        default:
            DrawPermaShowRow(dl, x, width, indicatorWidth, padding,
                             currentY, lineHeight,
                             name, "", false, false, color);
            break;
        }
    }

    // Pull the bottom-right anchor for the PermaShow box. Falls back to the
    // current ImGui display size when no main viewport is active.
    inline ImVec2 GetPermaShowAnchor() {
        return GetOverlayDisplaySize();
    }

    inline void DrawPermaShowOverlay() {
        if (!Config::PermaShow::enabled) {
            permaShowDragging = false;
            permaShowBoundsRight = 0.0f;
            permaShowBoundsBottom = 0.0f;
            return;
        }

        const int rows = SDK::UI::PermaShow::Count();
        ImDrawList* dl = ImGui::GetForegroundDrawList();
        if (!dl) {
            permaShowBoundsRight = 0.0f;
            permaShowBoundsBottom = 0.0f;
            return;
        }

        const float width = static_cast<float>(Config::PermaShow::width);
        const float indicatorWidth = static_cast<float>(Config::PermaShow::indicatorWidth);
        const float padding = 8.0f;
        const float lineHeight = ImGui::GetFontSize() * 1.35f;
        const float titleHeight = lineHeight + 4.0f;
        const float totalHeight = GetPermaShowTotalHeight(rows);

        // Anchor to the bottom-right corner the very first time we render,
        // and re-anchor when the configured position is offscreen.
        EnsurePermaShowPositionInitialized(width, totalHeight);

        float x = static_cast<float>(Config::PermaShow::x);
        float y = static_cast<float>(Config::PermaShow::y);

        if (showMenu) {
            const ImVec2 mouse = ImGui::GetIO().MousePos;
            const bool inHeader =
                mouse.x >= x &&
                mouse.x <= x + width &&
                mouse.y >= y &&
                mouse.y <= y + padding + titleHeight;

            if (ImGui::IsMouseClicked(0) && inHeader && !isDragging) {
                permaShowDragging = true;
                permaShowDragOffX = mouse.x - x;
                permaShowDragOffY = mouse.y - y;
            }

            if (!ImGui::IsMouseDown(0)) {
                permaShowDragging = false;
            }

            if (permaShowDragging) {
                Config::PermaShow::x = static_cast<int>(mouse.x - permaShowDragOffX);
                Config::PermaShow::y = static_cast<int>(mouse.y - permaShowDragOffY);
                Config::PermaShow::positionInitialized = true;
                ClampPermaShowPosition(width, totalHeight);
                x = static_cast<float>(Config::PermaShow::x);
                y = static_cast<float>(Config::PermaShow::y);
            }
        } else {
            permaShowDragging = false;
        }

        permaShowBoundsRight = x + width;
        permaShowBoundsBottom = y + totalHeight;

        dl->AddRectFilled(ImVec2(x, y), ImVec2(x + width, y + totalHeight), IM_COL32(18, 20, 26, 255), 4.0f);
        dl->AddRect(ImVec2(x, y), ImVec2(x + width, y + totalHeight), COL_BORDER, 4.0f);
        dl->AddText(ImVec2(x + padding, y + padding - 1.0f), COL_ACCENT, "PermaShow");

        float currentY = y + padding + titleHeight;
        if (rows <= 0) {
            DrawPermaShowRow(dl, x, width, indicatorWidth, padding,
                             currentY, lineHeight,
                             "No PermaShow items", "", false, false, COL_TEXT_DIM);
        } else {
            for (int i = 0; i < rows; ++i) {
                DrawPermaShowEntry(dl, SDK::UI::PermaShow::At(i),
                                   x, width, indicatorWidth, padding,
                                   currentY, lineHeight);
            }
        }
    }

    inline void DrawCoreContentPanel(int secondaryIdx) {
        if (secondaryIdx == 0) {
            DrawLanguageSection();
        } else if (secondaryIdx == 1) {
            DrawMenuSection();
        } else if (secondaryIdx == 2) {
            DrawDebugSection();
        } else if (secondaryIdx == 3) {
            DrawPluginsSection();
        } else {
            DrawLanguageSection();
        }
    }

    inline bool IsPrimaryPluginEntry(const PluginRegistry::PluginEntry& p, int idx) {
        return idx >= 0 &&
            p.Name &&
            p.Loaded &&
            PluginRegistry::CanPluginLoad(idx) &&
            (p.Kind == PluginRegistry::PluginKind::SDK ||
             p.Kind == PluginRegistry::PluginKind::Plugin ||
             p.Kind == PluginRegistry::PluginKind::External);
    }

    inline SDK::UI::Menu* FindMenuByPluginName(SDK::UI::Menu* menu, const char* internalId, const char* displayName) {
        if (!menu) {
            return nullptr;
        }

        if ((internalId && menu->Name.equals(internalId)) ||
            (displayName && menu->Name.equals(displayName)) ||
            (displayName && menu->DisplayName.equals(displayName))) {
            return menu;
        }

        for (int i = 0; i < menu->Components.size(); ++i) {
            auto* component = menu->Components[i];
            if (!component || !component->IsMenu()) {
                continue;
            }

            if (auto* found = FindMenuByPluginName(static_cast<SDK::UI::Menu*>(component), internalId, displayName)) {
                return found;
            }
        }

        return nullptr;
    }

    inline SDK::UI::Menu* FindPluginSdkMenu(int pluginIdx) {
        if (pluginIdx < 0 || pluginIdx >= PluginRegistry::PluginCount) {
            return nullptr;
        }

        auto& p = PluginRegistry::Plugins[pluginIdx];
        auto& mm = SDK::UI::MenuManager::Instance();
        for (int i = 0; i < mm.Menus.size(); ++i) {
            SDK::UI::Menu* root = mm.Menus[i];
            if (!root || !p.InternalId) {
                continue;
            }

            if (auto* menu = FindMenuByPluginName(root, p.InternalId, p.Name)) {
                return menu;
            }
        }

        return nullptr;
    }

    inline bool HasRootLeafItems(SDK::UI::Menu* menu) {
        if (!menu) {
            return false;
        }

        for (int i = 0; i < menu->Components.size(); ++i) {
            auto* component = menu->Components[i];
            if (component && component->Visible && !component->IsMenu()) {
                return true;
            }
        }
        return false;
    }

    inline int CountRootSubMenus(SDK::UI::Menu* menu) {
        if (!menu) {
            return 0;
        }

        int count = 0;
        for (int i = 0; i < menu->Components.size(); ++i) {
            auto* component = menu->Components[i];
            if (component && component->Visible && component->IsMenu()) {
                ++count;
            }
        }
        return count;
    }

    inline int GetPluginSecondaryCount(int pluginIdx) {
        if (pluginIdx < 0 || pluginIdx >= PluginRegistry::PluginCount) {
            return 0;
        }

        SDK::UI::Menu* menu = FindPluginSdkMenu(pluginIdx);
        if (!menu) {
            return 1;
        }

        const int subMenus = CountRootSubMenus(menu);
        const bool hasLeafItems = HasRootLeafItems(menu);
        if (subMenus <= 0) {
            return 1;
        }
        return subMenus + (hasLeafItems ? 1 : 0);
    }

    inline SDK::UI::Menu* GetPluginSecondaryMenu(int pluginIdx, int secondaryIdx) {
        SDK::UI::Menu* root = FindPluginSdkMenu(pluginIdx);
        if (!root || secondaryIdx < 0) {
            return nullptr;
        }

        int seen = 0;
        for (int i = 0; i < root->Components.size(); ++i) {
            auto* component = root->Components[i];
            if (!component || !component->Visible || !component->IsMenu()) {
                continue;
            }

            if (seen == secondaryIdx) {
                return static_cast<SDK::UI::Menu*>(component);
            }
            ++seen;
        }

        return nullptr;
    }

    inline bool IsPluginLeafSection(int pluginIdx, int secondaryIdx) {
        SDK::UI::Menu* root = FindPluginSdkMenu(pluginIdx);
        if (!root || secondaryIdx < 0) {
            return false;
        }

        const int subMenus = CountRootSubMenus(root);
        return HasRootLeafItems(root) && secondaryIdx == subMenus;
    }

    inline const char* GetPluginSecondaryLabel(int pluginIdx, int secondaryIdx) {
        if (pluginIdx < 0 || pluginIdx >= PluginRegistry::PluginCount) {
            return "?";
        }

        if (SDK::UI::Menu* section = GetPluginSecondaryMenu(pluginIdx, secondaryIdx)) {
            return section->DisplayName.c_str();
        }

        if (IsPluginLeafSection(pluginIdx, secondaryIdx)) {
            SDK::UI::Menu* root = FindPluginSdkMenu(pluginIdx);
            return CountRootSubMenus(root) > 0 ? "General" : "Menu";
        }

        auto& p = PluginRegistry::Plugins[pluginIdx];
        if (p.RuntimeMenu) {
            return "Menu";
        }

        return "?";
    }

    inline void DrawRootLeafItems(SDK::UI::Menu* menu) {
        if (!menu) {
            return;
        }

        bool drewAny = false;
        for (int i = 0; i < menu->Components.size(); ++i) {
            auto* component = menu->Components[i];
            if (!component || !component->Visible || component->IsMenu()) {
                continue;
            }
            component->DrawImGui();
            drewAny = true;
        }

        if (!drewAny) {
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.6f, 1.0f), "No items in this section.");
        }
    }

    inline void DrawPluginContentPanel(int pluginIdx, int secondaryIdx) {
        if (pluginIdx < 0 || pluginIdx >= PluginRegistry::PluginCount) {
            return;
        }

        auto& p = PluginRegistry::Plugins[pluginIdx];
        if (!p.Loaded) {
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.6f, 1.0f), "Plugin is not loaded.");
            return;
        }

        SDK::UI::Menu* menu = FindPluginSdkMenu(pluginIdx);
        if (menu) {
            if (SDK::UI::Menu* section = GetPluginSecondaryMenu(pluginIdx, secondaryIdx)) {
                section->DrawChildren();
                return;
            }

            if (IsPluginLeafSection(pluginIdx, secondaryIdx) || CountRootSubMenus(menu) <= 0) {
                DrawRootLeafItems(menu);
                return;
            }

            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.6f, 1.0f), "No menu section selected.");
            return;
        }

        if (p.RuntimeMenu) {
            PluginRegistry::DrawPluginMenu(pluginIdx);
            return;
        }

        ImGui::Text("%s", p.Name ? p.Name : "Plugin");
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.6f, 1.0f), "This plugin has no custom ImGui menu callback.");
    }

    inline void Render() {
        DrawPermaShowOverlay();

        if (!showMenu) {
            menuBoundsRight = menuPosX;
            menuBoundsBottom = menuPosY;
            return;
        }

        constexpr int MAX_PRIMARY = 64;
        const char* primaryLabels[MAX_PRIMARY] = {};
        int primaryPluginMap[MAX_PRIMARY] = {};
        int primaryCount = 0;
        primaryLabels[0] = "Core";
        primaryPluginMap[0] = -1;
        primaryCount = 1;

        for (int i = 0; i < PluginRegistry::PluginCount && primaryCount < MAX_PRIMARY; ++i) {
            auto& p = PluginRegistry::Plugins[i];
            if (IsPrimaryPluginEntry(p, i)) {
                primaryLabels[primaryCount] = p.Name;
                primaryPluginMap[primaryCount] = i;
                ++primaryCount;
            }
        }

        if (activePrimaryIdx >= primaryCount) {
            activePrimaryIdx = -1;
            activeSecondaryIdx = -1;
            primarySelected = false;
            secondarySelected = false;
        }

        const bool showSecondary = primarySelected && activePrimaryIdx >= 0;
        const bool showContent = showSecondary && secondarySelected && activeSecondaryIdx >= 0;

        float totalW = PRIMARY_W;
        if (showSecondary) {
            totalW += PANEL_GAP + SECONDARY_W;
        }
        if (showContent) {
            totalW += PANEL_GAP + CONTENT_W;
        }

        ImVec2 mouse = ImGui::GetIO().MousePos;
        const float titleMaxX = menuPosX + totalW;
        const float titleMaxY = menuPosY + HEADER_H;
        const bool inTitle =
            mouse.x >= menuPosX &&
            mouse.x <= titleMaxX &&
            mouse.y >= menuPosY &&
            mouse.y <= titleMaxY;

        if (ImGui::IsMouseClicked(0) &&
            inTitle &&
            !ImGui::IsAnyItemHovered() &&
            !ImGui::IsAnyItemActive()) {
            isDragging = true;
            dragOffX = mouse.x - menuPosX;
            dragOffY = mouse.y - menuPosY;
        }
        if (!ImGui::IsMouseDown(0)) {
            isDragging = false;
        }
        if (isDragging) {
            menuPosX = mouse.x - dragOffX;
            menuPosY = mouse.y - dragOffY;
        }

        ImDrawList* dl = ImGui::GetForegroundDrawList();
        if (!dl) {
            return;
        }

        ImVec2 primaryPos(menuPosX, menuPosY);
        ImVec2 secondaryPos(menuPosX + PRIMARY_W + PANEL_GAP, menuPosY);
        ImVec2 contentPos(menuPosX + PRIMARY_W + SECONDARY_W + PANEL_GAP * 2.0f, menuPosY);

        int secCount = 0;
        int activePluginMap = -1;
        if (showSecondary && activePrimaryIdx >= 0 && activePrimaryIdx < primaryCount) {
            activePluginMap = primaryPluginMap[activePrimaryIdx];
            secCount = activePluginMap < 0
                ? CORE_SECONDARY_COUNT
                : GetPluginSecondaryCount(activePluginMap);
        }

        const float primaryH = HEADER_H + ITEM_H * static_cast<float>(MaxI(1, primaryCount)) + 4.0f;
        float secondaryH = showSecondary
            ? HEADER_H + ITEM_H * static_cast<float>(MaxI(1, secCount)) + 4.0f
            : 0.0f;
        float sidebarH = MaxF(primaryH, secondaryH > 0.0f ? secondaryH : primaryH);
        float contentH = MaxF(sidebarH, MAX_CONTENT_H);

        // Primary sidebar.
        dl->AddRectFilled(
            primaryPos,
            ImVec2(primaryPos.x + PRIMARY_W, primaryPos.y + sidebarH),
            COL_BG,
            4.0f);
        dl->AddRectFilled(
            primaryPos,
            ImVec2(primaryPos.x + PRIMARY_W, primaryPos.y + HEADER_H),
            COL_HEADER,
            4.0f);
        dl->AddRect(
            primaryPos,
            ImVec2(primaryPos.x + PRIMARY_W, primaryPos.y + sidebarH),
            COL_BORDER,
            4.0f);
        dl->AddText(ImVec2(primaryPos.x + 10.0f, primaryPos.y + 8.0f), COL_ACCENT, "NightSharp");
        dl->AddLine(
            ImVec2(primaryPos.x, primaryPos.y + HEADER_H),
            ImVec2(primaryPos.x + PRIMARY_W, primaryPos.y + HEADER_H),
            COL_BORDER);

        float y = primaryPos.y + HEADER_H + 2.0f;
        for (int i = 0; i < primaryCount; ++i) {
            if (DrawSidebarItem(
                    dl,
                    ImVec2(primaryPos.x, y),
                    PRIMARY_W,
                    primaryLabels[i],
                    activePrimaryIdx == i,
                    true)) {
                if (activePrimaryIdx != i) {
                    activePrimaryIdx = i;
                    activeSecondaryIdx = -1;
                    secondarySelected = false;
                }
                primarySelected = true;
                activePluginIdx = primaryPluginMap[i];
            }
            y += ITEM_H;
        }

        if (!(primarySelected && activePrimaryIdx >= 0 && activePrimaryIdx < primaryCount)) {
            menuBoundsRight = menuPosX + PRIMARY_W;
            menuBoundsBottom = menuPosY + primaryH;
            return;
        }

        activePluginMap = primaryPluginMap[activePrimaryIdx];
        activePluginIdx = activePluginMap;
        secCount = activePluginMap < 0
            ? CORE_SECONDARY_COUNT
            : GetPluginSecondaryCount(activePluginMap);
        if (activeSecondaryIdx >= secCount) {
            activeSecondaryIdx = -1;
            secondarySelected = false;
        }

        secondaryH = HEADER_H + ITEM_H * static_cast<float>(MaxI(1, secCount)) + 4.0f;
        sidebarH = MaxF(primaryH, secondaryH);
        contentH = MaxF(sidebarH, MAX_CONTENT_H);

        // Secondary sidebar.
        dl->AddRectFilled(
            secondaryPos,
            ImVec2(secondaryPos.x + SECONDARY_W, secondaryPos.y + sidebarH),
            COL_BG,
            4.0f);
        dl->AddRectFilled(
            secondaryPos,
            ImVec2(secondaryPos.x + SECONDARY_W, secondaryPos.y + HEADER_H),
            COL_HEADER,
            4.0f);
        dl->AddRect(
            secondaryPos,
            ImVec2(secondaryPos.x + SECONDARY_W, secondaryPos.y + sidebarH),
            COL_BORDER,
            4.0f);

        const char* headerLabel = primaryLabels[activePrimaryIdx] ? primaryLabels[activePrimaryIdx] : "?";
        dl->AddText(ImVec2(secondaryPos.x + 10.0f, secondaryPos.y + 8.0f), COL_ACCENT, headerLabel);
        dl->AddLine(
            ImVec2(secondaryPos.x, secondaryPos.y + HEADER_H),
            ImVec2(secondaryPos.x + SECONDARY_W, secondaryPos.y + HEADER_H),
            COL_BORDER);

        y = secondaryPos.y + HEADER_H + 2.0f;
        for (int i = 0; i < secCount; ++i) {
            const char* secLabel = activePluginMap < 0
                ? CORE_SECONDARY[i].label
                : GetPluginSecondaryLabel(activePluginMap, i);
            if (!secLabel) {
                secLabel = "?";
            }

            if (DrawSidebarItem(
                    dl,
                    ImVec2(secondaryPos.x, y),
                    SECONDARY_W,
                    secLabel,
                    activeSecondaryIdx == i,
                    true)) {
                activeSecondaryIdx = i;
                secondarySelected = true;
            }
            y += ITEM_H;
        }

        if (!(secondarySelected && activeSecondaryIdx >= 0)) {
            menuBoundsRight = menuPosX + PRIMARY_W + PANEL_GAP + SECONDARY_W;
            menuBoundsBottom = menuPosY + sidebarH;
            return;
        }

        // Content panel.
        dl->AddRectFilled(
            contentPos,
            ImVec2(contentPos.x + CONTENT_W, contentPos.y + contentH),
            COL_CONTENT_BG,
            4.0f);
        dl->AddRectFilled(
            contentPos,
            ImVec2(contentPos.x + CONTENT_W, contentPos.y + HEADER_H),
            COL_HEADER,
            4.0f);
        dl->AddRect(
            contentPos,
            ImVec2(contentPos.x + CONTENT_W, contentPos.y + contentH),
            COL_BORDER,
            4.0f);

        const char* sectionLabel = "?";
        if (activePluginMap < 0 && activeSecondaryIdx < CORE_SECONDARY_COUNT) {
            sectionLabel = CORE_SECONDARY[activeSecondaryIdx].label;
        } else if (activePluginMap >= 0) {
            sectionLabel = GetPluginSecondaryLabel(activePluginMap, activeSecondaryIdx);
        }

        dl->AddText(ImVec2(contentPos.x + 10.0f, contentPos.y + 8.0f), COL_ACCENT, sectionLabel);
        dl->AddLine(
            ImVec2(contentPos.x, contentPos.y + HEADER_H),
            ImVec2(contentPos.x + CONTENT_W, contentPos.y + HEADER_H),
            COL_BORDER);

        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.08f, 0.13f, 0.22f, 0.72f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.12f, 0.20f, 0.34f, 0.88f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.15f, 0.26f, 0.44f, 0.96f));
        ImGui::PushStyleColor(ImGuiCol_SliderGrab, ImVec4(0.20f, 0.39f, 0.72f, 0.95f));
        ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, ImVec4(0.28f, 0.50f, 0.88f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.10f, 0.17f, 0.28f, 0.78f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.16f, 0.28f, 0.46f, 0.92f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.20f, 0.36f, 0.58f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.12f, 0.22f, 0.36f, 0.78f));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.18f, 0.30f, 0.50f, 0.90f));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.22f, 0.38f, 0.62f, 0.98f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 4.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 5.0f));

        ImGui::SetNextWindowPos(ImVec2(contentPos.x, contentPos.y + HEADER_H), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(CONTENT_W, contentH - HEADER_H), ImGuiCond_Always);
        ImGui::Begin(
            "##ns_content",
            nullptr,
            ImGuiWindowFlags_NoTitleBar |
                ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoMove |
                ImGuiWindowFlags_NoSavedSettings |
                ImGuiWindowFlags_NoCollapse |
                ImGuiWindowFlags_NoBackground);

        if (activePluginMap < 0) {
            DrawCoreContentPanel(activeSecondaryIdx);
        } else {
            DrawPluginContentPanel(activePluginMap, activeSecondaryIdx);
        }

        ImGui::End();
        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor(14);

        menuBoundsRight = menuPosX + PRIMARY_W + PANEL_GAP + SECONDARY_W + PANEL_GAP + CONTENT_W;
        menuBoundsBottom = menuPosY + contentH;
    }

} // namespace NightSharpMenu
