#include "../Learning/AdaptiveWeights.h"
#include "../Math/Kinematics.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <random>
#include <string>
#include <vector>

namespace {

using ZDPrediction::AdaptiveParameters;
using ZDPrediction::AdaptiveWeights;
using ZDPrediction::MotionModel;
using ZDPrediction::MotionModelWeights;
using ZDPrediction::Math::Distance;
using ZDPrediction::Math::PositionOnPath;
using ZDPrediction::Math::Vector2;

enum class ScenarioType {
    Straight,
    Acceleration,
    RightTurn,
    ZigZag,
    StopGo,
    Circular,
    RandomJuke
};

struct HyperParameters {
    double velocityAlpha = 0.5;
    double accelerationAlpha = 0.3;
    double accelerationScale = 0.5;
    double decay = 0.92;
    double temperature = 3.2;
};

struct Evaluation {
    double meanError = 0.0;
    double rootMeanSquaredError = 0.0;
    double percentile95 = 0.0;
    double pathError = 0.0;
    double velocityError = 0.0;
    double accelerationError = 0.0;
    MotionModelWeights finalWeights;
    std::uint64_t samples = 0;
};

Vector2 Position(ScenarioType scenario, double time, std::uint64_t seed) {
    switch (scenario) {
    case ScenarioType::Straight:
        return {120.0 + 345.0 * time, 250.0};
    case ScenarioType::Acceleration:
        return {150.0 + 190.0 * time + 32.0 * time * time,
                100.0 + 250.0 * time - 11.0 * time * time};
    case ScenarioType::RightTurn:
        if (time <= 2.2) return {100.0 + 350.0 * time, 100.0};
        return {870.0, 100.0 + 350.0 * (time - 2.2)};
    case ScenarioType::ZigZag:
        return {100.0 + 330.0 * time, 250.0 + 115.0 * std::sin(time * 5.2)};
    case ScenarioType::StopGo: {
        const double cycle = std::fmod(time, 2.6);
        const double completedCycles = std::floor(time / 2.6);
        const double base = completedCycles * 345.0 * 1.7;
        const double movingTime = std::min(cycle, 1.7);
        return {100.0 + base + 345.0 * movingTime, 350.0};
    }
    case ScenarioType::Circular: {
        const double angle = time * 0.72;
        return {650.0 + 470.0 * std::cos(angle),
                650.0 + 470.0 * std::sin(angle)};
    }
    case ScenarioType::RandomJuke: {
        std::mt19937_64 random(seed);
        Vector2 position{200.0, 200.0};
        double remaining = time;
        int segment = 0;
        while (remaining > 0.0) {
            const double duration = 0.22 + static_cast<double>(random() % 70) / 100.0;
            const double angle = static_cast<double>(random() % 6283) / 1000.0;
            const double speed = 300.0 + static_cast<double>(random() % 110);
            const double used = std::min(remaining, duration);
            position += Vector2{std::cos(angle), std::sin(angle)} * speed * used;
            remaining -= used;
            if (++segment > 200) break;
        }
        return position;
    }
    }
    return {};
}

Vector2 TrueVelocity(ScenarioType scenario, double time, std::uint64_t seed) {
    constexpr double step = 0.002;
    const double previous = std::max(0.0, time - step);
    return (Position(scenario, time + step, seed) - Position(scenario, previous, seed)) /
        (time + step - previous);
}

std::vector<Vector2> KnownPath(ScenarioType scenario,
                               double time,
                               const Vector2& current,
                               const Vector2& velocity,
                               std::uint64_t seed) {
    switch (scenario) {
    case ScenarioType::Straight:
        return {current, current + velocity.Normalized() * 2500.0};
    case ScenarioType::Acceleration:
    case ScenarioType::Circular:
    case ScenarioType::ZigZag:
        return {current, current + velocity.Normalized() * 900.0};
    case ScenarioType::RightTurn:
        if (time < 2.2) return {current, {870.0, 100.0}, {870.0, 2200.0}};
        return {current, {870.0, 2200.0}};
    case ScenarioType::StopGo:
        if (std::fmod(time, 2.6) >= 1.7) return {current};
        return {current, current + Vector2{1.0, 0.0} * 1400.0};
    case ScenarioType::RandomJuke: {
        const Vector2 nearFuture = Position(scenario, time + 0.35, seed);
        return {current, nearFuture, nearFuture + (nearFuture - current).Normalized() * 650.0};
    }
    }
    return {current};
}

Vector2 AccelerationForecast(const Vector2& position,
                             const Vector2& velocity,
                             const Vector2& acceleration,
                             double angularVelocity,
                             double horizon,
                             const HyperParameters& parameters) {
    if (velocity.Length() > 40.0 && std::abs(angularVelocity) >= 0.08 &&
        std::abs(angularVelocity) <= 4.0) {
        return ZDPrediction::Math::PositionWithTurn(
            position, velocity, angularVelocity, horizon);
    }
    Vector2 offset = velocity * horizon + acceleration *
        (0.5 * horizon * horizon * parameters.accelerationScale);
    const double maximum = std::max(30.0, velocity.Length() * horizon * 1.45 + 20.0);
    const double length = offset.Length();
    if (length > maximum && length > ZDPrediction::Math::Epsilon) offset = offset * (maximum / length);
    return position + offset;
}

Evaluation EvaluateScenario(ScenarioType scenario,
                            const HyperParameters& parameters,
                            std::uint64_t seed) {
    constexpr double step = 0.05;
    constexpr double duration = 14.0;
    static constexpr std::array<double, 5> horizons = {0.15, 0.30, 0.50, 0.75, 1.00};

    std::mt19937_64 random(seed ^ 0xA17D9B13ULL);
    std::normal_distribution<double> noise(0.0, 1.5);
    AdaptiveWeights adaptive({parameters.decay, parameters.temperature, 0.04});
    Vector2 filteredVelocity;
    Vector2 filteredAcceleration;
    Vector2 previousPosition = Position(scenario, 0.0, seed);
    Vector2 previousVelocity;
    double filteredAngularVelocity = 0.0;
    bool initialized = false;
    std::vector<double> selectedErrors;
    double squared = 0.0;
    double total = 0.0;
    double pathTotal = 0.0;
    double velocityTotal = 0.0;
    double accelerationTotal = 0.0;

    for (double time = step; time + horizons.back() <= duration; time += step) {
        const Vector2 measured = Position(scenario, time, seed) + Vector2{noise(random), noise(random)};
        const Vector2 measuredVelocity = (measured - previousPosition) / step;
        if (!initialized) {
            filteredVelocity = measuredVelocity;
            previousVelocity = measuredVelocity;
            initialized = true;
        } else {
            filteredVelocity = filteredVelocity * (1.0 - parameters.velocityAlpha) +
                measuredVelocity * parameters.velocityAlpha;
        }
        const Vector2 measuredAcceleration = (filteredVelocity - previousVelocity) / step;
        filteredAcceleration = filteredAcceleration * (1.0 - parameters.accelerationAlpha) +
            measuredAcceleration * parameters.accelerationAlpha;
        if (previousVelocity.Length() > 40.0 && filteredVelocity.Length() > 40.0) {
            const double angle = std::atan2(
                previousVelocity.Cross(filteredVelocity),
                previousVelocity.Dot(filteredVelocity));
            const double measuredTurn = std::clamp(angle / step, -6.0, 6.0);
            filteredAngularVelocity = filteredAngularVelocity *
                (1.0 - parameters.accelerationAlpha) +
                measuredTurn * parameters.accelerationAlpha;
        }
        previousVelocity = filteredVelocity;
        previousPosition = measured;

        const double speed = std::max(20.0, filteredVelocity.Length());
        const std::vector<Vector2> path = KnownPath(
            scenario, time, measured, filteredVelocity, seed);
        for (const double horizon : horizons) {
            const Vector2 actual = Position(scenario, time + horizon, seed);
            std::array<Vector2, 3> predictions = {
                PositionOnPath(path, speed, horizon),
                measured + filteredVelocity * horizon,
                AccelerationForecast(measured, filteredVelocity, filteredAcceleration,
                                     filteredAngularVelocity, horizon, parameters)
            };
            std::array<double, 3> errors = {
                Distance(predictions[0], actual),
                Distance(predictions[1], actual),
                Distance(predictions[2], actual)
            };
            const MotionModelWeights before = adaptive.Weights();
            MotionModel selected = MotionModel::Path;
            if (before.velocity > before.path) selected = MotionModel::Velocity;
            if (before.acceleration > before[selected]) selected = MotionModel::Acceleration;
            const double selectedError = errors[static_cast<std::size_t>(selected)];
            selectedErrors.push_back(selectedError);
            total += selectedError;
            squared += selectedError * selectedError;
            pathTotal += errors[0];
            velocityTotal += errors[1];
            accelerationTotal += errors[2];
            adaptive.Observe(errors, speed * horizon + 35.0);
        }
    }

    Evaluation evaluation;
    evaluation.samples = selectedErrors.size();
    if (evaluation.samples == 0) return evaluation;
    std::sort(selectedErrors.begin(), selectedErrors.end());
    const double divisor = static_cast<double>(evaluation.samples);
    evaluation.meanError = total / divisor;
    evaluation.rootMeanSquaredError = std::sqrt(squared / divisor);
    evaluation.percentile95 = selectedErrors[static_cast<std::size_t>(
        std::floor((selectedErrors.size() - 1) * 0.95))];
    evaluation.pathError = pathTotal / divisor;
    evaluation.velocityError = velocityTotal / divisor;
    evaluation.accelerationError = accelerationTotal / divisor;
    evaluation.finalWeights = adaptive.Weights();
    return evaluation;
}

std::string ScenarioName(ScenarioType scenario) {
    switch (scenario) {
    case ScenarioType::Straight: return "straight";
    case ScenarioType::Acceleration: return "acceleration";
    case ScenarioType::RightTurn: return "right-turn";
    case ScenarioType::ZigZag: return "zigzag";
    case ScenarioType::StopGo: return "stop-go";
    case ScenarioType::Circular: return "circular";
    case ScenarioType::RandomJuke: return "random-juke";
    }
    return "unknown";
}

double Objective(const HyperParameters& parameters) {
    static constexpr std::array<ScenarioType, 7> scenarios = {
        ScenarioType::Straight,
        ScenarioType::Acceleration,
        ScenarioType::RightTurn,
        ScenarioType::ZigZag,
        ScenarioType::StopGo,
        ScenarioType::Circular,
        ScenarioType::RandomJuke
    };
    double objective = 0.0;
    for (std::size_t index = 0; index < scenarios.size(); ++index) {
        const Evaluation evaluation = EvaluateScenario(
            scenarios[index], parameters, 0xC001D00DULL + index * 7919ULL);
        objective += evaluation.meanError * 0.55 +
            evaluation.rootMeanSquaredError * 0.25 +
            evaluation.percentile95 * 0.20;
    }
    return objective / static_cast<double>(scenarios.size());
}

}

