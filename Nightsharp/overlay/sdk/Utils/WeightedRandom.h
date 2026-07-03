#pragma once

#include "../Core/Variables.h"

#include <cstdint>
#include <cstddef>
#include <cmath>
#include <numeric>
#include <random>
#include <vector>

namespace SDK::Core::Utils {

class WeightedRandom {
public:
    static std::mt19937& Random() {
        static std::mt19937 rng(static_cast<std::uint32_t>(SDK::Variables::TickCount()));
        return rng;
    }

    static int Next(int min, int max) {
        if (max <= 0) {
            return min;
        }

        std::vector<int> values;
        values.reserve(static_cast<std::size_t>(max));
        for (int i = 0; i < max; ++i) {
            values.push_back(min + i);
        }

        const double mean = std::accumulate(values.begin(), values.end(), 0.0) / values.size();
        double variance = 0.0;
        for (int value : values) {
            const double delta = static_cast<double>(value) - mean;
            variance += delta * delta;
        }
        variance /= values.size();

        std::normal_distribution<double> distribution(mean, std::sqrt(variance));
        return static_cast<int>(distribution(Random()));
    }
};

} // namespace SDK::Core::Utils

namespace SDK::Utils {
    using WeightedRandom = ::SDK::Core::Utils::WeightedRandom;
} // namespace SDK::Utils
