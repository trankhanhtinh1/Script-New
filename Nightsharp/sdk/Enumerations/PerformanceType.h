#pragma once

namespace SDK {

enum class PerformanceType : int {
    TickCount = 0,
    Ticks = TickCount,
    Milliseconds = 1,
    TimeSpan = 2
};

} // namespace SDK
