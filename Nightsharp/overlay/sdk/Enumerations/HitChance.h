#pragma once

#include <cstdint>

namespace SDK {

enum class HitChance : std::int32_t {
    None = -2,
    Collision = -1,
    OutOfRange = 0,
    Low = 1,
    Medium = 2,
    High = 3,
    VeryHigh = 4,
    Immobile = 5,
    Dash = 6,
};

} // namespace SDK
