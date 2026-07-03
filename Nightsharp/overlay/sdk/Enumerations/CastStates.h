#pragma once

#include <cstdint>

namespace SDK {

enum class CastStates : std::int32_t {
    SuccessfullyCasted,
    NotReady,
    NotCasted,
    OutOfRange,
    Collision,
    NotEnoughTargets,
    LowHitChance,
    InvalidTarget,
    LowMana,
    FailedCondition,
};

} // namespace SDK
