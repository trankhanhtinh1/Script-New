#pragma once

// Pure Fiddlesticks hitboxes and channel/vision gates. Runtime spell state,
// navmesh and event ownership remain in AIFiddleSticksController.h.
#include "../AIGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace Plugins::KuroAIO::AI::Controllers::FiddleSticks::Geometry {

using Vec3 = ::Vec3;
using SharedGeometry::Direction2D;
using SharedGeometry::ProjectPointToSegment2D;

inline constexpr float kQRange = 575.0f;
inline constexpr float kQFearRadius = 80.0f;
inline constexpr float kQDelay = 0.25f;
inline constexpr float kWRange = 650.0f;
inline constexpr float kWDrainRadius = 650.0f;
inline constexpr float kWChannelSeconds = 2.0f;
inline constexpr float kEDistance = 850.0f;
inline constexpr float kEHalfAngleRadians = 0.58f;
inline constexpr float kEHalfWidth = 105.0f;
inline constexpr float kEDelay = 0.25f;
inline constexpr float kRChannelSeconds = 1.50f;
inline constexpr float kRRange = 800.0f;
inline constexpr float kRStormRadius = 600.0f;
inline constexpr float kRDelay = 0.25f;

inline float RankValue(int rank, const std::array<float, 5>& values) {
    return values[static_cast<std::size_t>(std::clamp(rank, 1, 5) - 1)];
}

inline float QCurrentHealthDamage(int rank, float currentHealth) {
    return std::max(0.0f, currentHealth) *
        RankValue(rank, {0.07f, 0.08f, 0.09f, 0.10f, 0.11f});
}

inline float QRepeatCurrentHealthDamage(int rank, float currentHealth) {
    return std::max(0.0f, currentHealth) *
        RankValue(rank, {0.04f, 0.045f, 0.05f, 0.055f, 0.06f});
}

inline float WMissingHealthExecute(int rank, float missingHealth) {
    return std::max(0.0f, missingHealth) *
        RankValue(rank, {0.06f, 0.07f, 0.08f, 0.09f, 0.10f});
}

inline float ESlowFraction(int rank) {
    return RankValue(rank, {0.30f, 0.35f, 0.40f, 0.45f, 0.50f});
}

inline float RRawDamagePerSecond(int rank, float abilityPower) {
    return RankValue(rank, {37.5f, 62.5f, 87.5f}) +
        0.225f * std::max(0.0f, abilityPower);
}

inline Vec3 ClampRDestination(const Vec3& origin, const Vec3& requested) {
    if (!origin.IsValid() || !requested.IsValid()) return {};
    const Vec3 direction = Direction2D(origin, requested);
    if (direction.IsZero()) return {};
    return origin + direction * std::min(kRRange, origin.Distance2D(requested));
}

inline bool InDrainRadius(const Vec3& center, const Vec3& target,
                          float targetRadius = 0.0f) {
    return center.IsValid() && target.IsValid() &&
        center.Distance2D(target) <= kWDrainRadius + std::max(0.0f, targetRadius);
}

inline int CountDrainTargets(const Vec3& center,
                             const std::array<Vec3, 8>& targets,
                             int count, float radius = kWDrainRadius) {
    if (!center.IsValid()) return 0;
    int hits = 0;
    const int limit = std::clamp(count, 0, static_cast<int>(targets.size()));
    for (int i = 0; i < limit; ++i) {
        if (targets[static_cast<std::size_t>(i)].IsValid() &&
            center.Distance2D(targets[static_cast<std::size_t>(i)]) <= radius) ++hits;
    }
    return hits;
}

struct FearGate {
    bool TargetValid = false;
    bool ActivationReady = false;
    bool UnseenOrEffigy = false;
    bool RecentlyFeared = false;
    bool SpellShielded = false;
    bool Lethal = false;
};

inline bool CanFear(const FearGate& gate) {
    return gate.TargetValid && gate.ActivationReady && !gate.SpellShielded &&
        (!gate.RecentlyFeared || gate.Lethal) && gate.UnseenOrEffigy;
}

struct DrainGate {
    bool Ready = false;
    bool Owned = false;
    bool ChannelActive = false;
    bool Interrupted = false;
    bool VisibleToEnemy = false;
    bool InBrush = false;
    bool VisionSafe = false;
    bool TargetInRadius = false;
    bool Lethal = false;
    bool Emergency = false;
    int NearbyTargets = 0;
    int MinimumTargets = 1;
};

