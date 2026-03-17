#pragma once
#include "RenderObject.h"
#include "../../UI/Drawing.h"
#include "../../Math/MathUtils.h"

// ============================================================================
// RenderCircle
//   C# original: ezEvade.Draw.RenderCircle (RenderCircle.cs, 58 lines)
//   Line-by-line port preserving original logic
// ============================================================================

namespace EzEvade {

    // ========================================================================
    //   C# lines 17-56
    //
    //   class RenderCircle : RenderObject
    //   {
    //       public Vector2 renderPosition = new Vector2(0, 0);
    //       public int radius = 65;
    //       public int width = 5;
    //       public Color color = Color.White;
    //
    //       public RenderCircle(Vector2 renderPosition, float renderTime,
    //           int radius = 65, int width = 5)
    //       {
    //           this.startTime = EvadeUtils.TickCount;
    //           this.endTime = this.startTime + renderTime;
    //           this.renderPosition = renderPosition;
    //           this.radius = radius;
    //           this.width = width;
    //       }
    //
    //       public RenderCircle(Vector2 renderPosition, float renderTime,
    //           Color color, int radius = 65, int width = 5)
    //       {
    //           this.startTime = EvadeUtils.TickCount;
    //           this.endTime = this.startTime + renderTime;
    //           this.renderPosition = renderPosition;
    //           this.color = color;
    //           this.radius = radius;
    //           this.width = width;
    //       }
    //
    //       override public void Draw()
    //       {
    //           if (renderPosition.IsOnScreen())
    //               Render.Circle.DrawCircle(renderPosition.To3D(), radius, color, width);
    //       }
    //   }
    // ========================================================================
    class RenderCircle : public RenderObject {
    public:
        Vec2  renderPosition = { 0, 0 };                                        // C# line 19
        int   radius = 65;                                                       // C# line 21
        int   width  = 5;                                                        // C# line 22
        ImU32 color  = IM_COL32(255, 255, 255, 255);                            // C# line 23: Color.White

        // Constructor 1: no color
        // C# lines 25-34
        RenderCircle(Vec2 renderPosition_, float renderTime,
                     int radius_ = 65, int width_ = 5)
        {
            this->startTime       = EvadeUtils::TickCount();                    // C# line 28
            this->endTime         = this->startTime + renderTime;               // C# line 29
            this->renderPosition  = renderPosition_;                             // C# line 30
            this->radius          = radius_;                                     // C# line 32
            this->width           = width_;                                      // C# line 33
        }

        // Constructor 2: with color
        // C# lines 36-47
        RenderCircle(Vec2 renderPosition_, float renderTime,
                     ImU32 color_, int radius_ = 65, int width_ = 5)
        {
            this->startTime       = EvadeUtils::TickCount();                    // C# line 39
            this->endTime         = this->startTime + renderTime;               // C# line 40
            this->renderPosition  = renderPosition_;                             // C# line 41
            this->color           = color_;                                      // C# line 43
            this->radius          = radius_;                                     // C# line 45
            this->width           = width_;                                      // C# line 46
        }

        // C# lines 49-55
        void Draw() override {
            // C# line 51: if (renderPosition.IsOnScreen())
            Vec2 screenPt;
            Vec3 worldPt(renderPosition.x, 0, renderPosition.y);                // To3D()
            if (SDK::Drawing::WorldToScreen(worldPt, screenPt))                  // IsOnScreen check
            {
                // C# line 53: Render.Circle.DrawCircle(renderPosition.To3D(), radius, color, width)
                SDK::Drawing::DrawCircle(worldPt, (float)radius, color, (float)width);
            }
        }
    };

} // namespace EzEvade
