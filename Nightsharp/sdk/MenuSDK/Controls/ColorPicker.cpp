#include "../Rendering/Renderer.h"

namespace NightSharp::Menu {

bool Renderer::RenderColor(MenuItem& item) {
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(item.label.c_str());
    ImGui::SameLine();

    const float width = 52.0f * theme_.dpiScale;
    const float height = 22.0f * theme_.dpiScale;
    const float targetX = ImGui::GetCursorPosX() +
        ImGui::GetContentRegionAvail().x - width;
    if (targetX > ImGui::GetCursorPosX()) {
        ImGui::SetCursorPosX(targetX);
    }

    const ImVec2 position = ImGui::GetCursorScreenPos();
    if (interactiveLayer_ && ImGui::InvisibleButton("##color_button", ImVec2(width, height))) {
        ImGui::OpenPopup("##color_popup");
    }

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(
        position,
        ImVec2(position.x + width, position.y + height),
        RenderColor(ImVec4(
            item.color[0],
            item.color[1],
            item.color[2],
            item.color[3])),
        0.0f);
    drawList->AddRect(
        position,
        ImVec2(position.x + width, position.y + height),
        RenderColor(interactiveLayer_ && ImGui::IsItemHovered()
            ? theme_.accent
            : theme_.border),
        0.0f,
        0,
        1.0f * theme_.dpiScale);

    bool changed = false;
    if (interactiveLayer_ && ImGui::BeginPopup("##color_popup")) {
        std::array<float, 4> next = item.color;
        if (ImGui::ColorPicker4(
                "##picker",
                next.data(),
                ImGuiColorEditFlags_AlphaBar |
                    ImGuiColorEditFlags_AlphaPreviewHalf)) {
            changed = item.SetColor(next);
        }
        ImGui::EndPopup();
    }
    return changed;
}

}
