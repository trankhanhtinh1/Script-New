#pragma once

#include <cstdint>

namespace SDK {

enum class PerformanceType : std::int32_t {
    TickCount,
    Milliseconds,
    TimeSpan,
};

} // namespace SDK
