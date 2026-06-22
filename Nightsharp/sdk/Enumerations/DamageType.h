#pragma once

#include <cstdint>

namespace SDK {

enum class DamageType : std::int32_t {
    Physical = 0,
    Magical = 1,
    Mixed = 2,
    True = 3,
};

} // namespace SDK