inline bool CanStartDrain(const DrainGate& gate) {
    return gate.Ready && !gate.Owned && !gate.Interrupted &&
        !gate.VisibleToEnemy && (gate.InBrush || gate.VisionSafe) &&
        gate.TargetInRadius &&
        (gate.Lethal || gate.Emergency ||
         gate.NearbyTargets >= std::max(1, gate.MinimumTargets));
}

inline bool ShouldContinueDrain(const DrainGate& gate) {
    return gate.Owned && gate.ChannelActive && !gate.Interrupted &&
        !gate.VisibleToEnemy && (gate.InBrush || gate.VisionSafe) &&
        gate.TargetInRadius;
}

inline bool ShouldFinishDrain(const DrainGate& gate, float elapsedSeconds) {
    if (!gate.Owned || !gate.ChannelActive || gate.Interrupted ||
        gate.VisibleToEnemy || !gate.TargetInRadius) return false;
    return elapsedSeconds >= kWChannelSeconds - 0.08f || gate.Lethal || gate.Emergency;
}

inline Vec3 ConeDirection(const Vec3& origin, const Vec3& requested) {
    return Direction2D(origin, requested);
}

inline bool InReapCone(const Vec3& origin, const Vec3& requested,
                       const Vec3& target, float targetRadius = 0.0f) {
    if (!origin.IsValid() || !requested.IsValid() || !target.IsValid()) return false;
    const Vec3 direction = ConeDirection(origin, requested);
    if (direction.IsZero()) return false;
    const Vec3 relative = target - origin;
    const float forward = relative.x * direction.x + relative.z * direction.z;
    const float lateral = std::fabs(SharedGeometry::Cross2D(direction, relative));
    const float radius = std::max(0.0f, targetRadius);
    return forward >= -radius && forward <= kEDistance + radius &&
        lateral <= kEHalfWidth + radius &&
        forward >= std::max(-radius, lateral / std::max(0.01f, std::tan(kEHalfAngleRadians)) - radius);
}

struct ReapAim {
    bool Valid = false;
    bool CenterHit = false;
    bool WallBlocked = false;
    bool PredictionAccepted = false;
    float Forward = 0.0f;
    float Lateral = 0.0f;
};

inline ReapAim EvaluateReap(const Vec3& origin, const Vec3& requested,
                            const Vec3& target, float targetRadius,
                            bool wallBlocked, bool predictionAccepted) {
    if (!origin.IsValid() || !requested.IsValid() || !target.IsValid()) return {};
    const Vec3 direction = ConeDirection(origin, requested);
    if (direction.IsZero()) return {};
    const Vec3 relative = target - origin;
    const float forward = relative.x * direction.x + relative.z * direction.z;
    const float lateral = std::fabs(SharedGeometry::Cross2D(direction, relative));
    const bool inside = InReapCone(origin, requested, target, targetRadius);
    const bool center = inside && lateral <= kEHalfWidth * 0.52f + targetRadius;
    return {inside, center, wallBlocked, predictionAccepted, forward, lateral};
}

struct TeleportGate {
    bool Ready = false;
    bool ControllerOwned = false;
    bool ChannelActive = false;
    bool Interrupted = false;
    bool DestinationValid = false;
    bool DestinationVisible = false;
    bool BrushOrFog = false;
    bool WallBlocked = false;
    bool TurretRisk = false;
    bool UnsafeMobility = false;
    bool HasPredictedVictim = false;
    bool Lethal = false;
    bool Flee = false;
    int PredictedVictims = 0;
    int MinimumVictims = 1;
    int MaximumEnemies = 2;
};

inline bool CanStartCrowstorm(const TeleportGate& gate) {
    if (!gate.Ready || gate.ControllerOwned || gate.ChannelActive || gate.Interrupted ||
        !gate.DestinationValid || gate.DestinationVisible || !gate.BrushOrFog ||
        gate.WallBlocked || gate.TurretRisk || gate.UnsafeMobility ||
        !gate.HasPredictedVictim) return false;
    if (gate.Flee) return true;
    return gate.Lethal || gate.PredictedVictims >= std::max(1, gate.MinimumVictims);
}

inline bool CanTeleportStorm(const TeleportGate& gate) {
    if (!gate.ControllerOwned || !gate.ChannelActive || gate.Interrupted ||
        !gate.DestinationValid || gate.WallBlocked || gate.UnsafeMobility) return false;
    return !gate.TurretRisk && (gate.Flee || gate.HasPredictedVictim);
}

inline bool SafeChannelVisibility(bool inBrush, bool destinationFog,
                                  bool currentlyVisible, bool effigyReady) {
    return !currentlyVisible && (inBrush || destinationFog || effigyReady);
}

} // namespace Plugins::KuroAIO::AI::Controllers::FiddleSticks::Geometry
