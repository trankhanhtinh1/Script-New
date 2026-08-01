#pragma once

// Pure Dr. Mundo health, cooldown and hit geometry. Runtime ownership and
// prediction stay in AIDrMundoController so this header is standalone-testable.
#include "../../AIGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace Plugins::KuroAIO::AI::Controllers::DrMundo::Geometry {

using Vec3 = ::Vec3;
using SharedGeometry::Direction2D;
using SharedGeometry::ProjectPointToSegment2D;

inline constexpr float kQRange = 1050.0f;
inline constexpr float kQDisplayRange = 975.0f;
inline constexpr float kQHalfWidth = 30.0f;
inline constexpr float kQSpeed = 1500.0f;
inline constexpr float kQDelay = 0.25f;
inline constexpr float kQSlowSeconds = 2.0f;
inline constexpr float kQSlowAmount = 0.40f;
inline constexpr float kWRadius = 325.0f;
inline constexpr float kWDurationSeconds = 3.0f;
inline constexpr float kWHealthCostPercent = 0.08f;
inline constexpr float kWRecastRange = 325.0f;
inline constexpr float kWRecastLockoutSeconds = 0.50f;
inline constexpr float kERange = 180.0f;
inline constexpr float kECastTime = 1.0f;
inline constexpr float kEHealthCostRankOne = 10.0f;
inline constexpr float kEHealthCostPerRank = 15.0f;
inline constexpr float kRDurationSeconds = 10.0f;
inline constexpr float kRRadius = 600.0f;
inline constexpr float kRBaseCooldownSeconds = 120.0f;
inline constexpr float kRHealthRegenRankOne = 0.10f;
inline constexpr float kRHealthRegenPerRank = 0.05f;
inline constexpr float kRMaxHealthHotRankOne = 0.0f;
inline constexpr float kRMaxHealthHotPerRank = 0.20f;
inline constexpr float kPassiveBaseCooldownSeconds = 60.0f;
inline constexpr float kPassiveCooldownLevelReduction = 9.0f;
inline constexpr float kPassivePickupRefundSeconds = 15.0f;

inline constexpr float RankValue(int rank, const std::array<float, 5>& values) {
    return values[static_cast<std::size_t>(std::clamp(rank, 1, 5) - 1)];
}
inline constexpr float RankValue3(int rank, const std::array<float, 3>& values) {
    return values[static_cast<std::size_t>(std::clamp(rank, 1, 3) - 1)];
}

inline constexpr float QCurrentHealthDamagePercent(int rank) {
    return RankValue(rank, {0.175f, 0.20f, 0.225f, 0.25f, 0.275f});
}
inline constexpr float QMinimumDamage(int rank) {
    return RankValue(rank, {80.0f, 130.0f, 180.0f, 230.0f, 280.0f});
}
inline constexpr float QHealthCost(int rank) {
    return RankValue(rank, {50.0f, 60.0f, 70.0f, 80.0f, 90.0f});
}
inline constexpr float QRawDamage(int rank, float targetCurrentHealth) {
    return std::max(QMinimumDamage(rank), std::max(0.0f, targetCurrentHealth) *
        QCurrentHealthDamagePercent(rank));
}
inline constexpr float EHealthCost(int rank) {
    return kEHealthCostRankOne + kEHealthCostPerRank * std::max(0, std::clamp(rank, 1, 5) - 1);
}
inline constexpr float EMissingHealthDamageAmp(float playerHealthPercent) {
    const float missing = std::clamp(100.0f - playerHealthPercent, 0.0f, 70.0f) / 70.0f;
    return 1.0f + 0.40f * missing;
}
inline constexpr float EAdditionalDamage(int rank, float bonusHealth,
                                          float playerHealthPercent) {
    const float base = RankValue(rank, {5.0f, 15.0f, 25.0f, 35.0f, 45.0f}) +
        0.05f * std::max(0.0f, bonusHealth);
    return base * EMissingHealthDamageAmp(playerHealthPercent);
}
inline constexpr float EBonusAttackDamage(int rank, float currentHealth) {
    return std::max(0.0f, currentHealth) *
        RankValue(rank, {0.02f, 0.023f, 0.026f, 0.029f, 0.032f});
}
inline constexpr float RMissingHealthRegen(int rank) {
    return RankValue3(rank, {0.15f, 0.25f, 0.35f});
}
inline constexpr float RMaxHealthHot(int rank) {
    return RankValue3(rank, {0.20f, 0.60f, 1.00f});
}
inline constexpr float RSpeedBoost(int rank) {
    return RankValue3(rank, {0.15f, 0.35f, 0.55f});
}
inline constexpr float PassiveCooldownSeconds(int championLevel) {
    const int level = std::max(0, championLevel);
    const int reductions = (level >= 3 ? 1 : 0) +
        (level >= 6 ? 1 : 0) + (level >= 9 ? 1 : 0) +
        (level >= 12 ? 1 : 0) + (level >= 15 ? 1 : 0) +
        (level >= 21 ? 1 : 0);
    return std::max(6.0f, kPassiveBaseCooldownSeconds -
        reductions * kPassiveCooldownLevelReduction);
}
inline constexpr float PassiveHealthCostPercent() { return 0.04f; }
inline constexpr float PassiveHealthGainPercent() { return 0.04f; }

