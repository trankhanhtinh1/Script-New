#pragma once

#include "../../AIGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace Plugins::KuroAIO::AI::Controllers::Teemo::Geometry {

using Vec3 = ::Vec3;
using SharedGeometry::Direction2D;

inline constexpr float kQRange = 680.0f;
inline constexpr float kQMissileSpeed = 2500.0f;
inline constexpr float kRRange = 900.0f;
inline constexpr float kRTriggerRadius = 160.0f;
inline constexpr float kRVisionRadius = 210.0f;
inline constexpr float kRExplosionRadius = 450.0f;
inline constexpr float kRBounceDistance = 550.0f;
inline constexpr float kRArmSeconds = 1.0f;
inline constexpr float kTeemoAttackRange = 550.0f;

inline float PoisonImpactDamage(int rank, float abilityPower, float bonusAttackDamage) {
    static constexpr std::array<float, 6> base{ -5.0f, 9.0f, 23.0f, 37.0f, 51.0f, 65.0f };
    const int clamped = std::clamp(rank, 0, 5);
    return std::max(0.0f, base[static_cast<std::size_t>(clamped)] +
        0.30f * std::max(0.0f, abilityPower) +
        0.05f * std::max(0.0f, bonusAttackDamage));
}

inline float PoisonTickDamage(int rank, float abilityPower, float bonusAttackDamage) {
    static constexpr std::array<float, 6> base{ 0.0f, 6.0f, 12.0f, 18.0f, 24.0f, 30.0f };
    const int clamped = std::clamp(rank, 0, 5);
    return std::max(0.0f, base[static_cast<std::size_t>(clamped)] +
        0.10f * std::max(0.0f, abilityPower) +
        0.025f * std::max(0.0f, bonusAttackDamage));
}

inline float PoisonTotalDamage(int rank, float abilityPower, float bonusAttackDamage,
                               float secondsRemaining = 4.0f) {
    const float ticks = std::clamp(secondsRemaining, 0.0f, 4.0f);
    return PoisonImpactDamage(rank, abilityPower, bonusAttackDamage) +
           PoisonTickDamage(rank, abilityPower, bonusAttackDamage) * ticks;
}

inline bool PoisonKillable(float damage, float health, float shield) {
    return std::isfinite(damage) && damage >= std::max(0.0f, health) +
        std::max(0.0f, shield);
}

inline bool BlindingDartReach(const Vec3& origin, const Vec3& target,
                              float targetRadius = 0.0f) {
    return origin.IsValid() && target.IsValid() && !origin.IsZero() &&
           !target.IsZero() && origin.Distance2D(target) <=
               kQRange + std::max(0.0f, targetRadius);
}

inline bool ProjectilePathClear(const Vec3& origin, const Vec3& aim,
                                const Vec3& target, float width,
                                float targetRadius = 0.0f) {
    if (!BlindingDartReach(origin, aim, targetRadius) || !target.IsValid()) return false;
    const auto projection = SharedGeometry::ProjectPointToSegment2D(target, origin, aim);
    return projection.T >= 0.0f && projection.T <= 1.0f &&
           projection.Distance <= std::max(0.0f, width) + std::max(0.0f, targetRadius);
}

inline bool TrapLandingValid(const Vec3& origin, const Vec3& landing,
                             float range = kRRange, bool wall = false,
                             bool enemyTurret = false, int nearbyEnemies = 0,
                             int maximumEnemies = 2) {
    if (!origin.IsValid() || !landing.IsValid() || origin.IsZero() || landing.IsZero()) return false;
    if (origin.Distance2D(landing) > range || wall || enemyTurret ||
        nearbyEnemies > std::max(0, maximumEnemies)) return false;
    return true;
}

inline bool TrapCanArm(int castTick, int now, float armSeconds = kRArmSeconds) {
    return castTick > 0 && now >= castTick + static_cast<int>(std::max(0.0f, armSeconds) * 1000.0f);
}

inline bool MushroomBounceValid(const Vec3& previous, const Vec3& landing,
                                float minimumDistance = 150.0f,
                                float maximumDistance = kRBounceDistance) {
    if (!previous.IsValid() || !landing.IsValid() || previous.IsZero() || landing.IsZero()) return false;
    const float distance = previous.Distance2D(landing);
    return distance >= std::max(0.0f, minimumDistance) && distance <= maximumDistance;
}

inline bool SafeFleeDestination(const Vec3& player, const Vec3& destination,
                                const Vec3& pursuer, float minimumSeparation,
                                bool wall, bool turret, int enemies,
                                int maximumEnemies = 1) {
    return player.IsValid() && destination.IsValid() && !destination.IsZero() &&
           !wall && !turret && enemies <= std::max(0, maximumEnemies) &&
           (!pursuer.IsValid() || destination.Distance2D(pursuer) >= minimumSeparation);
}

inline float TrapValue(const Vec3& candidate, const Vec3& target,
                       int enemiesAtPoint, bool visionChoke, bool turret) {
    if (!candidate.IsValid() || candidate.IsZero() || turret) return -10000.0f;
    float score = visionChoke ? 220.0f : 0.0f;
    score += static_cast<float>(std::max(0, enemiesAtPoint)) * 180.0f;
    if (target.IsValid()) score -= candidate.Distance2D(target) * 0.18f;
    return score;
}

} // namespace Plugins::KuroAIO::AI::Controllers::Teemo::Geometry
