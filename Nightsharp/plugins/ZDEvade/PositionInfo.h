#pragma once

#include <cfloat>

namespace ZDEvade {

struct PositionInfo {
    Vec2 position = {};
    int posDangerLevel = 0;
    int posDangerCount = 0;
    bool isDangerousPos = false;
    float distanceToMouse = 0.0f;
    float closestDistance = FLT_MAX;
    bool rejectPosition = false;

    PositionInfo() = default;
};

} // namespace ZDEvade
