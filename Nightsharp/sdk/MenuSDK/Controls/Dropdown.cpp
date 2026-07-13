#include "../Rendering/Renderer.h"

#include <algorithm>
#include <string>

namespace NightSharp::Menu {

bool Renderer::RenderDropdown(
    MenuItem& item,
    const std::string& animationKey) {
    if (item.options.empty()) {
        return false;
    }

    item.selected = std::clamp(
        item.selected,
        0,
        static_cast<int>(item.options.size()) - 1);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(item.label.c_str());
    ImGui::SameLine();

    const float width = 210.0f * theme_.dpiScale;
    const float height = 26.0f * theme_.dpiScale;
    const float targetX = ImGui::GetCursorPosX() +
        ImGui::GetContentRegionAvail().x - width;
    if (targetX > ImGui::GetCursorPosX()) {
        ImGui::SetCursorPosX(targetX);
    }

    const ImVec2 position = ImGui::GetCursorScreenPos();
    const bool clicked = interactiveLayer_ && ImGui::InvisibleButton(
        "##dropdown_button",
        ImVec2(width, height));
    bool open = interactiveLayer_ && ImGui::IsPopupOpen("##dropdown_popup");
    if (clicked && !open) {
        closingDropdownIdentity_.clear();
        ImGui::OpenPopup("##dropdown_popup");
        open = true;
    } else if (clicked && open) {
        closingDropdownIdentity_ = animationKey;
    }
    const bool closing = closingDropdownIdentity_ == animationKey;

    const bool hovered = interactiveLayer_ && ImGui::IsItemHovered();
    const float hover = Animate(
        hoverAnimator_,
        "dropdown_hover/" + animationKey,
        hovered ? 1.0f : 0.0f);
    const float openAmount = activeAnimator_.Update(
        "dropdown_open/" + animationKey,
        open && !closing ? 1.0f : 0.0f,
        14.0f,
        ImGui::GetIO().DeltaTime);
    const float reveal = openAmount * openAmount *
        (3.0f - 2.0f * openAmount);
    ImVec4 background = Lerp(
        theme_.itemBackground,
        theme_.itemHover,
        hover * 0.42f);
    background = Lerp(
        background,
        theme_.itemActive,
        openAmount * 0.72f);
    if (!item.enabled) {
        background = Lerp(background, theme_.disabled, 0.65f);
    }

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(
        position,
        ImVec2(position.x + width, position.y + height),
        RenderColor(background),
        0.0f);
    drawList->AddRect(
        position,
        ImVec2(position.x + width, position.y + height),
        RenderColor(Lerp(
            theme_.border,
            theme_.accent,
            openAmount * 0.80f)),
        0.0f,
        0,
        1.0f * theme_.dpiScale);

    const char* preview = item.options[item.selected].c_str();
    drawList->PushClipRect(
        ImVec2(position.x + 10.0f * theme_.dpiScale, position.y),
        ImVec2(position.x + width - 34.0f * theme_.dpiScale, position.y + height),
        true);
    drawList->AddText(
        ImVec2(
            position.x + 10.0f * theme_.dpiScale,
            position.y + (height - ImGui::GetFontSize()) * 0.5f),
        RenderColor(item.enabled ? theme_.text : theme_.disabled),
        preview);
    drawList->PopClipRect();

    const float arrowCenterX =
        position.x + width - 15.0f * theme_.dpiScale;
    const float arrowCenterY = position.y + height * 0.5f;
    const float arrowWidth = 4.0f * theme_.dpiScale;
    const float arrowHeight = 2.8f * theme_.dpiScale;
    const float direction = 1.0f - openAmount * 2.0f;
    drawList->PathLineTo(ImVec2(
        arrowCenterX - arrowWidth,
        arrowCenterY - arrowHeight * direction));
    drawList->PathLineTo(ImVec2(
        arrowCenterX,
        arrowCenterY + arrowHeight * direction));
    drawList->PathLineTo(ImVec2(
        arrowCenterX + arrowWidth,
        arrowCenterY - arrowHeight * direction));
    drawList->PathStroke(
        RenderColor(Lerp(
            theme_.textDim,
            theme_.accent,
            std::max(hover, openAmount))),
        ImDrawFlags_None,
        (1.25f + openAmount * 0.15f) * theme_.dpiScale);

    const float optionHeight = 29.0f * theme_.dpiScale;
    const float popupPadding = 4.0f * theme_.dpiScale;
    const float optionsHeight =
        optionHeight * static_cast<float>(item.options.size());
    const float popupHeight = popupPadding * 2.0f +
        std::max(1.0f * theme_.dpiScale, optionsHeight * reveal);
    if (interactiveLayer_) {
        ImGui::SetNextWindowPos(
            ImVec2(
                position.x,
                position.y + height + 2.0f * theme_.dpiScale),
            ImGuiCond_Always);
        ImGui::SetNextWindowSize(
            ImVec2(width, popupHeight),
            ImGuiCond_Always);
    }
    const ImVec4 popupColor = Lerp(
        theme_.contentBackground,
        theme_.panelBackground,
        reveal);
    ImVec4 popupBorder = Lerp(
        theme_.contentBackground,
        theme_.border,
        reveal);
    popupBorder.w *= 0.35f + reveal * 0.65f;
    if (interactiveLayer_) {
        ImGui::SetNextWindowBgAlpha(0.94f + reveal * 0.06f);
    }
    ImGui::PushStyleColor(ImGuiCol_PopupBg, popupColor);
    ImGui::PushStyleColor(ImGuiCol_Border, popupBorder);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(
        ImGuiStyleVar_WindowMinSize,
        ImVec2(1.0f, 1.0f));
    ImGui::PushStyleVar(
        ImGuiStyleVar_WindowPadding,
        ImVec2(popupPadding, popupPadding));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f * theme_.dpiScale);

