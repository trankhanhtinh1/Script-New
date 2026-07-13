#include "../Rendering/Renderer.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace NightSharp::Menu {

bool Renderer::RenderSlider(
    MenuItem& item,
    const std::string& animationKey) {
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(item.label.c_str());
    ImGui::SameLine();

    const float width = 210.0f * theme_.dpiScale;
    const float height = 22.0f * theme_.dpiScale;
    const float targetX = ImGui::GetCursorPosX() +
        ImGui::GetContentRegionAvail().x - width;
    if (targetX > ImGui::GetCursorPosX()) {
        ImGui::SetCursorPosX(targetX);
    }

    const ImVec2 position = ImGui::GetCursorScreenPos();
    if (interactiveLayer_) {
        ImGui::InvisibleButton("##slider", ImVec2(width, height));
    } else {
        ImGui::Dummy(ImVec2(width, height));
    }
    const bool hovered = interactiveLayer_ && ImGui::IsItemHovered();
    const bool held = interactiveLayer_ && ImGui::IsItemActive();
    bool changed = false;
    if ((hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) || held) {
        const float normalized = Clamp01(
            (ImGui::GetIO().MousePos.x - position.x) / width);
        const int next = item.minimum + static_cast<int>(std::round(
            normalized * static_cast<float>(item.maximum - item.minimum)));
        changed = item.SetInteger(next);
    }

    const float targetValue = item.maximum > item.minimum
        ? static_cast<float>(item.integer - item.minimum) /
            static_cast<float>(item.maximum - item.minimum)
        : 0.0f;
    const float value = Animate(
        valueAnimator_,
        "slider_value/" + animationKey,
        Clamp01(targetValue));
    const float hover = Animate(
        hoverAnimator_,
        "slider_hover/" + animationKey,
        hovered ? 1.0f : 0.0f);
    const float press = Animate(
        activeAnimator_,
        "slider_press/" + animationKey,
        held ? 1.0f : 0.0f);

    ImVec4 baseColor = Lerp(
        theme_.itemBackground,
        theme_.itemHover,
        hover * 0.45f);
    ImVec4 fillColor = Lerp(
        theme_.itemActive,
        theme_.accent,
        press * 0.55f);
    if (!item.enabled) {
        baseColor = Lerp(baseColor, theme_.disabled, 0.65f);
        fillColor = Lerp(fillColor, theme_.disabled, 0.65f);
    }

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(
        position,
        ImVec2(position.x + width, position.y + height),
        RenderColor(baseColor),
        0.0f);
    drawList->AddRectFilled(
        position,
        ImVec2(position.x + width * value, position.y + height),
        RenderColor(fillColor),
        0.0f);

    char valueText[24] = {};
    std::snprintf(valueText, sizeof(valueText), "%d", item.integer);
    const ImVec2 textSize = ImGui::CalcTextSize(valueText);
    drawList->AddText(
        ImVec2(
            position.x + (width - textSize.x) * 0.5f,
            position.y + (height - textSize.y) * 0.5f),
        RenderColor(item.enabled ? theme_.text : theme_.disabled),
        valueText);
    return changed;
}

}
