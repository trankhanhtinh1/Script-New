#pragma once
#include "sdk/EzEvade/Draw/RenderObject.h"

namespace EzEvade {
namespace Draw {

class RenderLine : public RenderObject {
public:
    Vec2 Start = Vec2();
    Vec2 End = Vec2();
    int Width = 3;
    ImU32 Color = IM_COL32(255, 255, 255, 255);

    RenderLine(const Vec2& start, const Vec2& end, float renderTime, int width = 3) {
        StartTime = EvadeUtils::TickCount();
        EndTime = StartTime + renderTime;
        Start = start;
        End = end;
        Width = width;
    }

    RenderLine(const Vec2& start, const Vec2& end, float renderTime, ImU32 color, int width = 3) {
        StartTime = EvadeUtils::TickCount();
        EndTime = StartTime + renderTime;
        Start = start;
        End = end;
        Color = color;
        Width = width;
    }

    void Draw() override {
        Vec2 s, e;
        const float y = SDK::GameObjects::Player.GetPosition().y;
        const bool sOk = SDK::Drawing::WorldToScreen(Vec3::From2D(Start, y), s);
        const bool eOk = SDK::Drawing::WorldToScreen(Vec3::From2D(End, y), e);
        if (sOk || eOk) {
            SDK::Drawing::DrawLine(Vec3::From2D(Start, y), Vec3::From2D(End, y), Color, (float)Width);
        }
    }
};

} // namespace Draw
} // namespace EzEvade

