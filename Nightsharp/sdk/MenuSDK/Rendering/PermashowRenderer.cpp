#include "PermashowRenderer.h"

#include <algorithm>
#include <string>
#include <utility>

namespace NightSharp::Menu {

PermashowRenderer::PermashowRenderer(PermashowRegistry& registry)
    : registry_(registry) {
    animator_.Reserve(128);
}

ImU32 PermashowRenderer::Color(
    const ImVec4& value,
    float alpha) const {
    ImVec4 adjusted = value;
    adjusted.w *= std::clamp(alpha, 0.0f, 1.0f);
    return ImGui::ColorConvertFloat4ToU32(adjusted);
}

ImVec2 PermashowRenderer::TargetPosition(
    const PermashowLayout& layout,
    const ImVec2& displaySize,
    const ImVec2& panelSize) const {
    const float left = layout.offset.x;
    const float top = layout.offset.y;
    const float right = displaySize.x - panelSize.x - layout.offset.x;
    const float bottom = displaySize.y - panelSize.y - layout.offset.y;
    switch (layout.corner) {
    case PermashowCorner::TopLeft:
        return ImVec2(left, top);
    case PermashowCorner::BottomLeft:
        return ImVec2(left, std::max(top, bottom));
    case PermashowCorner::BottomRight:
        return ImVec2(std::max(left, right), std::max(top, bottom));
    case PermashowCorner::TopRight:
    default:
        return ImVec2(std::max(left, right), top);
    }
}

void PermashowRenderer::Render(
    const Theme& theme,
    ImFont* font,
    const ImVec2& displaySize,
    float deltaTime) {
    if (registry_.Entries().empty()) {
        positionInitialized_ = false;
        animator_.Update("visible", 0.0f, 18.0f, deltaTime);
        return;
    }

    std::vector<PermashowEntryHandle> entries;
    entries.reserve(registry_.Entries().size());
    for (const PermashowEntryHandle& entry : registry_.Entries()) {
        if (entry && entry->visible) {
            entries.push_back(entry);
        }
    }
    if (entries.empty()) {
        return;
    }
    std::stable_sort(
        entries.begin(),
        entries.end(),
        [](const PermashowEntryHandle& left, const PermashowEntryHandle& right) {
            return left->order < right->order;
        });

    const PermashowLayout& layout = registry_.Layout();
    const float scale = theme.dpiScale;
    const float padding = layout.padding * scale;
    const float rowHeight = layout.rowHeight * scale;
    const float groupHeight = layout.groupHeight * scale;
    const float gap = layout.gap * scale;
    float width = layout.width * scale;
    if (width <= 0.0f) {
        width = 220.0f * scale;
    }
    for (const PermashowEntryHandle& entry : entries) {
        const ImVec2 labelSize = ImGui::CalcTextSize(entry->label.c_str());
        const std::string value = entry->ValueText();
        const ImVec2 valueSize = ImGui::CalcTextSize(value.c_str());
        width = std::max(
            width,
            padding * 2.0f + labelSize.x + valueSize.x + 38.0f * scale);
    }

    float height = padding * 2.0f;
    std::string previousGroup;
    if (layout.showHeader) {
        height += groupHeight;
    }
    for (const PermashowEntryHandle& entry : entries) {
        if (!entry->group.empty() && entry->group != previousGroup) {
            height += groupHeight;
            previousGroup = entry->group;
        }
        height += rowHeight;
    }

    const ImVec2 panelSize(width, height);
    const ImVec2 target = TargetPosition(layout, displaySize, panelSize);
    if (!positionInitialized_) {
        position_ = target;
        positionInitialized_ = true;
    }
    position_.x = Animator::Smooth(
        position_.x,
        target.x,
        22.0f,
        deltaTime);
    position_.y = Animator::Smooth(
        position_.y,
        target.y,
        22.0f,
        deltaTime);
    const float alpha = animator_.Update(
        "visible",
        registry_.visible ? 1.0f : 0.0f,
        18.0f,
        deltaTime);
    if (alpha <= 0.001f) {
        return;
    }

    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    drawList->AddRectFilled(
        position_,
        ImVec2(position_.x + panelSize.x, position_.y + panelSize.y),
        Color(theme.panelBackground, alpha * 0.98f),
        0.0f);
    drawList->AddRect(
        position_,
        ImVec2(position_.x + panelSize.x, position_.y + panelSize.y),
        Color(theme.border, alpha),
        0.0f,
        0,
        1.0f * scale);
    drawList->AddRectFilled(
        position_,
        ImVec2(position_.x + panelSize.x, position_.y + 2.0f * scale),
        Color(theme.accent, alpha * 0.72f),
        0.0f);

    float cursorY = position_.y + padding;
    if (layout.showHeader) {
        drawList->AddText(
            font,
            ImGui::GetFontSize() * 0.90f,
            ImVec2(position_.x + padding, cursorY),
            Color(theme.textDim, alpha),
            layout.header.c_str());
        cursorY += groupHeight;
    }

    previousGroup.clear();
    for (const PermashowEntryHandle& entry : entries) {
        if (!entry->group.empty() && entry->group != previousGroup) {
            previousGroup = entry->group;
            drawList->AddText(
                font,
                ImGui::GetFontSize() * 0.82f,
                ImVec2(position_.x + padding, cursorY),
                Color(theme.textDim, alpha * 0.82f),
                entry->group.c_str());
            cursorY += groupHeight;
        }

        const ImVec2 rowPosition(
            position_.x + padding,
            cursorY);
        const ImVec2 rowSize(
            panelSize.x - padding * 2.0f,
            rowHeight);
        const bool hovered = registry_.visible && entry->interactive &&
            ImGui::IsMouseHoveringRect(
                rowPosition,
                ImVec2(rowPosition.x + rowSize.x, rowPosition.y + rowSize.y),
                false);
        const float hover = animator_.Update(
            "hover/" + entry->id,
            hovered ? 1.0f : 0.0f,
            theme.animationSpeed,
            deltaTime);
        if (hover > 0.001f) {
            drawList->AddRectFilled(
                rowPosition,
                ImVec2(rowPosition.x + rowSize.x, rowPosition.y + rowSize.y),
                Color(theme.itemHover, alpha * hover * 0.55f),
                0.0f);
        }
        if (registry_.visible && hovered &&
            ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
            entry->onClick) {
            entry->onClick(*entry);
        }

        drawList->AddText(
            font,
            ImGui::GetFontSize() * 1.0f,
            ImVec2(
                rowPosition.x + 2.0f * scale,
                rowPosition.y + (rowHeight - ImGui::GetFontSize()) * 0.5f),
            Color(entry->enabled ? theme.text : theme.disabled, alpha),
            entry->label.c_str());

        const std::string value = entry->ValueText();
        if (entry->kind == PermashowValueKind::Custom && entry->customRenderer) {
            entry->customRenderer({
                drawList,
                ImVec2(
                    rowPosition.x + rowSize.x * 0.52f,
                    rowPosition.y),
                ImVec2(rowSize.x * 0.46f, rowSize.y),
                alpha,
                scale });
        } else {
            const ImVec2 valueSize = ImGui::CalcTextSize(value.c_str());
            drawList->AddText(
                font,
                ImGui::GetFontSize() * 0.96f,
                ImVec2(
                    rowPosition.x + rowSize.x - valueSize.x - 2.0f * scale,
                    rowPosition.y +
                        (rowHeight - ImGui::GetFontSize() * 0.96f) * 0.5f),
                Color(entry->valueColor, alpha),
                value.c_str());
        }

        if (entry->kind == PermashowValueKind::Progress) {
            const float progress = std::clamp(entry->progress, 0.0f, 1.0f);
            const float barWidth = rowSize.x * 0.36f;
            const ImVec2 barPosition(
                rowPosition.x + rowSize.x - barWidth,
                rowPosition.y + rowHeight - 3.0f * scale);
            drawList->AddRectFilled(
                barPosition,
                ImVec2(barPosition.x + barWidth, barPosition.y + scale),
                Color(theme.itemBackground, alpha),
                0.0f);
            drawList->AddRectFilled(
                barPosition,
                ImVec2(barPosition.x + barWidth * progress, barPosition.y + scale),
                Color(entry->valueColor, alpha),
                0.0f);
        }
        cursorY += rowHeight;
    }
}

void PermashowRenderer::Reset() {
    animator_.Clear();
    position_ = {};
    positionInitialized_ = false;
}

}
