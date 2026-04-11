#pragma once

#include "../imgui/imgui.h"
#include "Translations.h"

namespace MenuTheme {

    inline bool g_inputEnabled = true;

    constexpr float ITEM_H        = 30.0f;
    constexpr float HEADER_H      = 32.0f;
    constexpr float PANEL_GAP     = 3.0f;
    constexpr float MAX_CONTENT_H = 620.0f;

    inline ImU32 COL_BG           = IM_COL32(8, 10, 18, 214);
    inline ImU32 COL_CONTENT_BG   = IM_COL32(8, 10, 18, 128);
    inline ImU32 COL_HEADER       = IM_COL32(16, 18, 28, 236);
    inline ImU32 COL_ITEM         = IM_COL32(18, 20, 30, 118);
    inline ImU32 COL_ITEM_HOVER   = IM_COL32(52, 48, 82, 215);
    inline ImU32 COL_ITEM_ACTIVE  = IM_COL32(82, 66, 132, 232);
    inline ImU32 COL_ACCENT       = IM_COL32(120, 235, 120, 255);
    inline ImU32 COL_TEXT         = IM_COL32(255, 255, 255, 255);
    inline ImU32 COL_TEXT_DIM     = IM_COL32(185, 185, 205, 255);
    inline ImU32 COL_BORDER       = IM_COL32(88, 100, 148, 180);

    inline bool DrawStateButton(const char* id, const char* label, bool active, bool positive, float width = 44.0f) {
        ImVec4 activeBase   = positive ? ImVec4(0.18f,0.55f,0.28f,0.98f) : ImVec4(0.65f,0.22f,0.24f,0.98f);
        ImVec4 activeHover  = positive ? ImVec4(0.22f,0.64f,0.32f,1.0f)  : ImVec4(0.75f,0.27f,0.29f,1.0f);
        ImVec4 activePress  = positive ? ImVec4(0.15f,0.48f,0.24f,1.0f)  : ImVec4(0.58f,0.18f,0.20f,1.0f);
        ImVec4 inactiveBase = ImVec4(0.14f,0.16f,0.24f,0.98f);
        ImVec4 inactiveHov  = ImVec4(0.20f,0.24f,0.34f,1.0f);
        ImVec4 inactivePr   = ImVec4(0.24f,0.28f,0.40f,1.0f);
        ImVec4 text = active ? ImVec4(0.96f,0.97f,1.0f,1.0f) : ImVec4(0.78f,0.81f,0.90f,1.0f);

        ImGui::PushID(id);
        ImGui::PushStyleColor(ImGuiCol_Button,        active ? activeBase  : inactiveBase);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,  active ? activeHover : inactiveHov);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,   active ? activePress : inactivePr);
        ImGui::PushStyleColor(ImGuiCol_Text, text);
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0,0,0,0));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 7.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
        bool clicked = ImGui::Button(Translations::T(label), ImVec2(width, 0));
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(5);
        ImGui::PopID();
        return clicked;
    }

} // namespace MenuTheme
