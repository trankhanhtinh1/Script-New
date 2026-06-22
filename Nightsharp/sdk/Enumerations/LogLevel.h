#pragma once

#include <cstdint>

namespace SDK {

enum class LogLevel : std::int32_t {
    Debug = 2,
    Error = 5,
    Fatal = 6,
    Info = 1,
    Trace = 3,
    Warn = 4,
};

} // namespace SDK
