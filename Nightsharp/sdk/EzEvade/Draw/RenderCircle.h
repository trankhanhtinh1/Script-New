#pragma once
#include "sdk/EzEvade/Draw/RenderObject.h"

namespace EzEvade {
namespace Draw {

class RenderCircle : public RenderObject {
public:
    Vec2 RenderPosition = Vec2();
    int Radius = 65;
    int Width = 5;
    ImU32 Color = IM_COL32(255, 255, 255, 255);

    RenderCircle(const Vec2& renderPosition, float renderTime, int radius = 65, int width = 5) {
        StartTime = EvadeUtils::TickCount();
        EndTime = StartTime + renderTime;
        RenderPosition = renderPosition;
        Radius = radius;
        Width = width;
    }

    RenderCircle(const Vec2& renderPosition, float renderTime, ImU32 color, int radius = 65, int width = 5) {
        StartTime = EvadeUtils::TickCount();
        EndTime = StartTime + renderTime;
        RenderPosition = renderPosition;
        Color = color;
        Radius = radius;
        Width = width;
    }

    void Draw() override {
        Vec2 screen;
        if (SDK::Drawing::WorldToScreen(Vec3::From2D(RenderPosition, SDK::GameObjects::Player.GetPosition().y), screen)) {
            SDK::Drawing::DrawCircle(Vec3::From2D(RenderPosition, SDK::GameObjects::Player.GetPosition().y), (float)Radius, Color, (float)Width);
        }
    }
};

} // namespace Draw
} // namespace EzEvade