    bool changed = false;
    if (interactiveLayer_ && ImGui::BeginPopup(
            "##dropdown_popup",
            ImGuiWindowFlags_NoTitleBar |
                ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoMove |
                ImGuiWindowFlags_NoSavedSettings |
                ImGuiWindowFlags_NoScrollbar |
                ImGuiWindowFlags_NoScrollWithMouse)) {
        ImDrawList* popupDrawList = ImGui::GetWindowDrawList();
        for (int index = 0;
             index < static_cast<int>(item.options.size());
             ++index) {
            ImGui::PushID(index);
            const ImVec2 optionPosition = ImGui::GetCursorScreenPos();
            const bool selected = item.selected == index;
            const bool optionClicked = ImGui::InvisibleButton(
                "##option",
                ImVec2(width - popupPadding * 2.0f, optionHeight));
            const bool optionHovered = ImGui::IsItemHovered();
            const std::string optionKey =
                animationKey + "/option/" + std::to_string(index);
            const float optionHover = Animate(
                hoverAnimator_,
                "dropdown_option_hover/" + optionKey,
                optionHovered ? 1.0f : 0.0f);
            const float optionSelected = Animate(
                activeAnimator_,
                "dropdown_option_selected/" + optionKey,
                selected ? 1.0f : 0.0f);
            const float optionStart = std::min(
                0.72f,
                static_cast<float>(index) * 0.055f);
            const float optionReveal = Clamp01(
                (reveal - optionStart) /
                std::max(0.001f, 1.0f - optionStart));
            const float optionEase = optionReveal * optionReveal *
                (3.0f - 2.0f * optionReveal);
            ImVec4 optionColor = Lerp(
                theme_.panelBackground,
                theme_.itemActive,
                optionSelected * 0.88f);
            optionColor = Lerp(
                optionColor,
                theme_.itemHover,
                optionHover * 0.78f);
            optionColor.w *= optionEase;
            popupDrawList->AddRectFilled(
                optionPosition,
                ImVec2(
                    optionPosition.x + width - popupPadding * 2.0f,
                    optionPosition.y + optionHeight),
                RenderColor(optionColor),
                0.0f);
            ImVec4 optionText = selected ? theme_.text : theme_.textDim;
            optionText.w *= optionEase;
            popupDrawList->AddText(
                ImVec2(
                    optionPosition.x +
                        (11.0f - (1.0f - optionEase) * 4.0f) *
                            theme_.dpiScale,
                    optionPosition.y +
                        (optionHeight - ImGui::GetFontSize()) * 0.5f),
                RenderColor(optionText),
                item.options[index].c_str());

            if (optionClicked && optionReveal > 0.98f) {
                changed = item.SetSelected(index);
                closingDropdownIdentity_ = animationKey;
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
            ImGui::PopID();
        }
        if (closing && openAmount <= 0.025f) {
            ImGui::CloseCurrentPopup();
            closingDropdownIdentity_.clear();
        }
        ImGui::EndPopup();
    } else if (!open) {
        closingDropdownIdentity_.clear();
    }

    ImGui::PopStyleVar(5);
    ImGui::PopStyleColor(2);
    return changed;
}

}
