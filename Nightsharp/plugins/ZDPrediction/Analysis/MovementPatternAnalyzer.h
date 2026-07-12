#pragma once

#include "../Math/Vector2.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace ZDPrediction {

struct MovementHistoryPoint {
    double ageSeconds = 0.0;
    Math::Vector2 position = {};
    Math::Vector2 velocity = {};
};

struct MovementHistorySummary {
    Math::Vector2 recentCenter = {};
    double displacementEfficiency = 1.0;
    double directionReversalsPerSecond = 0.0;
    int directionReversalCount = 0;
};

struct MovementPatternMetrics {
    double directionStability = 1.0;
    double speedStability = 1.0;
    double displacementEfficiency = 1.0;
    double pathChangesPerSecond = 0.0;
    double directionReversalsPerSecond = 0.0;
    int directionReversalCount = 0;
    double pathAgeSeconds = 10.0;
    double angularVelocity = 0.0;
};

struct MovementModelPolicy {
    double jukeScore = 0.0;
    double pathWeight = 0.46;
    double velocityWeight = 0.34;
    double accelerationWeight = 0.20;
    double velocityScale = 1.0;
    double centerPull = 0.0;
    double displacementScale = 1.15;
};

class MovementPatternAnalyzer {
public:
    static bool IsPositionDiscontinuity(const Math::Vector2& previous,
                                        const Math::Vector2& current,
                                        double elapsedSeconds,
                                        double moveSpeed,
                                        double minimumJump = 110.0) {
        if (!previous.IsFinite() || !current.IsFinite() ||
            elapsedSeconds <= 0.0 || elapsedSeconds > 0.50) return false;
        const double physicallyPlausible = std::max(
            minimumJump,
            std::max(0.0, moveSpeed) * elapsedSeconds * 2.75 + 45.0);
        return Math::Distance(previous, current) > physicallyPlausible;
    }

    static bool IsHistoryReliable(int sampleCount,
                                  double sampleSpanSeconds,
                                  double visibleSeconds,
                                  double positionStableSeconds) {
        return sampleCount >= 3 && sampleSpanSeconds >= 0.07 &&
            visibleSeconds >= 0.10 && positionStableSeconds >= 0.10;
    }

    static double RequiredDirectionCommitment(double travelTime,
                                              int reversalCount) {
        const double base = 0.08 + std::clamp(travelTime, 0.0, 1.5) * 0.09;
        const double repeatedJukeHold = reversalCount >= 2 ? 0.04 : 0.0;
        return std::clamp(base + repeatedJukeHold, 0.08, 0.22);
    }

    static MovementHistorySummary SummarizeHistory(
        const std::vector<MovementHistoryPoint>& points) {
        MovementHistorySummary summary;
        if (points.empty()) return summary;

        std::vector<MovementHistoryPoint> ordered;
        ordered.reserve(points.size());
        for (const auto& point : points) {
            if (point.position.IsFinite()) ordered.push_back(point);
        }
        if (ordered.empty()) return summary;
        std::sort(ordered.begin(), ordered.end(), [](const auto& left, const auto& right) {
            return left.ageSeconds > right.ageSeconds;
        });

        Math::Vector2 weightedCenter;
        double centerWeight = 0.0;
        double traveled = 0.0;
        Math::Vector2 oldestPosition = ordered.front().position;
        Math::Vector2 newestPosition = ordered.back().position;
        Math::Vector2 previousPosition;
        Math::Vector2 previousDirection;
        double oldestAge = ordered.front().ageSeconds;
        double newestAge = ordered.back().ageSeconds;
        double firstDirectionAge = -1.0;
        double lastDirectionAge = 0.0;
        int reversals = 0;
        bool hasPreviousPosition = false;
        bool hasPreviousDirection = false;

        for (const auto& point : ordered) {
            const double age = std::max(0.0, point.ageSeconds);
            if (age <= 0.45) {
                const double weight = 1.0 / (1.0 + age / 0.14);
                weightedCenter += point.position * weight;
                centerWeight += weight;
            }
            if (age <= 0.65) {
                if (hasPreviousPosition) {
                    const double segment = Math::Distance(previousPosition, point.position);
                    if (segment >= 1.0 && segment <= 250.0) traveled += segment;
                }
                previousPosition = point.position;
                hasPreviousPosition = true;
                oldestAge = std::max(oldestAge, age);
                newestAge = std::min(newestAge, age);
            }
            if (age > 0.60 || !point.velocity.IsFinite() || point.velocity.Length() < 60.0) continue;
            const Math::Vector2 direction = point.velocity.Normalized();
            if (hasPreviousDirection && previousDirection.Dot(direction) < -0.25) ++reversals;
            previousDirection = direction;
            hasPreviousDirection = true;
            if (firstDirectionAge < 0.0) firstDirectionAge = age;
            firstDirectionAge = std::max(firstDirectionAge, age);
            lastDirectionAge = std::min(lastDirectionAge, age);
        }

        summary.recentCenter = centerWeight > Math::Epsilon
            ? weightedCenter / centerWeight
            : newestPosition;
        const double net = Math::Distance(oldestPosition, newestPosition);
        summary.displacementEfficiency = traveled > 5.0
            ? std::clamp(net / traveled, 0.0, 1.0)
            : 1.0;
        const double directionDuration = firstDirectionAge >= 0.0
            ? std::max(0.0, firstDirectionAge - lastDirectionAge)
            : 0.0;
        summary.directionReversalsPerSecond = directionDuration >= 0.18
            ? static_cast<double>(reversals) / directionDuration
            : 0.0;
        summary.directionReversalCount = reversals;
        return summary;
    }

