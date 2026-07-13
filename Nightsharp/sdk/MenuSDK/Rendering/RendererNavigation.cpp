#include "Renderer.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace NightSharp::Menu {

void Renderer::RenderAnimatedTitle(
    ImDrawList* drawList,
    const ImVec2& position,
    const char* title) {
    if (!title || !title[0]) {
        return;
    }

    constexpr int paletteSize = 4;
    const ImVec4 palette[paletteSize] = {
        ImVec4(0.27f, 0.58f, 0.34f, 1.0f),
        ImVec4(0.29f, 0.51f, 0.43f, 1.0f),
        ImVec4(0.34f, 0.45f, 0.55f, 1.0f),
        ImVec4(0.40f, 0.38f, 0.48f, 1.0f),
    };
    const float fontSize = ImGui::GetFontSize();
    const ImVec2 textSize = ImGui::CalcTextSize(title);
    float cursorX = position.x +
        (theme_.panelWidth - textSize.x) * 0.5f;
    const float animation = static_cast<float>(
        std::fmod(ImGui::GetTime() * 0.12, 1.0));
    const int length = static_cast<int>(std::strlen(title));

    for (int index = 0; index < length; ++index) {
        char glyph[2] = { title[index], '\0' };
        const ImVec2 glyphSize = ImGui::CalcTextSize(glyph);
        const float normalized = textSize.x > 0.0f
            ? (cursorX - position.x) / textSize.x
            : 0.0f;
        const float palettePosition = std::fmod(
            (normalized - animation + 1.0f) * paletteSize,
            static_cast<float>(paletteSize));
        const int paletteIndex = static_cast<int>(palettePosition);
        const float blend = palettePosition -
            static_cast<float>(paletteIndex);
        const ImVec4 color = Lerp(
            palette[paletteIndex % paletteSize],
            palette[(paletteIndex + 1) % paletteSize],
            blend);
        drawList->AddText(
            font_,
            fontSize,
            ImVec2(
                cursorX,
                position.y + (theme_.headerHeight - fontSize) * 0.5f),
            RenderColor(color),
            glyph);
        cursorX += glyphSize.x;
    }
}

void Renderer::RenderPanel(
    ImDrawList* drawList,
    const ImVec2& position,
    float height,
    const char* title) {
    const ImVec2 panelMax(
        position.x + theme_.panelWidth,
        position.y + height);
    drawList->AddRectFilled(
        position,
        panelMax,
        RenderColor(theme_.panelBackground),
        theme_.panelRounding);
    drawList->AddRectFilled(
        position,
        ImVec2(panelMax.x, position.y + theme_.headerHeight),
        RenderColor(theme_.headerBackground),
        theme_.panelRounding);
    drawList->AddRect(
        position,
        panelMax,
        RenderColor(theme_.border),
        theme_.panelRounding,
        0,
        1.0f * theme_.dpiScale);
    drawList->AddLine(
        ImVec2(position.x, position.y + theme_.headerHeight),
        ImVec2(panelMax.x, position.y + theme_.headerHeight),
        RenderColor(theme_.border),
        1.0f * theme_.dpiScale);
    if (title && title[0]) {
        if (std::strcmp(title, "NightSharp") == 0) {
            RenderAnimatedTitle(
                drawList,
                position,
                title);
        } else {
            drawList->AddText(
                font_,
                ImGui::GetFontSize(),
                ImVec2(
                    position.x + theme_.padding,
                    position.y + 8.0f * theme_.dpiScale),
                RenderColor(theme_.accent),
                title);
        }
    }
}

bool Renderer::RenderNodeRow(
    ImDrawList* drawList,
    const ImVec2& position,
    const MenuNode& node,
    const Path& path,
    bool active,
    bool interactive,
    const ImVec4* childPanelRect) {
    const ImVec2 rowMax(
        position.x + theme_.panelWidth,
        position.y + theme_.rowHeight);
    const bool hovered = interactive &&
        !keyboardNavigation_ &&
        ImGui::IsMouseHoveringRect(position, rowMax, false);
    const std::string key = PathKey(path);
    const float hoverAmount = Animate(
        hoverAnimator_,
        "node_hover/" + key,
        hovered ? 1.0f : 0.0f);
    const float activeAmount = Animate(
        activeAnimator_,
        "node_active/" + key,
        active ? 1.0f : 0.0f);
    const ImVec4 base = Lerp(
        theme_.itemBackground,
        theme_.itemActive,
        activeAmount);
    const ImVec4 background = Lerp(
        base,
        theme_.itemHover,
        hoverAmount);

    drawList->AddRectFilled(
        position,
        rowMax,
        RenderColor(background),
        0.0f);
    if (activeAmount > 0.01f) {
        drawList->AddRectFilled(
            ImVec2(
                position.x,
                position.y + 2.0f * theme_.dpiScale),
            ImVec2(
                position.x + 3.0f * theme_.dpiScale,
                rowMax.y - 2.0f * theme_.dpiScale),
            RenderColor(theme_.accent),
            0.0f);
    }

    const bool hasSecondaryLabel = !node.secondaryLabel.empty();
    drawList->AddText(
        font_,
        ImGui::GetFontSize(),
        ImVec2(
            position.x + 12.0f * theme_.dpiScale,
            position.y + (hasSecondaryLabel ? 3.0f : 7.0f) * theme_.dpiScale),
        RenderColor(node.enabled ? theme_.text : theme_.disabled),
        node.label.c_str());
    if (hasSecondaryLabel) {
        drawList->AddText(
            font_,
            ImGui::GetFontSize() * 0.72f,
            ImVec2(
                position.x + 12.0f * theme_.dpiScale,
                position.y + 19.0f * theme_.dpiScale),
            RenderColor(theme_.textDim),
            node.secondaryLabel.c_str());
    }
    if (!node.children.empty()) {
        const float arrowAmount = Animate(
            activeAnimator_,
            "node_arrow/" + key,
            hovered || active ? 1.0f : 0.0f);
        const float centerX = rowMax.x -
            (15.0f - arrowAmount * 1.5f) * theme_.dpiScale;
        const float centerY = position.y + theme_.rowHeight * 0.5f;
        const float halfWidth =
            (2.8f + arrowAmount * 0.4f) * theme_.dpiScale;
        const float halfHeight =
            (4.3f + arrowAmount * 0.4f) * theme_.dpiScale;
        const ImVec4 arrowColor = Lerp(
            theme_.textDim,
            theme_.accent,
            arrowAmount * 0.85f);
        drawList->PathLineTo(ImVec2(
            centerX - halfWidth,
            centerY - halfHeight));
        drawList->PathLineTo(ImVec2(centerX, centerY));
        drawList->PathLineTo(ImVec2(
            centerX - halfWidth,
            centerY + halfHeight));
        drawList->PathStroke(
            RenderColor(arrowColor),
            ImDrawFlags_None,
            (1.25f + arrowAmount * 0.20f) * theme_.dpiScale);
    }

    if (hovered) {
        bool movingTowardChild = false;
        if (childPanelRect && !hoverPath_.empty() && hoverPath_ != path) {
            const bool childOnRight = childPanelRect->x >= rowMax.x;
            movingTowardChild = navigation_.IsMovingToward(childOnRight);
        }
        if (navigation_.ConsiderHover(
                path,
                hoverPath_,
                movingTowardChild,
                theme_.hoverIntentDelay)) {
            hoverPath_ = path;
        }
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && node.enabled) {
            selectedPath_ = path;
            hoverPath_ = path;
            keyboardNavigation_ = false;
        }
    }
    return hovered;
}

