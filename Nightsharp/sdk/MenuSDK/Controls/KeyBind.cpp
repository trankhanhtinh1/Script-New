#include "../Rendering/Renderer.h"

namespace NightSharp::Menu {

bool Renderer::RenderKeyBind(
    const MenuItemHandle& item,
    const std::string& animationKey) {
    if (!item) {
        return false;
    }

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(item->label.c_str());
    ImGui::SameLine();

    const float width = 116.0f * theme_.dpiScale;
    const float height = 24.0f * theme_.dpiScale;
    const float targetX = ImGui::GetCursorPosX() +
        ImGui::GetContentRegionAvail().x - width;
    if (targetX > ImGui::GetCursorPosX()) {
        ImGui::SetCursorPosX(targetX);
    }

    const ImVec2 position = ImGui::GetCursorScreenPos();
    const bool clicked = interactiveLayer_ && ImGui::InvisibleButton(
        "##keybind",
        ImVec2(width, height));
    if (clicked) {
        keyBindings_.BeginCapture(item);
    }

    const bool capturing = keyBindings_.IsCapturing(*item);
    const bool hovered = interactiveLayer_ && ImGui::IsItemHovered();
    const float hover = Animate(
        hoverAnimator_,
        "keybind_hover/" + animationKey,
        hovered ? 1.0f : 0.0f);
    const float active = Animate(
        activeAnimator_,
        "keybind_capture/" + animationKey,
        capturing ? 1.0f : 0.0f);
    ImVec4 color = Lerp(
        theme_.itemBackground,
        theme_.itemHover,
        hover * 0.65f);
    color = Lerp(color, theme_.itemActive, active);
    if (!item->enabled) {
        color = Lerp(color, theme_.disabled, 0.65f);
    }

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(
        position,
        ImVec2(position.x + width, position.y + height),
        RenderColor(color),
        0.0f);
    drawList->AddRect(
        position,
        ImVec2(position.x + width, position.y + height),
        RenderColor(Lerp(theme_.border, theme_.accent, active)),
        0.0f,
        0,
        1.0f * theme_.dpiScale);

    const std::string label = capturing
        ? "Press a key..."
        : KeyBindController::KeyName(item->key);
    const ImVec2 labelSize = ImGui::CalcTextSize(label.c_str());
    drawList->AddText(
        ImVec2(
            position.x + (width - labelSize.x) * 0.5f,
            position.y + (height - labelSize.y) * 0.5f),
        RenderColor(capturing ? theme_.accent : theme_.text),
        label.c_str());
    return clicked;
}

}
