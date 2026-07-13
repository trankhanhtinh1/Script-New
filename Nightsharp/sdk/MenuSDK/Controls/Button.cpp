#include "../Rendering/Renderer.h"

namespace NightSharp::Menu {

bool Renderer::RenderButton(
    MenuItem& item,
    const std::string& animationKey) {
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(item.label.c_str());
    ImGui::SameLine();

    const float width = 96.0f * theme_.dpiScale;
    const float height = 26.0f * theme_.dpiScale;
    const float targetX = ImGui::GetCursorPosX() +
        ImGui::GetContentRegionAvail().x - width;
    if (targetX > ImGui::GetCursorPosX()) {
        ImGui::SetCursorPosX(targetX);
    }

    const ImVec2 position = ImGui::GetCursorScreenPos();
    const bool clicked = interactiveLayer_ && ImGui::InvisibleButton(
        "##button",
        ImVec2(width, height));
    const bool hovered = interactiveLayer_ && ImGui::IsItemHovered();
    const bool held = interactiveLayer_ && ImGui::IsItemActive();
    const float hover = Animate(
        hoverAnimator_,
        "button_hover/" + animationKey,
        hovered ? 1.0f : 0.0f);
    const float press = Animate(
        activeAnimator_,
        "button_press/" + animationKey,
        held ? 1.0f : 0.0f);

    ImVec4 color = Lerp(theme_.itemActive, theme_.itemHover, hover);
    color = Lerp(color, theme_.accent, press * 0.65f);
    if (!item.enabled) {
        color = Lerp(color, theme_.disabled, 0.65f);
    }
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(
        position,
        ImVec2(position.x + width, position.y + height),
        RenderColor(color),
        0.0f);

    const char* label = item.actionLabel.empty()
        ? "Run"
        : item.actionLabel.c_str();
    const ImVec2 labelSize = ImGui::CalcTextSize(label);
    drawList->AddText(
        ImVec2(
            position.x + (width - labelSize.x) * 0.5f,
            position.y + (height - labelSize.y) * 0.5f),
        RenderColor(item.enabled ? theme_.text : theme_.disabled),
        label);
    if (clicked) {
        item.Notify();
    }
    return clicked;
}

}
