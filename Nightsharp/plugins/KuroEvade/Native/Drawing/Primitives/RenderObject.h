#pragma once

#include "../../Helpers/Utils.h"

namespace Plugins::KuroEvade {

struct RenderObject {
    virtual ~RenderObject() = default;
    virtual void Draw() = 0;

    float startTime = 0.0f;
    float endTime = 0.0f;

    bool Expired() const {
        return endTime <= static_cast<float>(EvadeUtils::TickCount());
    }
};

} // namespace Plugins::KuroEvade
