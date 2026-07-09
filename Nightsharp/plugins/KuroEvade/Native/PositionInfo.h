#pragma once

#include "../../../SDK/SDK.h"

#include <cfloat>
#include <vector>

namespace Plugins::KuroEvade {

struct PositionInfo {
    Vec2 position;
    int dangerLevel = 0;
    int dangerCount = 0;
    bool dangerous = false;
    bool wall = false;
    bool rejectPosition = false;
    bool hasExtraDistance = false;
    float distToMouse = 0.0f;
    float closestDistance = FLT_MAX;
    float distToEnemy = FLT_MAX;
    std::vector<int> spellList;
    std::vector<int> dodgeableSpells;
    std::vector<int> undodgeableSpells;
};

} // namespace Plugins::KuroEvade
