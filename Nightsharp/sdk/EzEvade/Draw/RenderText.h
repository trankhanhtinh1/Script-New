#pragma once
#include "sdk/EzEvade/Draw/RenderObject.h"

namespace EzEvade {
namespace Draw {

class RenderText : public RenderObject {
public:
    Vec2 RenderPosition = Vec2();
    std::string Text = "";
    ImU32 Color = IM_COL32(255, 255, 255, 255);

    RenderText(const std::string& text, const Vec2& renderPosition, float renderTime) {
        StartTime = EvadeUtils::TickCount();
        EndTime = StartTime + renderTime;
        RenderPosition = renderPosition;
        Text = text;
    }

    RenderText(const std::string& text, const Vec2& renderPosition, float renderTime, ImU32 color) {
        StartTime = EvadeUtils::TickCount();
        EndTime = StartTime + renderTime;
        RenderPosition = renderPosition;
        Text = text;
        Color = color;
    }

    void Draw() override {
        Vec2 screen;
        const float y = SDK::GameObjects::Player.GetPosition().y;
        if (SDK::Drawing::WorldToScreen(Vec3::From2D(RenderPosition, y), screen)) {
            ImVec2 textSize = ImGui::CalcTextSize(Text.c_str());
            ImDrawList* draw = ImGui::GetBackgroundDrawList();
            if (draw) {
                draw->AddText(ImVec2(screen.x - textSize.x * 0.5f, screen.y), Color, Text.c_str());
            }
        }
    }
};

} // namespace Draw
} // namespace EzEvade

