#pragma once
#include <vector>
#include <algorithm>
#include <functional>
#include "../Utils/EvadeUtils.h"
#include "../Utils/DelayAction.h"
#include "../../UI/Drawing.h"

// ============================================================================
// RenderObject / RenderObjects
//   C# original: ezEvade.Draw.RenderObject (RenderObject.cs)
//   Line-by-line port preserving original logic
//
//   Base class for timed render objects. All objects have a start/end time;
//   once expired they are removed from the list on the next render tick.
// ============================================================================

namespace EzEvade {

    // DelayAction is available via DelayAction.h include

    // ========================================================================
    // RenderObject (abstract base)
    //   C# original lines 15-21
    //
    //   abstract class RenderObject
    //   {
    //       public float endTime = 0;        // line 17
    //       public float startTime = 0;      // line 18
    //       abstract public void Draw();     // line 20
    //   }
    // ========================================================================
    class RenderObject {
    public:
        float endTime   = 0;                                                    // C# line 17
        float startTime = 0;                                                    // C# line 18

        virtual ~RenderObject() = default;
        virtual void Draw() = 0;                                                // C# line 20 (abstract)
    };

    // ========================================================================
    // RenderObjects (static manager)
    //   C# original lines 23-56
    //
    //   class RenderObjects
    //   {
    //       private static List<RenderObject> objects = new List<RenderObject>();
    //
    //       static RenderObjects()
    //       {
    //           Drawing.OnDraw += Drawing_OnDraw;
    //       }
    //
    //       private static void Drawing_OnDraw(EventArgs args) { Render(); }
    //
    //       private static void Render()
    //       {
    //           foreach (RenderObject obj in objects)
    //           {
    //               if (obj.endTime - EvadeUtils.TickCount > 0)
    //                   obj.Draw();
    //               else
    //                   DelayAction.Add(1, () => objects.Remove(obj));
    //           }
    //       }
    //
    //       public static void Add(RenderObject obj) { objects.Add(obj); }
    //   }
    // ========================================================================
    class RenderObjects {
    public:
        // C# line 52-55: public static void Add(RenderObject obj)
        static void Add(RenderObject* obj) {
            Objects().push_back(obj);                                            // C# line 54
        }

        // Called each frame from the Drawing hook (replaces Drawing.OnDraw event)
        // C# line 32-34: Drawing_OnDraw → Render()
        static void OnDraw() {
            Render();                                                            // C# line 34
        }

    private:
        // C# line 25: private static List<RenderObject> objects
        static std::vector<RenderObject*>& Objects() {
            static std::vector<RenderObject*> objects;
            return objects;
        }

        // C# line 37-49: private static void Render()
        static void Render() {
            auto& objects = Objects();
            for (int i = (int)objects.size() - 1; i >= 0; --i)                  // iterate backwards for safe removal
            {
                RenderObject* obj = objects[i];
                if (obj->endTime - EvadeUtils::TickCount() > 0)                 // C# line 41
                {
                    obj->Draw();                                                 // C# line 43
                }
                else                                                             // C# line 45
                {
                    // C# line 47: DelayAction.Add(1, () => objects.Remove(obj));
                    // In C++ we delete + erase immediately (no GC)
                    delete obj;
                    objects.erase(objects.begin() + i);
                }
            }
        }
    };

} // namespace EzEvade
