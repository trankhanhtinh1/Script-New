#pragma once

#include "../imgui/imgui.h"

namespace OverlayStatus {

enum class Mode {
    Internal,
    External
};

inline const char* ToText(Mode mode) {
    return mode == Mode::External ? "External" : "Internal";
}

inline void DrawTextWithShadow(ImDrawList* draw,
                               const ImVec2& pos,
                               ImU32 color,
                               const char* text) {
    if (!draw || !text || !*text) {
        return;
    }

    const ImU32 shadow = IM_COL32(0, 0, 0, 210);
    draw->AddText(ImVec2(pos.x + 1.0f, pos.y + 1.0f), shadow, text);
    draw->AddText(ImVec2(pos.x - 1.0f, pos.y + 1.0f), shadow, text);
    draw->AddText(pos, color, text);
}

inline void Render(Mode mode) {
    if (!ImGui::GetCurrentContext()) {
        return;
    }

    ImGuiIO& io = ImGui::GetIO();
    ImVec2 display = io.DisplaySize;
    if (ImGuiViewport* viewport = ImGui::GetMainViewport()) {
        if (viewport->Size.x > 0.0f && viewport->Size.y > 0.0f) {
            display = viewport->Size;
        }
    }

    if (display.x <= 0.0f || display.y <= 0.0f) {
        return;
    }

    ImDrawList* draw = ImGui::GetForegroundDrawList();
    const ImU32 green = IM_COL32(0, 255, 35, 255);
    const float y = 4.0f;
    const float lineHeight = ImGui::GetFontSize() + 1.0f;
    const char* title = "NightSharp";
    const char* status = ToText(mode);

    const ImVec2 titleSize = ImGui::CalcTextSize(title);
    const ImVec2 statusSize = ImGui::CalcTextSize(status);

    DrawTextWithShadow(
        draw,
        ImVec2((display.x - titleSize.x) * 0.5f, y),
        green,
        title);
    DrawTextWithShadow(
        draw,
        ImVec2((display.x - statusSize.x) * 0.5f, y + lineHeight),
        green,
        status);
}

} // namespace OverlayStatus
