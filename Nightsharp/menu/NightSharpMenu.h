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
#include "../SDK/UI/DrawingVisibility.h"
#include "../SDK/UI/UI.h"
#include "../SDK/UI/PermaShow.h"
#include "../FpsDropDebug.h"
#include "MenuConfig.h"
#include "ConfigStore.h"
#include "EnsoulSharpMenuTheme.h"
#include "BgxUbuntuFont.h"

#include <Windows.h>
#include <cstdint>
#include <cstdio>

namespace NightSharpMenu {

    inline ImFont* InstallBgxFont(float sizePixels = 16.0f) {
        ImGuiIO& io = ImGui::GetIO();
        ImFontConfig config{};
        config.OversampleH = 2;
        config.OversampleV = 1;
        config.PixelSnapH = true;
        config.RasterizerMultiply = 1.0f;
        config.FontDataOwnedByAtlas = false; // Do not free static memory

        ImFont* font = io.Fonts->AddFontFromMemoryTTF(
            const_cast<unsigned char*>(BgxEmbeddedFont::BgxUbuntuFont_raw_data),
            static_cast<int>(BgxEmbeddedFont::BgxUbuntuFont_raw_size),
            sizePixels,
            &config,
            io.Fonts->GetGlyphRangesVietnamese());
        if (!font) font = io.Fonts->AddFontDefault();
        io.FontDefault = font;
        return font;
    }

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

    inline float menuBoundsRight = 0.0f;
    inline float menuBoundsBottom = 0.0f;
    inline float permaShowBoundsRight = 0.0f;
    inline float permaShowBoundsBottom = 0.0f;
    inline std::uint32_t mouseCaptureButtons = 0;
    inline std::uint32_t rawMouseCaptureButtons = 0;

    constexpr std::uint32_t MOUSE_CAPTURE_LEFT   = 1u << 0;
    constexpr std::uint32_t MOUSE_CAPTURE_RIGHT  = 1u << 1;
    constexpr std::uint32_t MOUSE_CAPTURE_MIDDLE = 1u << 2;
    constexpr std::uint32_t MOUSE_CAPTURE_X1     = 1u << 3;
    constexpr std::uint32_t MOUSE_CAPTURE_X2     = 1u << 4;

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

    inline SDK::UI::Menu* ensoulCoreRoot = nullptr;
    inline SDK::UI::MenuList* ensoulLanguage = nullptr;
    inline SDK::UI::MenuBool* ensoulSkinChanger = nullptr;
    inline SDK::UI::MenuSlider* ensoulSkinId = nullptr;
    inline SDK::UI::MenuBool* ensoulZoomHack = nullptr;
    inline SDK::UI::MenuSliderF* ensoulMaxZoom = nullptr;
    inline SDK::UI::MenuBool* ensoulPermaShow = nullptr;
    inline SDK::UI::MenuBool* ensoulPermaAllowDrag = nullptr;
    inline SDK::UI::MenuSlider* ensoulPermaX = nullptr;
    inline SDK::UI::MenuSlider* ensoulPermaY = nullptr;
    inline SDK::UI::MenuSlider* ensoulPermaWidth = nullptr;
    inline SDK::UI::MenuSlider* ensoulPermaIndicatorWidth = nullptr;
    inline SDK::UI::MenuSlider* ensoulMenuX = nullptr;
    inline SDK::UI::MenuSlider* ensoulMenuY = nullptr;
    inline SDK::UI::MenuBool* ensoulBypassObs = nullptr;
    inline SDK::UI::MenuBool* ensoulClickThrough = nullptr;
    inline SDK::UI::MenuBool* ensoulProfiler = nullptr;
    inline SDK::UI::MenuBool* ensoulProfilerLog = nullptr;
    inline SDK::UI::MenuRuntime* ensoulProfilerRuntime = nullptr;
    inline SDK::UI::MenuRuntime* ensoulPluginSelectorRuntime = nullptr;
    inline SDK::UI::Menu* ensoulRuntimeMenus = nullptr;

    inline void DrawPluginSelectorRuntimeBridge(void*);

    inline void DrawProfilerRuntimeBridge(void*) {
        NightSharpPerf::DrawStatsBody();
    }

    inline SDK::UI::MenuColor* ensoulTextColor = nullptr;
    inline SDK::UI::MenuColor* ensoulHoverColor = nullptr;
    inline SDK::UI::MenuColor* ensoulRootContainerColor = nullptr;
    inline SDK::UI::MenuColor* ensoulContainerSelectedColor = nullptr;
    inline SDK::UI::MenuColor* ensoulContainerSeparatorColor = nullptr;
    inline SDK::UI::MenuColor* ensoulBorderColor = nullptr;
    inline SDK::UI::MenuColor* ensoulEnabledColor = nullptr;
    inline SDK::UI::MenuColor* ensoulDisabledColor = nullptr;
    inline SDK::UI::MenuColor* ensoulSliderColor = nullptr;
    inline SDK::UI::MenuColor* ensoulSliderActiveColor = nullptr;
    inline SDK::UI::MenuColor* ensoulButtonColor = nullptr;
    inline SDK::UI::MenuColor* ensoulButtonHoverColor = nullptr;

    inline SDK::UI::MenuSliderF* ensoulThemeFontSize = nullptr;
    inline SDK::UI::MenuSliderF* ensoulThemeRowHeight = nullptr;
    inline SDK::UI::MenuSliderF* ensoulThemePanelWidth = nullptr;
    inline SDK::UI::MenuSliderF* ensoulThemeTextPadding = nullptr;

    inline SDK::UI::MenuList* ensoulFontFamily = nullptr;
    inline SDK::UI::MenuList* ensoulThemePreset = nullptr;
    inline SDK::UI::MenuList* ensoulMenuStyle = nullptr;
    inline SDK::UI::MenuSlider* ensoulMaxItemsPerColumn = nullptr;



    struct RuntimeMenuBinding {
        int registryIndex = -1;
        SDK::UI::Menu* menu = nullptr;
        SDK::UI::MenuRuntime* runtime = nullptr;
    };

    inline RuntimeMenuBinding ensoulRuntimeBindings[PluginRegistry::MAX_PLUGINS] = {};
    inline int ensoulRuntimeBindingCount = 0;

    inline void ResetMenuSystem() {
        if (ensoulCoreRoot) {
            SDK::UI::MenuManager::Instance().Remove(ensoulCoreRoot);
            delete ensoulCoreRoot;
        }
        ensoulCoreRoot = nullptr;
        ensoulLanguage = nullptr;
        ensoulSkinChanger = nullptr;
        ensoulSkinId = nullptr;
        ensoulZoomHack = nullptr;
        ensoulMaxZoom = nullptr;
        ensoulPermaShow = nullptr;
        ensoulPermaAllowDrag = nullptr;
        ensoulPermaX = nullptr;
        ensoulPermaY = nullptr;
        ensoulPermaWidth = nullptr;
        ensoulPermaIndicatorWidth = nullptr;
        ensoulMenuX = nullptr;
        ensoulMenuY = nullptr;
        ensoulBypassObs = nullptr;
        ensoulClickThrough = nullptr;
        ensoulProfiler = nullptr;
        ensoulProfilerLog = nullptr;
        ensoulProfilerRuntime = nullptr;
        ensoulPluginSelectorRuntime = nullptr;
        ensoulRuntimeMenus = nullptr;
        ensoulTextColor = nullptr;
        ensoulHoverColor = nullptr;
        ensoulRootContainerColor = nullptr;
        ensoulContainerSelectedColor = nullptr;
        ensoulContainerSeparatorColor = nullptr;
        ensoulBorderColor = nullptr;
        ensoulEnabledColor = nullptr;
        ensoulDisabledColor = nullptr;
        ensoulSliderColor = nullptr;
        ensoulSliderActiveColor = nullptr;
        ensoulButtonColor = nullptr;
        ensoulButtonHoverColor = nullptr;
        ensoulThemeFontSize = nullptr;
        ensoulThemeRowHeight = nullptr;
        ensoulThemePanelWidth = nullptr;
        ensoulThemeTextPadding = nullptr;
        ensoulFontFamily = nullptr;
        ensoulThemePreset = nullptr;
        ensoulMenuStyle = nullptr;
        ensoulMaxItemsPerColumn = nullptr;
        ensoulRuntimeBindingCount = 0;
        EnsoulSharpTheme::OpenList = nullptr;
        EnsoulSharpTheme::OpenColor = nullptr;
        EnsoulSharpTheme::OpenRuntime = nullptr;
        EnsoulSharpTheme::DragSliderButton = nullptr;
        mouseCaptureButtons = 0;
        rawMouseCaptureButtons = 0;
        EnsoulSharpTheme::CancelRootDrag();
        EnsoulSharpTheme::SetFont(nullptr);
        SDK::UI::PermaShow::SetFont(nullptr);
    }

    inline int CoreLanguageToListIndex() {
        return Config::Language::index == 2 ? 1 : 0;
    }

    inline ImVec2 GetOverlayDisplaySize();

    inline void EnsureNightSharpMenuLogo() {
        static constexpr int size = 24;
        if (SDK::UI::Icons::HasIcon("nightsharp_menu_logo")) return;

        std::uint8_t pixels[size * size * 4] = {};
        for (int y = 0; y < size; ++y) {
            for (int x = 0; x < size; ++x) {
                const int index = (y * size + x) * 4;
                const float dx = static_cast<float>(x) - 11.5f;
                const float dy = static_cast<float>(y) - 11.5f;
                const bool inside = dx * dx + dy * dy <= 11.5f * 11.5f;
                if (!inside) continue;
                const float t = static_cast<float>(x) / static_cast<float>(size - 1);
                pixels[index + 0] = static_cast<std::uint8_t>(255.0f + (156.0f - 255.0f) * t);
                pixels[index + 1] = static_cast<std::uint8_t>(170.0f + (64.0f - 170.0f) * t);
                pixels[index + 2] = static_cast<std::uint8_t>(64.0f + (255.0f - 64.0f) * t);
                pixels[index + 3] = 255;

                const bool leftStroke = x >= 6 && x <= 8 && y >= 5 && y <= 18;
                const bool rightStroke = x >= 15 && x <= 17 && y >= 5 && y <= 18;
                const bool diagonal = y >= 5 && y <= 18 &&
                    std::abs((x - 7) - (y - 5) * 9 / 13) <= 1;
                if (leftStroke || rightStroke || diagonal) {
                    pixels[index + 0] = 255;
                    pixels[index + 1] = 255;
                    pixels[index + 2] = 255;
                }
            }
        }
        SDK::UI::Icons::LoadIconFromRgba(
            "nightsharp_menu_logo", pixels, size, size);
    }

