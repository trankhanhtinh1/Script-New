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
};

class ConfidenceEvaluator {
public:
    static double Score(const ConfidenceContext& context) {
        if (!context.unit || !context.movement || !context.movement->valid) return 0.0;
        const MovementSnapshot& movement = *context.movement;
        if (!context.unit->IsHero()) return 0.95;

        double score = 0.38;
        score += 0.18 * movement.directionStability;
        score += 0.10 * movement.speedStability;
        score += 0.035 * static_cast<double>(std::min(4, movement.repeatedDestinationCount)) *
            (1.0 - std::clamp(context.jukeScore, 0.0, 1.0));
        score -= 0.020 * std::min(4.0, movement.pathChangesPerSecond);

        if (movement.pathAgeSeconds <= 0.12) score += 0.10;
        else if (movement.pathAgeSeconds > 0.8) score -= 0.08;
        if (movement.visibleSeconds > 0.0 && movement.visibleSeconds < 0.12) score -= 0.18;
        if (!movement.moving) {
            score += movement.stationarySeconds >= context.reactionTime ? 0.30 : 0.12;
        }

        const double escapeDistance = context.effectiveMoveSpeed *
            std::max(0.0, context.reactionTime + context.travelTime * 0.12);
        const double tolerance = context.radius / std::max(1.0, context.radius + escapeDistance);
        score += 0.22 * tolerance;
        score -= std::clamp(context.travelTime - 0.35, 0.0, 2.5) * 0.085;
        if (context.instantProjectile) score += 0.07;
        score += context.wallRestriction * 0.16;

        const Math::Vector2 radial = (movement.position - context.source).Normalized();
        const Math::Vector2 direction = movement.velocity.Normalized();
        if (!radial.IsZero() && !direction.IsZero()) {
            const double radialAlignment = std::abs(radial.Dot(direction));
            score += radialAlignment * 0.10;
        }

        const double sourceDistance = Math::Distance(context.source, movement.position);
        if (sourceDistance < 350.0) score += 0.10;
        else if (sourceDistance > 1600.0) score -= 0.08;

        const double acceleration = movement.acceleration.Length();
        if (acceleration > 800.0) score -= 0.10;
        const double juke = std::clamp(context.jukeScore, 0.0, 1.0);
        const double travelExposure = std::clamp(
            (context.travelTime - 0.20) / 0.80, 0.0, 1.0);
        const double repeatedFactor = movement.directionReversalCount >= 2 ? 1.0 : 0.35;
        score -= juke * (0.10 + travelExposure * repeatedFactor * 0.22);
        return std::clamp(score, 0.0, 1.0);
    }

    static SDK::HitChance ToHitChance(double score, const PredictionConfig& config) {
        if (score >= config.veryHighThreshold) return SDK::HitChance::VeryHigh;
        if (score >= config.highThreshold) return SDK::HitChance::High;
        if (score >= std::max(0.35f, config.highThreshold - 0.22f)) return SDK::HitChance::Medium;
        return score > 0.18 ? SDK::HitChance::Low : SDK::HitChance::None;
    }
};

}
