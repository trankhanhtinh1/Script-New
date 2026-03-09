#pragma once
#include "sdk/SDK.h"
#include "sdk/EzEvade/Utils/DelayAction.h"
#include "sdk/EzEvade/Utils/EvadeUtils.h"
#include <memory>
#include <vector>

namespace EzEvade {
namespace Draw {

class RenderObject {
public:
    float EndTime = 0.0f;
    float StartTime = 0.0f;
    virtual ~RenderObject() = default;
    virtual void Draw() = 0;
};

class RenderObjects {
public:
    static inline std::vector<std::shared_ptr<RenderObject>> Objects = {};

    static void Render() {
        for (int i = (int)Objects.size() - 1; i >= 0; --i) {
            auto& obj = Objects[(size_t)i];
            if (!obj) {
                Objects.erase(Objects.begin() + i);
                continue;
            }

            if (obj->EndTime - EvadeUtils::TickCount() > 0.0f) {
                obj->Draw();
            } else {
                Objects.erase(Objects.begin() + i);
            }
        }
    }

    static void Add(const std::shared_ptr<RenderObject>& obj) {
        if (!obj) return;
        Objects.push_back(obj);
    }
};

} // namespace Draw
} // namespace EzEvade

