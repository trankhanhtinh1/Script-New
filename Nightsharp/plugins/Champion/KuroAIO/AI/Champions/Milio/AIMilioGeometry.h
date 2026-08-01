#pragma once

#include "../../AIGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace Plugins::KuroAIO::AI::Controllers::Milio::Geometry {

using SharedGeometry::Direction2D;
using SharedGeometry::ProjectPointToSegment2D;

inline constexpr float kQRange = 1200.0f;
inline constexpr float kQWidth = 60.0f;
inline constexpr float kQCastDelay = 0.25f;
inline constexpr float kQSpeed = 1200.0f;
inline constexpr float kQKickDistance = 140.0f;
inline constexpr float kQLandingRadius = 250.0f;
inline constexpr float kWRange = 650.0f;
inline constexpr float kWRadius = 350.0f;
inline constexpr float kWDuration = 6.0f;
inline constexpr float kERange = 650.0f;
inline constexpr float kShieldDuration = 2.5f;
inline constexpr int kEMaxCharges = 2;
inline constexpr float kRRadius = 800.0f;
inline constexpr float kRHealBase = 150.0f;
inline constexpr float kRHealApRatio = 0.5f;
inline constexpr float kRHardCcTenacity = 0.65f;

inline bool FinitePoint(const Vec3& point) { return point.IsValid(); }

inline float QTravelSeconds(float distance) {
    if (!std::isfinite(distance) || distance < 0.0f) return 0.0f;
    return kQCastDelay + distance / kQSpeed;
}

inline Vec3 QEndpoint(const Vec3& origin, const Vec3& requested) {
    if (!FinitePoint(origin) || !FinitePoint(requested)) return {};
    const Vec3 direction = Direction2D(origin, requested);
    if (direction.IsZero()) return {};
    return origin + direction * std::min(kQRange, origin.Distance2D(requested));
}

inline bool QTargetReachable(const Vec3& origin, const Vec3& target,
                             float radius = 0.0f) {
    return FinitePoint(origin) && FinitePoint(target) && std::isfinite(radius) &&
        origin.Distance2D(target) <= kQRange + std::clamp(radius, 0.0f, 200.0f);
}

inline bool QProjectileContacts(const Vec3& origin, const Vec3& endpoint,
                                const Vec3& target, float targetRadius = 0.0f) {
    if (!FinitePoint(origin) || !FinitePoint(endpoint) || !FinitePoint(target) ||
        !std::isfinite(targetRadius)) return false;
    const auto projection = ProjectPointToSegment2D(target, origin, endpoint);
    return projection.T >= 0.0f && projection.T <= 1.0f &&
        projection.Distance <= kQWidth * 0.5f + std::clamp(targetRadius, 0.0f, 200.0f);
}

inline Vec3 KickedPosition(const Vec3& impact, const Vec3& target) {
    if (!FinitePoint(impact) || !FinitePoint(target)) return {};
    const Vec3 direction = Direction2D(impact, target);
    return direction.IsZero() ? target : target + direction * kQKickDistance;
}

inline bool KickLandingContacts(const Vec3& landingCenter, const Vec3& allyOrEnemy,
                                float radius = 0.0f) {
    return FinitePoint(landingCenter) && FinitePoint(allyOrEnemy) && std::isfinite(radius) &&
        landingCenter.Distance2D(allyOrEnemy) <= kQLandingRadius + std::clamp(radius, 0.0f, 200.0f);
}

inline bool WCastInRange(const Vec3& origin, const Vec3& ally) {
    return FinitePoint(origin) && FinitePoint(ally) && origin.Distance2D(ally) <= kWRange;
}
inline bool WZoneContains(const Vec3& campCenter, const Vec3& ally,
                          float allyRadius = 0.0f) {
    return FinitePoint(campCenter) && FinitePoint(ally) && std::isfinite(allyRadius) &&
        campCenter.Distance2D(ally) <= kWRadius + std::clamp(allyRadius, 0.0f, 120.0f);
}
inline float WRangeMultiplier(int rank) {
    static constexpr std::array<float, 6> values{0.0f, 0.075f, 0.10f, 0.125f, 0.15f, 0.175f};
    return values[static_cast<std::size_t>(std::clamp(rank, 0, 5))];
}

inline bool EChargesAvailable(int charges) { return charges > 0 && charges <= kEMaxCharges; }
inline int EChargesAfterCast(int charges) {
    return EChargesAvailable(charges) ? std::max(0, charges - 1) : 0;
}
inline bool EShieldWorthwhile(float healthPercent, bool hardCcThreat,
                             bool incomingDamage, bool fleeing,
                             float healthThreshold = 82.0f) {
    if (!std::isfinite(healthPercent) || !std::isfinite(healthThreshold)) return false;
    return hardCcThreat || incomingDamage || fleeing ||
        healthPercent <= std::clamp(healthThreshold, 1.0f, 99.0f);
}

inline bool RInRange(const Vec3& origin, const Vec3& ally,
                    float allyRadius = 0.0f) {
    return FinitePoint(origin) && FinitePoint(ally) && std::isfinite(allyRadius) &&
        origin.Distance2D(ally) <= kRRadius + std::clamp(allyRadius, 0.0f, 200.0f);
}
inline bool RShouldCleanse(bool hardCc, bool threatened, float healthPercent) {
    if (!std::isfinite(healthPercent)) return false;
    return hardCc || (threatened && healthPercent <= 70.0f);
}
inline bool RHealWorthwhile(float healthPercent, bool threatened, int nearbyEnemies,
                           int minimumEnemies = 1) {
    if (!std::isfinite(healthPercent)) return false;
    return healthPercent <= 62.0f ||
        (threatened && nearbyEnemies >= std::max(0, minimumEnemies));
}
inline float RHealAmount(int rank, float ap) {
    if (!std::isfinite(ap)) return 0.0f;
    static constexpr std::array<float, 4> bases{0.0f, 150.0f, 250.0f, 350.0f};
    return bases[static_cast<std::size_t>(std::clamp(rank, 0, 3))] + kRHealApRatio * ap;
}

inline float PassiveBonusDamage(int championLevel, float ap, float attackDamage) {
    if (!std::isfinite(ap) || !std::isfinite(attackDamage)) return 0.0f;
    const float levelRatio = std::clamp(static_cast<float>(championLevel - 1) / 17.0f, 0.0f, 1.0f);
    const float base = 10.0f + 40.0f * levelRatio;
    const float adRatio = 0.07f + (championLevel >= 6 ? 0.04f : 0.0f) +
        (championLevel >= 9 ? 0.04f : 0.0f);
    return base + 0.20f * std::max(0.0f, ap) + adRatio * std::max(0.0f, attackDamage);
}
inline bool PassiveEmpowerReady(bool allyCastObserved, bool localAttackPending) {
    return allyCastObserved && localAttackPending;
}

} // namespace Plugins::KuroAIO::AI::Controllers::Milio::Geometry
