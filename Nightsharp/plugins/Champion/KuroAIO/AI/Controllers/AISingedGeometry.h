#pragma once

#include "../AIGeometry.h"

#include <algorithm>
#include <cmath>

namespace Plugins::KuroAIO::AI::Controllers::Singed::Geometry {

using Vec3 = ::Vec3;
using SharedGeometry::Direction2D;

inline constexpr float kPoisonRadius = 210.0f;
inline constexpr float kAdhesiveRange = 1000.0f;
inline constexpr float kAdhesiveRadius = 265.0f;
inline constexpr float kFlingRange = 125.0f;
inline constexpr float kFlingDistance = 420.0f;
inline constexpr float kFlingDamagePercentByRank[] = { 0.0f, 6.0f, 6.5f, 7.0f, 7.5f, 8.0f, 8.5f };
inline constexpr int kPotionDurationMs = 25000;

inline bool InFlingReach(const Vec3& origin, const Vec3& target,
                         float targetRadius = 0.0f) {
    if (!origin.IsValid() || !target.IsValid() || origin.IsZero() || target.IsZero()) return false;
    return origin.Distance2D(target) <= kFlingRange + std::max(0.0f, targetRadius);
}

inline Vec3 FlingEndpoint(const Vec3& origin, const Vec3& target,
                          float targetRadius = 0.0f) {
    if (!InFlingReach(origin, target, targetRadius)) return {};
    const Vec3 direction = Direction2D(origin, target);
    return direction.IsZero() ? Vec3{} : target + direction * kFlingDistance;
}

inline bool AdhesiveLandingValid(const Vec3& origin, const Vec3& landing) {
    if (!origin.IsValid() || !landing.IsValid() || origin.IsZero() || landing.IsZero()) return false;
    return origin.Distance2D(landing) <= kAdhesiveRange;
}

inline bool FlingEndpointSafe(bool endpointValid,
                              bool endpointWall,
                              bool endpointTurret,
                              bool originTurret,
                              int enemiesAtEndpoint,
                              int maximumEnemies,
                              bool lethal,
                              bool escaping) {
    if (!endpointValid || endpointWall || enemiesAtEndpoint < 0) return false;
    if (!escaping && endpointTurret && !lethal) return false;
    if (!escaping && originTurret && !lethal) return false;
    return escaping || lethal || enemiesAtEndpoint <= std::max(1, maximumEnemies);
}

inline bool AntiChaseAllowed(float targetDistance,
                             float targetSpeed,
                             bool targetMovingAway,
                             int nearbyEnemies,
                             bool endpointTurret,
                             bool lethal,
                             bool escaping) {
    if (escaping || lethal) return true;
    if (endpointTurret || nearbyEnemies > 2) return false;
    if (targetDistance > 900.0f) return false;
    return !targetMovingAway || targetSpeed <= 120.0f;
}

inline bool PoisonToggleWanted(bool currentlyOn,
                               bool hostileInsideTrail,
                               bool farming,
                               bool lowMana,
                               bool escaping) {
    if (escaping || hostileInsideTrail || farming) return true;
    return currentlyOn && !lowMana;
}

inline bool PotionCommitAllowed(float playerHealthPercent,
                                int enemiesNearPlayer,
                                bool targetInReach,
                                bool incomingThreat,
                                bool alreadyActive) {
    if (alreadyActive) return false;
    if (incomingThreat || playerHealthPercent <= 68.0f) return true;
    return targetInReach && enemiesNearPlayer > 0;
}

inline float FlingRawDamage(int rank, float abilityPower,
                            float targetMaximumHealth) {
    const int clamped = std::clamp(rank, 0, 6);
    return clamped == 0 ? 0.0f : 50.0f + 0.55f * std::max(0.0f, abilityPower) +
        kFlingDamagePercentByRank[clamped] * 0.01f * std::max(0.0f, targetMaximumHealth);
}

inline bool FlingLethal(float damage, float targetHealth, float targetShield) {
    return damage >= std::max(0.0f, targetHealth) + std::max(0.0f, targetShield);
}

} // namespace Plugins::KuroAIO::AI::Controllers::Singed::Geometry
