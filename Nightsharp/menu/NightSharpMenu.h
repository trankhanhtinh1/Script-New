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
#include "../FpsDropDebug.h"
#include "MenuConfig.h"
#include "ConfigStore.h"
#include "EnsoulSharpMenuTheme.h"

#include <cstdint>
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
    constexpr float ITEM_H = 30.0f;
    constexpr float HEADER_H = 32.0f;
    constexpr float PANEL_GAP = 0.0f;
    // Functional content panel: size follows content only (no fixed 560/620).
    constexpr float CONTENT_MIN_W = 200.0f;
    constexpr float CONTENT_PAD_X = 0.0f; // flush rows like sidebar
    constexpr float CONTENT_PAD_Y = 0.0f;
    constexpr float CONTENT_MAX_H_RATIO = 0.70f; // max 70% of screen height

    // Cached body size from the previous frame (height refined by measure; width estimated).
    inline float contentPanelW = CONTENT_MIN_W;
    inline float contentPanelBodyH = ITEM_H;
    inline int contentPanelSizeKey = -1;

    // Margin from the screen edges when auto-positioning PermaShow.
    constexpr float PERMASHOW_EDGE_MARGIN = 18.0f;

    inline ImU32 COL_BG = IM_COL32(8, 10, 18, 255);
    inline ImU32 COL_CONTENT_BG = IM_COL32(8, 10, 18, 255);
    inline ImU32 COL_HEADER = IM_COL32(16, 18, 28, 255);
    inline ImU32 COL_ITEM = IM_COL32(18, 20, 30, 118);
    inline ImU32 COL_ITEM_HOVER = IM_COL32(52, 48, 82, 215);
    inline ImU32 COL_ITEM_ACTIVE = IM_COL32(82, 66, 132, 232);
    inline ImU32 COL_ACCENT = IM_COL32(120, 235, 120, 255);
    inline ImU32 COL_TEXT = IM_COL32(255, 255, 255, 255);
    inline ImU32 COL_TEXT_DIM = IM_COL32(185, 185, 205, 255);
    inline ImU32 COL_BORDER = IM_COL32(88, 100, 148, 255);

    // Core controls used by the new EnsoulSharp-style tree.  Plugin menus
    // already attach themselves to MenuManager; these entries preserve the
    // NightSharp settings and plugin manager that previously lived only in the
    // old three-column renderer.
    inline SDK::UI::Menu* ensoulCoreRoot = nullptr;
    inline SDK::UI::MenuList* ensoulLanguage = nullptr;
    inline SDK::UI::MenuBool* ensoulSkinChanger = nullptr;
    inline SDK::UI::MenuSlider* ensoulSkinId = nullptr;
    inline SDK::UI::MenuBool* ensoulZoomHack = nullptr;
    inline SDK::UI::MenuSliderF* ensoulMaxZoom = nullptr;
    inline SDK::UI::MenuBool* ensoulPermaShow = nullptr;
    inline SDK::UI::MenuBool* ensoulBypassObs = nullptr;
    inline SDK::UI::MenuBool* ensoulProfiler = nullptr;
    inline SDK::UI::MenuBool* ensoulProfilerLog = nullptr;

    struct PluginMenuBinding {
        int registryIndex = -1;
        SDK::UI::MenuBool* loaded = nullptr;
        SDK::UI::MenuBool* alwaysLoad = nullptr;
    };

    inline PluginMenuBinding ensoulPluginBindings[PluginRegistry::MAX_PLUGINS] = {};
    inline int ensoulPluginBindingCount = 0;

    inline int CoreLanguageToListIndex() {
        return Config::Language::index == 2 ? 1 : 0;
    }

    inline void OnCoreLanguageChanged(SDK::UI::MenuItem* sender, void*) {
        auto* item = static_cast<SDK::UI::MenuList*>(sender);
        // Chinese is intentionally not exposed: 0 = English, 1 = Vietnamese.
        Config::Language::index = item->Index == 1 ? 2 : 0;
    }

    inline void OnCoreBoolChanged(SDK::UI::MenuItem* sender, void* userData) {
        const bool value = static_cast<SDK::UI::MenuBool*>(sender)->Value;
        switch (static_cast<int>(reinterpret_cast<intptr_t>(userData))) {
        case 1: Config::SkinChanger::enabled = value; break;
        case 2: Config::ZoomHack::enabled = value; break;
        case 3: Config::PermaShow::enabled = value; break;
        case 4: Config::StreamProtection::bypassObs = value; break;
        case 5: NightSharpPerf::Enabled = value; break;
        case 6: NightSharpPerf::LogEnabled = value; break;
        default: break;
        }
    }

    inline void OnCoreSkinIdChanged(SDK::UI::MenuItem* sender, void*) {
        Config::SkinChanger::skinId =
            static_cast<SDK::UI::MenuSlider*>(sender)->Value;
    }

    inline void OnCoreZoomChanged(SDK::UI::MenuItem* sender, void*) {
        Config::ZoomHack::maxZoom =
            static_cast<SDK::UI::MenuSliderF*>(sender)->Value;
    }

    inline int PluginIndexFromUserData(void* userData) {
        return static_cast<int>(reinterpret_cast<intptr_t>(userData)) - 1;
    }

    inline void OnPluginLoadedChanged(SDK::UI::MenuItem* sender, void* userData) {
        auto* item = static_cast<SDK::UI::MenuBool*>(sender);
        const int index = PluginIndexFromUserData(userData);
        if (index < 0 || index >= PluginRegistry::PluginCount) {
            return;
        }
        if (item->Value) {
            if (PluginRegistry::CanPluginLoad(index)) {
                PluginRegistry::LoadPlugin(index);
            }
        } else {
            PluginRegistry::UnloadPlugin(index);
        }
        // A guarded runtime load can fail; always reflect the registry result.
        item->Value = PluginRegistry::Plugins[index].Loaded;
    }

    inline void OnPluginAlwaysLoadChanged(SDK::UI::MenuItem* sender, void* userData) {
        const int index = PluginIndexFromUserData(userData);
        if (index < 0 || index >= PluginRegistry::PluginCount) {
            return;
        }
        PluginRegistry::SetAlwaysLoad(
            index,
            static_cast<SDK::UI::MenuBool*>(sender)->Value);
    }

    inline void BindCoreBool(SDK::UI::MenuBool* item, int settingId) {
        if (!item) return;
        item->ValueChanged = &OnCoreBoolChanged;
        item->ValueChangedUd = reinterpret_cast<void*>(static_cast<intptr_t>(settingId));
    }

    inline void EnsureEnsoulCoreMenu() {
        if (ensoulCoreRoot) {
            return;
        }

        ensoulCoreRoot = new SDK::UI::Menu("nightsharp.core", "NightSharp", true);

        auto* language = ensoulCoreRoot->AddSubMenu(
            new SDK::UI::Menu("Language", "Language"));
        ensoulLanguage = language->Add(new SDK::UI::MenuList(
            "SelectedLanguage",
            "Select Language",
            { "English", "Vietnamese" },
            CoreLanguageToListIndex()));
        ensoulLanguage->ValueChanged = &OnCoreLanguageChanged;

        auto* settings = ensoulCoreRoot->AddSubMenu(
            new SDK::UI::Menu("Menu", "Menu Settings"));
        ensoulSkinChanger = settings->Add(new SDK::UI::MenuBool(
            "SkinChanger", "Skin Changer", Config::SkinChanger::enabled));
        BindCoreBool(ensoulSkinChanger, 1);
        ensoulSkinId = settings->Add(new SDK::UI::MenuSlider(
            "SkinId", "Skin ID", Config::SkinChanger::skinId, 0, 100));
        ensoulSkinId->ValueChanged = &OnCoreSkinIdChanged;
        ensoulZoomHack = settings->Add(new SDK::UI::MenuBool(
            "ZoomHack", "Zoom Hack", Config::ZoomHack::enabled));
        BindCoreBool(ensoulZoomHack, 2);
        ensoulMaxZoom = settings->Add(new SDK::UI::MenuSliderF(
            "MaxZoom", "Maximum Zoom", Config::ZoomHack::maxZoom, 1000.0f, 10000.0f));
        ensoulMaxZoom->ValueChanged = &OnCoreZoomChanged;
        ensoulPermaShow = settings->Add(new SDK::UI::MenuBool(
            "PermaShow", "PermaShow", Config::PermaShow::enabled));
        BindCoreBool(ensoulPermaShow, 3);
        ensoulBypassObs = settings->Add(new SDK::UI::MenuBool(
            "BypassObs", "Bypass OBS", Config::StreamProtection::bypassObs));
        BindCoreBool(ensoulBypassObs, 4);

        auto* debug = ensoulCoreRoot->AddSubMenu(
            new SDK::UI::Menu("DebugInfo", "Debug Info"));
        debug->Add(new SDK::UI::MenuSeparator("Build", "NightSharp " __DATE__ " " __TIME__));
        debug->Add(new SDK::UI::MenuSeparator("Theme", "EnsoulSharp default DX9 theme"));
        ensoulProfiler = debug->Add(new SDK::UI::MenuBool(
            "Profiler", "Performance Profiler", NightSharpPerf::Enabled));
        BindCoreBool(ensoulProfiler, 5);
        ensoulProfilerLog = debug->Add(new SDK::UI::MenuBool(
            "ProfilerLog", "Write profiler log", NightSharpPerf::LogEnabled));
        BindCoreBool(ensoulProfilerLog, 6);

        auto* plugins = ensoulCoreRoot->AddSubMenu(
            new SDK::UI::Menu("Plugins", "Plugins"));
        ensoulPluginBindingCount = 0;
        for (int i = 0;
             i < PluginRegistry::PluginCount &&
             ensoulPluginBindingCount < PluginRegistry::MAX_PLUGINS;
             ++i) {
            auto& plugin = PluginRegistry::Plugins[i];
            if (plugin.Kind == PluginRegistry::PluginKind::SDK ||
                !plugin.Name || !plugin.InternalId) {
                continue;
            }
            if (plugin.Category == PluginRegistry::PluginCategory::Champion &&
                !PluginRegistry::CanPluginLoad(i)) {
                continue;
            }

            auto* pluginMenu = plugins->AddSubMenu(
                new SDK::UI::Menu(plugin.InternalId, plugin.Name));
            auto* loaded = pluginMenu->Add(new SDK::UI::MenuBool(
                "Loaded", "Loaded", plugin.Loaded));
            auto* alwaysLoad = pluginMenu->Add(new SDK::UI::MenuBool(
                "AlwaysLoad", "Always Load", plugin.AlwaysLoad));
            const void* encodedIndex =
                reinterpret_cast<void*>(static_cast<intptr_t>(i + 1));
            loaded->ValueChanged = &OnPluginLoadedChanged;
            loaded->ValueChangedUd = const_cast<void*>(encodedIndex);
            alwaysLoad->ValueChanged = &OnPluginAlwaysLoadChanged;
            alwaysLoad->ValueChangedUd = const_cast<void*>(encodedIndex);
            ensoulPluginBindings[ensoulPluginBindingCount++] = {
                i, loaded, alwaysLoad
            };
        }

        ensoulCoreRoot->Attach();

        // Keep NightSharp first, matching the previous menu's Core-first order.
        auto& menus = SDK::UI::MenuManager::Instance().Menus;
        if (menus.size() > 1 && menus[menus.size() - 1] == ensoulCoreRoot) {
            for (int i = menus.size() - 1; i > 0; --i) {
                menus[i] = menus[i - 1];
            }
            menus[0] = ensoulCoreRoot;
        }
    }

    inline void SyncEnsoulCoreMenu() {
        if (!ensoulCoreRoot) return;
        ensoulLanguage->Index = CoreLanguageToListIndex();
        ensoulSkinChanger->Value = Config::SkinChanger::enabled;
        ensoulSkinId->Value = Config::SkinChanger::skinId;
        ensoulZoomHack->Value = Config::ZoomHack::enabled;
        ensoulMaxZoom->Value = Config::ZoomHack::maxZoom;
        ensoulPermaShow->Value = Config::PermaShow::enabled;
        ensoulBypassObs->Value = Config::StreamProtection::bypassObs;
        ensoulProfiler->Value = NightSharpPerf::Enabled;
        ensoulProfilerLog->Value = NightSharpPerf::LogEnabled;
        for (int i = 0; i < ensoulPluginBindingCount; ++i) {
            auto& binding = ensoulPluginBindings[i];
            if (binding.registryIndex < 0 ||
                binding.registryIndex >= PluginRegistry::PluginCount) {
                continue;
            }
            const auto& plugin = PluginRegistry::Plugins[binding.registryIndex];
            binding.loaded->Value = plugin.Loaded;
            binding.alwaysLoad->Value = plugin.AlwaysLoad;
        }
    }

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

        if (ImGui::GetCurrentContext() && ImGui::GetIO().WantCaptureMouse) {
            return true;
        }

        const bool insideMenu = EnsoulSharpTheme::ContainsPoint(x, y);

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
            dl->AddRectFilled(mn, mx, isActive ? COL_ITEM_ACTIVE : COL_ITEM_HOVER, 0.0f);
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
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
        bool clicked = ImGui::Button(label, ImVec2(width, 0));
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(5);
        ImGui::PopID();
        return clicked;
    }

    inline bool DrawOnOffEditor(const char* label, bool& value, const char* id) {
        bool changed = false;
        SDK::UI::BeginFunctionalMenuRow(id);
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
        SDK::UI::EndFunctionalMenuRow();
        return changed;
    }

    inline void DrawSectionTitle(const char* title) {
        ImGui::TextColored(ImVec4(0.47f, 0.92f, 0.47f, 1.0f), "%s", title);
        ImGui::Separator();
    }

    inline void DrawLanguageSection() {
        DrawSectionTitle("Language");
        const char* langs[] = { "EN", "CN", "VN" };

        SDK::UI::BeginFunctionalMenuRow("##lang_row");
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Select Language");
        ImGui::SameLine();
        float targetX = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - 186.0f;
        if (targetX > ImGui::GetCursorPosX()) {
            ImGui::SetCursorPosX(targetX);
        }

        for (int i = 0; i < 3; ++i) {
            ImGui::PushID(langs[i]);
            if (DrawStateButton(langs[i], langs[i], Config::Language::index == i, true, 56.0f)) {
                Config::Language::index = i;
            }
            ImGui::PopID();
            if (i < 2) {
                ImGui::SameLine(0, 8);
            }
        }
        SDK::UI::EndFunctionalMenuRow();
    }

    inline void DrawMenuSection() {
        DrawSectionTitle("Menu Settings");
        DrawOnOffEditor("Skin Changer", Config::SkinChanger::enabled, "menu_skin");
        if (Config::SkinChanger::enabled) {
            SDK::UI::BeginFunctionalMenuRow("##skin_id_row");
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("Skin ID");
            ImGui::SameLine();
            float targetX = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - 120.0f;
            if (targetX > ImGui::GetCursorPosX()) {
                ImGui::SetCursorPosX(targetX);
            }
            ImGui::SetNextItemWidth(120.0f);
            ImGui::SliderInt("##skin_id", &Config::SkinChanger::skinId, 0, 100, "%d");
            SDK::UI::EndFunctionalMenuRow();

            SDK::UI::BeginFunctionalMenuRow("##skin_champ_row");
            ImGui::AlignTextToFramePadding();
            const std::string& champ = ConfigStore::g_skinChampion;
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.6f, 1.0f), "Skin luu rieng theo tuong: %s",
                               champ.empty() ? "(chua vao game)" : champ.c_str());
            SDK::UI::EndFunctionalMenuRow();
        }

        DrawOnOffEditor("Zoom Hack", Config::ZoomHack::enabled, "zoom_hack");
        if (Config::ZoomHack::enabled) {
            SDK::UI::BeginFunctionalMenuRow("##zoom_hack_info");
            ImGui::AlignTextToFramePadding();
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.6f, 1.0f), "Lan chuot de zoom xa tuy y");
            SDK::UI::EndFunctionalMenuRow();
        }

        DrawOnOffEditor("PermaShow", Config::PermaShow::enabled, "perma_show");
        DrawOnOffEditor("Bypass OBS", Config::StreamProtection::bypassObs, "bypass_obs");

        SDK::UI::BeginFunctionalMenuRow("##bypass_obs_info1");
        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.6f, 1.0f), "Bypass OBS: overlay hidden from screen capture");
        SDK::UI::EndFunctionalMenuRow();

        SDK::UI::BeginFunctionalMenuRow("##bypass_obs_info2");
        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.6f, 1.0f), "(requires Win10 2004+)");
        SDK::UI::EndFunctionalMenuRow();

        SDK::UI::BeginFunctionalMenuRow("##bypass_obs_info3");
        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.6f, 1.0f), "Click-through always on: clicks pass through the overlay.");
        SDK::UI::EndFunctionalMenuRow();
    }

    // Performance profiler panel, embedded in the Debug Info section.
    // ON/OFF drives NightSharpPerf::Enabled (the master switch that gates all
    // frame/plugin/event timing collection — zero overhead while OFF). When ON,
    // a sub-panel shows the live stats plus a toggle to append them to a txt log.
    inline void DrawProfilerSection() {
        ImGui::Separator();
        DrawSectionTitle("Performance Profiler");

        DrawOnOffEditor("Profiler", NightSharpPerf::Enabled, "perf_profiler");
        if (!NightSharpPerf::Enabled) {
            SDK::UI::BeginFunctionalMenuRow("##perf_info");
            ImGui::AlignTextToFramePadding();
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.6f, 1.0f),
                               "Bat de do frame/plugin/event timing. Tat = 0 overhead.");
            SDK::UI::EndFunctionalMenuRow();
            return;
        }

        DrawOnOffEditor("Ghi log ra file (.txt)", NightSharpPerf::LogEnabled, "perf_log");

        SDK::UI::BeginFunctionalMenuRow("##perf_log_info");
        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.6f, 1.0f),
                           "Log: C:\\Users\\Public\\nightsharp_fps_drop_debug.txt");
        SDK::UI::EndFunctionalMenuRow();

        ImGui::Separator();
        NightSharpPerf::DrawStatsBody();
    }

    inline void DrawDebugSection() {
        DrawSectionTitle("Debug Info");

        SDK::UI::BeginFunctionalMenuRow("##debug1");
        ImGui::AlignTextToFramePadding();
        ImGui::Text("NightSharp");
        SDK::UI::EndFunctionalMenuRow();

        SDK::UI::BeginFunctionalMenuRow("##debug2");
        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(ImVec4(0.55f, 0.85f, 0.55f, 1.0f), "Build: %s %s", __DATE__, __TIME__);
        SDK::UI::EndFunctionalMenuRow();

        SDK::UI::BeginFunctionalMenuRow("##debug3");
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Overlay: D3D11 External");
        SDK::UI::EndFunctionalMenuRow();

        SDK::UI::BeginFunctionalMenuRow("##debug4");
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Menu: Old sidebar style");
        SDK::UI::EndFunctionalMenuRow();

        SDK::UI::BeginFunctionalMenuRow("##debug5");
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Input: %s", Config::OverlayInput::clickThrough ? "Click-through" : "Menu capture");
        SDK::UI::EndFunctionalMenuRow();

        SDK::UI::BeginFunctionalMenuRow("##debug6");
        ImGui::AlignTextToFramePadding();
        ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
        SDK::UI::EndFunctionalMenuRow();

        if (ImGui::GetIO().Framerate > 0.0f) {
            SDK::UI::BeginFunctionalMenuRow("##debug7");
            ImGui::AlignTextToFramePadding();
            ImGui::Text("Frame: %.3f ms", 1000.0f / ImGui::GetIO().Framerate);
            SDK::UI::EndFunctionalMenuRow();
        }

        DrawProfilerSection();
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
                SDK::UI::BeginFunctionalMenuRow("##empty_row");
                ImGui::AlignTextToFramePadding();
                ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.6f, 1.0f), "%s", emptyText);
                SDK::UI::EndFunctionalMenuRow();
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

            SDK::UI::BeginFunctionalMenuRow("##plugin_row");

            ImVec4 statusColor = !canLoad
                ? ImVec4(0.75f, 0.55f, 0.18f, 1.0f)
                : (p.Loaded ? ImVec4(0.30f, 0.86f, 0.34f, 1.0f) : ImVec4(0.62f, 0.64f, 0.70f, 1.0f));
            ImGui::AlignTextToFramePadding();
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

            SDK::UI::EndFunctionalMenuRow();

            if (!p.RuntimeMenu && p.Loaded) {
                SDK::UI::BeginFunctionalMenuRow("##no_menu_callback");
                ImGui::AlignTextToFramePadding();
                ImGui::TextColored(ImVec4(0.55f, 0.55f, 0.64f, 1.0f), "  No custom ImGui menu callback.");
                SDK::UI::EndFunctionalMenuRow();
            }

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

        // Draw the label on the far left
        dl->AddText(ImVec2(x + padding, currentY), color, label);

        if (hasState) {
            // Far-right aligned box width (75 pixels fits "False" beautifully)
            const float boxWidth = 75.0f;
            const float boxRight = x + width - padding;
            const float boxLeft = boxRight - boxWidth;

            if (boxLeft > x + padding) {
                // Background color box: green for true, red for false
                dl->AddRectFilled(
                    ImVec2(boxLeft, currentY + 1.0f),
                    ImVec2(boxRight, currentY + lineHeight - 2.0f),
                    stateOn ? IM_COL32(0, 140, 60, 255) : IM_COL32(170, 40, 40, 255),
                    0.0f);

                // Draw status text inside the box (centered, white text color for high contrast)
                const ImVec2 valSize = ImGui::CalcTextSize(value);
                const float textX = boxLeft + (boxWidth - valSize.x) * 0.5f;
                const float textY = currentY + (lineHeight - valSize.y) * 0.5f;
                dl->AddText(ImVec2(textX, textY - 1.0f), IM_COL32(255, 255, 255, 255), value);
            }
        } else {
            // Draw regular values (numeric/text/sliders) right-aligned on the far right
            const ImVec2 valueSize = ImGui::CalcTextSize(value);
            const float valueX = x + width - padding - valueSize.x;
            dl->AddText(ImVec2(valueX, currentY), color, value);
        }

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

        dl->AddRectFilled(ImVec2(x, y), ImVec2(x + width, y + totalHeight), IM_COL32(18, 20, 26, 178), 0.0f);
        dl->AddRect(ImVec2(x, y), ImVec2(x + width, y + totalHeight), COL_BORDER, 0.0f);
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
            // Same sidebar cell chrome as Menu::DrawChildren (rect + border per item).
            if (SDK::UI::g_FunctionalMenuStyle.enabled) {
                SDK::UI::BeginFunctionalMenuRow(component);
                component->DrawImGui();
                SDK::UI::EndFunctionalMenuRow();
            } else {
                component->DrawImGui();
            }
            drewAny = true;
        }

        if (!drewAny) {
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.6f, 1.0f), "No items in this section.");
        }
    }

    inline void ApplyFunctionalMenuSidebarStyle() {
        auto& s = SDK::UI::g_FunctionalMenuStyle;
        s.enabled = true;
        s.itemHeight = ITEM_H;
        s.padX = 12.0f;
        s.colItem = COL_ITEM;
        s.colItemHover = COL_ITEM_HOVER;
        s.colItemActive = COL_ITEM_ACTIVE;
        s.colBorder = COL_BORDER;
        s.colText = COL_TEXT;
        s.colTextDim = COL_TEXT_DIM;
        s.colAccent = COL_ACCENT;
    }

    inline void ClearFunctionalMenuSidebarStyle() {
        SDK::UI::g_FunctionalMenuStyle.enabled = false;
        SDK::UI::g_FunctionalMenuRowNest = 0;
    }

    // Preferred control width for each functional menu item kind (matches UI.h DrawImGui).
    inline float PreferredControlWidthForItem(const SDK::UI::AMenuComponent* component) {
        if (!component || component->IsMenu()) {
            return 0.0f;
        }

        switch (component->Kind()) {
            case SDK::UI::MenuValueType::Boolean:   return 86.0f;
            case SDK::UI::MenuValueType::Slider:    return 280.0f;
            case SDK::UI::MenuValueType::SliderF:   return 280.0f;
            case SDK::UI::MenuValueType::KeyBind:   return 300.0f;
            case SDK::UI::MenuValueType::List:      return 212.0f;
            case SDK::UI::MenuValueType::Button:    return 136.0f;
            case SDK::UI::MenuValueType::Color:     return 136.0f;
            case SDK::UI::MenuValueType::Separator: return 0.0f;
            case SDK::UI::MenuValueType::SliderBtn: return 320.0f;
            default:                               return 160.0f;
        }
    }

    inline void AccumulateMenuContentMetrics(
        const SDK::UI::AMenuComponent* component,
        float& maxRowW,
        int& rowCount,
        int depth = 0) {
        if (!component || !component->Visible) {
            return;
        }

        const float indentAmount = 16.0f;
        const float currentIndent = static_cast<float>(depth) * indentAmount;

        const char* label = component->DisplayName.c_str();
        const float labelW = ImGui::CalcTextSize(label ? label : "").x;

        if (component->IsMenu()) {
            // Collapsing header row + nested children (headers default open).
            maxRowW = MaxF(maxRowW, labelW + 24.0f + currentIndent);
            ++rowCount;
            const auto* menu = static_cast<const SDK::UI::Menu*>(component);
            for (int i = 0; i < menu->Components.size(); ++i) {
                AccumulateMenuContentMetrics(menu->Components[i], maxRowW, rowCount, depth + 1);
            }
            return;
        }

        const float controlW = PreferredControlWidthForItem(component);
        const float rowW = labelW + (controlW > 0.0f ? (8.0f + controlW) : 0.0f) + currentIndent;
        maxRowW = MaxF(maxRowW, rowW);
        ++rowCount;
    }

    // Estimate functional panel size from an SDK menu section (content-driven).
    inline void EstimateSdkMenuContentSize(SDK::UI::Menu* menu, bool leafOnly, float& outW, float& outBodyH) {
        float maxRowW = 120.0f;
        int rowCount = 0;

        if (menu) {
            for (int i = 0; i < menu->Components.size(); ++i) {
                auto* component = menu->Components[i];
                if (!component || !component->Visible) {
                    continue;
                }
                if (leafOnly && component->IsMenu()) {
                    continue;
                }
                AccumulateMenuContentMetrics(component, maxRowW, rowCount);
            }
        }

        if (rowCount <= 0) {
            rowCount = 1;
            maxRowW = MaxF(maxRowW, 180.0f);
        }

        // Width = longest label+control; height = exact item rows (sidebar ITEM_H each).
        outW = maxRowW + 24.0f + 8.0f; // side pad (12*2) + scrollbar slack
        outBodyH = static_cast<float>(rowCount) * ITEM_H;
    }

    inline void EstimateCoreContentSize(int secondaryIdx, float& outW, float& outBodyH) {
        // Core panels are hand-built; estimate from known sections (refined by measured height).
        int rows = 4;
        float rowW = 280.0f;
        if (secondaryIdx == 0) {
            rows = 3;
            rowW = 220.0f;
        } else if (secondaryIdx == 1) {
            rows = 10;
            rowW = 300.0f;
        } else if (secondaryIdx == 2) {
            rows = 14;
            rowW = 360.0f;
        } else if (secondaryIdx == 3) {
            rows = MaxI(4, PluginRegistry::PluginCount + 3);
            rowW = 420.0f;
        }
        outW = rowW + 24.0f;
        outBodyH = static_cast<float>(rows) * ITEM_H;
    }

    inline float ClampContentWidth(float width) {
        const ImVec2 display = GetOverlayDisplaySize();
        const float maxW = MaxF(CONTENT_MIN_W, display.x * 0.55f);
        if (width < CONTENT_MIN_W) {
            return CONTENT_MIN_W;
        }
        if (width > maxW) {
            return maxW;
        }
        return width;
    }

    inline float GetContentMaxTotalHeight() {
        return GetOverlayDisplaySize().y * CONTENT_MAX_H_RATIO;
    }

    // Match functional controls to sidebar row height (ITEM_H) and default font.
    // Slight inset so widgets sit inside the bordered cell without touching edges.
    inline void PushFunctionalMenuItemStyle() {
        const float fontSize = ImGui::GetFontSize();
        const float innerH = ITEM_H - 4.0f;
        float padY = (innerH - fontSize) * 0.5f;
        if (padY < 1.0f) {
            padY = 1.0f;
        }
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6.0f, padY));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
    }

    inline void PopFunctionalMenuItemStyle() {
        ImGui::PopStyleVar(4);
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
        // Persist any pending menu/core changes (debounced). Runs even while the
        // menu is hidden so a change made just before hiding still flushes.
        ConfigStore::Tick();
        EnsureEnsoulCoreMenu();
        SyncEnsoulCoreMenu();

        DrawPermaShowOverlay();

        if (!showMenu) {
            menuBoundsRight = menuPosX;
            menuBoundsBottom = menuPosY;
            return;
        }

        // The SDK menu is rendered by the default EnsoulSharp theme: a root
        // column at (30, 30), recursive child columns, and 30px component rows.
        EnsoulSharpTheme::Render();
        menuPosX = EnsoulSharpTheme::PositionX;
        menuPosY = EnsoulSharpTheme::PositionY;
        menuBoundsRight = EnsoulSharpTheme::BoundsRight;
        menuBoundsBottom = EnsoulSharpTheme::BoundsBottom;
        return;

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

        // Content panel width is content-driven (cached / estimated). Height is capped later.
        float totalW = PRIMARY_W;
        if (showSecondary) {
            totalW += PANEL_GAP + SECONDARY_W;
        }
        if (showContent) {
            totalW += PANEL_GAP + contentPanelW;
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

        ImVec2 primaryPos(static_cast<float>((int)menuPosX), static_cast<float>((int)menuPosY));
        ImVec2 secondaryPos(static_cast<float>((int)(menuPosX + PRIMARY_W + PANEL_GAP)), static_cast<float>((int)menuPosY));
        ImVec2 contentPos(static_cast<float>((int)(menuPosX + PRIMARY_W + SECONDARY_W + PANEL_GAP * 2.0f)), static_cast<float>((int)menuPosY));

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

        // Primary sidebar.
        dl->AddRectFilled(
            primaryPos,
            ImVec2(primaryPos.x + PRIMARY_W, primaryPos.y + primaryH),
            COL_BG,
            0.0f);
        dl->AddRectFilled(
            primaryPos,
            ImVec2(primaryPos.x + PRIMARY_W, primaryPos.y + HEADER_H),
            COL_HEADER,
            0.0f);
        dl->AddRect(
            primaryPos,
            ImVec2(primaryPos.x + PRIMARY_W, primaryPos.y + primaryH),
            COL_BORDER,
            0.0f);
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
                if (activePrimaryIdx == i) {
                    activePrimaryIdx = -1;
                    activeSecondaryIdx = -1;
                    primarySelected = false;
                    secondarySelected = false;
                } else {
                    activePrimaryIdx = i;
                    activeSecondaryIdx = -1;
                    secondarySelected = false;
                    primarySelected = true;
                    activePluginIdx = primaryPluginMap[i];
                }
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

        // Secondary sidebar.
        dl->AddRectFilled(
            secondaryPos,
            ImVec2(secondaryPos.x + SECONDARY_W, secondaryPos.y + secondaryH),
            COL_BG,
            0.0f);
        dl->AddRectFilled(
            secondaryPos,
            ImVec2(secondaryPos.x + SECONDARY_W, secondaryPos.y + HEADER_H),
            COL_HEADER,
            0.0f);
        dl->AddRect(
            secondaryPos,
            ImVec2(secondaryPos.x + SECONDARY_W, secondaryPos.y + secondaryH),
            COL_BORDER,
            0.0f);

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
                if (activeSecondaryIdx == i) {
                    activeSecondaryIdx = -1;
                    secondarySelected = false;
                } else {
                    activeSecondaryIdx = i;
                    secondarySelected = true;
                }
            }
            y += ITEM_H;
        }

        if (!(secondarySelected && activeSecondaryIdx >= 0)) {
            menuBoundsRight = menuPosX + PRIMARY_W + PANEL_GAP + SECONDARY_W;
            menuBoundsBottom = menuPosY + sidebarH;
            return;
        }

        // Content panel — pure content size (no fixed 620). Max height = 70% screen.
        // Each functional item is a sidebar-style rectangle with border.
        const int sizeKey = (activePluginMap + 1) * 1000 + activeSecondaryIdx;
        if (sizeKey != contentPanelSizeKey) {
            contentPanelSizeKey = sizeKey;
            float estW = CONTENT_MIN_W;
            float estBodyH = ITEM_H;
            if (activePluginMap < 0) {
                EstimateCoreContentSize(activeSecondaryIdx, estW, estBodyH);
            } else {
                SDK::UI::Menu* section = GetPluginSecondaryMenu(activePluginMap, activeSecondaryIdx);
                const bool leafOnly =
                    IsPluginLeafSection(activePluginMap, activeSecondaryIdx) ||
                    (!section && CountRootSubMenus(FindPluginSdkMenu(activePluginMap)) <= 0);
                SDK::UI::Menu* measureMenu = section
                    ? section
                    : (leafOnly ? FindPluginSdkMenu(activePluginMap) : nullptr);
                EstimateSdkMenuContentSize(measureMenu, leafOnly && !section, estW, estBodyH);
            }
            contentPanelW = ClampContentWidth(estW);
            contentPanelBodyH = MaxF(ITEM_H, estBodyH);
        }

        const float maxTotalH = GetContentMaxTotalHeight();
        const float maxBodyH = MaxF(ITEM_H, maxTotalH - HEADER_H);
        // Height follows content only — never pad up to a fixed panel size.
        const float idealBodyH = MaxF(ITEM_H, contentPanelBodyH);
        const bool contentScroll = idealBodyH > maxBodyH + 0.5f;
        const float bodyH = contentScroll ? maxBodyH : idealBodyH;
        const float contentH = HEADER_H + bodyH;
        const float contentW = contentPanelW;

        dl->AddRectFilled(
            contentPos,
            ImVec2(contentPos.x + contentW, contentPos.y + HEADER_H),
            COL_HEADER,
            0.0f);
        dl->AddRect(
            contentPos,
            ImVec2(contentPos.x + contentW, contentPos.y + contentH),
            COL_BORDER,
            0.0f);

        const char* sectionLabel = "?";
        if (activePluginMap < 0 && activeSecondaryIdx < CORE_SECONDARY_COUNT) {
            sectionLabel = CORE_SECONDARY[activeSecondaryIdx].label;
        } else if (activePluginMap >= 0) {
            sectionLabel = GetPluginSecondaryLabel(activePluginMap, activeSecondaryIdx);
        }

        dl->AddText(ImVec2(contentPos.x + 10.0f, contentPos.y + 8.0f), COL_ACCENT, sectionLabel);
        dl->AddLine(
            ImVec2(contentPos.x, contentPos.y + HEADER_H),
            ImVec2(contentPos.x + contentW, contentPos.y + HEADER_H),
            COL_BORDER);

        // Sidebar colors as the primary look for every functional control.
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(8.0f / 255.0f, 10.0f / 255.0f, 18.0f / 255.0f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(18.0f / 255.0f, 20.0f / 255.0f, 30.0f / 255.0f, 180.0f / 255.0f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(52.0f / 255.0f, 48.0f / 255.0f, 82.0f / 255.0f, 215.0f / 255.0f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(82.0f / 255.0f, 66.0f / 255.0f, 132.0f / 255.0f, 232.0f / 255.0f));
        ImGui::PushStyleColor(ImGuiCol_SliderGrab, ImVec4(120.0f / 255.0f, 235.0f / 255.0f, 120.0f / 255.0f, 0.85f));
        ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, ImVec4(120.0f / 255.0f, 235.0f / 255.0f, 120.0f / 255.0f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(18.0f / 255.0f, 20.0f / 255.0f, 30.0f / 255.0f, 200.0f / 255.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(52.0f / 255.0f, 48.0f / 255.0f, 82.0f / 255.0f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(82.0f / 255.0f, 66.0f / 255.0f, 132.0f / 255.0f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(18.0f / 255.0f, 20.0f / 255.0f, 30.0f / 255.0f, 200.0f / 255.0f));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(52.0f / 255.0f, 48.0f / 255.0f, 82.0f / 255.0f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(82.0f / 255.0f, 66.0f / 255.0f, 132.0f / 255.0f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_GrabRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
        PushFunctionalMenuItemStyle();
        ApplyFunctionalMenuSidebarStyle();

        ImGui::SetNextWindowPos(ImVec2(contentPos.x, contentPos.y + HEADER_H), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(contentW, bodyH), ImGuiCond_Always);
        ImGuiWindowFlags contentFlags =
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoFocusOnAppearing;
        if (!contentScroll) {
            contentFlags |= ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
        }

        ImGui::Begin("##ns_content", nullptr, contentFlags);

        if (activePluginMap < 0) {
            DrawCoreContentPanel(activeSecondaryIdx);
        } else {
            DrawPluginContentPanel(activePluginMap, activeSecondaryIdx);
        }

        // Height = laid-out content only (row count * ITEM_H), not a fixed panel.
        const float measuredBodyH = ImGui::GetCursorPosY() + 4.0f;
        if (measuredBodyH > 0.5f) {
            contentPanelBodyH = MaxF(contentPanelBodyH, measuredBodyH);
        }

        ImGui::End();
        ClearFunctionalMenuSidebarStyle();
        PopFunctionalMenuItemStyle();
        ImGui::PopStyleVar(7);
        ImGui::PopStyleColor(14);

        menuBoundsRight = menuPosX + PRIMARY_W + PANEL_GAP + SECONDARY_W + PANEL_GAP + contentW;
        menuBoundsBottom = menuPosY + contentH;
    }

} // namespace NightSharpMenu
