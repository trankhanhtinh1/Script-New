#pragma once

#include "../../AIGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace Plugins::KuroAIO::AI::Controllers::Shyvana::Geometry {

using Vec3 = ::Vec3;
using SharedGeometry::ProjectPointToSegment2D;

enum class FormState : std::uint8_t { Human, Dragon };
enum class QResetState : std::uint8_t { Ready, Armed, Consumed };
enum class FlightState : std::uint8_t { Ready, Flying, Landed };

enum class FuryState : std::uint8_t { BelowThreshold, ReadyToTransform, Transforming };

inline constexpr float kAttackReach = 125.0f;
inline constexpr float kQReach = 125.0f;
inline constexpr float kWRadius = 350.0f;
inline constexpr float kERange = 925.0f;
inline constexpr float kEWidth = 60.0f;
inline constexpr float kRRange = 1000.0f;
inline constexpr float kRImpactRadius = 345.0f;
inline constexpr int kQResetWindowMs = 3500;
inline constexpr int kWDurationMs = 3000;
inline constexpr int kEMarkDurationMs = 5000;
inline constexpr int kRFlightMs = 700;
inline constexpr int kDragonDurationMs = 15000;
inline constexpr float kMaxFury = 100.0f;
inline constexpr float kRMinimumFury = 100.0f;

inline float ClampFury(float fury) {
    return std::clamp(std::isfinite(fury) ? fury : 0.0f, 0.0f, kMaxFury);
}

inline FuryState EvaluateFury(float fury, FormState form) {
    if (form == FormState::Dragon) return FuryState::Transforming;
    return ClampFury(fury) >= kRMinimumFury ? FuryState::ReadyToTransform : FuryState::BelowThreshold;
}

inline float FuryAfterAttack(float fury, FormState form, int attacks = 1) {
    if (form == FormState::Dragon) return ClampFury(fury);
    return ClampFury(ClampFury(fury) + 2.0f * static_cast<float>(std::max(0, attacks)));
}

inline float FuryAfterTick(float fury, FormState form, int elapsedMs) {
    if (elapsedMs <= 0) return ClampFury(fury);
    const float seconds = static_cast<float>(elapsedMs) / 1500.0f;
    if (form == FormState::Dragon)
        return ClampFury(fury - static_cast<float>(elapsedMs) / 1000.0f);
    return ClampFury(fury + std::floor(seconds));
}

inline bool CanCastR(float fury, FormState form, bool ready, bool manualOverride = false) {
    return form == FormState::Human && ready && (manualOverride || ClampFury(fury) >= kRMinimumFury);
}

inline bool InQResetWindow(QResetState state, int elapsedMs) {
    return state == QResetState::Armed && elapsedMs >= 0 && elapsedMs <= kQResetWindowMs;
}

inline bool ShouldResetAttack(QResetState state, int elapsedMs, bool windingUp, bool preserveAttack) {
    return InQResetWindow(state, elapsedMs) && (!windingUp || !preserveAttack);
}

inline bool BurnoutActive(int elapsedMs) {
    return elapsedMs >= 0 && elapsedMs < kWDurationMs;
}

inline float BurnoutMovementSpeed(float baseSpeed, int elapsedMs, FormState form) {
    if (!BurnoutActive(elapsedMs)) return std::max(0.0f, baseSpeed);
    const float bonus = form == FormState::Dragon ? 0.40f : 0.30f;
    return std::max(0.0f, baseSpeed) * (1.0f + bonus);
}

inline bool InBurnout(const Vec3& center, const Vec3& target, float targetRadius = 0.0f) {
    if (!center.IsValid() || !target.IsValid() || center.IsZero() || target.IsZero()) return false;
    return center.Distance2D(target) <= kWRadius + std::max(0.0f, targetRadius);
}

inline bool ProjectileReachable(const Vec3& origin, const Vec3& target, float targetRadius = 0.0f) {
    if (!origin.IsValid() || !target.IsValid() || origin.IsZero() || target.IsZero()) return false;
    return origin.Distance2D(target) <= kERange + std::max(0.0f, targetRadius);
}

