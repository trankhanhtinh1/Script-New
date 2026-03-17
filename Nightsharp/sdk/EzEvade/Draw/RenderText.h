#pragma once
#include "RenderObject.h"
#include "../../UI/Drawing.h"
#include "../../Math/MathUtils.h"
#include <string>

// ============================================================================
// RenderText
//   C# original: ezEvade.Draw.RenderText (RenderText.cs, 57 lines)
//   Line-by-line port preserving original logic
// ============================================================================

namespace EzEvade {

    // ========================================================================
    //   C# lines 17-55
    //
    //   class RenderText : RenderObject
    //   {
    //       public Vector2 renderPosition = new Vector2(0, 0);
    //       public string text = "";
    //       public Color color = Color.White;
    //
    //       public RenderText(string text, Vector2 renderPosition, float renderTime)
    //       {
    //           this.startTime = EvadeUtils.TickCount;
    //           this.endTime = this.startTime + renderTime;
    //           this.renderPosition = renderPosition;
    //           this.text = text;
    //       }
    //
    //       public RenderText(string text, Vector2 renderPosition, float renderTime,
    //           Color color)
    //       {
    //           this.startTime = EvadeUtils.TickCount;
    //           this.endTime = this.startTime + renderTime;
    //           this.renderPosition = renderPosition;
    //           this.color = color;
    //           this.text = text;
    //       }
    //
    //       override public void Draw()
    //       {
    //           if (renderPosition.IsOnScreen())
    //           {
    //               var textDimension = Drawing.GetTextEntent((text), 15);
    //               var wardScreenPos = Drawing.WorldToScreen(renderPosition.To3D());
    //               Drawing.DrawText(
    //                   wardScreenPos.X - textDimension.Width / 2,
    //                   wardScreenPos.Y, color, text);
    //           }
    //       }
    //   }
    // ========================================================================
    class RenderText : public RenderObject {
    public:
        Vec2        renderPosition = { 0, 0 };                                  // C# line 19
        std::string text;                                                        // C# line 20
        ImU32       color = IM_COL32(255, 255, 255, 255);                       // C# line 22: Color.White

        // Constructor 1: no color
        // C# lines 24-31
        RenderText(const std::string& text_, Vec2 renderPosition_, float renderTime)
        {
            this->startTime       = EvadeUtils::TickCount();                    // C# line 26
            this->endTime         = this->startTime + renderTime;               // C# line 27
            this->renderPosition  = renderPosition_;                             // C# line 28
            this->text            = text_;                                       // C# line 30
        }

        // Constructor 2: with color
        // C# lines 33-43
        RenderText(const std::string& text_, Vec2 renderPosition_, float renderTime,
                   ImU32 color_)
        {
            this->startTime       = EvadeUtils::TickCount();                    // C# line 36
            this->endTime         = this->startTime + renderTime;               // C# line 37
            this->renderPosition  = renderPosition_;                             // C# line 38
            this->color           = color_;                                      // C# line 40
            this->text            = text_;                                       // C# line 42
        }

        // C# lines 45-54
        void Draw() override {
            // C# line 47: if (renderPosition.IsOnScreen())
            Vec3 worldPt(renderPosition.x, 0, renderPosition.y);                // To3D()
            Vec2 screenPt;

            if (SDK::Drawing::WorldToScreen(worldPt, screenPt))                  // IsOnScreen check
            {
                // C# line 49: var textDimension = Drawing.GetTextEntent((text), 15);
                ImVec2 textDimension = ImGui::CalcTextSize(text.c_str());

                // C# line 50: var wardScreenPos = Drawing.WorldToScreen(renderPosition.To3D());
                // (already computed as screenPt)

                // C# line 52: Drawing.DrawText(wardScreenPos.X - textDimension.Width / 2,
                //                              wardScreenPos.Y, color, text);
                Vec2 drawPos;
                drawPos.x = screenPt.x - textDimension.x / 2.0f;               // centered X
                drawPos.y = screenPt.y;

                SDK::Drawing::DrawScreenText(drawPos, text.c_str(), color);
            }
        }
    };

} // namespace EzEvade
