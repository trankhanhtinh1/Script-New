#pragma once

#include "../Animation/Animator.h"
#include "../Core/MenuModel.h"
#include "../Input/KeyBindController.h"
#include "../Navigation/NavigationTransition.h"
#include "../Core/Permashow.h"
#include "PermashowRenderer.h"
#include "../Platform/Windows/FontLoader.h"
#include "../Styling/Theme.h"

#include <Windows.h>

#include <array>
#include <cstddef>
#include <string>
#include <vector>

namespace NightSharp::Menu {

class Renderer {
public:
    Renderer(MenuModel& model, Theme theme = {});

    bool Initialize(
        ImFontAtlas* atlas,
        float dpiScale = 1.0f,
        ImFont* preferredFont = nullptr,
        std::string preferredFontFamily = {});
    bool HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);
    void Render();
    void Shutdown();

    bool IsInitialized() const;
    bool IsVisible() const;
    void SetVisible(bool visible);
    const std::string& FontFamily() const;
    const Theme& GetTheme() const;
    void SetPosition(const ImVec2& position);
    bool IsPointInside(const ImVec2& point) const;
    PermashowRegistry& Permashow();
    const PermashowRegistry& Permashow() const;

private:
    using Path = MenuPath;

    struct PanelBounds {
        ImVec4 rect{};
    };

    struct LayoutMetrics {
        ImVec2 size{};
        std::size_t panelCount = 0;
        bool hasContent = false;
    };

    MenuModel& model_;
    Theme baseTheme_;
    Theme theme_;
    FontLoadResult fontResult_{};
    ImFont* font_ = nullptr;
    KeyBindController keyBindings_;
    NavigationTransition navigation_;
    PermashowRegistry permashow_;
    PermashowRenderer permashowRenderer_;
    bool initialized_ = false;
    bool visible_ = true;
    bool dragging_ = false;
    ImVec2 origin_ = ImVec2(24.0f, 24.0f);
    ImVec2 originTarget_ = ImVec2(24.0f, 24.0f);
    ImVec2 dragOffset_{};
    Path selectedPath_;
    Path hoverPath_;
    std::vector<PanelBounds> previousPanels_;
    std::vector<PanelBounds> hoverReferencePanels_;
    std::vector<PanelBounds> interactiveBounds_;
    Animator hoverAnimator_;
    Animator activeAnimator_;
    Animator valueAnimator_;
    std::string closingDropdownIdentity_;
    float hoverGraceTimer_ = 0.0f;
    float renderAlpha_ = 1.0f;
    bool interactiveLayer_ = true;
    bool keyboardNavigation_ = false;
    ImVec2 keyboardMouseAnchor_{};
    int renderedColumnCount_ = 0;

    static float Clamp01(float value);
    static bool PointInside(const ImVec2& point, const ImVec4& rect);
    static bool HasPrefix(const Path& path, const Path& prefix);
    static std::string PathKey(const Path& path);
    static ImVec4 Lerp(const ImVec4& a, const ImVec4& b, float amount);

    ImU32 RenderColor(const ImVec4& color, float alpha = 1.0f) const;
    float Smooth(float current, float target, float speed) const;
    float Animate(
        Animator& animator,
        const std::string& key,
        float target) const;
    bool MouseInsidePreviousPanels() const;
    std::vector<Path> BuildPanelPaths(const Path& displayPath) const;
    LayoutMetrics MeasureLayout(const Path& displayPath) const;
    bool HasContent(const Path& path) const;
    std::string BuildBreadcrumb(const Path& path) const;
    std::string Ellipsize(const std::string& text, float maximumWidth) const;
    void ClampOrigin(const ImVec2& displaySize, const ImVec2& layoutSize);
    bool NavigateKeyboard(WPARAM key);
    void RenderColumns();
    void RenderPanelLayer(
        const Path& panelPath,
        std::size_t depth,
        float visibility,
        float offset,
        bool interactive);
    void RenderPanel(
        ImDrawList* drawList,
        const ImVec2& position,
        float height,
        const char* title);
    void RenderAnimatedTitle(
        ImDrawList* drawList,
        const ImVec2& position,
        const char* title);
    bool RenderNodeRow(
        ImDrawList* drawList,
        const ImVec2& position,
        const MenuNode& node,
        const Path& path,
        bool active,
        bool interactive,
        const ImVec4* childPanelRect);
    bool RenderCheckbox(MenuItem& item, const std::string& animationKey);
    bool RenderButton(MenuItem& item, const std::string& animationKey);
    bool RenderSlider(MenuItem& item, const std::string& animationKey);
    bool RenderDropdown(MenuItem& item, const std::string& animationKey);
    bool RenderKeyBind(
        const MenuItemHandle& item,
        const std::string& animationKey);
    bool RenderColor(MenuItem& item);
    void RenderContent();
    void RenderContentLayer(
        const Path& path,
        float visibility,
        float offset,
        bool interactive,
        const char* layerId);
};

}
