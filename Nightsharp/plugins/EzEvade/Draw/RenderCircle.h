#pragma once

// ============================================================================
// RenderCircle.h — 1:1 port of RenderCircle.cs
//
//   class RenderCircle : RenderObject {
//       public Vector2 renderPosition = new Vector2(0, 0);
//       public int radius = 65;
//       public int width = 5;
//       public Color color = Color.White;
//
//       public RenderCircle(Vector2, float renderTime, int radius = 65, int width = 5)
//       public RenderCircle(Vector2, float renderTime, Color color, int radius = 65, int width = 5)
//
//       override public void Draw() {
//           if (renderPosition.IsOnScreen()) {
//               Render.Circle.DrawCircle(renderPosition.To3D(), radius, color, width);
//           }
//       }
//   }
// ============================================================================

#include "RenderObject.h"

namespace Plugins::EzEvade::Draw {

struct RenderCircle : RenderObject {
    Vec2 renderPosition{ 0.0f, 0.0f };
    int radius = 65;
    int width = 5;
    std::uint32_t color = 0xFFFFFFFFu; // Color.White

    // C# ctor 1: (Vector2, float renderTime, int radius = 65, int width = 5)
    RenderCircle(Vec2 renderPosition, float renderTime, int radius = 65, int width = 5)
        : renderPosition(renderPosition), radius(radius), width(width) {
        this->startTime = TickCount();
        this->endTime = this->startTime + renderTime;
    }

    // C# ctor 2: (Vector2, float renderTime, Color color, int radius = 65, int width = 5)
    RenderCircle(Vec2 renderPosition, float renderTime, std::uint32_t color, int radius = 65, int width = 5)
        : renderPosition(renderPosition), radius(radius), width(width), color(color) {
        this->startTime = TickCount();
        this->endTime = this->startTime + renderTime;
    }

    void Draw() override {
        const Vec3 world = Vec3::From2D(renderPosition);
        if (SDK::Drawing::OnScreen(world)) {
            SDK::Drawing::DrawCircle(
                world,
                static_cast<float>(radius),
                color,
                static_cast<float>(width));
        }
    }
};

} // namespace Plugins::EzEvade::Draw
