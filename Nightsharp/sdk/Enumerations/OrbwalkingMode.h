#pragma once

#include <cstdint>

namespace SDK {

enum class OrbwalkingMode : std::int32_t {
    Combo = 0,
    Harass = 1,
    Hybrid = Harass,
    LaneClear = 2,
    LastHit = 3,
    Flee = 4,
    None = 5,
};

} // namespace SDK
