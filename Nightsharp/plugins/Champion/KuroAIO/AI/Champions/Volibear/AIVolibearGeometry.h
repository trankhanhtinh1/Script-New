#pragma once

#include "../../AIGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace Plugins::KuroAIO::AI::Controllers::Volibear::Geometry {

using Vec3 = ::Vec3;
using SharedGeometry::ProjectPointToSegment2D;

enum class PassiveState : std::uint8_t { Inactive, Stacking, Empowered };
enum class QState : std::uint8_t { Ready, Charging, Stunned };
enum class WState : std::uint8_t { Ready, Marked, RecastReady };
enum class EState : std::uint8_t { Ready, LightningPending, Shielded };
enum class RState : std::uint8_t { Ready, Leaping, Landed };

inline constexpr float kAttackReach = 125.0f;
inline constexpr float kQRange = 350.0f;
inline constexpr float kWRange = 350.0f;
inline constexpr float kERange = 750.0f;
inline constexpr float kERadius = 325.0f;
inline constexpr float kRRange = 700.0f;
inline constexpr float kRRadius = 300.0f;
inline constexpr float kQWidth = 90.0f;
inline constexpr int kPassiveMaxStacks = 5;
inline constexpr int kPassiveDurationMs = 6000;
inline constexpr int kQMoveMs = 1500;
inline constexpr int kQStunMs = 1000;
inline constexpr int kWMarkDurationMs = 8000;
inline constexpr int kELightningDelayMs = 1000;
inline constexpr int kEShieldDurationMs = 3000;
inline constexpr int kRDisableDurationMs = 2500;
inline constexpr int kRLeapMs = 650;

inline int ClampPassiveStacks(int stacks) {
    return std::clamp(stacks, 0, kPassiveMaxStacks);
}

inline int PassiveStacksAfterHit(int stacks, bool damagingHit) {
    return damagingHit ? ClampPassiveStacks(stacks + 1) : ClampPassiveStacks(stacks);
}

inline PassiveState EvaluatePassive(int stacks, int elapsedMs) {
    if (elapsedMs < 0 || elapsedMs > kPassiveDurationMs || stacks <= 0) return PassiveState::Inactive;
    return ClampPassiveStacks(stacks) >= kPassiveMaxStacks ? PassiveState::Empowered : PassiveState::Stacking;
}

inline bool PassiveLightningReady(int stacks, int elapsedMs) {
    return ClampPassiveStacks(stacks) >= kPassiveMaxStacks && elapsedMs >= 0 &&
        elapsedMs <= kPassiveDurationMs;
}

inline float PassiveAttackSpeedPercent(int stacks) {
    return 5.0f * static_cast<float>(ClampPassiveStacks(stacks));
}

inline bool QMovementActive(QState state, int elapsedMs) {
    return state == QState::Charging && elapsedMs >= 0 && elapsedMs < kQMoveMs;
}

inline bool QCanStun(QState state, int elapsedMs, bool targetReachable, bool endpointWall) {
    return state == QState::Charging && elapsedMs >= 0 && elapsedMs <= kQMoveMs &&
        targetReachable && !endpointWall;
}

inline bool QStunActive(QState state, int elapsedMs) {
    return state == QState::Stunned && elapsedMs >= 0 && elapsedMs < kQStunMs;
}

inline bool WMarkActive(int markTargetId, int targetId, int elapsedMs) {
    return markTargetId != 0 && markTargetId == targetId && elapsedMs >= 0 &&
        elapsedMs < kWMarkDurationMs;
}

inline WState EvaluateW(int markTargetId, int targetId, int elapsedMs) {
    return WMarkActive(markTargetId, targetId, elapsedMs) ? WState::RecastReady : WState::Ready;
}

inline float WDamage(int rank, float bonusAttackDamage, bool recast) {
    static constexpr std::array<float, 5> base{10.0f, 35.0f, 60.0f, 85.0f, 110.0f};
    const int i = std::clamp(rank - 1, 0, 4);
    const float multiplier = recast ? 1.5f : 1.0f;
    return multiplier * (base[static_cast<std::size_t>(i)] +
        0.60f * std::max(0.0f, bonusAttackDamage));
}

inline bool ProjectileCollision(const Vec3& start, const Vec3& end, const Vec3& target,
                                float width = kQWidth, float targetRadius = 0.0f) {
    if (!start.IsValid() || !end.IsValid() || !target.IsValid() ||
        start.IsZero() || end.IsZero() || target.IsZero()) return false;
    const auto projection = ProjectPointToSegment2D(target, start, end);
    return projection.T >= 0.0f && projection.T <= 1.0f &&
        projection.Distance <= std::max(0.0f, width) + std::max(0.0f, targetRadius);
}

