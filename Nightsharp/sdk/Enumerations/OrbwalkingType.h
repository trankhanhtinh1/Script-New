#pragma once

#include <cstdint>

namespace SDK {

enum class OrbwalkingType : std::int32_t {
    None,
    Movement,
    StopMovement,
    BeforeAttack,
    AfterAttack,
    OnAttack,
    NonKillableMinion,
    TargetSwitch,
};

} // namespace SDK
