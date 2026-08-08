#pragma once

// ============================================================================
// RenderObject.h — 1:1 port of RenderObject.cs
//
//   abstract class RenderObject {
//       public float endTime = 0;
//       public float startTime = 0;
//       abstract public void Draw();
//   }
//
//   class RenderObjects {
//       private static List<RenderObject> objects = new List<RenderObject>();
//       static RenderObjects() { Drawing.OnDraw += Drawing_OnDraw; }
//       private static void Drawing_OnDraw(EventArgs args) { Render(); }
//       private static void Render() { ... }
//       public static void Add(RenderObject obj) { objects.Add(obj); }
//   }
// ============================================================================

#include "../../SDK/SDK.h"

#include <Windows.h>
#include <memory>
#include <vector>

namespace Plugins::EzEvade::Draw {

// ----------------------------------------------------------------------------
// EvadeUtils.TickCount equivalent (float milliseconds).
// C#: (float)DateTime.Now.Subtract(assemblyLoadTime).TotalMilliseconds
// NightSharp: GetTickCount() returns DWORD ms since boot; cast to float.
// ----------------------------------------------------------------------------
inline float TickCount() {
    return static_cast<float>(GetTickCount());
}

// ----------------------------------------------------------------------------
// RenderObject — abstract base
// ----------------------------------------------------------------------------
struct RenderObject {
    float endTime = 0.0f;
    float startTime = 0.0f;

    virtual ~RenderObject() = default;
    virtual void Draw() = 0;
};

// ----------------------------------------------------------------------------
// RenderObjects — static manager
// ----------------------------------------------------------------------------
class RenderObjects {
public:
    using ObjectList = std::vector<std::shared_ptr<RenderObject>>;

    static void Add(std::shared_ptr<RenderObject> obj) {
        EnsureHooked();
        Objects().push_back(obj);
    }

    static void Drawing_OnDraw() {
        Render();
    }

private:
    static ObjectList& Objects() {
        static ObjectList objects;
        return objects;
    }

    static bool& Hooked() {
        static bool hooked = false;
        return hooked;
    }

    static void EnsureHooked() {
        if (Hooked()) {
            return;
        }
        Hooked() = SDK::Drawing::AddOnDraw(&RenderObjects::Drawing_OnDraw);
    }

    // C#:
    //   foreach (RenderObject obj in objects) {
    //       if (obj.endTime - EvadeUtils.TickCount > 0) {
    //           obj.Draw();
    //       } else {
    //           DelayAction.Add(1, () => objects.Remove(obj));
    //       }
    //   }
    // NightSharp: no DelayAction; remove expired entries immediately.
    static void Render() {
        auto& objects = Objects();
        for (auto it = objects.begin(); it != objects.end();) {
            const auto& obj = *it;
            if (obj->endTime - TickCount() > 0.0f) {
                obj->Draw();
                ++it;
            } else {
                it = objects.erase(it);
            }
        }
    }
};

} // namespace Plugins::EzEvade::Draw
