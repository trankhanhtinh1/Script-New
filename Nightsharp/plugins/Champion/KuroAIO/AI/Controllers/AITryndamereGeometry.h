#pragma once

#include "../AIGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace Plugins::KuroAIO::AI::Controllers::Tryndamere::Geometry {

using Vec3 = ::Vec3;
using SharedGeometry::Direction2D;

inline constexpr float kSpinRange = 650.0f;
inline constexpr float kSpinWidth = 80.0f;
inline constexpr float kSpinDamageRadius = 225.0f;
inline constexpr float kShoutRadius = 850.0f;
inline constexpr float kShoutSlowRadius = 830.0f;
inline constexpr float kAutoAttackReach = 200.0f;
inline constexpr int kUndyingRageDurationMs = 5000;

inline Vec3 SpinEndpoint(const Vec3& origin, const Vec3& desired,
                         float range = kSpinRange) {
    if (!origin.IsValid() || !desired.IsValid() || origin.IsZero() ||
        desired.IsZero() || !std::isfinite(range) || range <= 0.0f) {
        return {};
    }
    const Vec3 direction = Direction2D(origin, desired);
    if (direction.IsZero()) return {};
    return origin + direction * std::min(origin.Distance2D(desired), range);
}

inline bool SpinLineHits(const Vec3& origin, const Vec3& endpoint,
                         const Vec3& target, float targetRadius = 0.0f) {
    if (!origin.IsValid() || !endpoint.IsValid() || !target.IsValid() ||
        origin.IsZero() || endpoint.IsZero() || target.IsZero()) return false;
    if (origin.Distance2D(endpoint) > kSpinRange + 0.5f) return false;
    const auto projection = SharedGeometry::ProjectPointToSegment2D(
        target, origin, endpoint);
    return projection.T >= 0.0f && projection.T <= 1.0f &&
           projection.Distance <= kSpinWidth + std::max(0.0f, targetRadius);
}

inline bool SpinDestinationSafe(const Vec3& origin, const Vec3& destination,
                                bool wall, bool underEnemyTurret,
                                int nearbyEnemies, int maximumEnemies,
                                bool lethalDive) {
    return origin.IsValid() && destination.IsValid() && !origin.IsZero() &&
           !destination.IsZero() && origin.Distance2D(destination) <=
               kSpinRange + 0.5f && !wall &&
           (lethalDive || !underEnemyTurret) &&
           (lethalDive || nearbyEnemies <= std::max(1, maximumEnemies));
}

inline float SpinRawDamage(int rank, float attackDamage, float abilityPower) {
    static constexpr std::array<float, 6> base = {0.0f, 40.0f, 80.0f,
                                                    120.0f, 160.0f, 200.0f};
    const int clamped = std::clamp(rank, 0, 5);
    return base[static_cast<std::size_t>(clamped)] +
           std::max(0.0f, attackDamage) +
           std::max(0.0f, abilityPower) * 0.8f;
}

inline float FuryHeal(int rank, float fury, float abilityPower) {
    static constexpr std::array<float, 6> base = {0.0f, 20.0f, 30.0f,
                                                    40.0f, 50.0f, 60.0f};
    static constexpr std::array<float, 6> perFury = {0.0f, 0.05f, 0.5f,
                                                       0.95f, 1.4f, 1.85f};
    const int clamped = std::clamp(rank, 0, 5);
    const float safeFury = std::clamp(fury, 0.0f, 100.0f);
    return base[static_cast<std::size_t>(clamped)] +
           safeFury * perFury[static_cast<std::size_t>(clamped)] +
           std::max(0.0f, abilityPower) * (0.3f + safeFury * 0.012f);
}

inline bool MockingShoutInRange(float distance, float targetRadius = 0.0f) {
    return std::isfinite(distance) && distance >= 0.0f &&
           distance <= kShoutRadius + std::max(0.0f, targetRadius);
}

inline bool MockingShoutSlowApplies(float distance, bool targetFacingTryndamere,
                                    float targetRadius = 0.0f) {
    return MockingShoutInRange(distance, targetRadius) &&
           distance <= kShoutSlowRadius + std::max(0.0f, targetRadius) &&
           !targetFacingTryndamere;
}

inline bool UndyingRageCastWindow(float healthPercent,
                                  float projectedIncomingDamage,
                                  bool attacking, int nearbyEnemies,
                                  int maximumEnemies = 2) {
    if (!attacking || nearbyEnemies <= 0 || nearbyEnemies >
            std::max(1, maximumEnemies)) return false;
    const float projectedHealth = healthPercent -
        std::max(0.0f, projectedIncomingDamage);
    return healthPercent <= 35.0f || projectedHealth <= 12.0f;
}

inline bool ShouldPostRKill(float targetHealth, float targetShield,
                            float autoDamage, float spinDamage,
                            bool targetInReach) {
    return targetInReach && targetHealth + std::max(0.0f, targetShield) <=
        std::max(0.0f, autoDamage) + std::max(0.0f, spinDamage);
}

inline bool ShouldPostREscape(int remainingMs, int exitBufferMs,
                              float playerHealthPercent, int nearbyEnemies,
                              bool killAvailable) {
    return !killAvailable && remainingMs <= std::max(0, exitBufferMs) &&
           (playerHealthPercent <= 45.0f || nearbyEnemies >= 1);
}

} // namespace Plugins::KuroAIO::AI::Controllers::Tryndamere::Geometry