inline Vec3 ClampQEndpoint(const Vec3& origin, const Vec3& requested) {
    if (!origin.IsValid() || !requested.IsValid()) return {};
    const Vec3 direction = Direction2D(origin, requested);
    if (direction.IsZero()) return {};
    return origin + direction * std::min(kQRange, origin.Distance2D(requested));
}
inline bool QPathHits(const Vec3& origin, const Vec3& endpoint,
                      const Vec3& unit, float unitRadius = 0.0f) {
    if (!origin.IsValid() || !endpoint.IsValid() || !unit.IsValid()) return false;
    const auto projection = ProjectPointToSegment2D(unit, origin,
        ClampQEndpoint(origin, endpoint));
    return projection.T >= 0.0f && projection.T <= 1.0f &&
        projection.Distance <= kQHalfWidth + std::max(0.0f, unitRadius);
}
inline bool WInRange(const Vec3& center, const Vec3& target,
                     float targetRadius = 0.0f) {
    return center.IsValid() && target.IsValid() &&
        center.Distance2D(target) <= kWRadius + std::max(0.0f, targetRadius);
}
inline bool EInRange(const Vec3& player, const Vec3& target,
                     float targetRadius = 0.0f) {
    return player.IsValid() && target.IsValid() &&
        player.Distance2D(target) <= kERange + std::max(0.0f, targetRadius);
}
inline bool SafeHealthSpend(float currentHealth, float maximumHealth,
                            float cost, float minimumPercent,
                            bool emergency = false) {
    if (maximumHealth <= 0.0f || currentHealth <= cost) return false;
    const float after = (currentHealth - std::max(0.0f, cost)) / maximumHealth * 100.0f;
    return emergency || after >= minimumPercent;
}
inline bool SafeRegeneration(float healthPercent, bool antiGrievousWounds,
                             bool incomingThreat, int enemies,
                             float minimumHealthPercent = 22.0f) {
    if (healthPercent > 88.0f && !incomingThreat) return false;
    if (antiGrievousWounds && healthPercent > 14.0f) return false;
    if (enemies > 3 && healthPercent > 32.0f && !incomingThreat) return false;
    return healthPercent <= minimumHealthPercent || incomingThreat ||
        healthPercent <= 64.0f;
}
inline bool SafeWToggle(float healthPercent, float currentHealthCostPercent,
                        bool incomingThreat, bool lethalTarget) {
    if (healthPercent <= currentHealthCostPercent * 100.0f + 8.0f) return false;
    return incomingThreat || lethalTarget || healthPercent <= 64.0f;
}
inline bool PreferWTenacity(bool active, bool manual, bool incomingHardCc,
                            bool lethalTarget) {
    return active && !manual && incomingHardCc && !lethalTarget;
}

struct QCastContext {
    bool Ready = false;
    bool Owned = false;
    bool AimValid = false;
    bool PredictionAccepted = false;
    bool CollisionBlocked = false;
    bool ProjectileWall = false;
    bool TurretRisk = false;
    bool TargetValid = false;
    bool HealthAffordable = false;
};
inline bool ShouldCastQ(const QCastContext& c) {
    return c.Ready && c.Owned && c.AimValid && c.PredictionAccepted &&
        !c.CollisionBlocked && !c.ProjectileWall && !c.TurretRisk &&
        c.TargetValid && c.HealthAffordable;
}
struct WCastContext {
    bool Ready = false;
    bool Active = false;
    bool Recast = false;
    bool InRange = false;
    bool HealthAffordable = false;
    bool Manual = false;
    bool IncomingThreat = false;
    bool Lethal = false;
};
inline bool ShouldCastW(const WCastContext& c) {
    if (!c.Ready || c.Manual || !c.HealthAffordable) return false;
    return c.Recast ? c.Active && c.InRange && (c.IncomingThreat || c.Lethal)
                    : !c.Active && (c.IncomingThreat || c.Lethal);
}

} // namespace Plugins::KuroAIO::AI::Controllers::DrMundo::Geometry
