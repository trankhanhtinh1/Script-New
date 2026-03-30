#pragma once

#include "../UI/Drawing.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <mutex>
#include <new>
#include <vector>

namespace SDK::Utils::Render {

class RenderObject {
public:
    virtual ~RenderObject() = default;

    float Layer = 0.0f;
    bool Visible = true;

    virtual void Dispose() {}
    virtual void OnPreReset() {}
    virtual void OnPostReset() {}
    virtual void OnDraw() {}
    virtual void OnEndScene() {}

    bool HasValidLayer() const {
        return std::isfinite(Layer);
    }
};

inline void Reset();

namespace detail {
    inline std::mutex*& ObjectsMutex() {
        static auto* mutex = new(std::nothrow) std::mutex();
        return mutex;
    }

    inline std::vector<std::shared_ptr<RenderObject>>*& Objects() {
        static auto* objects = new(std::nothrow) std::vector<std::shared_ptr<RenderObject>>();
        return objects;
    }

    inline std::vector<std::shared_ptr<RenderObject>>*& VisibleObjects() {
        static auto* objects = new(std::nothrow) std::vector<std::shared_ptr<RenderObject>>();
        return objects;
    }
}

inline std::shared_ptr<RenderObject> Add(const std::shared_ptr<RenderObject>& renderObject,
                                         float layer = std::numeric_limits<float>::quiet_NaN()) {
    auto* objects = detail::Objects();
    auto* mutex = detail::ObjectsMutex();
    if (!objects || !mutex || !renderObject) {
        return {};
    }

    if (std::isfinite(layer)) {
        renderObject->Layer = layer;
    }

    std::scoped_lock lock(*mutex);
    objects->push_back(renderObject);
    return renderObject;
}

inline void Remove(const std::shared_ptr<RenderObject>& renderObject) {
    auto* objects = detail::Objects();
    auto* mutex = detail::ObjectsMutex();
    if (!objects || !mutex || !renderObject) {
        return;
    }

    std::scoped_lock lock(*mutex);
    objects->erase(
        std::remove_if(objects->begin(), objects->end(), [&](const std::shared_ptr<RenderObject>& item) {
            return item == renderObject;
        }),
        objects->end());
}

inline void Initialize() {}

inline size_t Count() {
    auto* objects = detail::Objects();
    auto* mutex = detail::ObjectsMutex();
    if (!objects || !mutex) {
        return 0;
    }

    std::scoped_lock lock(*mutex);
    return objects->size();
}

inline size_t VisibleCount() {
    if (auto* visibleObjects = detail::VisibleObjects()) {
        return visibleObjects->size();
    }
    return 0;
}

inline void RemoveAll() {
    Reset();
}

inline void Reset() {
    auto* objects = detail::Objects();
    auto* visibleObjects = detail::VisibleObjects();
    auto* mutex = detail::ObjectsMutex();
    if (objects && mutex) {
        std::scoped_lock lock(*mutex);
        for (const auto& object : *objects) {
            if (object) {
                object->Dispose();
            }
        }
        objects->clear();
    }

    if (visibleObjects) {
        visibleObjects->clear();
    }
}

inline void Update() {
    auto* objects = detail::Objects();
    auto* visibleObjects = detail::VisibleObjects();
    auto* mutex = detail::ObjectsMutex();
    if (!objects || !visibleObjects || !mutex) {
        return;
    }

    std::scoped_lock lock(*mutex);
    visibleObjects->clear();
    for (const auto& object : *objects) {
        if (object && object->Visible && object->HasValidLayer()) {
            visibleObjects->push_back(object);
        }
    }

    std::sort(visibleObjects->begin(), visibleObjects->end(), [](const auto& lhs, const auto& rhs) {
        return lhs->Layer < rhs->Layer;
    });
}

inline void PrepareObjects() {
    Update();
}

inline void OnPreReset() {
    auto* objects = detail::Objects();
    auto* mutex = detail::ObjectsMutex();
    if (!objects || !mutex) {
        return;
    }

    std::scoped_lock lock(*mutex);
    for (const auto& object : *objects) {
        if (object) {
            object->OnPreReset();
        }
    }
}

inline void OnPostReset() {
    auto* objects = detail::Objects();
    auto* mutex = detail::ObjectsMutex();
    if (!objects || !mutex) {
        return;
    }

    std::scoped_lock lock(*mutex);
    for (const auto& object : *objects) {
        if (object) {
            object->OnPostReset();
        }
    }
}

inline void DrawObjects() {
    if (auto* visibleObjects = detail::VisibleObjects()) {
        for (const auto& object : *visibleObjects) {
            if (object) {
                object->OnDraw();
            }
        }
    }
}

inline void OnDraw() {
    DrawObjects();
}

inline void EndSceneObjects() {
    if (auto* visibleObjects = detail::VisibleObjects()) {
        for (const auto& object : *visibleObjects) {
            if (object) {
                object->OnEndScene();
            }
        }
    }
}

inline void OnEndScene() {
    EndSceneObjects();
}

inline bool OnScreen(const Vector2& point) {
    const auto& io = ImGui::GetIO();
    return point.x >= 0.0f && point.y >= 0.0f && point.x <= io.DisplaySize.x && point.y <= io.DisplaySize.y;
}

inline bool OnScreen(const Vector3& world) {
    Vector2 screen = {};
    return Drawing::WorldToScreen(world, screen) && OnScreen(screen);
}

inline void DrawLine(const Vector2& start, const Vector2& end, ImU32 color, float thickness = 1.0f) {
    Drawing::DrawLine(start, end, color, thickness);
}

inline void DrawLine(const Vector3& start, const Vector3& end, ImU32 color, float thickness = 1.0f) {
    Drawing::DrawLine(start, end, color, thickness);
}

inline void DrawText(const Vector2& position, const char* text, ImU32 color = IM_COL32_WHITE, bool centered = false) {
    Drawing::DrawText(position, text, color, centered);
}

inline void DrawText(const Vector3& world, const char* text, ImU32 color = IM_COL32_WHITE, bool centered = false) {
    Drawing::DrawText(world, text, color, centered);
}

inline void DrawCircle(const Vector3& center, float radius, ImU32 color, float thickness = 1.0f, int segments = 48) {
    Drawing::DrawCircle(center, radius, color, thickness, segments);
}

} // namespace SDK::Utils::Render
