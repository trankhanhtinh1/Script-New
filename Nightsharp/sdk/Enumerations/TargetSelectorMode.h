#pragma once

namespace SDK {

enum class TargetSelectorMode : int {
    Priority = 0,
    Closest = 1,
    LeastHealth = 2,
    MostAttackDamage = 3,
    MostAbilityPower = 4,
    NearMouse = 5,
    LessAttacksToKill = 6,
    LessCastsToKill = 7,
    Weight = 8
};

} // namespace SDK
