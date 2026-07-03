#pragma once

#include <cstdint>

namespace SDK {

enum class NotificationIconType : std::int32_t {
    None = 0,
    Error = 1,
    Warning = 2,
    Check = 3,
    Select = 4,
};

} // namespace SDK
