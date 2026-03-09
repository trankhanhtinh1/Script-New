#pragma once
// ============================================================================
// WeightedRandom.h — Gaussian (bell-curve) weighted random number generator
// Ported from EnsoulSharp.SDK/Core/Utils/WeightedRandom.cs
// ============================================================================

#include <random>
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace SDK {

    // ========================================================================
    // WeightedRandom — generates numbers clustered around the mean (Gaussian)
    //   Useful for humanizing delays, movement offsets, etc.
    // ========================================================================
    class WeightedRandom {
    public:
        // --------------------------------------------------------------------
        // Next — returns a random integer in [min, max) weighted toward center
        //   Uses Box-Muller transform for Gaussian distribution
        // --------------------------------------------------------------------
        static int Next(int minVal, int maxVal) {
            if (minVal >= maxVal) return minVal;

            // Mean = midpoint, StdDev = range / 6 (99.7% within range)
            double mean = (minVal + maxVal) / 2.0;
            double stdDev = (maxVal - minVal) / 6.0;
            if (stdDev < 0.01) stdDev = 0.01;

            double result = NextGaussian(mean, stdDev);

            // Clamp to [min, max)
            return (std::max)(minVal, (std::min)(maxVal - 1, static_cast<int>(std::round(result))));
        }

        // --------------------------------------------------------------------
        // NextFloat — returns a random float in [min, max] weighted toward center
        // --------------------------------------------------------------------
        static float NextFloat(float minVal, float maxVal) {
            if (minVal >= maxVal) return minVal;

            double mean = (minVal + maxVal) / 2.0;
            double stdDev = (maxVal - minVal) / 6.0;
            if (stdDev < 0.001) stdDev = 0.001;

            double result = NextGaussian(mean, stdDev);
            return (std::max)(minVal, (std::min)(maxVal, static_cast<float>(result)));
        }

        // --------------------------------------------------------------------
        // NextUniform — simple uniform random in [min, max]
        // --------------------------------------------------------------------
        static int NextUniform(int minVal, int maxVal) {
            if (minVal >= maxVal) return minVal;
            std::uniform_int_distribution<int> dist(minVal, maxVal);
            return dist(GetEngine());
        }

        static float NextUniformFloat(float minVal, float maxVal) {
            if (minVal >= maxVal) return minVal;
            std::uniform_real_distribution<float> dist(minVal, maxVal);
            return dist(GetEngine());
        }

        // --------------------------------------------------------------------
        // Seed — reseed the RNG (optional, auto-seeded by default)
        // --------------------------------------------------------------------
        static void Seed(unsigned int seed) {
            GetEngine().seed(seed);
        }

    private:
        // Box-Muller transform for Gaussian distributed value
        static double NextGaussian(double mean, double stdDev) {
            auto& engine = GetEngine();
            std::uniform_real_distribution<double> dist(0.0, 1.0);

            double u1 = dist(engine);
            double u2 = dist(engine);

            // Avoid log(0)
            while (u1 <= 1e-10) u1 = dist(engine);

            double randStdNormal = std::sqrt(-2.0 * std::log(u1)) * std::sin(2.0 * M_PI * u2);
            return mean + stdDev * randStdNormal;
        }

        static std::mt19937& GetEngine() {
            static std::mt19937 engine(static_cast<unsigned int>(
                std::chrono::steady_clock::now().time_since_epoch().count()));
            return engine;
        }
    };

} // namespace SDK
