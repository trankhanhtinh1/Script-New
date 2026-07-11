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
    double pathWeight = 0.0;
    double velocityWeight = 0.0;
    double accelerationWeight = 0.0;
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
            elapsedSeconds < 0.0 || elapsedSeconds > 0.50) {
            return false;
        }
        const double elapsed = std::max(0.001, elapsedSeconds);
        const double physicallyPlausible = std::max(
            minimumJump,
            std::max(0.0, moveSpeed) * elapsed * 2.75 + 45.0);
        return Math::Distance(previous, current) > physicallyPlausible;
    }

    static bool IsHistoryReliable(int sampleCount,
                                  double sampleSpanSeconds,
                                  double visibleSeconds,
                                  double positionStableSeconds) {
        return sampleCount >= 3 && sampleSpanSeconds >= 0.07 &&
            visibleSeconds >= 0.10 && positionStableSeconds >= 0.10;
    }

    static int ActiveDirectionReversalCount(int reversalCount,
                                            double directionStableSeconds) {
        const int count = std::max(0, reversalCount);
        return directionStableSeconds >= 0.45 ? std::min(count, 1) : count;
    }

    static double RequiredDirectionCommitment(double travelTime,
                                              int reversalCount) {
        const double base = 0.08 +
            std::clamp(travelTime, 0.0, 1.5) * 0.09;
        const double repeatedJukeHold = reversalCount >= 2 ? 0.04 : 0.0;
        return std::clamp(base + repeatedJukeHold, 0.08, 0.22);
    }

    static MovementHistorySummary SummarizeHistory(
        const std::vector<MovementHistoryPoint>& points) {
        MovementHistorySummary summary;
        if (points.empty()) return summary;

        Math::Vector2 weightedCenter;
        double centerWeight = 0.0;
        double traveled = 0.0;
        Math::Vector2 previousPosition;
        Math::Vector2 oldestPosition;
        Math::Vector2 newestPosition;
        Math::Vector2 previousDirection;
        double oldestPositionAge = -1.0;
        double newestPositionAge = 1.0;
        double oldestDirectionAge = 0.0;
        double newestDirectionAge = 0.55;
        double reversals = 0.0;
        bool hasPreviousPosition = false;
        bool hasRecentDirection = false;

        for (const MovementHistoryPoint& point : points) {
            const double age = std::max(0.0, point.ageSeconds);
            if (!point.position.IsFinite()) continue;

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
                if (age > oldestPositionAge) {
                    oldestPositionAge = age;
                    oldestPosition = point.position;
                }
                if (age < newestPositionAge) {
                    newestPositionAge = age;
                    newestPosition = point.position;
                }
            }
            if (age > 0.55 || !point.velocity.IsFinite() ||
                point.velocity.Length() < 80.0) continue;

            const Math::Vector2 direction = point.velocity.Normalized();
            if (hasRecentDirection && previousDirection.Dot(direction) < -0.25) {
                reversals += 1.0;
            }
            previousDirection = direction;
            hasRecentDirection = true;
            oldestDirectionAge = std::max(oldestDirectionAge, age);
            newestDirectionAge = std::min(newestDirectionAge, age);
        }

        summary.recentCenter = centerWeight > Math::Epsilon
            ? weightedCenter / centerWeight
            : (hasPreviousPosition ? newestPosition : points.back().position);
        const double net = oldestPositionAge >= 0.0
            ? Math::Distance(oldestPosition, newestPosition)
            : 0.0;
        summary.displacementEfficiency = traveled > 5.0
            ? std::clamp(net / traveled, 0.0, 1.0)
            : 1.0;
        const double recentDuration = oldestDirectionAge - newestDirectionAge;
        summary.directionReversalsPerSecond = recentDuration >= 0.18
            ? reversals / recentDuration
            : 0.0;
        summary.directionReversalCount = static_cast<int>(reversals);
        return summary;
    }

    static MovementModelPolicy Evaluate(const MovementPatternMetrics& metrics) {
        const double direction = std::clamp(metrics.directionStability, 0.0, 1.0);
        const double speed = std::clamp(metrics.speedStability, 0.0, 1.0);
        const double efficiency = std::clamp(metrics.displacementEfficiency, 0.0, 1.0);
        const double changePressure = std::clamp(
            (metrics.pathChangesPerSecond - 1.0) / 4.0, 0.0, 1.0);
        const double repeatedAlternation = std::clamp(
            (static_cast<double>(metrics.directionReversalCount) - 1.0) / 2.0,
            0.0,
            1.0);
        const double reversalPressure = std::clamp(
            metrics.directionReversalsPerSecond / 3.0, 0.0, 1.0) *
            (0.15 + repeatedAlternation * 0.85);

        double rawJuke = changePressure * 0.25 + reversalPressure * 0.35 +
            (1.0 - efficiency) * 0.23 + (1.0 - direction) * 0.12 +
            (1.0 - speed) * 0.05;
        if (metrics.directionReversalCount == 1) rawJuke *= 0.55;
        if (metrics.pathChangesPerSecond < 2.0 &&
            metrics.directionReversalCount == 0) {
            rawJuke *= 0.35;
        }

        MovementModelPolicy policy;
        policy.jukeScore = std::clamp((rawJuke - 0.12) / 0.72, 0.0, 1.0);

        const double pathFreshness = metrics.pathAgeSeconds <= 0.35
            ? 1.0
            : std::clamp(1.0 - (metrics.pathAgeSeconds - 0.35) / 1.2, 0.25, 1.0);
        const double stableTurn = std::clamp(
            (std::abs(metrics.angularVelocity) - 0.06) / 0.75, 0.0, 1.0) *
            direction * efficiency * (1.0 - changePressure) *
            (1.0 - policy.jukeScore);

        policy.pathWeight = 0.50 * pathFreshness * (0.65 + direction * 0.35) *
            (1.0 - policy.jukeScore * 0.92);
        policy.velocityWeight = 0.34 * (0.75 + speed * 0.25) +
            policy.jukeScore * 0.16;
        policy.accelerationWeight = (0.20 + stableTurn * 0.75) *
            (1.0 - policy.jukeScore * 0.92);
        policy.velocityScale = std::clamp(1.0 - policy.jukeScore * 0.70, 0.25, 1.0);
        policy.centerPull = std::clamp(
            (policy.jukeScore - 0.25) / 0.75, 0.0, 1.0) * 0.55;
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
