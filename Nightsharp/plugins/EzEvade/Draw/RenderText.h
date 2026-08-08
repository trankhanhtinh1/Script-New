#pragma once

// ============================================================================
// RenderText.h — 1:1 port of RenderText.cs
//
//   class RenderText : RenderObject {
//       public Vector2 renderPosition = new Vector2(0, 0);
//       public string text = "";
//       public Color color = Color.White;
//
//       public RenderText(string text, Vector2 renderPosition, float renderTime)
//       public RenderText(string text, Vector2 renderPosition, float renderTime,
//                         Color color)
//
//       override public void Draw() {
//           if (renderPosition.IsOnScreen()) {
//               var textDimension = Drawing.GetTextEntent((text), 15);
//               var wardScreenPos = Drawing.WorldToScreen(renderPosition.To3D());
//               Drawing.DrawText(wardScreenPos.X - textDimension.Width / 2,
//                                wardScreenPos.Y, color, text);
//           }
//       }
//   }
// ============================================================================

#include "RenderObject.h"

#include <string>

namespace Plugins::EzEvade::Draw {

struct RenderText : RenderObject {
    Vec2 renderPosition{ 0.0f, 0.0f };
    std::string text;
    std::uint32_t color = 0xFFFFFFFFu; // Color.White

    // C# ctor 1: (string text, Vector2 renderPosition, float renderTime)
    RenderText(std::string text, Vec2 renderPosition, float renderTime)
        : renderPosition(renderPosition), text(std::move(text)) {
        this->startTime = TickCount();
        this->endTime = this->startTime + renderTime;
    }

    // C# ctor 2: (string text, Vector2 renderPosition, float renderTime, Color color)
    RenderText(std::string text, Vec2 renderPosition, float renderTime, std::uint32_t color)
        : renderPosition(renderPosition), text(std::move(text)), color(color) {
        this->startTime = TickCount();
        this->endTime = this->startTime + renderTime;
    }

    void Draw() override {
        const Vec3 world = Vec3::From2D(renderPosition);
        if (SDK::Drawing::OnScreen(world)) {
            const Vec2 wardScreenPos = SDK::Drawing::WorldToScreen(world);
            ImVec2 textSize{ 0.0f, 0.0f };
            if (ImGui::GetCurrentContext()) {
                textSize = ImGui::CalcTextSize(text.c_str());
            }
            SDK::Drawing::DrawText(
                wardScreenPos.x - textSize.x * 0.5f,
                wardScreenPos.y,
                color,
                text.c_str());
        }
    }
};

} // namespace Plugins::EzEvade::Draw
