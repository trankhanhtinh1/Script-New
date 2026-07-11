#pragma once

#include <cfloat>

namespace ZDEvade {

struct PositionInfo {
    Vec2 position = {};
    int posDangerLevel = 0;
    int posDangerCount = 0;
    bool isDangerousPos = false;
    float distanceToMouse = 0.0f;
    float travelDistance = FLT_MAX;
    float closestDistance = FLT_MAX;
    float timeMargin = FLT_MAX;
    float exitDistance = FLT_MAX;
    float arrivalTimeMs = FLT_MAX;
    int debugIndex = -1;
    bool rejectPosition = false;
    bool endpointSafe = true;
    bool pathSafe = true;
    bool collisionSafe = true;
    int rejectReason = 0;

    PositionInfo() = default;
};

} // namespace ZDEvade
