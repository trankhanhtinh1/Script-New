#pragma once

#include "TrainedProfile.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace ZDPrediction {

enum class MotionModel : std::size_t {
    Path = 0,
    Velocity = 1,
    Acceleration = 2,
    Count = 3
};

struct AdaptiveParameters {
    double decay = TrainedProfile::LossDecay;
    double temperature = TrainedProfile::LossTemperature;
    double minimumWeight = TrainedProfile::MinimumModelWeight;
};

struct MotionModelWeights {
    double path = 0.46;
    double velocity = 0.34;
    double acceleration = 0.20;

    double operator[](MotionModel model) const {
        switch (model) {
        case MotionModel::Path: return path;
        case MotionModel::Velocity: return velocity;
        case MotionModel::Acceleration: return acceleration;
        default: return 0.0;
        }
    }
};

class AdaptiveWeights {
public:
    explicit AdaptiveWeights(AdaptiveParameters parameters = {})
        : parameters_(parameters) {}

    void Observe(const std::array<double, 3>& errors,
                 double movementScale) {
        const double scale = std::max(35.0, movementScale);
        std::array<double, 3> normalized = {};
        for (std::size_t index = 0; index < errors.size(); ++index) {
            normalized[index] = std::log1p(std::max(0.0, errors[index]) / scale);
        }
        const double best = *std::min_element(normalized.begin(), normalized.end());
        for (std::size_t index = 0; index < errors.size(); ++index) {
            const double absoluteLoss = normalized[index] * normalized[index];
            const double regret = normalized[index] - best;
            const double sampleLoss = absoluteLoss * 0.35 + regret * regret * 0.65;
            losses_[index] = samples_ == 0
                ? sampleLoss
                : parameters_.decay * losses_[index] +
                    (1.0 - parameters_.decay) * sampleLoss;
        }
        ++samples_;
    }

    MotionModelWeights Weights() const {
        static constexpr std::array<double, 3> prior = {0.46, 0.34, 0.20};
        std::array<double, 3> learned = {};
        double learnedTotal = 0.0;
        const double minimumLoss = *std::min_element(losses_.begin(), losses_.end());
        for (std::size_t index = 0; index < learned.size(); ++index) {
            learned[index] = std::max(parameters_.minimumWeight,
                std::exp(-parameters_.temperature * (losses_[index] - minimumLoss)));
            learnedTotal += learned[index];
        }
        if (learnedTotal <= 0.0) return {};
        for (double& value : learned) value /= learnedTotal;

        const double maturity = std::clamp(static_cast<double>(samples_) / 40.0, 0.0, 1.0);
        std::array<double, 3> blended = {};
        double total = 0.0;
        for (std::size_t index = 0; index < blended.size(); ++index) {
            blended[index] = prior[index] * (1.0 - maturity) + learned[index] * maturity;
            total += blended[index];
        }
        if (total <= 0.0) return {};
        return {blended[0] / total, blended[1] / total, blended[2] / total};
    }

    double Reliability() const {
        const double initial = 0.65;
        if (samples_ == 0) return initial;
        const double minimumLoss = *std::min_element(losses_.begin(), losses_.end());
        const double learned = std::clamp(std::exp(-3.0 * minimumLoss), 0.05, 1.0);
        const double maturity = std::clamp(static_cast<double>(samples_) / 40.0, 0.0, 1.0);
        return initial * (1.0 - maturity) + learned * maturity;
    }

    MotionModel BestModel(bool allowAcceleration = true) const {
        const MotionModelWeights weights = Weights();
        MotionModel best = weights.velocity > weights.path
            ? MotionModel::Velocity
            : MotionModel::Path;
        if (allowAcceleration && weights.acceleration > weights[best]) {
            best = MotionModel::Acceleration;
        }
        return best;
    }

    const std::array<double, 3>& Losses() const {
        return losses_;
    }

    std::uint64_t Samples() const {
        return samples_;
    }

private:
    AdaptiveParameters parameters_;
    std::array<double, 3> losses_ = {0.35, 0.45, 0.60};
    std::uint64_t samples_ = 0;
};

}
