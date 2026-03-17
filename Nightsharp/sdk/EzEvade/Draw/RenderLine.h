#pragma once
#include "RenderObject.h"
#include "../../UI/Drawing.h"
#include "../../Math/MathUtils.h"

// ============================================================================
// RenderLine
//   C# original: ezEvade.Draw.RenderLine (RenderLine.cs, 61 lines)
//   Line-by-line port preserving original logic
// ============================================================================

namespace EzEvade {

    // ========================================================================
    //   C# lines 17-59
    //
    //   class RenderLine : RenderObject
    //   {
    //       public Vector2 start = new Vector2(0, 0);
    //       public Vector2 end = new Vector2(0, 0);
    //       public int width = 3;
    //       public Color color = Color.White;
    //
    //       public RenderLine(Vector2 start, Vector2 end, float renderTime,
    //           int radius = 65, int width = 3)
    //       {
    //           this.startTime = EvadeUtils.TickCount;
    //           this.endTime = this.startTime + renderTime;
    //           this.start = start;
    //           this.end = end;
    //           this.width = width;
    //       }
    //
    //       public RenderLine(Vector2 start, Vector2 end, float renderTime,
    //           Color color, int radius = 65, int width = 3)
    //       {
    //           this.startTime = EvadeUtils.TickCount;
    //           this.endTime = this.startTime + renderTime;
    //           this.start = start;
    //           this.end = end;
    //           this.color = color;
    //           this.width = width;
    //       }
    //
    //       override public void Draw()
    //       {
    //           if (start.IsOnScreen() || end.IsOnScreen())
    //           {
    //               var realStart = Drawing.WorldToScreen(start.To3D());
    //               var realEnd = Drawing.WorldToScreen(end.To3D());
    //               Drawing.DrawLine(realStart, realEnd, width, color);
    //           }
    //       }
    //   }
    // ========================================================================
    class RenderLine : public RenderObject {
    public:
        Vec2  start = { 0, 0 };                                                 // C# line 19
        Vec2  end   = { 0, 0 };                                                 // C# line 20
        int   width = 3;                                                         // C# line 22
        ImU32 color = IM_COL32(255, 255, 255, 255);                             // C# line 23: Color.White

        // Constructor 1: no color
        // C# lines 25-34
        RenderLine(Vec2 start_, Vec2 end_, float renderTime,
                   int radius_ = 65, int width_ = 3)
        {
            (void)radius_;                                                       // unused (kept for signature compat)
            this->startTime = EvadeUtils::TickCount();                           // C# line 28
            this->endTime   = this->startTime + renderTime;                     // C# line 29
            this->start     = start_;                                            // C# line 30
            this->end       = end_;                                              // C# line 31
            this->width     = width_;                                            // C# line 33
        }

        // Constructor 2: with color
        // C# lines 36-47
        RenderLine(Vec2 start_, Vec2 end_, float renderTime,
                   ImU32 color_, int radius_ = 65, int width_ = 3)
        {
            (void)radius_;
            this->startTime = EvadeUtils::TickCount();                           // C# line 39
            this->endTime   = this->startTime + renderTime;                     // C# line 40
            this->start     = start_;                                            // C# line 41
            this->end       = end_;                                              // C# line 42
            this->color     = color_;                                            // C# line 44
            this->width     = width_;                                            // C# line 46
        }

        // C# lines 49-58
        void Draw() override {
            // C# line 51: if (start.IsOnScreen() || end.IsOnScreen())
            Vec3 worldStart(start.x, 0, start.y);                               // To3D()
            Vec3 worldEnd(end.x, 0, end.y);                                     // To3D()

            Vec2 screenStart, screenEnd;
            bool startOnScreen = SDK::Drawing::WorldToScreen(worldStart, screenStart);   // C# line 53
            bool endOnScreen   = SDK::Drawing::WorldToScreen(worldEnd,   screenEnd);     // C# line 54

            if (startOnScreen || endOnScreen)                                    // C# line 51
            {
                // C# line 56: Drawing.DrawLine(realStart, realEnd, width, color)
                SDK::Drawing::DrawScreenLine(screenStart, screenEnd, color, (float)width);
            }
        }
    };

} // namespace EzEvade
