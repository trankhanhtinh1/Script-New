#pragma once

// ============================================================================
// RenderLine.h — 1:1 port of RenderLine.cs
//
//   class RenderLine : RenderObject {
//       public Vector2 start = new Vector2(0, 0);
//       public Vector2 end = new Vector2(0, 0);
//       public int width = 3;
//       public Color color = Color.White;
//
//       public RenderLine(Vector2 start, Vector2 end, float renderTime,
//                         int radius = 65, int width = 3)
//       public RenderLine(Vector2 start, Vector2 end, float renderTime,
//                         Color color, int radius = 65, int width = 3)
//
//       override public void Draw() {
//           if (start.IsOnScreen() || end.IsOnScreen()) {
//               var realStart = Drawing.WorldToScreen(start.To3D());
//               var realEnd = Drawing.WorldToScreen(end.To3D());
//               Drawing.DrawLine(realStart, realEnd, width, color);
//           }
//       }
//   }
// ============================================================================

#include "RenderObject.h"

namespace Plugins::EzEvade::Draw {

struct RenderLine : RenderObject {
    Vec2 start{ 0.0f, 0.0f };
    Vec2 end{ 0.0f, 0.0f };
    int width = 3;
    std::uint32_t color = 0xFFFFFFFFu; // Color.White

    // C# ctor 1: (Vector2 start, Vector2 end, float renderTime, int radius = 65, int width = 3)
    RenderLine(Vec2 start, Vec2 end, float renderTime, int radius = 65, int width = 3)
        : start(start), end(end), width(width) {
        this->startTime = TickCount();
        this->endTime = this->startTime + renderTime;
    }

    // C# ctor 2: (Vector2 start, Vector2 end, float renderTime, Color color, int radius = 65, int width = 3)
    RenderLine(Vec2 start, Vec2 end, float renderTime, std::uint32_t color, int radius = 65, int width = 3)
        : start(start), end(end), width(width), color(color) {
        this->startTime = TickCount();
        this->endTime = this->startTime + renderTime;
    }

    void Draw() override {
        const Vec3 worldStart = Vec3::From2D(start);
        const Vec3 worldEnd = Vec3::From2D(end);
        if (SDK::Drawing::OnScreen(worldStart) || SDK::Drawing::OnScreen(worldEnd)) {
            const Vec2 realStart = SDK::Drawing::WorldToScreen(worldStart);
            const Vec2 realEnd = SDK::Drawing::WorldToScreen(worldEnd);
            SDK::Drawing::DrawLine(
                realStart,
                realEnd,
                static_cast<float>(width),
                color);
        }
    }
};

} // namespace Plugins::EzEvade::Draw
