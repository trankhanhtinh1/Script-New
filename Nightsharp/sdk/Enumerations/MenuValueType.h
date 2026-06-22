#pragma once

#include <cstdint>

namespace SDK {

enum class MenuValueType : std::int32_t {
    None,
    Boolean,
    Color,
    KeyBind,
    List,
    Slider,
};

} // namespace SDK