int main() {
    const std::array<double, 4> velocityAlphas = {0.25, 0.40, 0.55, 0.70};
    const std::array<double, 4> accelerationAlphas = {0.10, 0.20, 0.35, 0.50};
    const std::array<double, 4> accelerationScales = {0.25, 0.50, 0.75, 1.00};
    const std::array<double, 4> decays = {0.84, 0.89, 0.93, 0.97};
    const std::array<double, 4> temperatures = {1.8, 2.6, 3.4, 4.2};

    HyperParameters best;
    double bestObjective = std::numeric_limits<double>::infinity();
    std::uint64_t combinations = 0;
    for (const double velocityAlpha : velocityAlphas) {
        for (const double accelerationAlpha : accelerationAlphas) {
            for (const double accelerationScale : accelerationScales) {
                for (const double decay : decays) {
                    for (const double temperature : temperatures) {
                        HyperParameters parameters{
                            velocityAlpha,
                            accelerationAlpha,
                            accelerationScale,
                            decay,
                            temperature
                        };
                        const double objective = Objective(parameters);
                        ++combinations;
                        if (objective < bestObjective) {
                            bestObjective = objective;
                            best = parameters;
                        }
                    }
                }
            }
        }
    }

    std::cout << std::fixed << std::setprecision(4);
    std::cout << "combinations=" << combinations << '\n';
    std::cout << "objective=" << bestObjective << '\n';
    std::cout << "velocity_alpha=" << best.velocityAlpha << '\n';
    std::cout << "acceleration_alpha=" << best.accelerationAlpha << '\n';
    std::cout << "acceleration_scale=" << best.accelerationScale << '\n';
    std::cout << "decay=" << best.decay << '\n';
    std::cout << "temperature=" << best.temperature << '\n';

    static constexpr std::array<ScenarioType, 7> scenarios = {
        ScenarioType::Straight,
        ScenarioType::Acceleration,
        ScenarioType::RightTurn,
        ScenarioType::ZigZag,
        ScenarioType::StopGo,
        ScenarioType::Circular,
        ScenarioType::RandomJuke
    };
    for (std::size_t index = 0; index < scenarios.size(); ++index) {
        const Evaluation evaluation = EvaluateScenario(
            scenarios[index], best, 0xC001D00DULL + index * 7919ULL);
        std::cout << ScenarioName(scenarios[index])
                  << " mean=" << evaluation.meanError
                  << " rmse=" << evaluation.rootMeanSquaredError
                  << " p95=" << evaluation.percentile95
                  << " raw=" << evaluation.pathError << '/'
                  << evaluation.velocityError << '/'
                  << evaluation.accelerationError
                  << " weights=" << evaluation.finalWeights.path << '/'
                  << evaluation.finalWeights.velocity << '/'
                  << evaluation.finalWeights.acceleration << '\n';
    }
    return 0;
}
