#pragma once

#include "FsPredPolicy.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <span>

namespace Plugins::FsPred {

inline constexpr double kEvasiveTurnWindowMs = 400.0;

struct MotionFacts {
    bool IsDashing = false;
    bool IsRecalling = false;
    bool HasHardCrowdControl = false;
    bool CanMove = true;
    bool BecameVisible = false;
    double VisibleAgeMs = std::numeric_limits<double>::infinity();
    bool IsCasting = false;
    bool IsMoving = false;
    double StopAgeMs = std::numeric_limits<double>::infinity();
    double PathAgeMs = std::numeric_limits<double>::infinity();
    float ReversalAngleDegrees = 0.0f;
    double ReversalAgeMs = std::numeric_limits<double>::infinity();
    bool HasPath = false;
    bool NearPathEnd = false;
    bool HasStableHeading = false;
};

struct PredictionHorizonFacts {
    float ArrivalSeconds = std::numeric_limits<float>::infinity();
    float ProjectileTravelSeconds = std::numeric_limits<float>::infinity();
    float CastDistance = 0.0f;
    float SpellRange = std::numeric_limits<float>::infinity();
    float MoveSpeed = 0.0f;
    float EffectiveRadius = 0.0f;
};

struct StablePathIntent {
    float DestinationX = 0.0f;
    float DestinationZ = 0.0f;
    float HeadingX = 0.0f;
    float HeadingZ = 0.0f;
    bool HasDestination = false;
    bool HasHeading = false;
};

template <typename Point>
inline StablePathIntent ExtractStablePathIntent(
    std::span<const Point> path) {
    StablePathIntent intent{};
    if (path.empty()) {
        return intent;
    }

    const Point& destination = path.back();
    if (!std::isfinite(destination.x) || !std::isfinite(destination.z)) {
        return intent;
    }
    intent.DestinationX = destination.x;
    intent.DestinationZ = destination.z;
    intent.HasDestination = true;

    // CachedWaypoints()[0] is a moving server-position prefix. Prefer the
    // stable suffix when it exists. A direct click only has prefix+destination,
    // so use that sole segment; destination de-duplication prevents the moving
    // prefix from being recorded as a fresh intent every frame.
    const std::size_t startIndex = path.size() >= 3 ? 1 : 0;
    const Point& start = path[startIndex];
    for (std::size_t index = startIndex + 1; index < path.size(); ++index) {
        const float dx = path[index].x - start.x;
        const float dz = path[index].z - start.z;
        const float lengthSquared = dx * dx + dz * dz;
        if (!std::isfinite(lengthSquared) || lengthSquared <= 1.0f) {
            continue;
        }
        const float inverseLength = 1.0f / std::sqrt(lengthSquared);
        intent.HeadingX = dx * inverseLength;
        intent.HeadingZ = dz * inverseLength;
        intent.HasHeading = true;
        break;
    }
    return intent;
}

template <typename Point>
inline StablePathIntent ExtractStablePathIntent(
    std::span<const Point> path,
    const Point& origin) {
    StablePathIntent intent = ExtractStablePathIntent(path);
    if (!intent.HasDestination || intent.HasHeading ||
        !std::isfinite(origin.x) || !std::isfinite(origin.z)) {
        return intent;
    }

    const float dx = intent.DestinationX - origin.x;
    const float dz = intent.DestinationZ - origin.z;
    const float lengthSquared = dx * dx + dz * dz;
    if (!std::isfinite(lengthSquared) || lengthSquared <= 1.0f) {
        return intent;
    }
    const float inverseLength = 1.0f / std::sqrt(lengthSquared);
    intent.HeadingX = dx * inverseLength;
    intent.HeadingZ = dz * inverseLength;
    intent.HasHeading = true;
    return intent;
}

inline bool SameStableDestination(const StablePathIntent& left,
                                  const StablePathIntent& right) {
    if (left.HasDestination != right.HasDestination) {
        return false;
    }
    if (!left.HasDestination) {
        return true;
    }
    const float dx = left.DestinationX - right.DestinationX;
    const float dz = left.DestinationZ - right.DestinationZ;
    return dx * dx + dz * dz <= 1.0f;
}


inline bool SameStablePathIntent(const StablePathIntent& left,
                                 const StablePathIntent& right) {
    if (left.HasDestination != right.HasDestination ||
        left.HasHeading != right.HasHeading) {
        return false;
    }
    if (!left.HasDestination) {
        return true;
    }

    const float dx = left.DestinationX - right.DestinationX;
    const float dz = left.DestinationZ - right.DestinationZ;
    if (dx * dx + dz * dz > 1.0f) {
        return false;
    }
    if (!left.HasHeading) {
        return true;
    }
    return left.HeadingX * right.HeadingX +
               left.HeadingZ * right.HeadingZ >=
           0.999f;
}

inline bool HasRecentEvasiveTurn(
    const MotionFacts& facts,
    const AntiBaitWeights& weights) {
    const float turnAngle = static_cast<float>(std::clamp(
        weights.EvasiveTurnAngleDegrees,
        45,
        90));
    return facts.ReversalAngleDegrees >= turnAngle &&
           facts.ReversalAgeMs <= kEvasiveTurnWindowMs;
}

inline SDK::HitChance ClassifyMotion(
    const MotionFacts& facts,
    const AntiBaitWeights& weights,
    bool antiBaitEnabled) {
    if (facts.IsDashing) {
        return SDK::HitChance::Dash;
    }
    if (facts.IsRecalling) {
        return SDK::HitChance::VeryHigh;
    }
    if (facts.HasHardCrowdControl || !facts.CanMove) {
        return SDK::HitChance::Immobile;
    }
    if (facts.BecameVisible && facts.VisibleAgeMs < 100.0) {
        return SDK::HitChance::Medium;
    }
    if (facts.IsCasting) {
        return SDK::HitChance::High;
    }
    if (!facts.IsMoving) {
        return facts.StopAgeMs < 100.0
            ? SDK::HitChance::Medium
            : SDK::HitChance::VeryHigh;
    }

    const bool recentEvasiveTurn =
        antiBaitEnabled &&
        HasRecentEvasiveTurn(facts, weights);
    if (facts.PathAgeMs < 140.0 || recentEvasiveTurn) {
        return SDK::HitChance::Low;
    }
    if (!facts.HasPath || facts.NearPathEnd) {
        return SDK::HitChance::Medium;
    }
    if (facts.HasStableHeading && facts.PathAgeMs >= 300.0) {
        return SDK::HitChance::High;
    }
    return SDK::HitChance::Medium;
}

inline bool IsFreelyMoving(const MotionFacts& facts) {
    return facts.IsMoving &&
           facts.CanMove &&
           !facts.IsDashing &&
           !facts.IsRecalling &&
           !facts.HasHardCrowdControl &&
           !facts.IsCasting;
}

inline SDK::HitChance ApplyPredictionHorizonConfidence(
    SDK::HitChance confidence,
    const MotionFacts& motion,
    const PredictionHorizonFacts& horizon,
    const AntiBaitWeights& weights,
    bool antiBaitEnabled,
    bool ignoreMaxRangePenalty,
    float openEscapeFraction = 1.0f) {
    if (!antiBaitEnabled ||
        confidence < SDK::HitChance::Medium ||
        !IsFreelyMoving(motion)) {
        return confidence;
    }

    const bool hasFiniteRange =
        std::isfinite(horizon.SpellRange) &&
        horizon.SpellRange > 0.0f &&
        horizon.SpellRange < std::numeric_limits<float>::max();
    SDK::HitChance adjustedConfidence = confidence;
    const float maxRangeRatio = static_cast<float>(std::clamp(
        weights.MaxRangeThresholdPercent,
        70,
        100)) / 100.0f;
    const bool nearMaximumRange =
        !ignoreMaxRangePenalty &&
        hasFiniteRange &&
        std::isfinite(horizon.CastDistance) &&
        horizon.CastDistance >= horizon.SpellRange * maxRangeRatio;
    if (nearMaximumRange) {
        adjustedConfidence = std::min(
            adjustedConfidence,
            SDK::HitChance::Medium);
    }

    if (!std::isfinite(horizon.ArrivalSeconds) ||
        horizon.ArrivalSeconds < 0.0f ||
        !std::isfinite(horizon.MoveSpeed) ||
        horizon.MoveSpeed <= 0.0f) {
        return std::min(adjustedConfidence, SDK::HitChance::Medium);
    }

    const float reactionFloorSeconds =
        static_cast<float>(std::clamp(
            weights.ReactionFloorMs,
            100,
            250)) / 1000.0f;
    const float responseWindow = std::max(
        0.0f,
        horizon.ArrivalSeconds - reactionFloorSeconds);
    const float steeringDistance = responseWindow * horizon.MoveSpeed;
    const float hitEnvelope = std::max(
        100.0f,
        std::max(0.0f, horizon.EffectiveRadius) * 1.25f);

    const float impactThresholdSeconds =
        static_cast<float>(std::clamp(
            weights.LongImpactHorizonMs,
            300,
            1000)) / 1000.0f;
    const float flightThresholdSeconds =
        static_cast<float>(std::clamp(
            weights.LongProjectileFlightMs,
            300,
            1000)) / 1000.0f;
    const bool longHorizon =
        horizon.ArrivalSeconds >= impactThresholdSeconds;
    const bool longFlight =
        !std::isfinite(horizon.ProjectileTravelSeconds) ||
        horizon.ProjectileTravelSeconds >= flightThresholdSeconds;
    const bool longDistance =
        std::isfinite(horizon.CastDistance) &&
        horizon.CastDistance >= static_cast<float>(std::clamp(
            weights.LongCastDistance,
            400,
            2000));
    const bool extremeHorizon =
        horizon.ArrivalSeconds >= impactThresholdSeconds * 2.0f ||
        !std::isfinite(horizon.ProjectileTravelSeconds) ||
        horizon.ProjectileTravelSeconds >= flightThresholdSeconds * 2.0f;
    // Relaxed combo callers accept Medium. Multiple exposure signals must
    // therefore cross that gate instead of merely capping confidence at it.
    const int riskFactorCount =
        static_cast<int>(nearMaximumRange) +
        static_cast<int>(longHorizon) +
        static_cast<int>(longFlight) +
        static_cast<int>(longDistance);

    const float openFraction = weights.TerrainCorridorBoost &&
                               !HasRecentEvasiveTurn(motion, weights)
        ? std::clamp(openEscapeFraction, 0.0f, 1.0f)
        : 1.0f;
    const float effectiveSteeringDistance = extremeHorizon
        ? steeringDistance
        : steeringDistance * openFraction;
    const bool hasSteeringOpportunity =
        effectiveSteeringDistance >= hitEnvelope;

    if (hasSteeringOpportunity &&
        (extremeHorizon || riskFactorCount >= 2)) {
        return std::min(adjustedConfidence, SDK::HitChance::Low);
    }
    return hasSteeringOpportunity && riskFactorCount > 0
        ? std::min(adjustedConfidence, SDK::HitChance::Medium)
        : adjustedConfidence;
}

inline bool IsMovementLockType(int type) {
    // EnsoulSharp prediction BuffType values.
    switch (type) {
    case 23: // Charm
    case 30: // Knockup
    case 5:  // Stun
    case 25: // Suppression
    case 12: // Snare
    case 22: // Fear
    case 8:  // Taunt
    case 31: // Knockback
    case 35: // Asleep
        return true;
    default:
        return false;
    }
}

template <typename Snapshot>
inline double RemainingImmobilitySeconds(const Snapshot* snapshot,
                                         float gameTime) {
    if (!snapshot || !std::isfinite(gameTime)) {
        return -1.0;
    }

    double maxEndTime = 0.0;
    for (int index = 0; index < snapshot->count; ++index) {
        const auto& entry = snapshot->entries[index];
        if (!entry.isActive || !IsMovementLockType(entry.type)) {
            continue;
        }
        if (gameTime <= entry.endTime && entry.endTime > maxEndTime) {
            maxEndTime = entry.endTime;
        }
    }
    return maxEndTime > 0.0
        ? maxEndTime - static_cast<double>(gameTime)
        : -1.0;
}

} // namespace Plugins::FsPred