void Renderer::RenderPanelLayer(
    const Path& panelPath,
    std::size_t depth,
    float visibility,
    float offset,
    bool interactive) {
    if (visibility <= 0.001f) {
        return;
    }

    const std::vector<MenuNodeHandle>* nodes = nullptr;
    const MenuNode* parent = nullptr;
    if (panelPath.empty()) {
        nodes = &model_.roots;
    } else {
        parent = model_.Resolve(panelPath);
        if (parent) {
            nodes = &parent->children;
        }
    }
    if (!nodes || nodes->empty()) {
        return;
    }

    const int visibleCount = static_cast<int>(std::count_if(
        nodes->begin(),
        nodes->end(),
        [](const MenuNodeHandle& node) {
            return node && node->visible;
        }));
    if (visibleCount == 0) {
        return;
    }

    const float height = theme_.headerHeight +
        theme_.rowHeight * static_cast<float>(visibleCount) +
        4.0f * theme_.dpiScale;
    const ImVec2 position(
        origin_.x + static_cast<float>(depth) *
            (theme_.panelWidth + theme_.panelGap) + offset,
        origin_.y + (1.0f - visibility) * 2.0f * theme_.dpiScale);
    const char* title = panelPath.empty()
        ? "NightSharp"
        : (parent ? parent->label.c_str() : "");

    renderAlpha_ = visibility;
    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    RenderPanel(drawList, position, height, title);
    if (interactive) {
        const ImVec4 bounds(
            position.x,
            position.y,
            position.x + theme_.panelWidth,
            position.y + height);
        previousPanels_.push_back({ bounds });
        interactiveBounds_.push_back({ bounds });
    }

    const ImVec4* childPanelRect =
        depth + 1 < hoverReferencePanels_.size()
            ? &hoverReferencePanels_[depth + 1].rect
            : nullptr;
    int visibleIndex = 0;
    for (int index = 0; index < static_cast<int>(nodes->size()); ++index) {
        const MenuNodeHandle& node = (*nodes)[index];
        if (!node || !node->visible) {
            continue;
        }
        Path rowPath = panelPath;
        rowPath.push_back(index);
        const bool active =
            depth < selectedPath_.size() &&
            selectedPath_[depth] == index &&
            HasPrefix(selectedPath_, panelPath);
        const ImVec2 rowPosition(
            position.x,
            position.y + theme_.headerHeight +
                2.0f * theme_.dpiScale +
                static_cast<float>(visibleIndex) * theme_.rowHeight);
        RenderNodeRow(
            drawList,
            rowPosition,
            *node,
            rowPath,
            active,
            interactive,
            childPanelRect);
        ++visibleIndex;
    }
    renderAlpha_ = 1.0f;
}

void Renderer::RenderColumns() {
    hoverReferencePanels_ = previousPanels_;
    previousPanels_.clear();
    const auto& slots = navigation_.Panels();
    const std::size_t targetCount = navigation_.TargetPanelCount();
    previousPanels_.reserve(targetCount);

    for (std::size_t depth = 0; depth < slots.size(); ++depth) {
        const PanelTransitionSlot& slot = slots[depth];
        const float eased = NavigationTransition::Ease(slot.progress);
        const float direction = slot.direction == NavigationDirection::Backward
            ? -1.0f
            : 1.0f;
        if (slot.hasOutgoing) {
            RenderPanelLayer(
                slot.outgoingPath,
                depth,
                1.0f - eased,
                -direction * theme_.panelTransitionDistance * eased,
                false);
        }
        if (slot.hasCurrent) {
            RenderPanelLayer(
                slot.currentPath,
                depth,
                eased,
                direction * theme_.panelTransitionDistance * (1.0f - eased),
                depth < targetCount && eased >= 0.55f);
        }
    }
    renderedColumnCount_ = static_cast<int>(targetCount);
    renderAlpha_ = 1.0f;
}

}
