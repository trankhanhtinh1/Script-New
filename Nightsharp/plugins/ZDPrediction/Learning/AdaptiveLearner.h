#pragma once

#include "AdaptiveWeights.h"
#include "../Core/PredictionConfig.h"
#include "../Math/Kinematics.h"
#include "../Tracking/MovementTracker.h"
#include "../../../sdk/SDK.h"

#include <Windows.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <deque>
#include <unordered_map>

namespace ZDPrediction {

struct AdaptiveLearningStatistics {
    std::uint64_t evaluatedForecasts = 0;
    double pathWeight = 0.46;
    double velocityWeight = 0.34;
    double accelerationWeight = 0.20;
};

class AdaptiveLearner {
public:
    static void Reset() {
        AcquireSRWLockExclusive(&lock_);
        units_.clear();
        evaluatedForecasts_ = 0;
        ReleaseSRWLockExclusive(&lock_);
    }

    static void Update(const PredictionConfig& config) {
        const int now = SDK::Variables::TickCount();
        for (const auto& hero : SDK::ObjectManager::EnemyHeroes()) {
            if (!hero.IsValid() || hero.IsDead() || !hero.IsVisible()) continue;
            const MovementSnapshot movement = MovementTracker::Snapshot(hero, config.historyWindowMs);
            if (!movement.valid) continue;
            Observe(hero.NetworkId(), movement, now, config);
        }
    }

    static MotionModelWeights Weights(int networkId) {
        AcquireSRWLockShared(&lock_);
        const auto iterator = units_.find(networkId);
        const MotionModelWeights weights = iterator != units_.end()
            ? iterator->second.weights.Weights()
            : MotionModelWeights{};
        ReleaseSRWLockShared(&lock_);
        return weights;
    }

    static double Reliability(int networkId) {
        AcquireSRWLockShared(&lock_);
        const auto iterator = units_.find(networkId);
        const double reliability = iterator != units_.end()
            ? iterator->second.weights.Reliability()
            : 0.65;
        ReleaseSRWLockShared(&lock_);
        return reliability;
    }

    static AdaptiveLearningStatistics Statistics() {
        AdaptiveLearningStatistics statistics;
        AcquireSRWLockShared(&lock_);
        statistics.evaluatedForecasts = evaluatedForecasts_;
        if (!units_.empty()) {
            for (const auto& pair : units_) {
                const MotionModelWeights weights = pair.second.weights.Weights();
                statistics.pathWeight += weights.path;
                statistics.velocityWeight += weights.velocity;
                statistics.accelerationWeight += weights.acceleration;
            }
            const double divisor = static_cast<double>(units_.size() + 1);
            statistics.pathWeight /= divisor;
            statistics.velocityWeight /= divisor;
            statistics.accelerationWeight /= divisor;
        }
        ReleaseSRWLockShared(&lock_);
        return statistics;
    }

private:
    struct Forecast {
        int dueTick = 0;
        double movementScale = 0.0;
        std::array<Math::Vector2, 3> positions = {};
    };

    struct UnitLearningState {
        AdaptiveWeights weights;
        std::deque<Forecast> forecasts;
        int lastForecastTick = 0;
        int lastSeenTick = 0;
    };

    static inline SRWLOCK lock_ = SRWLOCK_INIT;
    static inline std::unordered_map<int, UnitLearningState> units_;
    static inline std::uint64_t evaluatedForecasts_ = 0;

    static Math::Vector2 AccelerationForecast(const MovementSnapshot& movement,
                                              double horizon) {
        if (movement.velocity.Length() > 40.0 &&
            std::abs(movement.angularVelocity) >= 0.08 &&
            std::abs(movement.angularVelocity) <= 4.0) {
            return Math::PositionWithTurn(
                movement.position,
                movement.velocity,
                movement.angularVelocity,
                horizon);
        }
        Math::Vector2 offset = movement.velocity * horizon +
            movement.acceleration * (0.5 * horizon * horizon *
                TrainedProfile::AccelerationScale);
        const double referenceSpeed = std::max({20.0, movement.averageSpeed,
                                                movement.velocity.Length()});
        const double maximumDistance = referenceSpeed * horizon * 1.45 + 20.0;
        const double distance = offset.Length();
        if (distance > maximumDistance && distance > Math::Epsilon) {
            offset = offset * (maximumDistance / distance);
        }
        return movement.position + offset;
    }

    static void Observe(int networkId,
                        const MovementSnapshot& movement,
                        int now,
                        const PredictionConfig& config) {
        AcquireSRWLockExclusive(&lock_);
        UnitLearningState& state = units_[networkId];
        state.lastSeenTick = now;

        while (!state.forecasts.empty() && state.forecasts.front().dueTick <= now) {
            const Forecast forecast = state.forecasts.front();
            state.forecasts.pop_front();
            std::array<double, 3> errors = {};
            for (std::size_t index = 0; index < errors.size(); ++index) {
                errors[index] = Math::Distance(forecast.positions[index], movement.position);
            }
            state.weights.Observe(errors, forecast.movementScale);
            ++evaluatedForecasts_;
        }

        if (now - state.lastForecastTick >= 80) {
            static constexpr std::array<double, 5> horizons = {0.15, 0.30, 0.50, 0.75, 1.00};
            const double speed = std::max({20.0, movement.averageSpeed,
                                           movement.velocity.Length()});
            for (const double horizon : horizons) {
                Forecast forecast;
                forecast.dueTick = now + static_cast<int>(horizon * 1000.0);
                forecast.movementScale = speed * horizon + 35.0;
                forecast.positions[static_cast<std::size_t>(MotionModel::Path)] =
                    movement.path.empty()
                        ? movement.position + movement.velocity * horizon
                        : Math::PositionOnPath(movement.path, speed, horizon);
                forecast.positions[static_cast<std::size_t>(MotionModel::Velocity)] =
                    movement.position + movement.velocity * horizon;
                forecast.positions[static_cast<std::size_t>(MotionModel::Acceleration)] =
                    AccelerationForecast(movement, horizon);
                state.forecasts.push_back(forecast);
            }
            std::sort(state.forecasts.begin(), state.forecasts.end(),
                [](const Forecast& left, const Forecast& right) {
                    return left.dueTick < right.dueTick;
                });
            state.lastForecastTick = now;
        }
        while (state.forecasts.size() > 80) state.forecasts.pop_back();

        for (auto iterator = units_.begin(); iterator != units_.end();) {
            if (now - iterator->second.lastSeenTick > 10000) iterator = units_.erase(iterator);
            else ++iterator;
        }
        ReleaseSRWLockExclusive(&lock_);
    }
};

}