    inline void DrawRuntimeMenuBridge(void* userData) {
        const int index = static_cast<int>(reinterpret_cast<intptr_t>(userData)) - 1;
        if (index < 0 || index >= PluginRegistry::PluginCount) return;
        if (!PluginRegistry::Plugins[index].Loaded) return;
        PluginRegistry::DrawPluginMenu(index);
    }

    inline void ResetPermaShowGeometry() {
        const ImVec2 display = GetOverlayDisplaySize();
        Config::PermaShow::width = 300;
        Config::PermaShow::indicatorWidth = 45;
        Config::PermaShow::x = static_cast<int>(display.x - 0.682f * display.x);
        Config::PermaShow::y = static_cast<int>(display.y - 0.965f * display.y);
        Config::PermaShow::positionInitialized = true;

        if (ensoulPermaX) ensoulPermaX->Value = Config::PermaShow::x;
        if (ensoulPermaY) ensoulPermaY->Value = Config::PermaShow::y;
        if (ensoulPermaWidth) ensoulPermaWidth->Value = Config::PermaShow::width;
        if (ensoulPermaIndicatorWidth) {
            ensoulPermaIndicatorWidth->Value = Config::PermaShow::indicatorWidth;
        }
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
        case 7: Config::PermaShow::allowDrag = value; break;
        case 8: Config::OverlayInput::clickThrough = value; break;
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

    inline void OnPermaShowGeometryChanged(SDK::UI::MenuItem* sender, void* userData) {
        const int value = static_cast<SDK::UI::MenuSlider*>(sender)->Value;
        switch (static_cast<int>(reinterpret_cast<intptr_t>(userData))) {
        case 1: Config::PermaShow::x = value; break;
        case 2: Config::PermaShow::y = value; break;
        case 3: Config::PermaShow::width = value; break;
        case 4: Config::PermaShow::indicatorWidth = value; break;
        default: return;
        }
        Config::PermaShow::positionInitialized = true;
    }

    inline void OnPermaShowReset(SDK::UI::MenuButton*, void*) {
        ResetPermaShowGeometry();
    }

    inline void ResetMenuPositionGeometry() {
        Config::MenuPosition::x = 30;
        Config::MenuPosition::y = 30;
        Config::MenuPosition::positionInitialized = true;
        EnsoulSharpTheme::PositionX = 30.0f;
        EnsoulSharpTheme::PositionY = 30.0f;
        menuPosX = 30.0f;
        menuPosY = 30.0f;
        if (ensoulMenuX) ensoulMenuX->Value = Config::MenuPosition::x;
        if (ensoulMenuY) ensoulMenuY->Value = Config::MenuPosition::y;
    }

    inline void OnMenuPositionGeometryChanged(SDK::UI::MenuItem* sender, void* userData) {
        const int value = static_cast<SDK::UI::MenuSlider*>(sender)->Value;
        switch (static_cast<int>(reinterpret_cast<intptr_t>(userData))) {
        case 1: Config::MenuPosition::x = value; break;
        case 2: Config::MenuPosition::y = value; break;
        default: return;
        }
        Config::MenuPosition::positionInitialized = true;
        EnsoulSharpTheme::PositionX = static_cast<float>(Config::MenuPosition::x);
        EnsoulSharpTheme::PositionY = static_cast<float>(Config::MenuPosition::y);
        menuPosX = static_cast<float>(Config::MenuPosition::x);
        menuPosY = static_cast<float>(Config::MenuPosition::y);
    }

    inline void OnMenuPositionReset(SDK::UI::MenuButton*, void*) {
        ResetMenuPositionGeometry();
    }

    inline void OnThemeColorChanged(SDK::UI::MenuItem* sender, void* userData) {
        auto* item = static_cast<SDK::UI::MenuColor*>(sender);
        ImU32 color = item->Value;
        switch (static_cast<int>(reinterpret_cast<intptr_t>(userData))) {
        case 1: EnsoulSharpTheme::TextColor = color; break;
        case 2: EnsoulSharpTheme::HoverColor = color; break;
        case 3: EnsoulSharpTheme::RootContainerColor = color; break;
        case 4: EnsoulSharpTheme::ContainerSelectedColor = color; break;
        case 5: EnsoulSharpTheme::ContainerSeparatorColor = color; break;
        case 6: EnsoulSharpTheme::BorderColor = color; break;
        case 7: EnsoulSharpTheme::EnabledColor = color; break;
        case 8: EnsoulSharpTheme::DisabledColor = color; break;
        case 9: EnsoulSharpTheme::SliderColor = color; break;
        case 10: EnsoulSharpTheme::SliderActiveColor = color; break;
        case 11: EnsoulSharpTheme::ButtonColor = color; break;
        case 12: EnsoulSharpTheme::ButtonHoverColor = color; break;
        default: break;
        }
        EnsoulSharpTheme::SelectedPresetIndex = 0; // Custom
        if (ensoulThemePreset) ensoulThemePreset->Index = 0;
    }

    inline void OnThemePresetChanged(SDK::UI::MenuItem* sender, void*) {
        auto* item = static_cast<SDK::UI::MenuList*>(sender);
        int index = item->Index;
        if (index > 0) {
            EnsoulSharpTheme::ApplyThemePreset(index);
            if (ensoulTextColor) ensoulTextColor->Value = EnsoulSharpTheme::TextColor;
            if (ensoulHoverColor) ensoulHoverColor->Value = EnsoulSharpTheme::HoverColor;
            if (ensoulRootContainerColor) ensoulRootContainerColor->Value = EnsoulSharpTheme::RootContainerColor;
            if (ensoulContainerSelectedColor) ensoulContainerSelectedColor->Value = EnsoulSharpTheme::ContainerSelectedColor;
            if (ensoulContainerSeparatorColor) ensoulContainerSeparatorColor->Value = EnsoulSharpTheme::ContainerSeparatorColor;
            if (ensoulBorderColor) ensoulBorderColor->Value = EnsoulSharpTheme::BorderColor;
            if (ensoulEnabledColor) ensoulEnabledColor->Value = EnsoulSharpTheme::EnabledColor;
            if (ensoulDisabledColor) ensoulDisabledColor->Value = EnsoulSharpTheme::DisabledColor;
            if (ensoulSliderColor) ensoulSliderColor->Value = EnsoulSharpTheme::SliderColor;
            if (ensoulSliderActiveColor) ensoulSliderActiveColor->Value = EnsoulSharpTheme::SliderActiveColor;
            if (ensoulButtonColor) ensoulButtonColor->Value = EnsoulSharpTheme::ButtonColor;
            if (ensoulButtonHoverColor) ensoulButtonHoverColor->Value = EnsoulSharpTheme::ButtonHoverColor;
        }
    }

    inline void OnThemeSliderFChanged(SDK::UI::MenuItem* sender, void* userData) {
        auto* item = static_cast<SDK::UI::MenuSliderF*>(sender);
        float value = item->Value;
        switch (static_cast<int>(reinterpret_cast<intptr_t>(userData))) {
        case 1: EnsoulSharpTheme::FontSize = value; break;
        case 2: EnsoulSharpTheme::ContainerHeight = value; break;
        case 3: EnsoulSharpTheme::ContainerWidth = value; break;
        case 4: EnsoulSharpTheme::ContainerTextOffset = value; break;
        default: break;
        }
    }

    inline void OnThemeFontFamilyChanged(SDK::UI::MenuItem* sender, void*) {
        auto* item = static_cast<SDK::UI::MenuList*>(sender);
        EnsoulSharpTheme::SelectedFontIndex = item->Index;
    }

    inline void OnMenuStyleChanged(SDK::UI::MenuItem* sender, void*) {
        auto* item = static_cast<SDK::UI::MenuList*>(sender);
        Config::MenuStyle::index = item->Index;
    }

    inline void OnMaxItemsPerColumnChanged(SDK::UI::MenuItem* sender, void*) {
        auto* item = static_cast<SDK::UI::MenuSlider*>(sender);
        Config::MenuStyle::maxItemsPerColumn = item->Value;
        EnsoulSharpTheme::MaxItemsPerColumn = item->Value;
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

    inline void KeepCoreMenuFirst() {
        if (!ensoulCoreRoot) return;
        auto& menus = SDK::UI::MenuManager::Instance().Menus;
        if (menus.empty() || menus[0] == ensoulCoreRoot) return;

        int idx = -1;
        for (int i = 0; i < menus.size(); ++i) {
            if (menus[i] == ensoulCoreRoot) {
                idx = i;
                break;
            }
        }
        if (idx > 0) {
            SDK::UI::Menu* core = menus[idx];
            for (int i = idx; i > 0; --i) {
                menus[i] = menus[i - 1];
            }
            menus[0] = core;
        }
    }

    inline void EnsureEnsoulCoreMenu() {
        SDK::UI::g_MenuSystemResetHook = &ResetMenuSystem;
        if (ensoulCoreRoot) {
            return;
        }

        ensoulCoreRoot = new SDK::UI::Menu("nightsharp.core", "NightSharp", true);
        EnsoulSharpTheme::MaxItemsPerColumn = Config::MenuStyle::maxItemsPerColumn;
        EnsureNightSharpMenuLogo();
        ensoulCoreRoot->SetLogo("nightsharp_menu_logo");

        // 1. Features & Tools (Skin Changer, Zoom Hack, OBS Bypass)
        auto* features = ensoulCoreRoot->AddSubMenu(
            new SDK::UI::Menu("Features", "Features & Tools"));
        ensoulSkinChanger = features->Add(new SDK::UI::MenuBool(
            "SkinChanger", "Skin Changer", Config::SkinChanger::enabled));
        BindCoreBool(ensoulSkinChanger, 1);
        ensoulSkinId = features->Add(new SDK::UI::MenuSlider(
            "SkinId", "Skin ID", Config::SkinChanger::skinId, 0, 100));
        ensoulSkinId->ValueChanged = &OnCoreSkinIdChanged;
        ensoulZoomHack = features->Add(new SDK::UI::MenuBool(
            "ZoomHack", "Zoom Hack", Config::ZoomHack::enabled));
        BindCoreBool(ensoulZoomHack, 2);
        ensoulMaxZoom = features->Add(new SDK::UI::MenuSliderF(
            "MaxZoom", "Maximum Zoom", Config::ZoomHack::maxZoom, 1000.0f, 10000.0f));
        ensoulMaxZoom->ValueChanged = &OnCoreZoomChanged;
        ensoulBypassObs = features->Add(new SDK::UI::MenuBool(
            "BypassObs", "Bypass OBS (Stream Protection)", Config::StreamProtection::bypassObs));
        BindCoreBool(ensoulBypassObs, 4);
        ensoulClickThrough = features->Add(new SDK::UI::MenuBool(
            "ClickThrough", "Click-Through Input Mode", Config::OverlayInput::clickThrough));
        BindCoreBool(ensoulClickThrough, 8);

        // 2. Permashow Configuration
        if (!Config::PermaShow::positionInitialized) {
            ResetPermaShowGeometry();
        }
        const ImVec2 display = GetOverlayDisplaySize();
        const int displayWidth = display.x > 1.0f ? static_cast<int>(display.x) : 1;
        const int displayHeight = display.y > 1.0f ? static_cast<int>(display.y) : 1;

        auto* permaShow = ensoulCoreRoot->AddSubMenu(
            new SDK::UI::Menu("PermaShow", "Permashow"));
        ensoulPermaShow = permaShow->Add(new SDK::UI::MenuBool(
            "Enabled", "Enable Permashow", Config::PermaShow::enabled));
        BindCoreBool(ensoulPermaShow, 3);
        ensoulPermaAllowDrag = permaShow->Add(new SDK::UI::MenuBool(
            "AllowDrag", "Enable Dragging", Config::PermaShow::allowDrag));
        BindCoreBool(ensoulPermaAllowDrag, 7);
        ensoulPermaX = permaShow->Add(new SDK::UI::MenuSlider(
            "X", "Position (X)", Config::PermaShow::x, 0, displayWidth));
        ensoulPermaX->ValueChanged = &OnPermaShowGeometryChanged;
        ensoulPermaX->ValueChangedUd = reinterpret_cast<void*>(static_cast<intptr_t>(1));
        ensoulPermaY = permaShow->Add(new SDK::UI::MenuSlider(
            "Y", "Position (Y)", Config::PermaShow::y, 0, displayHeight));
        ensoulPermaY->ValueChanged = &OnPermaShowGeometryChanged;
        ensoulPermaY->ValueChangedUd = reinterpret_cast<void*>(static_cast<intptr_t>(2));
        ensoulPermaWidth = permaShow->Add(new SDK::UI::MenuSlider(
            "BorderWidth", "Width", Config::PermaShow::width, 100, 400));
        ensoulPermaWidth->ValueChanged = &OnPermaShowGeometryChanged;
        ensoulPermaWidth->ValueChangedUd = reinterpret_cast<void*>(static_cast<intptr_t>(3));
        ensoulPermaIndicatorWidth = permaShow->Add(new SDK::UI::MenuSlider(
            "IndicatorWidth", "Indicator Width",
            Config::PermaShow::indicatorWidth, 30, 90));
        ensoulPermaIndicatorWidth->ValueChanged = &OnPermaShowGeometryChanged;
        ensoulPermaIndicatorWidth->ValueChangedUd =
            reinterpret_cast<void*>(static_cast<intptr_t>(4));
        permaShow->Add(new SDK::UI::MenuButton(
            "Reset", "Reset to Default", "Reset", &OnPermaShowReset));

        // 3. Menu Settings & Theme
        auto* settings = ensoulCoreRoot->AddSubMenu(
            new SDK::UI::Menu("MenuSettings", "Menu Settings & Theme"));
        ensoulLanguage = settings->Add(new SDK::UI::MenuList(
            "SelectedLanguage",
            "Language",
            { "English", "Vietnamese" },
            CoreLanguageToListIndex()));
        ensoulLanguage->ValueChanged = &OnCoreLanguageChanged;

        ensoulMenuStyle = settings->Add(new SDK::UI::MenuList(
            "MenuStyle",
            "Menu Style",
            { "EnsoulSharp", "BGX" },
            Config::MenuStyle::index));
        ensoulMenuStyle->ValueChanged = &OnMenuStyleChanged;

        auto* menuPos = settings->AddSubMenu(
            new SDK::UI::Menu("MenuPosition", "Menu Position"));
        ensoulMenuX = menuPos->Add(new SDK::UI::MenuSlider(
            "X", "Position (X)", Config::MenuPosition::x, 0, displayWidth));
        ensoulMenuX->ValueChanged = &OnMenuPositionGeometryChanged;
        ensoulMenuX->ValueChangedUd = reinterpret_cast<void*>(static_cast<intptr_t>(1));
        ensoulMenuY = menuPos->Add(new SDK::UI::MenuSlider(
            "Y", "Position (Y)", Config::MenuPosition::y, 0, displayHeight));
        ensoulMenuY->ValueChanged = &OnMenuPositionGeometryChanged;
        ensoulMenuY->ValueChangedUd = reinterpret_cast<void*>(static_cast<intptr_t>(2));
        menuPos->Add(new SDK::UI::MenuButton(
            "Reset", "Reset to Default", "Reset", &OnMenuPositionReset));

        ensoulThemePreset = settings->Add(new SDK::UI::MenuList(
            "ThemePreset",
            "Theme Preset",
            { "Custom", "Default DX9", "Pitch Black", "Dark Minimal" },
            EnsoulSharpTheme::SelectedPresetIndex));
        ensoulThemePreset->ValueChanged = &OnThemePresetChanged;

        {
            const float displayH = GetOverlayDisplaySize().y;
            int maxAllowed = static_cast<int>(displayH / EnsoulSharpTheme::ContainerHeight) - 2;
            if (maxAllowed < 10) maxAllowed = 10;
            if (maxAllowed > 40) maxAllowed = 40;
            ensoulMaxItemsPerColumn = settings->Add(new SDK::UI::MenuSlider(
                "MaxItemsPerColumn", "Max Items per Column",
                Config::MenuStyle::maxItemsPerColumn, 10, maxAllowed));
            ensoulMaxItemsPerColumn->ValueChanged = &OnMaxItemsPerColumnChanged;
        }

        ensoulFontFamily = settings->Add(new SDK::UI::MenuList(
            "FontFamily",
            "Font Family",
            { "Tonos Mono", "Tahoma", "Arial", "Consolas", "Segoe UI", "Ubuntu" },
            EnsoulSharpTheme::SelectedFontIndex));
        ensoulFontFamily->ValueChanged = &OnThemeFontFamilyChanged;

        auto* colors = settings->AddSubMenu(
            new SDK::UI::Menu("Colors", "Menu Colors"));

        ensoulTextColor = colors->Add(new SDK::UI::MenuColor(
            "TextColor", "Text Color", EnsoulSharpTheme::TextColor));
        ensoulTextColor->ValueChanged = &OnThemeColorChanged;
        ensoulTextColor->ValueChangedUd = reinterpret_cast<void*>(static_cast<intptr_t>(1));

        ensoulHoverColor = colors->Add(new SDK::UI::MenuColor(
            "HoverColor", "Hover Color", EnsoulSharpTheme::HoverColor));
        ensoulHoverColor->ValueChanged = &OnThemeColorChanged;
        ensoulHoverColor->ValueChangedUd = reinterpret_cast<void*>(static_cast<intptr_t>(2));

        ensoulRootContainerColor = colors->Add(new SDK::UI::MenuColor(
            "RootContainerColor", "Background Color", EnsoulSharpTheme::RootContainerColor));
        ensoulRootContainerColor->ValueChanged = &OnThemeColorChanged;
        ensoulRootContainerColor->ValueChangedUd = reinterpret_cast<void*>(static_cast<intptr_t>(3));

        ensoulContainerSelectedColor = colors->Add(new SDK::UI::MenuColor(
            "ContainerSelectedColor", "Selected Color", EnsoulSharpTheme::ContainerSelectedColor));
        ensoulContainerSelectedColor->ValueChanged = &OnThemeColorChanged;
        ensoulContainerSelectedColor->ValueChangedUd = reinterpret_cast<void*>(static_cast<intptr_t>(4));

        ensoulContainerSeparatorColor = colors->Add(new SDK::UI::MenuColor(
            "ContainerSeparatorColor", "Separator Color", EnsoulSharpTheme::ContainerSeparatorColor));
        ensoulContainerSeparatorColor->ValueChanged = &OnThemeColorChanged;
        ensoulContainerSeparatorColor->ValueChangedUd = reinterpret_cast<void*>(static_cast<intptr_t>(5));

        ensoulBorderColor = colors->Add(new SDK::UI::MenuColor(
            "BorderColor", "Border Color", EnsoulSharpTheme::BorderColor));
        ensoulBorderColor->ValueChanged = &OnThemeColorChanged;
        ensoulBorderColor->ValueChangedUd = reinterpret_cast<void*>(static_cast<intptr_t>(6));

        ensoulEnabledColor = colors->Add(new SDK::UI::MenuColor(
            "EnabledColor", "Enabled Item Color", EnsoulSharpTheme::EnabledColor));
        ensoulEnabledColor->ValueChanged = &OnThemeColorChanged;
        ensoulEnabledColor->ValueChangedUd = reinterpret_cast<void*>(static_cast<intptr_t>(7));

        ensoulDisabledColor = colors->Add(new SDK::UI::MenuColor(
            "DisabledColor", "Disabled Item Color", EnsoulSharpTheme::DisabledColor));
        ensoulDisabledColor->ValueChanged = &OnThemeColorChanged;
        ensoulDisabledColor->ValueChangedUd = reinterpret_cast<void*>(static_cast<intptr_t>(8));

        ensoulSliderColor = colors->Add(new SDK::UI::MenuColor(
            "SliderColor", "Slider Color", EnsoulSharpTheme::SliderColor));
        ensoulSliderColor->ValueChanged = &OnThemeColorChanged;
        ensoulSliderColor->ValueChangedUd = reinterpret_cast<void*>(static_cast<intptr_t>(9));

        ensoulSliderActiveColor = colors->Add(new SDK::UI::MenuColor(
            "SliderActiveColor", "Slider Active Color", EnsoulSharpTheme::SliderActiveColor));
        ensoulSliderActiveColor->ValueChanged = &OnThemeColorChanged;
        ensoulSliderActiveColor->ValueChangedUd = reinterpret_cast<void*>(static_cast<intptr_t>(10));

        ensoulButtonColor = colors->Add(new SDK::UI::MenuColor(
            "ButtonColor", "Button Color", EnsoulSharpTheme::ButtonColor));
        ensoulButtonColor->ValueChanged = &OnThemeColorChanged;
        ensoulButtonColor->ValueChangedUd = reinterpret_cast<void*>(static_cast<intptr_t>(11));

        ensoulButtonHoverColor = colors->Add(new SDK::UI::MenuColor(
            "ButtonHoverColor", "Button Hover Color", EnsoulSharpTheme::ButtonHoverColor));
        ensoulButtonHoverColor->ValueChanged = &OnThemeColorChanged;
        ensoulButtonHoverColor->ValueChangedUd = reinterpret_cast<void*>(static_cast<intptr_t>(12));

        auto* sizes = settings->AddSubMenu(
            new SDK::UI::Menu("Sizes", "Menu Sizes"));

        ensoulThemeFontSize = sizes->Add(new SDK::UI::MenuSliderF(
            "FontSize", "Font Size", EnsoulSharpTheme::FontSize, 10.0f, 24.0f));
        ensoulThemeFontSize->ValueChanged = &OnThemeSliderFChanged;
        ensoulThemeFontSize->ValueChangedUd = reinterpret_cast<void*>(static_cast<intptr_t>(1));

        ensoulThemeRowHeight = sizes->Add(new SDK::UI::MenuSliderF(
            "RowHeight", "Row Height", EnsoulSharpTheme::ContainerHeight, 20.0f, 45.0f));
        ensoulThemeRowHeight->ValueChanged = &OnThemeSliderFChanged;
        ensoulThemeRowHeight->ValueChangedUd = reinterpret_cast<void*>(static_cast<intptr_t>(2));

        ensoulThemePanelWidth = sizes->Add(new SDK::UI::MenuSliderF(
            "PanelWidth", "Panel Width", EnsoulSharpTheme::ContainerWidth, 150.0f, 350.0f));
        ensoulThemePanelWidth->ValueChanged = &OnThemeSliderFChanged;
        ensoulThemePanelWidth->ValueChangedUd = reinterpret_cast<void*>(static_cast<intptr_t>(3));

        ensoulThemeTextPadding = sizes->Add(new SDK::UI::MenuSliderF(
            "TextPadding", "Text Padding", EnsoulSharpTheme::ContainerTextOffset, 5.0f, 30.0f));
        ensoulThemeTextPadding->ValueChanged = &OnThemeSliderFChanged;
        ensoulThemeTextPadding->ValueChangedUd = reinterpret_cast<void*>(static_cast<intptr_t>(4));

        // 4. Plugins Manager
        auto* plugins = ensoulCoreRoot->AddSubMenu(
            new SDK::UI::Menu("Plugins", "Plugins"));
        ensoulPluginSelectorRuntime = plugins->Add(new SDK::UI::MenuRuntime(
            "PluginSelector",
            "Plugin Selector",
            &DrawPluginSelectorRuntimeBridge,
            nullptr,
            550.0f));
        ensoulPluginSelectorRuntime->Open = true; // Default to open!

        ensoulRuntimeMenus = ensoulCoreRoot->AddSubMenu(
            new SDK::UI::Menu("RuntimeMenus", "Runtime Panels"));
        for (int i = 0;
             i < PluginRegistry::PluginCount &&
             ensoulRuntimeBindingCount < PluginRegistry::MAX_PLUGINS;
             ++i) {
            auto& plugin = PluginRegistry::Plugins[i];
            if (!plugin.Name || !plugin.InternalId) {
                continue;
            }
            if (plugin.Category == PluginRegistry::PluginCategory::Champion &&
                !PluginRegistry::CanPluginLoad(i)) {
                continue;
            }
            if (!PluginRegistry::HasPluginMenuCallback(i)) {
                continue;
            }

            auto* runtimeMenu = ensoulRuntimeMenus->AddSubMenu(
                new SDK::UI::Menu(plugin.InternalId, plugin.Name));
            auto* runtime = runtimeMenu->Add(new SDK::UI::MenuRuntime(
                "RuntimePanel",
                "Open Runtime Panel",
                &DrawRuntimeMenuBridge,
                reinterpret_cast<void*>(static_cast<intptr_t>(i + 1)),
                360.0f));
            runtimeMenu->Visible = plugin.Loaded;
            ensoulRuntimeBindings[ensoulRuntimeBindingCount++] = {
                i, runtimeMenu, runtime
            };
        }

        // 5. Debug & Profiler
        auto* debug = ensoulCoreRoot->AddSubMenu(
            new SDK::UI::Menu("DebugInfo", "Debug & Profiler"));
        debug->SetFontColor(IM_COL32(120, 235, 120, 255));
        debug->Add(new SDK::UI::MenuSeparator("Build", "NightSharp " __DATE__ " " __TIME__));
        debug->Add(new SDK::UI::MenuSeparator("Theme", "EnsoulSharp default DX9 theme"));
        ensoulProfiler = debug->Add(new SDK::UI::MenuBool(
            "Profiler", "Performance Profiler", NightSharpPerf::Enabled));
        BindCoreBool(ensoulProfiler, 5);
        ensoulProfilerLog = debug->Add(new SDK::UI::MenuBool(
            "ProfilerLog", "Write profiler log", NightSharpPerf::LogEnabled));
        BindCoreBool(ensoulProfilerLog, 6);
        ensoulProfilerRuntime = debug->Add(new SDK::UI::MenuRuntime(
            "ProfilerLivePanel",
            "Open Live Profiler Panel",
            &DrawProfilerRuntimeBridge,
            nullptr,
            550.0f));
        auto* themeTests = debug->AddSubMenu(
            new SDK::UI::Menu("ThemeTests", "Theme Tests"));
        themeTests->SetLogo("nightsharp_menu_logo");
        themeTests->Add(new SDK::UI::MenuSeparator(
            "GradientText", "Animated Gradient Text"))
            ->SetAnimatedGradientText(
                IM_COL32(255, 170, 64, 255),
                IM_COL32(156, 64, 255, 255),
                1.0f);
        themeTests->Add(new SDK::UI::MenuBool(
            "CustomColor", "Custom component font color", true))
            ->SetFontColor(IM_COL32(64, 210, 255, 255));
        ensoulRuntimeBindingCount = 0;
        for (int i = 0;
             i < PluginRegistry::PluginCount &&
             ensoulRuntimeBindingCount < PluginRegistry::MAX_PLUGINS;
             ++i) {
            auto& plugin = PluginRegistry::Plugins[i];
            if (!plugin.RuntimeMenu || !plugin.HasRuntimeMenuUI ||
                !plugin.InternalId || !plugin.Name) {
                continue;
            }
            auto* runtimeMenu = ensoulRuntimeMenus->AddSubMenu(
                new SDK::UI::Menu(plugin.InternalId, plugin.Name));
            auto* runtime = runtimeMenu->Add(new SDK::UI::MenuRuntime(
                "RuntimePanel",
                "Open Runtime Panel",
                &DrawRuntimeMenuBridge,
                reinterpret_cast<void*>(static_cast<intptr_t>(i + 1)),
                360.0f));
            runtimeMenu->Visible = plugin.Loaded;
            ensoulRuntimeBindings[ensoulRuntimeBindingCount++] = {
                i, runtimeMenu, runtime
            };
        }

        ensoulCoreRoot->Attach();
        KeepCoreMenuFirst();

        // Copy loaded configuration values to theme globals on startup
        if (ensoulThemePreset) {
            EnsoulSharpTheme::SelectedPresetIndex = ensoulThemePreset->Index;
            EnsoulSharpTheme::ApplyThemePreset(EnsoulSharpTheme::SelectedPresetIndex);
        }
        if (ensoulTextColor) EnsoulSharpTheme::TextColor = ensoulTextColor->Value;
        if (ensoulHoverColor) EnsoulSharpTheme::HoverColor = ensoulHoverColor->Value;
        if (ensoulRootContainerColor) EnsoulSharpTheme::RootContainerColor = ensoulRootContainerColor->Value;
        if (ensoulContainerSelectedColor) EnsoulSharpTheme::ContainerSelectedColor = ensoulContainerSelectedColor->Value;
        if (ensoulContainerSeparatorColor) EnsoulSharpTheme::ContainerSeparatorColor = ensoulContainerSeparatorColor->Value;
        if (ensoulBorderColor) EnsoulSharpTheme::BorderColor = ensoulBorderColor->Value;
        if (ensoulEnabledColor) EnsoulSharpTheme::EnabledColor = ensoulEnabledColor->Value;
        if (ensoulDisabledColor) EnsoulSharpTheme::DisabledColor = ensoulDisabledColor->Value;
        if (ensoulSliderColor) EnsoulSharpTheme::SliderColor = ensoulSliderColor->Value;
        if (ensoulSliderActiveColor) EnsoulSharpTheme::SliderActiveColor = ensoulSliderActiveColor->Value;
        if (ensoulButtonColor) EnsoulSharpTheme::ButtonColor = ensoulButtonColor->Value;
        if (ensoulButtonHoverColor) EnsoulSharpTheme::ButtonHoverColor = ensoulButtonHoverColor->Value;

        if (ensoulThemeFontSize) EnsoulSharpTheme::FontSize = ensoulThemeFontSize->Value;
        if (ensoulThemeRowHeight) EnsoulSharpTheme::ContainerHeight = ensoulThemeRowHeight->Value;
        if (ensoulThemePanelWidth) EnsoulSharpTheme::ContainerWidth = ensoulThemePanelWidth->Value;
        if (ensoulThemeTextPadding) EnsoulSharpTheme::ContainerTextOffset = ensoulThemeTextPadding->Value;
        if (ensoulFontFamily) EnsoulSharpTheme::SelectedFontIndex = ensoulFontFamily->Index;
    }

    inline void SyncEnsoulCoreMenu() {
        if (!ensoulCoreRoot) return;
        ensoulLanguage->Index = CoreLanguageToListIndex();
        ensoulSkinChanger->Value = Config::SkinChanger::enabled;
        ensoulSkinId->Value = Config::SkinChanger::skinId;
        ensoulZoomHack->Value = Config::ZoomHack::enabled;
        ensoulMaxZoom->Value = Config::ZoomHack::maxZoom;
        ensoulPermaShow->Value = Config::PermaShow::enabled;
        const ImVec2 display = GetOverlayDisplaySize();
        ensoulPermaX->MaxValue = display.x > 1.0f ? static_cast<int>(display.x) : 1;
        ensoulPermaY->MaxValue = display.y > 1.0f ? static_cast<int>(display.y) : 1;
        ensoulPermaX->Value = Config::PermaShow::x;
        ensoulPermaY->Value = Config::PermaShow::y;
        if (ensoulMenuX) {
            ensoulMenuX->MaxValue = display.x > 1.0f ? static_cast<int>(display.x) : 1;
            ensoulMenuX->Value = Config::MenuPosition::x;
        }
        if (ensoulMenuY) {
            ensoulMenuY->MaxValue = display.y > 1.0f ? static_cast<int>(display.y) : 1;
            ensoulMenuY->Value = Config::MenuPosition::y;
        }
        ensoulPermaWidth->Value = Config::PermaShow::width;
        ensoulPermaIndicatorWidth->Value = Config::PermaShow::indicatorWidth;
        ensoulBypassObs->Value = Config::StreamProtection::bypassObs;
        if (ensoulClickThrough) ensoulClickThrough->Value = Config::OverlayInput::clickThrough;
        ensoulProfiler->Value = NightSharpPerf::Enabled;
        ensoulProfilerLog->Value = NightSharpPerf::LogEnabled;

        if (ensoulTextColor) ensoulTextColor->Value = EnsoulSharpTheme::TextColor;
        if (ensoulHoverColor) ensoulHoverColor->Value = EnsoulSharpTheme::HoverColor;
        if (ensoulRootContainerColor) ensoulRootContainerColor->Value = EnsoulSharpTheme::RootContainerColor;
        if (ensoulContainerSelectedColor) ensoulContainerSelectedColor->Value = EnsoulSharpTheme::ContainerSelectedColor;
        if (ensoulContainerSeparatorColor) ensoulContainerSeparatorColor->Value = EnsoulSharpTheme::ContainerSeparatorColor;
        if (ensoulBorderColor) ensoulBorderColor->Value = EnsoulSharpTheme::BorderColor;
        if (ensoulEnabledColor) ensoulEnabledColor->Value = EnsoulSharpTheme::EnabledColor;
        if (ensoulDisabledColor) ensoulDisabledColor->Value = EnsoulSharpTheme::DisabledColor;
        if (ensoulSliderColor) ensoulSliderColor->Value = EnsoulSharpTheme::SliderColor;
        if (ensoulSliderActiveColor) ensoulSliderActiveColor->Value = EnsoulSharpTheme::SliderActiveColor;
        if (ensoulButtonColor) ensoulButtonColor->Value = EnsoulSharpTheme::ButtonColor;
        if (ensoulButtonHoverColor) ensoulButtonHoverColor->Value = EnsoulSharpTheme::ButtonHoverColor;

        if (ensoulThemeFontSize) ensoulThemeFontSize->Value = EnsoulSharpTheme::FontSize;
        if (ensoulThemeRowHeight) ensoulThemeRowHeight->Value = EnsoulSharpTheme::ContainerHeight;
        if (ensoulThemePanelWidth) ensoulThemePanelWidth->Value = EnsoulSharpTheme::ContainerWidth;
        if (ensoulThemeTextPadding) ensoulThemeTextPadding->Value = EnsoulSharpTheme::ContainerTextOffset;

        if (ensoulFontFamily) ensoulFontFamily->Index = EnsoulSharpTheme::SelectedFontIndex;
        if (ensoulThemePreset) ensoulThemePreset->Index = EnsoulSharpTheme::SelectedPresetIndex;
        if (ensoulMenuStyle) ensoulMenuStyle->Index = Config::MenuStyle::index;
        if (ensoulMaxItemsPerColumn) ensoulMaxItemsPerColumn->Value = Config::MenuStyle::maxItemsPerColumn;


        bool anyRuntimeVisible = false;
        for (int i = 0; i < ensoulRuntimeBindingCount; ++i) {
            auto& binding = ensoulRuntimeBindings[i];
            if (binding.registryIndex < 0 ||
                binding.registryIndex >= PluginRegistry::PluginCount) {
                continue;
            }
            const bool visible = PluginRegistry::Plugins[binding.registryIndex].Loaded;
            binding.menu->Visible = visible;
            anyRuntimeVisible = anyRuntimeVisible || visible;
            if (!visible && EnsoulSharpTheme::OpenRuntime == binding.runtime) {
                EnsoulSharpTheme::OpenRuntime = nullptr;
            }
        }
        ensoulRuntimeMenus->Visible = anyRuntimeVisible;
    }

    struct SidebarEntry {
        const char* label;
    };

    constexpr int PRIMARY_COUNT = 1;
    inline const SidebarEntry PRIMARY[PRIMARY_COUNT] = {
        { "Core" },
    };

    constexpr int CORE_SECONDARY_COUNT = 5;
    inline const SidebarEntry CORE_SECONDARY[CORE_SECONDARY_COUNT] = {
        { "Features & Tools" },
        { "Permashow" },
        { "Menu & Theme" },
        { "Plugins" },
        { "Debug & Profiler" },
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

    inline void EnsurePermaShowPositionInitialized() {
        if (!Config::PermaShow::positionInitialized) {
            ResetPermaShowGeometry();
        }
        const ImVec2 display = GetOverlayDisplaySize();
        Config::PermaShow::x = ClampI(
            Config::PermaShow::x, 0, static_cast<int>(display.x));
        Config::PermaShow::y = ClampI(
            Config::PermaShow::y, 0, static_cast<int>(display.y));
    }

    inline constexpr float PermaShowFontHeight = 16.0f;
    inline constexpr float PermaShowRowHeight = PermaShowFontHeight * 1.4f;

    inline bool isPermaDragging = false;
    inline float permaDragOffX = 0.0f;
    inline float permaDragOffY = 0.0f;

    inline bool CheckAnyRuntimeOpen(SDK::UI::Menu* menu) {
        if (!menu) return false;
        for (int i = 0; i < static_cast<int>(menu->Components.size()); ++i) {
            auto* comp = menu->Components[i];
            if (!comp) continue;
            if (comp->IsMenu()) {
                if (CheckAnyRuntimeOpen(static_cast<SDK::UI::Menu*>(comp))) {
                    return true;
                }
            } else if (comp->Kind() == SDK::UI::MenuValueType::Runtime) {
                auto* r = static_cast<SDK::UI::MenuRuntime*>(comp);
                if (r->Open) {
                    return true;
                }
            }
        }
        return false;
    }

    inline bool HasAnyRuntimeOpen() {
        auto& mm = SDK::UI::MenuManager::Instance();
        for (int i = 0; i < static_cast<int>(mm.Menus.size()); ++i) {
            if (CheckAnyRuntimeOpen(mm.Menus[i])) {
                return true;
            }
        }
        return false;
    }

    inline bool IsPointInside(float x, float y) {
        if (::SDK::Drawing::IsAllDrawingHidden()) {
            return false;
        }

        if (HasAnyRuntimeOpen()) {
            if (ImGui::GetCurrentContext() && ImGui::GetIO().WantCaptureMouse) {
                return true;
            }
        }

        if (!showMenu) {
            return false;
        }

        // Once a press starts on the menu, retain ownership until its matching
        // release even if the pointer leaves the last rendered hit rectangle.
        if (mouseCaptureButtons != 0 || rawMouseCaptureButtons != 0) {
            return true;
        }

        if (ImGui::GetCurrentContext() && ImGui::GetIO().WantCaptureMouse) {
            return true;
        }

        // Keep pointer input captured while the root column is being dragged.
        // The external overlay otherwise becomes click-through when the cursor
        // moves beyond the hit rectangles produced by the previous frame.
        if (EnsoulSharpTheme::HasRootPointerCapture()) {
            return true;
        }

        if (isPermaDragging) {
            return true;
        }

        const bool insideMenu = EnsoulSharpTheme::ContainsPoint(x, y);
        if (insideMenu) {
            return true;
        }

        if (Config::PermaShow::enabled && Config::PermaShow::allowDrag && SDK::UI::PermaShow::Count() > 0) {
            const int rows = SDK::UI::PermaShow::Count();
            const ImVec2 display = GetOverlayDisplaySize();
            const float xFactor = display.x / 1366.0f;
            const float width = static_cast<float>(Config::PermaShow::width) * xFactor;
            const float centerX = static_cast<float>(Config::PermaShow::x);
            const float top = static_cast<float>(Config::PermaShow::y);
            const float left = centerX - width * 0.5f;
            const float height = static_cast<float>(rows) * PermaShowRowHeight;
            if (x >= left && x <= left + width && y >= top && y <= top + height) {
                return true;
            }
        }

        return false;
    }

    inline void ResetMouseInputCapture() {
        mouseCaptureButtons = 0;
        rawMouseCaptureButtons = 0;
    }

    inline bool HasMouseInputCapture() {
        if (::SDK::Drawing::IsAllDrawingHidden()) {
            return false;
        }
        return mouseCaptureButtons != 0 || rawMouseCaptureButtons != 0 ||
               EnsoulSharpTheme::HasRootPointerCapture();
    }

    inline std::uint32_t MouseCaptureBit(UINT msg, WPARAM wParam) {
        switch (msg) {
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_LBUTTONDBLCLK:
            return MOUSE_CAPTURE_LEFT;
        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP:
        case WM_RBUTTONDBLCLK:
            return MOUSE_CAPTURE_RIGHT;
        case WM_MBUTTONDOWN:
        case WM_MBUTTONUP:
        case WM_MBUTTONDBLCLK:
            return MOUSE_CAPTURE_MIDDLE;
        case WM_XBUTTONDOWN:
        case WM_XBUTTONUP:
        case WM_XBUTTONDBLCLK:
            return HIWORD(wParam) == XBUTTON1
                ? MOUSE_CAPTURE_X1
                : MOUSE_CAPTURE_X2;
        default:
            return 0;
        }
    }

    inline bool IsMouseButtonDownMessage(UINT msg) {
        switch (msg) {
        case WM_LBUTTONDOWN:
        case WM_LBUTTONDBLCLK:
        case WM_RBUTTONDOWN:
        case WM_RBUTTONDBLCLK:
        case WM_MBUTTONDOWN:
        case WM_MBUTTONDBLCLK:
        case WM_XBUTTONDOWN:
        case WM_XBUTTONDBLCLK:
            return true;
        default:
            return false;
        }
    }

    inline bool IsMouseButtonUpMessage(UINT msg) {
        return msg == WM_LBUTTONUP || msg == WM_RBUTTONUP ||
               msg == WM_MBUTTONUP || msg == WM_XBUTTONUP;
    }

    inline bool ShouldCaptureMouseMessage(
        UINT msg,
        WPARAM wParam,
        float clientX,
        float clientY) {
        if (::SDK::Drawing::IsAllDrawingHidden()) {
            ResetMouseInputCapture();
            return false;
        }

        if (!showMenu && !HasAnyRuntimeOpen()) {
            ResetMouseInputCapture();
            return false;
        }

        const bool wasCaptured = HasMouseInputCapture();
        const bool inside = IsPointInside(clientX, clientY);
        const std::uint32_t button = MouseCaptureBit(msg, wParam);

        if (IsMouseButtonDownMessage(msg)) {
            if (inside && button != 0) {
                mouseCaptureButtons |= button;
            }
            return inside || wasCaptured;
        }

        if (IsMouseButtonUpMessage(msg)) {
            const bool shouldCapture = inside || wasCaptured ||
                (button != 0 && (mouseCaptureButtons & button) != 0);
            mouseCaptureButtons &= ~button;
            return shouldCapture;
        }

        return inside || wasCaptured;
    }

    inline bool ShouldCaptureRawMouseInput(HWND hWnd, LPARAM lParam) {
        if (::SDK::Drawing::IsAllDrawingHidden() || !showMenu || !hWnd) {
            ResetMouseInputCapture();
            return false;
        }

        RAWINPUT input = {};
        UINT inputSize = sizeof(input);
        const UINT read = GetRawInputData(
            reinterpret_cast<HRAWINPUT>(lParam),
            RID_INPUT,
            &input,
            &inputSize,
            sizeof(RAWINPUTHEADER));
        if (read == static_cast<UINT>(-1) || read < sizeof(RAWINPUT) ||
            input.header.dwType != RIM_TYPEMOUSE) {
            return false;
        }

        POINT cursor = {};
        if (!GetCursorPos(&cursor) || !ScreenToClient(hWnd, &cursor)) {
            return HasMouseInputCapture();
        }

        const bool wasCaptured = HasMouseInputCapture();
        const bool inside = IsPointInside(
            static_cast<float>(cursor.x),
            static_cast<float>(cursor.y));
        const USHORT flags = input.data.mouse.usButtonFlags;

        std::uint32_t down = 0;
        std::uint32_t up = 0;
        if ((flags & RI_MOUSE_LEFT_BUTTON_DOWN) != 0) down |= MOUSE_CAPTURE_LEFT;
        if ((flags & RI_MOUSE_RIGHT_BUTTON_DOWN) != 0) down |= MOUSE_CAPTURE_RIGHT;
        if ((flags & RI_MOUSE_MIDDLE_BUTTON_DOWN) != 0) down |= MOUSE_CAPTURE_MIDDLE;
        if ((flags & RI_MOUSE_BUTTON_4_DOWN) != 0) down |= MOUSE_CAPTURE_X1;
        if ((flags & RI_MOUSE_BUTTON_5_DOWN) != 0) down |= MOUSE_CAPTURE_X2;
        if ((flags & RI_MOUSE_LEFT_BUTTON_UP) != 0) up |= MOUSE_CAPTURE_LEFT;
        if ((flags & RI_MOUSE_RIGHT_BUTTON_UP) != 0) up |= MOUSE_CAPTURE_RIGHT;
        if ((flags & RI_MOUSE_MIDDLE_BUTTON_UP) != 0) up |= MOUSE_CAPTURE_MIDDLE;
        if ((flags & RI_MOUSE_BUTTON_4_UP) != 0) up |= MOUSE_CAPTURE_X1;
        if ((flags & RI_MOUSE_BUTTON_5_UP) != 0) up |= MOUSE_CAPTURE_X2;

        if (inside) {
            rawMouseCaptureButtons |= down;
        }
        const bool shouldCapture = inside || wasCaptured ||
            (up != 0 && (rawMouseCaptureButtons & up) != 0);
        rawMouseCaptureButtons &= ~up;
        return shouldCapture;
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

    inline void DrawFeaturesSection() {
        DrawSectionTitle("Features & Tools");

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
            if (ImGui::SliderInt("##skin_id", &Config::SkinChanger::skinId, 0, 100, "%d")) {
                if (ensoulSkinId) ensoulSkinId->Value = Config::SkinChanger::skinId;
            }
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
            SDK::UI::BeginFunctionalMenuRow("##zoom_hack_row");
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("Maximum Zoom");
            ImGui::SameLine();
            float targetXZ = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - 140.0f;
            if (targetXZ > ImGui::GetCursorPosX()) {
                ImGui::SetCursorPosX(targetXZ);
            }
            ImGui::SetNextItemWidth(140.0f);
            if (ImGui::SliderFloat("##max_zoom", &Config::ZoomHack::maxZoom, 1000.0f, 10000.0f, "%.0f")) {
                if (ensoulMaxZoom) ensoulMaxZoom->Value = Config::ZoomHack::maxZoom;
            }
            SDK::UI::EndFunctionalMenuRow();
        }

        DrawOnOffEditor("Bypass OBS (Stream Protection)", Config::StreamProtection::bypassObs, "bypass_obs");

        SDK::UI::BeginFunctionalMenuRow("##bypass_obs_info1");
        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.6f, 1.0f), "Bypass OBS: overlay hidden from screen capture");
        SDK::UI::EndFunctionalMenuRow();

        bool hideAllDrawings = ::SDK::Drawing::IsAllDrawingHidden();
        const bool oldHideAllDrawings = hideAllDrawings;
        DrawOnOffEditor("Hide All Drawings (Hotkey L)", hideAllDrawings, "hide_all_drawings");
        if (hideAllDrawings != oldHideAllDrawings) {
            ::SDK::Drawing::SetAllDrawingHidden(hideAllDrawings);
        }
    }

    inline void DrawPermaShowSection() {
        DrawSectionTitle("Permashow");

        DrawOnOffEditor("Enable Permashow", Config::PermaShow::enabled, "perma_show");
        bool oldAllowDrag = Config::PermaShow::allowDrag;
        DrawOnOffEditor("Enable Dragging", Config::PermaShow::allowDrag, "perma_allow_drag");
        if (oldAllowDrag != Config::PermaShow::allowDrag && ensoulPermaAllowDrag) {
            ensoulPermaAllowDrag->Value = Config::PermaShow::allowDrag;
        }

        const ImVec2 display = GetOverlayDisplaySize();
        SDK::UI::BeginFunctionalMenuRow("##perma_pos_x_row");
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Position (X)");
        ImGui::SameLine();
        float tX = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - 120.0f;
        if (tX > ImGui::GetCursorPosX()) ImGui::SetCursorPosX(tX);
        ImGui::SetNextItemWidth(120.0f);
        if (ImGui::SliderInt("##perma_pos_x", &Config::PermaShow::x, 0, static_cast<int>(display.x))) {
            if (ensoulPermaX) ensoulPermaX->Value = Config::PermaShow::x;
        }
        SDK::UI::EndFunctionalMenuRow();

        SDK::UI::BeginFunctionalMenuRow("##perma_pos_y_row");
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Position (Y)");
        ImGui::SameLine();
        float tY = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - 120.0f;
        if (tY > ImGui::GetCursorPosX()) ImGui::SetCursorPosX(tY);
        ImGui::SetNextItemWidth(120.0f);
        if (ImGui::SliderInt("##perma_pos_y", &Config::PermaShow::y, 0, static_cast<int>(display.y))) {
            if (ensoulPermaY) ensoulPermaY->Value = Config::PermaShow::y;
        }
        SDK::UI::EndFunctionalMenuRow();

        SDK::UI::BeginFunctionalMenuRow("##perma_width_row");
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Width");
        ImGui::SameLine();
        float tW = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - 120.0f;
        if (tW > ImGui::GetCursorPosX()) ImGui::SetCursorPosX(tW);
        ImGui::SetNextItemWidth(120.0f);
        if (ImGui::SliderInt("##perma_width", &Config::PermaShow::width, 100, 400)) {
            if (ensoulPermaWidth) ensoulPermaWidth->Value = Config::PermaShow::width;
        }
        SDK::UI::EndFunctionalMenuRow();

        SDK::UI::BeginFunctionalMenuRow("##perma_ind_w_row");
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Indicator Width");
        ImGui::SameLine();
        float tIW = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - 120.0f;
        if (tIW > ImGui::GetCursorPosX()) ImGui::SetCursorPosX(tIW);
        ImGui::SetNextItemWidth(120.0f);
        if (ImGui::SliderInt("##perma_ind_w", &Config::PermaShow::indicatorWidth, 30, 90)) {
            if (ensoulPermaIndicatorWidth) ensoulPermaIndicatorWidth->Value = Config::PermaShow::indicatorWidth;
        }
        SDK::UI::EndFunctionalMenuRow();
    }

    inline void DrawMenuSection() {
        DrawSectionTitle("Menu Settings & Theme");

        const char* langs[] = { "EN", "CN", "VN" };
        SDK::UI::BeginFunctionalMenuRow("##lang_row");
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Language");
        ImGui::SameLine();
        float targetX = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - 186.0f;
        if (targetX > ImGui::GetCursorPosX()) {
            ImGui::SetCursorPosX(targetX);
        }
        for (int i = 0; i < 3; ++i) {
            ImGui::PushID(langs[i]);
            if (DrawStateButton(langs[i], langs[i], Config::Language::index == i, true, 56.0f)) {
                Config::Language::index = i;
                if (ensoulLanguage) ensoulLanguage->Index = CoreLanguageToListIndex();
            }
            ImGui::PopID();
            if (i < 2) {
                ImGui::SameLine(0, 8);
            }
        }
        SDK::UI::EndFunctionalMenuRow();

        SDK::UI::BeginFunctionalMenuRow("##menu_style_row");
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Menu Style");
        ImGui::SameLine();
        float targetXStyle = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - 140.0f;
        if (targetXStyle > ImGui::GetCursorPosX()) {
            ImGui::SetCursorPosX(targetXStyle);
        }
        ImGui::SetNextItemWidth(140.0f);
        const char* styles[] = { "EnsoulSharp", "BGX" };
        int currentStyle = Config::MenuStyle::index;
        if (ImGui::Combo("##menu_style_combo", &currentStyle, styles, 2)) {
            Config::MenuStyle::index = currentStyle;
            if (ensoulMenuStyle) ensoulMenuStyle->Index = currentStyle;
        }
        SDK::UI::EndFunctionalMenuRow();

        SDK::UI::BeginFunctionalMenuRow("##menu_pos_x_row");
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Menu Position (X)");
        ImGui::SameLine();
        float targetXX = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - 120.0f;
        if (targetXX > ImGui::GetCursorPosX()) {
            ImGui::SetCursorPosX(targetXX);
        }
        ImGui::SetNextItemWidth(120.0f);
        const ImVec2 display = GetOverlayDisplaySize();
        int posX = Config::MenuPosition::x;
        if (ImGui::SliderInt("##menu_pos_x", &posX, 0, static_cast<int>(display.x))) {
            Config::MenuPosition::x = posX;
            Config::MenuPosition::positionInitialized = true;
            EnsoulSharpTheme::PositionX = static_cast<float>(posX);
            menuPosX = static_cast<float>(posX);
            if (ensoulMenuX) ensoulMenuX->Value = posX;
        }
        SDK::UI::EndFunctionalMenuRow();

        SDK::UI::BeginFunctionalMenuRow("##menu_pos_y_row");
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Menu Position (Y)");
        ImGui::SameLine();
        float targetXY = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - 120.0f;
        if (targetXY > ImGui::GetCursorPosX()) {
            ImGui::SetCursorPosX(targetXY);
        }
        ImGui::SetNextItemWidth(120.0f);
        int posY = Config::MenuPosition::y;
        if (ImGui::SliderInt("##menu_pos_y", &posY, 0, static_cast<int>(display.y))) {
            Config::MenuPosition::y = posY;
            Config::MenuPosition::positionInitialized = true;
            EnsoulSharpTheme::PositionY = static_cast<float>(posY);
            menuPosY = static_cast<float>(posY);
            if (ensoulMenuY) ensoulMenuY->Value = posY;
        }
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
            // Reserve space for: [Unload 74] [gap 8] [Load 74] [gap 8] [Always Load checkbox]
            // Unload is on the left, Load is on the right — only one is visible at a time.
            constexpr float kBtnW = 74.0f;
            constexpr float kBtnGap = 8.0f;
            constexpr float kCheckboxW = 110.0f;
            float targetX = ImGui::GetCursorPosX() + rightEdge - (kBtnW * 2 + kBtnGap + kCheckboxW + kBtnGap);
            if (targetX > ImGui::GetCursorPosX()) {
                ImGui::SetCursorPosX(targetX);
            }

            if (canLoad) {
                if (p.Loaded) {
                    // Loaded → show Unload on the LEFT slot
                    if (DrawStateButton("unload", "Unload", true, false, kBtnW)) {
                        PluginRegistry::UnloadPlugin(i);
                    }
                    // Empty placeholder for the right slot so checkbox stays aligned
                    ImGui::SameLine(0, kBtnGap);
                    ImGui::Dummy(ImVec2(kBtnW, 0));
                } else {
                    // Not loaded → empty placeholder for the left slot, Load on the RIGHT slot
                    ImGui::Dummy(ImVec2(kBtnW, ImGui::GetFrameHeight()));
                    ImGui::SameLine(0, kBtnGap);
                    if (DrawStateButton("load", "Load", false, true, kBtnW)) {
                        PluginRegistry::LoadPlugin(i);
                    }
                }
            } else {
                // N/A spans both slots
                ImGui::BeginDisabled(true);
                ImGui::Button("N/A", ImVec2(kBtnW * 2 + kBtnGap, 0));
                ImGui::EndDisabled();
            }

            ImGui::SameLine(0, kBtnGap);
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

    inline void DrawPluginSelectorContent() {
        if (ImGui::BeginTabBar("PluginCategoriesTabBar")) {
            const char* tabNames[] = { "Core Plugins", "Champion Scripts", "Utility Plugins", "Misc Plugins" };
            PluginRegistry::PluginCategory categories[] = {
                PluginRegistry::PluginCategory::Core,
                PluginRegistry::PluginCategory::Champion,
                PluginRegistry::PluginCategory::Utility,
                PluginRegistry::PluginCategory::Misc
            };

            for (int catIdx = 0; catIdx < 4; ++catIdx) {
                if (ImGui::BeginTabItem(tabNames[catIdx])) {
                    if (ImGui::BeginTable("PluginSelectorTable", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY, ImVec2(0, 300.0f))) {
                        ImGui::TableSetupColumn("Plugin Name", ImGuiTableColumnFlags_WidthStretch);
                        ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed, 80.0f);
                        ImGui::TableHeadersRow();

                        for (int i = 0; i < PluginRegistry::PluginCount; ++i) {
                            auto& p = PluginRegistry::Plugins[i];
                            if (p.Category != categories[catIdx]) {
                                continue;
                            }

                            if (p.Category == PluginRegistry::PluginCategory::Champion && !PluginRegistry::CanPluginLoad(i)) {
                                continue;
                            }

                            ImGui::PushID(i);
                            ImGui::TableNextRow();

                            // Plugin Name Column
                            ImGui::TableNextColumn();
                            ImGui::Text("%s", p.Name ? p.Name : "Unknown");
                            if (p.ChampionName && p.ChampionName[0]) {
                                ImGui::SameLine();
                                ImGui::TextColored(ImVec4(0.68f, 0.78f, 1.0f, 0.95f), "(%s)", p.ChampionName);
                            }

                            // Action Column: show only the relevant button
                            ImGui::TableNextColumn();
                            if (p.Loaded) {
                                // Loaded → Unload button (left-aligned in cell)
                                if (ImGui::Button("Unload", ImVec2(70.0f, 0.0f))) {
                                    PluginRegistry::UnloadPlugin(i);
                                }
                            } else {
                                // Not loaded → Load button
                                if (ImGui::Button("Load", ImVec2(70.0f, 0.0f))) {
                                    PluginRegistry::LoadPlugin(i);
                                }
                            }

                            ImGui::PopID();
                        }
                        ImGui::EndTable();
                    }
                    ImGui::EndTabItem();
                }
            }
            ImGui::EndTabBar();
        }
    }

    inline void DrawPluginsSection() {
        DrawPluginSelectorContent();
    }

    inline void DrawPluginSelectorRuntimeBridge(void*) {
        DrawPluginSelectorContent();
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
    // EnsoulSharp PermaShow port. The source uses Config.EnsoulSharpFont
    // (Tahoma 16), a 1.4x row height, and screenWidth / 1366 width scaling.
    // ----------------------------------------------------------------

    inline constexpr ImU32 PermaShowBackground = IM_COL32(0, 0, 0, 102);
    inline constexpr ImU32 PermaShowEnabledBox = IM_COL32(0, 100, 0, 150);
    inline constexpr ImU32 PermaShowDisabledBox = IM_COL32(139, 0, 0, 150);

    struct PermaShowTextRect {
        float left;
        float top;
        float right;
        float bottom;
    };

    inline void DrawPermaShowText(ImDrawList* draw,
                                  const char* text,
                                  const PermaShowTextRect& rect,
                                  ImU32 color,
                                  bool centered) {
        ImFont* font = SDK::UI::PermaShow::Font();
        if (!draw || !font || !text || !text[0]) return;
        const ImVec2 size = font->CalcTextSizeA(
            PermaShowFontHeight, FLT_MAX, 0.0f, text);
        float x = rect.left;
        if (centered) {
            x = rect.left + (rect.right - rect.left - size.x) * 0.5f;
        }
        const float y = rect.top + (rect.bottom - rect.top - size.y) * 0.5f;
        const ImVec4 clip(rect.left, rect.top, rect.right, rect.bottom);
        draw->AddText(
            font,
            PermaShowFontHeight,
            ImVec2(std::floor(x), std::floor(y)),
            color,
            text,
            nullptr,
            0.0f,
            &clip);
    }

    inline void DrawPermaShowEntry(ImDrawList* draw,
                                   const SDK::UI::PermaShow::Entry& entry,
                                   int index,
                                   float centerX,
                                   float top,
                                   float width,
                                   float indicatorWidth) {
        SDK::UI::MenuItem* item = entry.Item;
        if (!draw || !item) return;

        const float rowY = top + PermaShowRowHeight * static_cast<float>(index);
        const float endX = centerX + width * 0.5f;
        const float drawBasicX = centerX - 0.96f * (width * 0.5f);
        const float boxX = endX - indicatorWidth;

        // Match the int RawRectangle conversion in PermashowItem.UpdatePosition.
        const PermaShowTextRect fullRect{
            static_cast<float>(static_cast<int>(drawBasicX)),
            static_cast<float>(static_cast<int>(rowY + PermaShowFontHeight * 0.2f)),
            static_cast<float>(static_cast<int>(endX)),
            static_cast<float>(static_cast<int>(rowY + PermaShowRowHeight))
        };
        const PermaShowTextRect boxRect{
            static_cast<float>(static_cast<int>(boxX - indicatorWidth * 0.5f)),
            static_cast<float>(static_cast<int>(rowY)),
            static_cast<float>(static_cast<int>(endX + indicatorWidth * 0.5f)),
            static_cast<float>(static_cast<int>(rowY + PermaShowRowHeight))
        };

        const auto drawStateBox = [&](bool enabled) {
            draw->AddRectFilled(
                ImVec2(boxX, rowY),
                ImVec2(boxX + indicatorWidth, rowY + PermaShowRowHeight),
                enabled ? PermaShowEnabledBox : PermaShowDisabledBox,
                0.0f);
        };
        char label[192] = {};
        char value[64] = {};

        switch (item->Kind()) {
        case SDK::UI::MenuValueType::Boolean: {
            auto* boolean = static_cast<SDK::UI::MenuBool*>(item);
            drawStateBox(boolean->Value);
            std::snprintf(label, sizeof(label), "%s:", entry.DisplayName);
            DrawPermaShowText(draw, label, fullRect, entry.Color, false);
            DrawPermaShowText(
                draw, boolean->Value ? "True" : "False", boxRect, entry.Color, true);
            break;
        }
        case SDK::UI::MenuValueType::KeyBind: {
            auto* keyBind = static_cast<SDK::UI::MenuKeyBind*>(item);
            drawStateBox(keyBind->Active);
            std::snprintf(
                label,
                sizeof(label),
                "%s [%s]:",
                entry.DisplayName,
                SDK::UI::MenuKeyBind::VkToText(keyBind->Key));
            DrawPermaShowText(draw, label, fullRect, entry.Color, false);
            DrawPermaShowText(
                draw, keyBind->Active ? "True" : "False", boxRect, entry.Color, true);
            break;
        }
        case SDK::UI::MenuValueType::List: {
            auto* list = static_cast<SDK::UI::MenuList*>(item);
            std::snprintf(label, sizeof(label), "%s:", entry.DisplayName);
            DrawPermaShowText(draw, label, fullRect, entry.Color, false);
            DrawPermaShowText(draw, list->SelectedValue(), boxRect, entry.Color, true);
            break;
        }
        case SDK::UI::MenuValueType::Slider: {
            auto* slider = static_cast<SDK::UI::MenuSlider*>(item);
            std::snprintf(label, sizeof(label), "%s:", entry.DisplayName);
            std::snprintf(value, sizeof(value), "%d  ", slider->Value);
            DrawPermaShowText(draw, label, fullRect, entry.Color, false);
            DrawPermaShowText(draw, value, boxRect, entry.Color, true);
            break;
        }
        case SDK::UI::MenuValueType::SliderBtn: {
            auto* sliderButton = static_cast<SDK::UI::MenuSliderButton*>(item);
            drawStateBox(sliderButton->Enabled);
            std::snprintf(label, sizeof(label), "%s:", entry.DisplayName);
            std::snprintf(value, sizeof(value), "%d  ", sliderButton->Value);
            DrawPermaShowText(draw, label, fullRect, entry.Color, false);
            DrawPermaShowText(draw, value, boxRect, entry.Color, true);
            break;
        }
        case SDK::UI::MenuValueType::Separator:
            DrawPermaShowText(draw, entry.DisplayName, fullRect, entry.Color, true);
            break;
        default:
            break;
        }
    }

    inline void DrawPermaShowOverlay() {
        permaShowBoundsRight = 0.0f;
        permaShowBoundsBottom = 0.0f;
        const int rows = SDK::UI::PermaShow::Count();
        if (!Config::PermaShow::enabled || rows <= 0) return;

        ImDrawList* draw = ImGui::GetForegroundDrawList();
        if (!draw) return;

        EnsurePermaShowPositionInitialized();
        const ImVec2 display = GetOverlayDisplaySize();
        const float xFactor = display.x / 1366.0f;
        const float width = static_cast<float>(Config::PermaShow::width) * xFactor;
        const float indicatorWidth =
            static_cast<float>(Config::PermaShow::indicatorWidth) * xFactor;
        float centerX = static_cast<float>(Config::PermaShow::x);
        float top = static_cast<float>(Config::PermaShow::y);
        float left = centerX - width * 0.5f;
        const float height = static_cast<float>(rows) * PermaShowRowHeight;

        if (Config::PermaShow::allowDrag && showMenu) {
            ImVec2 mouse = ImGui::GetIO().MousePos;
            const bool inPermaRect =
                mouse.x >= left && mouse.x <= (left + width) &&
                mouse.y >= top && mouse.y <= (top + height);

            if (ImGui::IsMouseClicked(0) && inPermaRect &&
                !ImGui::IsAnyItemHovered() && !ImGui::IsAnyItemActive() && !isDragging && !EnsoulSharpTheme::RootDragging) {
                isPermaDragging = true;
                permaDragOffX = mouse.x - centerX;
                permaDragOffY = mouse.y - top;
            }
            if (!ImGui::IsMouseDown(0)) {
                isPermaDragging = false;
            }
            if (isPermaDragging) {
                Config::PermaShow::x = static_cast<int>(mouse.x - permaDragOffX);
                Config::PermaShow::y = static_cast<int>(mouse.y - permaDragOffY);
                Config::PermaShow::positionInitialized = true;
                if (ensoulPermaX) ensoulPermaX->Value = Config::PermaShow::x;
                if (ensoulPermaY) ensoulPermaY->Value = Config::PermaShow::y;
                centerX = static_cast<float>(Config::PermaShow::x);
                top = static_cast<float>(Config::PermaShow::y);
                left = centerX - width * 0.5f;
            }
        } else {
            isPermaDragging = false;
        }

        permaShowBoundsRight = left + width;
        permaShowBoundsBottom = top + height;
        draw->AddRectFilled(
            ImVec2(left, top),
            ImVec2(left + width, top + height),
            PermaShowBackground,
            0.0f);

        for (int i = 0; i < rows; ++i) {
            DrawPermaShowEntry(
                draw,
                SDK::UI::PermaShow::At(i),
                i,
                centerX,
                top,
                width,
                indicatorWidth);
        }
    }

    inline void DrawCoreContentPanel(int secondaryIdx) {
        if (secondaryIdx == 0) {
            DrawFeaturesSection();
        } else if (secondaryIdx == 1) {
            DrawPermaShowSection();
        } else if (secondaryIdx == 2) {
            DrawMenuSection();
        } else if (secondaryIdx == 3) {
            DrawPluginsSection();
        } else if (secondaryIdx == 4) {
            DrawDebugSection();
        } else {
            DrawFeaturesSection();
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



    inline void RenderLegacy();

    inline void Render() {
        // Persist any pending menu/core changes (debounced). Runs even while the
        // menu is hidden so a change made just before hiding still flushes.
        ConfigStore::Tick();

        if (::SDK::Drawing::IsAllDrawingHidden()) {
            ResetMouseInputCapture();
            EnsoulSharpTheme::CancelRootDrag();
            ClearFunctionalMenuSidebarStyle();
            isPermaDragging = false;
            permaShowBoundsRight = 0.0f;
            permaShowBoundsBottom = 0.0f;
            menuBoundsRight = menuPosX;
            menuBoundsBottom = menuPosY;
            return;
        }

        EnsureEnsoulCoreMenu();
        SyncEnsoulCoreMenu();
        KeepCoreMenuFirst();

        DrawPermaShowOverlay();

        if (!showMenu) {
            ResetMouseInputCapture();
            EnsoulSharpTheme::CancelRootDrag();
            ClearFunctionalMenuSidebarStyle();
            menuBoundsRight = menuPosX;
            menuBoundsBottom = menuPosY;
            if (Config::MenuStyle::index != 1) {
                EnsoulSharpTheme::DrawRuntimePopup();
            }
            return;
        }

        if (Config::MenuStyle::index == 1) {
            ResetMouseInputCapture();
            EnsoulSharpTheme::CancelRootDrag();
            RenderLegacy();
        } else {
            ClearFunctionalMenuSidebarStyle();
            EnsoulSharpTheme::Render();
            menuPosX = EnsoulSharpTheme::PositionX;
            menuPosY = EnsoulSharpTheme::PositionY;
            menuBoundsRight = EnsoulSharpTheme::BoundsRight;
            menuBoundsBottom = EnsoulSharpTheme::BoundsBottom;
        }
    }

    inline void RenderLegacy() {
        bool pushedFont = false;
        if (EnsoulSharpTheme::FontUbuntu) {
            ImGui::PushFont(EnsoulSharpTheme::FontUbuntu);
            pushedFont = true;
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
            Config::MenuPosition::x = static_cast<int>(menuPosX);
            Config::MenuPosition::y = static_cast<int>(menuPosY);
            Config::MenuPosition::positionInitialized = true;
            EnsoulSharpTheme::PositionX = menuPosX;
            EnsoulSharpTheme::PositionY = menuPosY;
            if (ensoulMenuX) ensoulMenuX->Value = Config::MenuPosition::x;
            if (ensoulMenuY) ensoulMenuY->Value = Config::MenuPosition::y;
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
            contentPanelBodyH = MaxF(ITEM_H, measuredBodyH);
        }

        ImGui::End();
        ClearFunctionalMenuSidebarStyle();
        PopFunctionalMenuItemStyle();
        ImGui::PopStyleVar(7);
        ImGui::PopStyleColor(14);

        if (pushedFont) {
            ImGui::PopFont();
        }

        menuBoundsRight = menuPosX + PRIMARY_W + PANEL_GAP + SECONDARY_W + PANEL_GAP + contentW;
        menuBoundsBottom = menuPosY + contentH;
    }

} // namespace NightSharpMenu
