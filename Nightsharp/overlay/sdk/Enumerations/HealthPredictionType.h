#pragma once

#include <cstdint>

namespace SDK {

enum class HealthPredictionType : std::int32_t {
    Default,
    Simulated,
    Special,
};

} // namespace SDK
