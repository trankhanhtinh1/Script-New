#include "../Rendering/Renderer.h"

namespace NightSharp::Menu {

bool Renderer::RenderCheckbox(
    MenuItem& item,
    const std::string& animationKey) {
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(item.label.c_str());
    ImGui::SameLine();

    const float boxSize = 22.0f * theme_.dpiScale;
    const float targetX = ImGui::GetCursorPosX() +
        ImGui::GetContentRegionAvail().x - boxSize;
    if (targetX > ImGui::GetCursorPosX()) {
        ImGui::SetCursorPosX(targetX);
    }

    const ImVec2 position = ImGui::GetCursorScreenPos();
    const bool clicked = interactiveLayer_ && ImGui::InvisibleButton(
        "##checkbox",
        ImVec2(boxSize, boxSize));
    if (clicked) {
        item.SetChecked(!item.value);
    }

    const bool hovered = interactiveLayer_ && ImGui::IsItemHovered();
    const bool held = interactiveLayer_ && ImGui::IsItemActive();
    const float rawValue = valueAnimator_.Update(
        "checkbox_value/" + animationKey,
        item.value ? 1.0f : 0.0f,
        15.0f,
        ImGui::GetIO().DeltaTime);
    const float value = rawValue * rawValue *
        (3.0f - 2.0f * rawValue);
    const float hover = Animate(
        hoverAnimator_,
        "checkbox_hover/" + animationKey,
        hovered ? 1.0f : 0.0f);
    const float press = Animate(
        activeAnimator_,
        "checkbox_press/" + animationKey,
        held ? 1.0f : 0.0f);
    ImVec4 boxColor = Lerp(theme_.itemBackground, theme_.itemActive, value);
    boxColor = Lerp(boxColor, theme_.itemHover, hover * 0.55f);
    boxColor = Lerp(boxColor, theme_.accent, press * 0.16f);
    if (!item.enabled) {
        boxColor = Lerp(boxColor, theme_.disabled, 0.65f);
    }

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(
        position,
        ImVec2(position.x + boxSize, position.y + boxSize),
        RenderColor(boxColor),
        0.0f);
    drawList->AddRect(
        position,
        ImVec2(position.x + boxSize, position.y + boxSize),
        RenderColor(Lerp(theme_.border, theme_.accent, value)),
        0.0f,
        0,
        1.0f * theme_.dpiScale);

    if (value > 0.012f) {
        const ImVec2 center(
            position.x + boxSize * 0.5f,
            position.y + boxSize * 0.5f);
        const float checkScale = 0.84f + value * 0.16f;
        const ImVec2 start(
            center.x - 6.0f * theme_.dpiScale * checkScale,
            center.y - 0.2f * theme_.dpiScale * checkScale);
        const ImVec2 middle(
            center.x - 1.8f * theme_.dpiScale * checkScale,
            center.y + 4.7f * theme_.dpiScale * checkScale);
        const ImVec2 end(
            center.x + 7.0f * theme_.dpiScale * checkScale,
            center.y - 5.2f * theme_.dpiScale * checkScale);
        ImVec4 checkColor = theme_.text;
        checkColor.w *= value;
        drawList->PathLineTo(start);
        drawList->PathLineTo(middle);
        drawList->PathLineTo(end);
        drawList->PathStroke(
            RenderColor(checkColor),
            ImDrawFlags_None,
            (1.75f + value * 0.25f) * theme_.dpiScale);
    }
    return clicked;
}

}
