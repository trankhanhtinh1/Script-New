#pragma once

#include "../../AIGeometry.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cmath>
#include <cstddef>

namespace Plugins::KuroAIO::AI::Controllers::Warwick::Geometry {

using Vec3 = ::Vec3;
using SharedGeometry::Direction2D;

enum class QState : std::uint8_t { Ready, Holding, ReleasePending };
enum class EState : std::uint8_t { Ready, Reduction, RecastPending };
enum class RState : std::uint8_t { Ready, CastPending, Suppressing };

enum class BloodScentTier : std::uint8_t { None, Hunted, Frenzied };

inline constexpr float kQCastRange = 365.0f;
inline constexpr float kQFollowRange = 425.0f;
inline constexpr float kQWidth = 55.0f;
inline constexpr int kQChannelMs = 500;
inline constexpr int kQMinimumHoldMs = 220;
inline constexpr int kQReleaseWindowMs = 620;
inline constexpr float kWFirstThresholdPercent = 50.0f;
inline constexpr float kWSecondThresholdPercent = 25.0f;
inline constexpr float kERange = 375.0f;
inline constexpr int kEReductionMs = 2750;
inline constexpr int kEFearDelayMs = 1000;
inline constexpr int kEFearDurationMs = 1000;
inline constexpr float kRBaseRange = 2500.0f;
inline constexpr float kRWidth = 125.0f;
inline constexpr int kRSuppressionMs = 1500;
inline constexpr float kRMaxBonusRange = 1000.0f;

inline constexpr float RankValue(int rank, const std::array<float, 5>& values) {
    return values[static_cast<std::size_t>(std::clamp(rank, 1, 5) - 1)];
}

inline constexpr float QTargetHealthDamagePercent(int rank) {
    return RankValue(rank, {6.0f, 7.0f, 8.0f, 9.0f, 10.0f});
}

inline constexpr float QRawDamage(int rank, float totalAttackDamage, float abilityPower,
                                  float targetMaxHealth) {
    return std::max(0.0f, targetMaxHealth) * QTargetHealthDamagePercent(rank) / 100.0f +
           1.20f * std::max(0.0f, totalAttackDamage) + std::max(0.0f, abilityPower);
}

inline constexpr float QHealPercent(int rank) {
    return RankValue(rank, {12.5f, 25.0f, 37.5f, 50.0f, 62.5f});
}

inline constexpr float QRawHeal(int rank, float damageDealt) {
    return std::max(0.0f, damageDealt) * QHealPercent(rank) / 100.0f;
}

inline bool QReachable(const Vec3& origin, const Vec3& target,
                      float targetRadius = 0.0f, bool held = false) {
    if (!origin.IsValid() || !target.IsValid() || origin.IsZero() || target.IsZero()) return false;
    const float range = held ? kQFollowRange : kQCastRange;
    return origin.Distance2D(target) <= range + std::max(0.0f, targetRadius);
}

inline bool QReleaseAllowed(QState state, int elapsedMs, float targetHealthPercent,
                           bool targetOutOfRange, bool lethal, bool playerLow) {
    if (state != QState::Holding || elapsedMs < kQMinimumHoldMs) return false;
    if (targetOutOfRange) return true;
    if (lethal || playerLow) return true;
    (void)targetHealthPercent;
    return elapsedMs >= kQChannelMs;
}

inline BloodScentTier BloodScentForTarget(float healthPercent) {
    if (!std::isfinite(healthPercent) || healthPercent > kWFirstThresholdPercent) {
        return BloodScentTier::None;
    }
    return healthPercent <= kWSecondThresholdPercent
        ? BloodScentTier::Frenzied : BloodScentTier::Hunted;
}

inline bool BloodScentAllows(float targetHealthPercent, float playerHealthPercent,
                             bool activeCast, bool combatMode) {
    const BloodScentTier tier = BloodScentForTarget(targetHealthPercent);
    if (tier != BloodScentTier::None) return true;
    return activeCast && combatMode && playerHealthPercent <= 45.0f;
}

inline constexpr float EDamageReductionPercent(int rank) {
    return RankValue(rank, {35.0f, 40.0f, 45.0f, 50.0f, 55.0f});
}

inline bool ERecastAllowed(EState state, int elapsedMs, int nearbyEnemies,
                           bool threatened, bool fleeing) {
    if (state != EState::Reduction || elapsedMs < kEFearDelayMs || nearbyEnemies <= 0) {
        return false;
    }
    return threatened || fleeing || elapsedMs >= kEReductionMs - 250;
}

inline float RReach(float movementSpeed) {
    const float bonus = std::max(0.0f, movementSpeed - 325.0f);
    return kRBaseRange + std::min(kRMaxBonusRange, bonus * 1.5f);
}

inline bool SegmentHits(const Vec3& start, const Vec3& end, const Vec3& target,
                        float halfWidth, float targetRadius = 0.0f) {
    if (!start.IsValid() || !end.IsValid() || !target.IsValid() ||
        start.IsZero() || end.IsZero() || target.IsZero()) return false;
    const auto projection = SharedGeometry::ProjectPointToSegment2D(target, start, end);
    return projection.T >= 0.0f && projection.T <= 1.0f &&
           projection.Distance <= std::max(0.0f, halfWidth) + std::max(0.0f, targetRadius);
}

struct RCollisionTarget {
    int NetworkId = 0;
    Vec3 Position{};
    float Radius = 0.0f;
    bool Valid = false;
};

inline int FirstRCollision(const Vec3& start, const Vec3& end,
                           const std::array<RCollisionTarget, 8>& targets) {
    if (!start.IsValid() || !end.IsValid() || start.IsZero() || end.IsZero()) return 0;
    int result = 0;
    float closest = 2.0f;
    for (const auto& candidate : targets) {
        if (!candidate.Valid || candidate.NetworkId == 0 ||
            !SegmentHits(start, end, candidate.Position, kRWidth * 0.5f, candidate.Radius)) continue;
        const auto projection = SharedGeometry::ProjectPointToSegment2D(candidate.Position, start, end);
        if (projection.T < closest) {
            closest = projection.T;
            result = candidate.NetworkId;
        }
    }
    return result;
}

inline bool RCommitAllowed(bool targetValid, bool targetProtected, bool endpointWall,
                           bool endpointTurret, int enemiesAtEndpoint, int maximumEnemies,
                           bool lethal, bool playerLow, bool fleeing) {
    if (!targetValid || targetProtected || endpointWall || endpointTurret) return false;
    if (lethal || playerLow || fleeing) return true;
    return enemiesAtEndpoint <= std::max(0, maximumEnemies);
}

inline bool RHealingWorthwhile(float playerHealthPercent, float targetHealthPercent,
                               float predictedDamage, bool targetSuppressed) {
    if (targetSuppressed || predictedDamage <= 0.0f) return false;
    return playerHealthPercent <= 45.0f || targetHealthPercent <= 35.0f;
}

} // namespace Plugins::KuroAIO::AI::Controllers::Warwick::Geometry
