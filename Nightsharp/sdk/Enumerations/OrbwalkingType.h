#pragma once

namespace SDK {

enum class OrbwalkingType : int {
    None = 0,
    Movement = 1,
    StopMovement = 2,
    BeforeAttack = 3,
    AfterAttack = 4,
    OnAttack = 5,
    NonKillableMinion = 6,
    TargetSwitch = 7
};

} // namespace SDK
