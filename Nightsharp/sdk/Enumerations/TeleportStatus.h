#pragma once

#include <cstdint>

namespace SDK {

enum class TeleportStatus : std::int32_t {
    Start,
    Abort,
    Finish,
    Unknown,
};

} // namespace SDK
