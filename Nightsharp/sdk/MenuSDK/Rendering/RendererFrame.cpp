#include "Renderer.h"

#include <algorithm>
#include <cstdio>

namespace NightSharp::Menu {

void Renderer::Render() {
    if (!initialized_ || !ImGui::GetCurrentContext()) {
        return;
    }

    ImGui::PushFont(font_);
    const ImGuiIO& io = ImGui::GetIO();
    permashowRenderer_.Render(
        theme_,
        font_,
        io.DisplaySize,
        io.DeltaTime);
    if (!visible_) {
        ImGui::PopFont();
        return;
    }
    const ImVec2 displaySize = io.DisplaySize;
    const ImVec2 mouse = io.MousePos;
    navigation_.BeginPointerFrame(mouse, io.DeltaTime);
    if (keyboardNavigation_ &&
        navigation_.PointerMoved(theme_.keyboardMouseThreshold)) {
        keyboardNavigation_ = false;
    }

    const bool mouseInsidePanels = MouseInsidePreviousPanels();
    if (mouseInsidePanels) {
        hoverGraceTimer_ = 0.10f;
    } else {
        hoverGraceTimer_ = std::max(
            0.0f,
            hoverGraceTimer_ - std::clamp(io.DeltaTime, 0.0f, 0.05f));
    }
    if (!keyboardNavigation_ &&
        !mouseInsidePanels &&
        hoverGraceTimer_ <= 0.0f &&
        !previousPanels_.empty() &&
        !PointInside(mouse, previousPanels_.front().rect)) {
        hoverPath_.clear();
    }

    Path displayPath = selectedPath_;
    if (!keyboardNavigation_ &&
        !hoverPath_.empty() &&
        (mouseInsidePanels || hoverGraceTimer_ > 0.0f)) {
        displayPath = hoverPath_;
    }
    if (!displayPath.empty() && !model_.Resolve(displayPath)) {
        displayPath.clear();
        selectedPath_.clear();
        hoverPath_.clear();
    }

    const std::vector<Path> panelPaths = BuildPanelPaths(displayPath);
    const LayoutMetrics layout = MeasureLayout(displayPath);
    navigation_.SetTarget(displayPath, panelPaths, layout.hasContent);
    navigation_.Update(
        io.DeltaTime,
        theme_.panelTransitionDuration,
        theme_.contentTransitionDuration);

    const ImVec4 dragRect(
        origin_.x,
        origin_.y,
        origin_.x + theme_.panelWidth,
        origin_.y + theme_.headerHeight);
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
        PointInside(mouse, dragRect) &&
        !ImGui::IsAnyItemHovered() &&
        !ImGui::IsAnyItemActive()) {
        dragging_ = true;
        dragOffset_ = ImVec2(
            mouse.x - originTarget_.x,
            mouse.y - originTarget_.y);
    }
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        dragging_ = false;
    }
    if (dragging_) {
        originTarget_ = ImVec2(
            mouse.x - dragOffset_.x,
            mouse.y - dragOffset_.y);
    }
    ClampOrigin(displaySize, layout.size);
    origin_.x = Smooth(
        origin_.x,
        originTarget_.x,
        theme_.dragAnimationSpeed);
    origin_.y = Smooth(
        origin_.y,
        originTarget_.y,
        theme_.dragAnimationSpeed);

    interactiveBounds_.clear();
    RenderColumns();
    RenderContent();

    char status[160] = {};
    std::snprintf(
        status,
        sizeof(status),
        "MenuSDK  |  DX11  |  %.0f FPS  |  %s",
        ImGui::GetIO().Framerate,
        fontResult_.family.c_str());
    const ImVec2 statusSize = ImGui::CalcTextSize(status);
    ImGui::GetForegroundDrawList()->AddText(
        ImVec2(
            std::max(
                8.0f,
                displaySize.x - statusSize.x -
                    16.0f * theme_.dpiScale),
            std::max(
                8.0f,
                displaySize.y - statusSize.y -
                    12.0f * theme_.dpiScale)),
        RenderColor(theme_.textDim),
        status);
    ImGui::PopFont();
}

}
