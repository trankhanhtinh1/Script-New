#pragma once

// Pure Sion mechanics and geometry.  Runtime prediction, navmesh queries and
// spell ownership stay in AISionController; this header is safe to include
// from a standalone boundary test.
#include "../../AIGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace Plugins::KuroAIO::AI::Controllers::Sion::Geometry {

using Vec3 = ::Vec3;
using SharedGeometry::Direction2D;
using SharedGeometry::ProjectPointToSegment2D;

inline constexpr float kQRange = 750.0f;
inline constexpr float kQHalfWidthMin = 140.0f;
inline constexpr float kQHalfWidthMax = 350.0f;
inline constexpr float kQMaximumChargeSeconds = 2.0f;
inline constexpr float kQMinimumChargeSeconds = 0.10f;
inline constexpr float kQDelay = 0.40f;
inline constexpr float kWRange = 500.0f;
inline constexpr float kWRadius = 400.0f;
inline constexpr float kWShieldSeconds = 6.0f;
inline constexpr float kERange = 800.0f;
inline constexpr float kEHalfWidth = 70.0f;
inline constexpr float kESpeed = 1800.0f;
inline constexpr float kRMaximumRange = 3000.0f;
inline constexpr float kRHalfWidth = 160.0f;
inline constexpr float kRRadius = 160.0f;
inline constexpr float kRSpeed = 950.0f;
inline constexpr float kRMaximumSeconds = 8.0f;
inline constexpr float kPassiveLifetimeSeconds = 60.0f;

inline constexpr float RankValue(int rank, const std::array<float, 5>& values) {
    return values[static_cast<std::size_t>(std::clamp(rank, 1, 5) - 1)];
}

inline constexpr float ChargeFraction(float chargeSeconds) {
    return std::clamp(chargeSeconds, 0.0f, kQMaximumChargeSeconds) /
           kQMaximumChargeSeconds;
}

inline constexpr float QRawDamage(int rank, float chargeSeconds,
                                  float totalAttackDamage) {
    const float minimum = RankValue(rank, {40.0f, 60.0f, 80.0f, 100.0f, 120.0f}) +
        RankValue(rank, {0.40f, 0.45f, 0.50f, 0.55f, 0.60f}) *
            std::max(0.0f, totalAttackDamage);
    const float maximum = RankValue(rank, {90.0f, 135.0f, 180.0f, 225.0f, 270.0f}) +
        RankValue(rank, {1.35f, 1.35f, 1.35f, 1.35f, 1.35f}) *
            std::max(0.0f, totalAttackDamage);
    return minimum + (maximum - minimum) * ChargeFraction(chargeSeconds);
}

inline constexpr float QKnockupSeconds(int rank, float chargeSeconds) {
    const float minimum = 0.25f;
    const float maximum = RankValue(rank, {1.25f, 1.50f, 1.75f, 2.00f, 2.25f});
    return minimum + (maximum - minimum) * ChargeFraction(chargeSeconds);
}

inline constexpr float WShieldRaw(int rank, float abilityPower,
                                  float maximumHealth) {
    return RankValue(rank, {60.0f, 85.0f, 110.0f, 135.0f, 160.0f}) +
        0.40f * std::max(0.0f, abilityPower) +
        RankValue(rank, {0.08f, 0.09f, 0.10f, 0.11f, 0.12f}) *
            std::max(0.0f, maximumHealth);
}

inline constexpr float WExplosionRawDamage(int rank, float abilityPower,
                                            float targetMaximumHealth) {
    return RankValue(rank, {40.0f, 65.0f, 90.0f, 115.0f, 140.0f}) +
        0.40f * std::max(0.0f, abilityPower) +
        RankValue(rank, {0.10f, 0.11f, 0.12f, 0.13f, 0.14f}) *
            std::max(0.0f, targetMaximumHealth);
}

inline constexpr float ERawDamage(int rank, float abilityPower) {
    return RankValue(rank, {65.0f, 100.0f, 135.0f, 170.0f, 205.0f}) +
        0.40f * std::max(0.0f, abilityPower);
}

inline constexpr float RRawDamage(int rank, float bonusAttackDamage) {
    return RankValue(rank, {150.0f, 225.0f, 300.0f}) +
        0.40f * std::max(0.0f, bonusAttackDamage);
}

inline constexpr float QHalfWidth(float chargeSeconds) {
    return kQHalfWidthMin + (kQHalfWidthMax - kQHalfWidthMin) *
        ChargeFraction(chargeSeconds);
}

inline Vec3 ClampQEndpoint(const Vec3& origin, const Vec3& requested) {
    if (!origin.IsValid() || !requested.IsValid()) return {};
    const Vec3 direction = Direction2D(origin, requested);
    if (direction.IsZero()) return {};
    return origin + direction * std::min(kQRange, origin.Distance2D(requested));
}

