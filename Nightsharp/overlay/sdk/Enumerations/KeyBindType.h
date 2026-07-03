#pragma once

#include <cstdint>

namespace SDK {

enum class KeyBindType : std::int32_t {
    Toggle = 0,
    Press = 1,
    Hold = 2,
};

} // namespace SDK