inline bool ProjectileCollision(const Vec3& start, const Vec3& end, const Vec3& target,
                                float width = kEWidth, float targetRadius = 0.0f) {
    if (!start.IsValid() || !end.IsValid() || !target.IsValid() ||
        start.IsZero() || end.IsZero() || target.IsZero()) return false;
    const auto projection = ProjectPointToSegment2D(target, start, end);
    return projection.T >= 0.0f && projection.T <= 1.0f &&
        projection.Distance <= std::max(0.0f, width) + std::max(0.0f, targetRadius);
}

inline bool MarkActive(int elapsedMs) {
    return elapsedMs >= 0 && elapsedMs < kEMarkDurationMs;
}

inline bool DragonFlightReachable(const Vec3& origin, const Vec3& endpoint) {
    if (!origin.IsValid() || !endpoint.IsValid() || origin.IsZero() || endpoint.IsZero()) return false;
    return origin.Distance2D(endpoint) <= kRRange;
}

inline bool DragonImpactCovers(const Vec3& endpoint, const Vec3& target, float targetRadius = 0.0f) {
    if (!endpoint.IsValid() || !target.IsValid() || endpoint.IsZero() || target.IsZero()) return false;
    return endpoint.Distance2D(target) <= kRImpactRadius + std::max(0.0f, targetRadius);
}

inline bool UnsafeDragonEndpoint(bool endpointWall, bool endpointTurret, int enemiesAtEndpoint,
                                 int maximumEnemies, bool lethal, bool defensive, bool fleeing) {
    if (endpointWall || endpointTurret) return true;
    if (lethal || defensive || fleeing) return false;
    return enemiesAtEndpoint > std::max(0, maximumEnemies);
}

inline bool DragonEndpointSafe(bool endpointWall, bool endpointTurret, int enemiesAtEndpoint,
                               int maximumEnemies, bool lethal = false, bool defensive = false,
                               bool fleeing = false) {
    return !UnsafeDragonEndpoint(endpointWall, endpointTurret, enemiesAtEndpoint,
                                 maximumEnemies, lethal, defensive, fleeing);
}

inline bool ResourceAvailable(float resource, float maximum, float reservePercent, float cost) {
    if (!std::isfinite(resource) || !std::isfinite(maximum) || !std::isfinite(reservePercent) ||
        !std::isfinite(cost) || maximum <= 0.0f || cost < 0.0f) return false;
    const float reserve = maximum * std::clamp(reservePercent, 0.0f, 100.0f) / 100.0f;
    return resource >= cost && resource - cost >= reserve;
}

inline bool CooldownAvailable(bool runtimeReady, int elapsedMs, int minimumGapMs = 45) {
    return runtimeReady && elapsedMs >= std::max(0, minimumGapMs);
}

inline float QDamage(int rank, float totalAttackDamage, float bonusAttackDamage) {
    static constexpr std::array<float, 5> base{10.0f, 20.0f, 30.0f, 40.0f, 50.0f};
    const int index = std::clamp(rank - 1, 0, 4);
    return base[static_cast<std::size_t>(index)] + 0.25f * std::max(0.0f, totalAttackDamage) +
        0.75f * std::max(0.0f, bonusAttackDamage);
}

inline float WTickDamage(int rank, float bonusAttackDamage) {
    static constexpr std::array<float, 5> base{10.0f, 15.0f, 20.0f, 25.0f, 30.0f};
    const int index = std::clamp(rank - 1, 0, 4);
    return base[static_cast<std::size_t>(index)] + 0.10f * std::max(0.0f, bonusAttackDamage);
}

inline float EDamage(int rank, float abilityPower, float bonusAttackDamage) {
    static constexpr std::array<float, 5> base{60.0f, 100.0f, 140.0f, 180.0f, 220.0f};
    const int index = std::clamp(rank - 1, 0, 4);
    return base[static_cast<std::size_t>(index)] + 0.30f * std::max(0.0f, abilityPower) +
        0.30f * std::max(0.0f, bonusAttackDamage);
}

inline float RImpactDamage(int rank, float abilityPower, float bonusAttackDamage) {
    static constexpr std::array<float, 3> base{150.0f, 250.0f, 350.0f};
    const int index = std::clamp(rank - 1, 0, 2);
    return base[static_cast<std::size_t>(index)] + 0.70f * std::max(0.0f, abilityPower) +
        0.60f * std::max(0.0f, bonusAttackDamage);
}

} // namespace Plugins::KuroAIO::AI::Controllers::Shyvana::Geometry
