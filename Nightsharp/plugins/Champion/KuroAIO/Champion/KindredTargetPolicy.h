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

inline bool RZoneBlocksAction(const MechanicFacts& facts,
                              bool manualAssist) {
    // ManualAssist is an inspection/selection request, not a damage route.
    // Keep the identity explainable there, but never let a damage action
    // consume a target protected by Kindred's ultimate.
    return facts.RZone && !manualAssist;
}

inline float Score(const MechanicFacts& facts) {
    float result = 0.0f;
    if (facts.EMarked) result += 210.0f;
    if (facts.ETracker) result += 130.0f;
    result += static_cast<float>(std::clamp(facts.EStacks, 0, 3)) * 155.0f;
    if (facts.RZone) result -= 720.0f;
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
