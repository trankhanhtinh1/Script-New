#pragma once

#include "../Core/Variables.h"

#include <algorithm>
#include <cmath>
#include <new>
#include <random>

namespace SDK::Utils::WeightedRandom {

namespace detail {
    inline std::mt19937*& Engine() {
        static auto* engine = new(std::nothrow) std::mt19937(static_cast<uint32_t>(Variables::TickCount()));
        return engine;
    }
}

inline std::mt19937& Random() {
    return *detail::Engine();
}

inline int Next(int min, int max) {
    if (min >= max) {
        return min;
    }

    const double mean = (static_cast<double>(min) + static_cast<double>(max - 1)) * 0.5;
    const double variance = std::max(1.0, (static_cast<double>(max - min) * static_cast<double>(max - min)) / 12.0);
    const double stddev = std::sqrt(variance);

    std::normal_distribution<double> distribution(mean, stddev);
    const int value = static_cast<int>(std::llround(distribution(Random())));
    return std::clamp(value, min, max - 1);
}

} // namespace SDK::Utils::WeightedRandom

