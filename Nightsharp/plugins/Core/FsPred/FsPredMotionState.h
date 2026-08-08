#pragma once

#include "../../../sdk/Enumerations/HitChance.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <span>

namespace Plugins::FsPred {

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

    // CachedWaypoints()[0] is the moving server-position prefix. Derive a
    // heading only from the stable suffix, so normal movement cannot look like
    // a fresh path every frame.
    if (path.size() < 3) {
        return intent;
    }
    const Point& start = path[1];
    for (std::size_t index = 2; index < path.size(); ++index) {
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

inline SDK::HitChance ClassifyMotion(const MotionFacts& facts) {
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
    if (facts.PathAgeMs < 140.0 ||
        (facts.ReversalAngleDegrees > 100.0f &&
         facts.ReversalAgeMs <= 400.0)) {
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
