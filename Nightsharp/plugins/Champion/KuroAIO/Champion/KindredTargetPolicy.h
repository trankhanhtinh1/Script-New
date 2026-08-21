#pragma once

#include <algorithm>

namespace Plugins::KuroAIO::Kindred::TargetPolicy {

struct MechanicFacts {
    int EStacks = 0;
    bool EMarked = false;
    bool ETracker = false;
    bool RZone = false;
    bool RAtDeathFloor = false;
};

inline bool RZoneBlocksAction(const MechanicFacts& facts) {
    // The ultimate death floor is always protected during automatic targeting.
    return facts.RAtDeathFloor;
}

inline float Score(const MechanicFacts& facts) {
    float result = 0.0f;
    if (facts.EMarked) result += 210.0f;
    if (facts.ETracker) result += 130.0f;
    result += static_cast<float>(std::clamp(facts.EStacks, 0, 3)) * 155.0f;
    if (facts.RAtDeathFloor) result -= 720.0f;
    // Standing inside the zone but above the floor only delays the kill, so
    // it gets a mild hesitation instead of a hard block.
    else if (facts.RZone) result -= 60.0f;
    return std::clamp(result, -1000.0f, 1000.0f);
}

inline bool PreferTwoAttackAlternate(float markedEffectiveHealth,
                                     float markedAttackDamage,
                                     float alternateEffectiveHealth,
                                     float alternateAttackDamage) {
    return markedAttackDamage > 0.0f && alternateAttackDamage > 0.0f &&
        markedEffectiveHealth > markedAttackDamage * 3.0f &&
        alternateEffectiveHealth <= alternateAttackDamage * 2.0f;
}

} // namespace Plugins::KuroAIO::Kindred::TargetPolicy
