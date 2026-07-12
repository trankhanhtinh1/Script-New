#pragma once

#include "../Core/PredictionConfig.h"
#include "../Tracking/MovementTracker.h"
#include "../../../sdk/SDK.h"

#include <algorithm>
#include <cmath>

namespace ZDPrediction {

struct ConfidenceContext {
    const SDK::AIBaseClient* unit = nullptr;
    const MovementSnapshot* movement = nullptr;
    Math::Vector2 source = {};
    Math::Vector2 predicted = {};
    double travelTime = 0.0;
    double effectiveMoveSpeed = 0.0;
    double radius = 0.0;
    double reactionTime = 0.0;
    double wallRestriction = 0.0;
    double jukeScore = 0.0;
    bool instantProjectile = false;
    bool historyLimited = false;
};

class ConfidenceEvaluator {
public:
    static double Score(const ConfidenceContext& context) {
        if (!context.unit || !context.movement || !context.movement->valid) return 0.0;
        const MovementSnapshot& movement = *context.movement;
        if (!context.unit->IsHero()) return movement.moving ? 0.82 : 0.96;

        double score = 0.34;
        score += 0.20 * std::clamp(movement.directionStability, 0.0, 1.0);
        score += 0.10 * std::clamp(movement.speedStability, 0.0, 1.0);
        score += 0.04 * std::min(4.0, movement.repeatedDestinationCount);
        score -= 0.025 * std::min(4.0, movement.pathChangesPerSecond);
        score -= 0.035 * std::min(3.0, movement.directionReversalsPerSecond);

        if (movement.pathAgeSeconds <= 0.14) score += 0.08;
        else if (movement.pathAgeSeconds > 0.90) score -= 0.08;
        if (movement.visibleSeconds > 0.0 && movement.visibleSeconds < 0.12) score -= 0.18;
        if (context.historyLimited || !movement.historyReliable) score -= 0.14;
        if (movement.positionDiscontinuity) score -= 0.25;
        if (!movement.moving) {
            score += movement.stationarySeconds >= context.reactionTime ? 0.30 : 0.12;
        }

        const double escapeDistance = context.effectiveMoveSpeed *
            std::max(0.0, context.reactionTime + context.travelTime * 0.12);
        const double tolerance = context.radius / std::max(1.0, context.radius + escapeDistance);
        score += 0.22 * tolerance;
        score -= std::clamp(context.travelTime - 0.35, 0.0, 2.5) * 0.085;
        if (context.instantProjectile) score += 0.08;
        score += context.wallRestriction * 0.15;

        const Math::Vector2 radial = (movement.position - context.source).Normalized();
        const Math::Vector2 direction = movement.velocity.Normalized();
        if (!radial.IsZero() && !direction.IsZero()) {
            score += std::abs(radial.Dot(direction)) * 0.08;
        }

        const double sourceDistance = Math::Distance(context.source, movement.position);
        if (sourceDistance < 350.0) score += 0.08;
        else if (sourceDistance > 1600.0) score -= 0.08;

        const double acceleration = movement.acceleration.Length();
        if (acceleration > 800.0) score -= 0.10;
        const double juke = std::clamp(context.jukeScore, 0.0, 1.0);
        const double exposure = std::clamp((context.travelTime - 0.20) / 0.80, 0.0, 1.0);
        score -= juke * (0.10 + exposure * (movement.directionReversalCount >= 2 ? 0.22 : 0.08));
        return std::clamp(score, 0.0, 1.0);
    }

    static SDK::HitChance ToHitChance(double score, const PredictionConfig& config) {
        if (score >= config.veryHighThreshold) return SDK::HitChance::VeryHigh;
        if (score >= config.highThreshold) return SDK::HitChance::High;
        if (score >= std::max(0.34f, config.highThreshold - 0.22f)) return SDK::HitChance::Medium;
        if (score > 0.18) return SDK::HitChance::Low;
        return SDK::HitChance::None;
    }
};

}