inline bool QPathHits(const Vec3& origin, const Vec3& endpoint,
                      const Vec3& unit, float unitRadius,
                      float chargeSeconds) {
    if (!origin.IsValid() || !endpoint.IsValid() || !unit.IsValid()) return false;
    const auto projection = ProjectPointToSegment2D(unit, origin,
                                                      ClampQEndpoint(origin, endpoint));
    return projection.Distance <= QHalfWidth(chargeSeconds) +
        std::max(0.0f, unitRadius);
}

struct QReleaseContext {
    bool Charging = false;
    bool ControllerOwned = false;
    bool AimValid = false;
    bool PredictionAccepted = false;
    bool WallBlocked = false;
    bool TurretRisk = false;
    bool TargetValid = false;
    bool Lethal = false;
    bool Interrupt = false;
    bool Manual = false;
    float ChargeSeconds = 0.0f;
};

inline bool ShouldReleaseQ(const QReleaseContext& context) {
    if (!context.Charging || !context.ControllerOwned || context.Manual ||
        !context.AimValid || context.WallBlocked || context.TurretRisk ||
        !context.TargetValid || context.ChargeSeconds < kQMinimumChargeSeconds) {
        return false;
    }
    return context.Lethal || context.Interrupt || context.PredictionAccepted ||
           context.ChargeSeconds >= kQMaximumChargeSeconds - 0.05f;
}

inline bool WExplosionHits(const Vec3& center, const Vec3& target,
                           float targetRadius = 0.0f) {
    if (!center.IsValid() || !target.IsValid()) return false;
    return center.Distance2D(target) <= kWRadius + std::max(0.0f, targetRadius);
}

struct WDetonationContext {
    bool ShieldActive = false;
    bool ControllerOwned = false;
    bool TargetValid = false;
    bool TargetInRadius = false;
    bool Lethal = false;
    bool IncomingThreat = false;
    bool Manual = false;
};
inline bool ShouldDetonateW(const WDetonationContext& context) {
    if (!context.ShieldActive || !context.ControllerOwned || context.Manual ||
        !context.TargetValid || !context.TargetInRadius) return false;
    return context.Lethal || context.IncomingThreat;
}

inline Vec3 ClampEEndpoint(const Vec3& origin, const Vec3& requested) {
    if (!origin.IsValid() || !requested.IsValid()) return {};
    const Vec3 direction = Direction2D(origin, requested);
    if (direction.IsZero()) return {};
    return origin + direction * std::min(kERange, origin.Distance2D(requested));
}

inline bool EProjectileHits(const Vec3& origin, const Vec3& endpoint,
                            const Vec3& unit, float unitRadius = 0.0f) {
    if (!origin.IsValid() || !endpoint.IsValid() || !unit.IsValid()) return false;
    const auto projection = ProjectPointToSegment2D(unit, origin,
                                                      ClampEEndpoint(origin, endpoint));
    return projection.T >= 0.0f && projection.T <= 1.0f &&
        projection.Distance <= kEHalfWidth + std::max(0.0f, unitRadius);
}

inline Vec3 SteerREndpoint(const Vec3& origin, const Vec3& requested) {
    if (!origin.IsValid() || !requested.IsValid()) return {};
    const Vec3 direction = Direction2D(origin, requested);
    if (direction.IsZero()) return {};
    return origin + direction * std::min(kRMaximumRange,
                                           origin.Distance2D(requested));
}

inline bool RPathHits(const Vec3& origin, const Vec3& endpoint,
                      const Vec3& unit, float unitRadius = 0.0f) {
    if (!origin.IsValid() || !endpoint.IsValid() || !unit.IsValid()) return false;
    const auto projection = ProjectPointToSegment2D(unit, origin,
                                                      SteerREndpoint(origin, endpoint));
    return projection.T >= 0.0f && projection.T <= 1.0f &&
        projection.Distance <= kRRadius + std::max(0.0f, unitRadius);
}

struct RCollisionContext {
    bool Active = false;
    bool ControllerOwned = false;
    bool PredictedCollision = false;
    bool WallAhead = false;
    bool TurretRisk = false;
    bool TargetValid = false;
    bool Lethal = false;
    bool Interrupt = false;
    bool Manual = false;
    int PredictedEnemies = 0;
    int MinimumTargets = 1;
};
inline bool ShouldCommitR(const RCollisionContext& context) {
    if (!context.Active || !context.ControllerOwned || context.Manual ||
        context.WallAhead || context.TurretRisk || !context.TargetValid ||
        !context.PredictedCollision) return false;
    return context.Lethal || context.Interrupt ||
        context.PredictedEnemies >= std::max(1, context.MinimumTargets);
}

inline bool PassiveZombieActive(bool passiveBuff, bool spellbookDead,
                               int stateExpireTick, int now) {
    return passiveBuff || spellbookDead ||
        (stateExpireTick > 0 && now <= stateExpireTick);
}

inline bool SafeDestination(const Vec3& destination, bool wall,
                            bool enemyTurret, bool allowTurret) {
    return destination.IsValid() && !destination.IsZero() && !wall &&
        (allowTurret || !enemyTurret);
}

} // namespace Plugins::KuroAIO::AI::Controllers::Sion::Geometry
