#include "Renderer.h"

#include <algorithm>

namespace NightSharp::Menu {

void Renderer::RenderContentLayer(
    const Path& path,
    float visibility,
    float offset,
    bool interactive,
    const char* layerId) {
    MenuNode* node = model_.Resolve(path);
    if (!node || visibility <= 0.001f) {
        return;
    }

    const int visibleItemCount = static_cast<int>(std::count_if(
        node->items.begin(),
        node->items.end(),
        [](const MenuItemHandle& item) {
            return item && item->visible;
        }));
    if (visibleItemCount == 0) {
        return;
    }

    const std::size_t panelCount = BuildPanelPaths(path).size();
    const ImVec2 position(
        origin_.x + static_cast<float>(panelCount) *
            (theme_.panelWidth + theme_.panelGap) + offset,
        origin_.y + (1.0f - visibility) * 2.0f * theme_.dpiScale);
    const float height = std::max(
        theme_.contentHeight,
        theme_.headerHeight + theme_.padding * 2.0f +
            static_cast<float>(visibleItemCount) * 42.0f * theme_.dpiScale);
    const ImVec2 panelMax(
        position.x + theme_.contentWidth,
        position.y + height);

    if (interactive) {
        interactiveBounds_.push_back({ ImVec4(
            position.x,
            position.y,
            panelMax.x,
            panelMax.y) });
    }
    renderAlpha_ = visibility;
    interactiveLayer_ = interactive;
    ImDrawList* background = ImGui::GetBackgroundDrawList();
    background->AddRectFilled(
        position,
        panelMax,
        RenderColor(theme_.contentBackground),
        theme_.panelRounding);
    background->AddRectFilled(
        position,
        ImVec2(panelMax.x, position.y + theme_.headerHeight),
        RenderColor(theme_.headerBackground),
        theme_.panelRounding);
    background->AddRect(
        position,
        panelMax,
        RenderColor(theme_.border),
        theme_.panelRounding,
        0,
        1.0f * theme_.dpiScale);

    const std::string breadcrumb = Ellipsize(
        BuildBreadcrumb(path),
        theme_.contentWidth - theme_.padding * 2.0f);
    background->AddText(
        font_,
        ImGui::GetFontSize(),
        ImVec2(
            position.x + theme_.padding,
            position.y + 8.0f * theme_.dpiScale),
        RenderColor(theme_.accent),
        breadcrumb.c_str());

    ImGui::PushStyleColor(ImGuiCol_WindowBg, theme_.contentBackground);
    ImGui::PushStyleColor(ImGuiCol_Border, theme_.border);
    ImGui::PushStyleColor(ImGuiCol_Text, theme_.text);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, theme_.itemBackground);
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, theme_.itemHover);
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, theme_.itemActive);
    ImGui::PushStyleColor(ImGuiCol_Button, theme_.itemActive);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, theme_.itemHover);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, theme_.accent);
    ImGui::PushStyleColor(ImGuiCol_CheckMark, theme_.accent);
    ImGui::PushStyleColor(ImGuiCol_SliderGrab, theme_.itemActive);
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, theme_.accent);
    ImGui::PushStyleColor(ImGuiCol_Header, theme_.itemActive);
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, theme_.itemHover);
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, theme_.accent);
    ImGui::PushStyleColor(ImGuiCol_PopupBg, theme_.panelBackground);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, visibility);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(
        ImGuiStyleVar_WindowPadding,
        ImVec2(theme_.padding, theme_.padding));
    ImGui::PushStyleVar(
        ImGuiStyleVar_ItemSpacing,
        ImVec2(
            8.0f * theme_.dpiScale,
            9.0f * theme_.dpiScale));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);

    ImGui::SetNextWindowPos(
        ImVec2(position.x, position.y + theme_.headerHeight),
        ImGuiCond_Always);
    ImGui::SetNextWindowSize(
        ImVec2(theme_.contentWidth, height - theme_.headerHeight),
        ImGuiCond_Always);
    const std::string windowId =
        std::string("##nightsharp_menu_content_") + layerId;
    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoBackground |
        ImGuiWindowFlags_NoBringToFrontOnFocus;
    if (!interactive) {
        flags |= ImGuiWindowFlags_NoInputs;
    }

    if (ImGui::Begin(windowId.c_str(), nullptr, flags)) {
        const std::string pathKey = PathKey(path);
        for (const MenuItemHandle& item : node->items) {
            if (!item || !item->visible) {
                continue;
            }

            ImGui::PushID(item->id.c_str());
            if (!item->enabled) {
                ImGui::BeginDisabled(true);
            }

            const std::string animationKey = pathKey + "/" + item->id;
            switch (item->kind) {
            case ItemKind::Toggle:
                RenderCheckbox(*item, animationKey);
                break;
            case ItemKind::Action:
                RenderButton(*item, animationKey);
                break;
            case ItemKind::Slider:
                RenderSlider(*item, animationKey);
                break;
            case ItemKind::List:
                RenderDropdown(*item, animationKey);
                break;
            case ItemKind::KeyBind:
                RenderKeyBind(item, animationKey);
                break;
            case ItemKind::Color:
                RenderColor(*item);
                break;
            }

            if (!item->enabled) {
                ImGui::EndDisabled();
            }
            ImGui::PopID();
        }
    }
    ImGui::End();
    ImGui::PopStyleVar(5);
    ImGui::PopStyleColor(16);
    renderAlpha_ = 1.0f;
    interactiveLayer_ = true;
}

void Renderer::RenderContent() {
    const ContentTransitionSlot& slot = navigation_.Content();
    const float eased = NavigationTransition::Ease(slot.progress);
    const float direction = slot.direction == NavigationDirection::Backward
        ? -1.0f
        : 1.0f;
    if (slot.hasOutgoing) {
        RenderContentLayer(
            slot.outgoingPath,
            1.0f - eased,
            -direction * theme_.contentTransitionDistance * eased,
            false,
            "outgoing");
    }
    if (slot.hasCurrent) {
        RenderContentLayer(
            slot.currentPath,
            eased,
            direction * theme_.contentTransitionDistance * (1.0f - eased),
            eased >= 0.55f,
            "incoming");
    }
    renderAlpha_ = 1.0f;
}

}
