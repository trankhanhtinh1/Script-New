#pragma once

#include <cstdint>

namespace SDK {

enum class TeleportType : std::int32_t {
    Recall,
    Teleport,
    TwistedFate,
    Shen,
    Unknown,
};

} // namespace SDK
