#pragma once

namespace SDK {

enum class HitChance : int {
    Impossible = 0,
    OutOfRange = 1,
    Collision = 2,
    Low = 3,
    Medium = 4,
    High = 5,
    VeryHigh = 6,
    Dashing = 7,
    Immobile = 8
};

} // namespace SDK