    static MovementModelPolicy Evaluate(const MovementPatternMetrics& metrics) {
        const double direction = std::clamp(metrics.directionStability, 0.0, 1.0);
        const double speed = std::clamp(metrics.speedStability, 0.0, 1.0);
        const double efficiency = std::clamp(metrics.displacementEfficiency, 0.0, 1.0);
        const double changes = std::clamp((metrics.pathChangesPerSecond - 1.0) / 4.0, 0.0, 1.0);
        const double reversals = std::clamp(metrics.directionReversalsPerSecond / 3.0, 0.0, 1.0);
        const double repeated = std::clamp(
            (static_cast<double>(metrics.directionReversalCount) - 1.0) / 2.0,
            0.0,
            1.0);

        double rawJuke = changes * 0.27 + reversals * (0.20 + repeated * 0.25) +
            (1.0 - efficiency) * 0.25 + (1.0 - direction) * 0.18 +
            (1.0 - speed) * 0.05;
        if (metrics.directionReversalCount == 1) rawJuke *= 0.55;
        if (metrics.pathChangesPerSecond < 2.0 && metrics.directionReversalCount == 0) {
            rawJuke *= 0.35;
        }

        MovementModelPolicy policy;
        policy.jukeScore = std::clamp((rawJuke - 0.10) / 0.78, 0.0, 1.0);
        const double pathFreshness = metrics.pathAgeSeconds <= 0.35
            ? 1.0
            : std::clamp(1.0 - (metrics.pathAgeSeconds - 0.35) / 1.4, 0.20, 1.0);
        const double stableTurn = std::clamp(
            (std::abs(metrics.angularVelocity) - 0.06) / 0.75, 0.0, 1.0) *
            direction * efficiency * (1.0 - changes) * (1.0 - policy.jukeScore);
        policy.pathWeight = 0.50 * pathFreshness * (0.65 + direction * 0.35) *
            (1.0 - policy.jukeScore * 0.92);
        policy.velocityWeight = 0.34 * (0.75 + speed * 0.25) + policy.jukeScore * 0.16;
        policy.accelerationWeight = (0.20 + stableTurn * 0.75) *
            (1.0 - policy.jukeScore * 0.92);
        policy.velocityScale = std::clamp(1.0 - policy.jukeScore * 0.70, 0.25, 1.0);
        policy.centerPull = std::clamp((policy.jukeScore - 0.25) / 0.75, 0.0, 1.0) * 0.55;
        policy.displacementScale = 1.15 - policy.jukeScore * 0.45;
        return policy;
    }

    static Math::Vector2 StabilizedPosition(const Math::Vector2& current,
                                            const Math::Vector2& recentCenter,
                                            const MovementModelPolicy& policy) {
        if (!current.IsFinite() || !recentCenter.IsFinite()) return current;
        return Math::Lerp(current, recentCenter, policy.centerPull);
    }

    static Math::Vector2 StabilizedVelocity(const Math::Vector2& velocity,
                                            const MovementModelPolicy& policy) {
        return velocity.IsFinite() ? velocity * policy.velocityScale : Math::Vector2{};
    }

    static Math::Vector2 ClampDisplacement(const Math::Vector2& current,
                                           const Math::Vector2& predicted,
                                           double maximumDistance) {
        if (!current.IsFinite() || !predicted.IsFinite() || maximumDistance <= 0.0) {
            return current;
        }
        const Math::Vector2 offset = predicted - current;
        const double distance = offset.Length();
        if (distance <= maximumDistance || distance <= Math::Epsilon) return predicted;
        return current + offset * (maximumDistance / distance);
    }
};

}
