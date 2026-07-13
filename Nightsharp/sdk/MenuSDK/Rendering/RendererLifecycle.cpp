#include "Renderer.h"

#include <algorithm>
#include <utility>

namespace NightSharp::Menu {

Renderer::Renderer(MenuModel& model, Theme theme)
    : model_(model),
      baseTheme_(theme),
      theme_(theme),
      permashowRenderer_(permashow_) {}

bool Renderer::Initialize(
    ImFontAtlas* atlas,
    float dpiScale,
    ImFont* preferredFont,
    std::string preferredFontFamily) {
    if (initialized_) {
        return true;
    }

    theme_ = baseTheme_;
    theme_.Scale(std::max(dpiScale, 0.75f));
    originTarget_ = origin_;
    navigation_.Reset();
    permashowRenderer_.Reset();
    closingDropdownIdentity_.clear();
    hoverGraceTimer_ = 0.0f;
    renderAlpha_ = 1.0f;
    keyboardNavigation_ = false;
    keyboardMouseAnchor_ = {};
    previousPanels_.reserve(NavigationTransition::MaxPanelCount);
    hoverReferencePanels_.reserve(NavigationTransition::MaxPanelCount);
    selectedPath_.reserve(NavigationTransition::MaxPanelCount);
    hoverPath_.reserve(NavigationTransition::MaxPanelCount);
    hoverAnimator_.Reserve(256);
    activeAnimator_.Reserve(256);
    valueAnimator_.Reserve(128);

    if (preferredFont) {
        fontResult_.font = preferredFont;
        fontResult_.family = preferredFontFamily.empty()
            ? "Custom"
            : std::move(preferredFontFamily);
    } else {
        fontResult_ = LoadWindowsFont(
            atlas,
            15.0f * theme_.dpiScale);
    }
    font_ = fontResult_.font;
    initialized_ = font_ != nullptr;
    return initialized_;
}

bool Renderer::HandleMessage(UINT message, WPARAM wParam, LPARAM) {
    if (keyBindings_.HandleMessage(message, wParam)) {
        return true;
    }
    if (message != WM_KEYDOWN && message != WM_SYSKEYDOWN) {
        return false;
    }
    if (wParam == VK_INSERT) {
        SetVisible(!visible_);
        return true;
    }
    return visible_ && NavigateKeyboard(wParam);
}

void Renderer::Shutdown() {
    previousPanels_.clear();
    hoverReferencePanels_.clear();
    interactiveBounds_.clear();
    hoverAnimator_.Clear();
    activeAnimator_.Clear();
    valueAnimator_.Clear();
    navigation_.Reset();
    permashowRenderer_.Reset();
    closingDropdownIdentity_.clear();
    hoverGraceTimer_ = 0.0f;
    renderAlpha_ = 1.0f;
    keyboardNavigation_ = false;
    keyboardMouseAnchor_ = {};
    selectedPath_.clear();
    hoverPath_.clear();
    dragging_ = false;
    keyBindings_.Reset();
    font_ = nullptr;
    fontResult_ = {};
    initialized_ = false;
}

bool Renderer::IsInitialized() const {
    return initialized_;
}

bool Renderer::IsVisible() const {
    return visible_;
}

void Renderer::SetVisible(bool visible) {
    if (visible_ == visible) {
        return;
    }
    visible_ = visible;
    navigation_.Reset();
    permashowRenderer_.Reset();
    closingDropdownIdentity_.clear();
    hoverGraceTimer_ = 0.0f;
    keyboardNavigation_ = false;
    if (!visible_) {
        keyBindings_.Cancel();
    }
}

const std::string& Renderer::FontFamily() const {
    return fontResult_.family;
}

const Theme& Renderer::GetTheme() const {
    return theme_;
}

bool Renderer::IsPointInside(const ImVec2& point) const {
    if (!visible_) {
        return false;
    }
    for (const PanelBounds& bounds : interactiveBounds_) {
        if (PointInside(point, bounds.rect)) {
            return true;
        }
    }
    return false;
}

void Renderer::SetPosition(const ImVec2& position) {
    origin_ = position;
    originTarget_ = position;
}

PermashowRegistry& Renderer::Permashow() {
    return permashow_;
}

const PermashowRegistry& Renderer::Permashow() const {
    return permashow_;
}

float Renderer::Clamp01(float value) {
    return std::clamp(value, 0.0f, 1.0f);
}

bool Renderer::PointInside(const ImVec2& point, const ImVec4& rect) {
    return point.x >= rect.x && point.x <= rect.z &&
           point.y >= rect.y && point.y <= rect.w;
}

bool Renderer::HasPrefix(const Path& path, const Path& prefix) {
    if (prefix.size() > path.size()) {
        return false;
    }
    return std::equal(prefix.begin(), prefix.end(), path.begin());
}

std::string Renderer::PathKey(const Path& path) {
    std::string key;
    key.reserve(path.size() * 4);
    for (int index : path) {
        key += std::to_string(index);
        key.push_back('/');
    }
    return key;
}

ImVec4 Renderer::Lerp(
    const ImVec4& a,
    const ImVec4& b,
    float amount) {
    const float value = Clamp01(amount);
    return ImVec4(
        a.x + (b.x - a.x) * value,
        a.y + (b.y - a.y) * value,
        a.z + (b.z - a.z) * value,
        a.w + (b.w - a.w) * value);
}

ImU32 Renderer::RenderColor(const ImVec4& color, float alpha) const {
    ImVec4 adjusted = color;
    adjusted.w *= Clamp01(alpha) * Clamp01(renderAlpha_);
    return ImGui::ColorConvertFloat4ToU32(adjusted);
}

float Renderer::Smooth(float current, float target, float speed) const {
    return Animator::Smooth(
        current,
        target,
        speed,
        ImGui::GetIO().DeltaTime);
}

float Renderer::Animate(
    Animator& animator,
    const std::string& key,
    float target) const {
    return animator.Update(
        key,
        target,
        theme_.animationSpeed,
        ImGui::GetIO().DeltaTime);
}

bool Renderer::MouseInsidePreviousPanels() const {
    const ImVec2 mouse = ImGui::GetIO().MousePos;
    for (std::size_t index = 0; index < previousPanels_.size(); ++index) {
        if (PointInside(mouse, previousPanels_[index].rect)) {
            return true;
        }
        if (index + 1 < previousPanels_.size() &&
            NavigationTransition::IsInsideBridge(
                mouse,
                previousPanels_[index].rect,
                previousPanels_[index + 1].rect,
                theme_.hoverBridgePadding)) {
            return true;
        }
    }
    return false;
}

}
