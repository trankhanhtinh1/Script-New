#pragma once

#include <cstdint>

namespace SDK {

enum class OrbwalkingMode : std::int32_t {
    None,
    Combo,
    Hybrid,
    LastHit,
    LaneClear,
};

} // namespace SDK