inline bool InLightning(const Vec3& center, const Vec3& point, float pointRadius = 0.0f) {
    if (!center.IsValid() || !point.IsValid() || center.IsZero() || point.IsZero()) return false;
    return center.Distance2D(point) <= kERadius + std::max(0.0f, pointRadius);
}

inline bool ECanShield(const Vec3& center, const Vec3& player, int elapsedMs) {
    return elapsedMs >= 0 && elapsedMs <= kELightningDelayMs + kEShieldDurationMs &&
        InLightning(center, player);
}

inline bool ELightningActive(EState state, int elapsedMs) {
    return state == EState::LightningPending && elapsedMs >= kELightningDelayMs;
}

inline float EDamage(int rank, float abilityPower, float bonusHealth) {
    static constexpr std::array<float, 5> base{60.0f, 90.0f, 120.0f, 150.0f, 180.0f};
    const int i = std::clamp(rank - 1, 0, 4);
    return base[static_cast<std::size_t>(i)] + 0.80f * std::max(0.0f, abilityPower) +
        0.10f * std::max(0.0f, bonusHealth);
}

inline float RDamage(int rank, float abilityPower, float bonusHealth) {
    static constexpr std::array<float, 3> base{150.0f, 250.0f, 350.0f};
    const int i = std::clamp(rank - 1, 0, 2);
    return base[static_cast<std::size_t>(i)] + 0.80f * std::max(0.0f, abilityPower) +
        0.125f * std::max(0.0f, bonusHealth);
}

inline bool RReachable(const Vec3& origin, const Vec3& endpoint, float endpointRadius = 0.0f) {
    if (!origin.IsValid() || !endpoint.IsValid() || origin.IsZero() || endpoint.IsZero()) return false;
    return origin.Distance2D(endpoint) <= kRRange + std::max(0.0f, endpointRadius);
}

inline bool RLandingCovers(const Vec3& endpoint, const Vec3& target, float targetRadius = 0.0f) {
    if (!endpoint.IsValid() || !target.IsValid() || endpoint.IsZero() || target.IsZero()) return false;
    return endpoint.Distance2D(target) <= kRRadius + std::max(0.0f, targetRadius);
}

inline bool RDisableActive(int elapsedMs) {
    return elapsedMs >= 0 && elapsedMs < kRDisableDurationMs;
}

inline bool REndpointSafe(bool endpointWall, bool endpointTurret, int enemiesAtEndpoint,
                          int maximumEnemies, bool lethal = false, bool defensive = false,
                          bool fleeing = false, bool disablesTurret = true) {
    if (endpointWall) return false;
    if (endpointTurret && !disablesTurret) return false;
    if (lethal || defensive || fleeing) return true;
    return enemiesAtEndpoint <= std::max(0, maximumEnemies);
}

inline bool RDisableAllowed(bool endpointTurret, bool disablesTurret, int enemiesAtEndpoint,
                           int maximumEnemies, bool lethal = false, bool defensive = false,
                           bool fleeing = false) {
    return disablesTurret && REndpointSafe(false, endpointTurret, enemiesAtEndpoint,
                                           maximumEnemies, lethal, defensive, fleeing, true);
}

inline bool CooldownAvailable(bool runtimeReady, int elapsedMs, int minimumGapMs = 45) {
    return runtimeReady && elapsedMs >= std::max(0, minimumGapMs);
}

inline bool ResourceAvailable(float resource, float maximum, float reservePercent, float cost) {
    if (!std::isfinite(resource) || !std::isfinite(maximum) || !std::isfinite(reservePercent) ||
        !std::isfinite(cost) || maximum <= 0.0f || cost < 0.0f) return false;
    const float reserve = maximum * std::clamp(reservePercent, 0.0f, 100.0f) / 100.0f;
    return resource >= cost && resource - cost >= reserve;
}

inline float QDamage(int rank, float totalAttackDamage, float bonusAttackDamage) {
    static constexpr std::array<float, 5> base{10.0f, 20.0f, 30.0f, 40.0f, 50.0f};
    const int i = std::clamp(rank - 1, 0, 4);
    return base[static_cast<std::size_t>(i)] + 0.15f * std::max(0.0f, totalAttackDamage) +
        0.25f * std::max(0.0f, bonusAttackDamage);
}

} // namespace Plugins::KuroAIO::AI::Controllers::Volibear::Geometry
