#pragma once

#include "../../AIGeometry.h"

#include <algorithm>
#include <cmath>

namespace Plugins::KuroAIO::AI::Controllers::Nunu::Geometry {

using Vec3 = ::Vec3;
using SharedGeometry::Direction2D;

inline constexpr float kConsumeRange = 125.0f;
inline constexpr float kSnowballMinimumDistance = 750.0f;
inline constexpr float kSnowballMaximumDistance = 1750.0f;
inline constexpr float kSnowballMinimumRadius = 75.0f;
inline constexpr float kSnowballMaximumRadius = 200.0f;
inline constexpr float kSnowballMinimumWidth = 75.0f;
inline constexpr float kSnowballMaximumWidth = 200.0f;
inline constexpr float kSnowballMediumTime = 2.5f;
inline constexpr float kSnowballLargeTime = 5.0f;
inline constexpr float kSnowballSpeed = 350.0f;
inline constexpr float kSnowballMaxApRatio = 1.5f;
inline constexpr float kSnowballSlow = 0.50f;
inline constexpr float kSnowballSlowDuration = 1.0f;
inline constexpr float kSnowballBaseDamage = 135.0f;
inline constexpr float kSnowballNoImpactScalar = 0.333f;
inline constexpr float kSnowballStunBase = 0.5f;
inline constexpr float kSnowballStunAdditional = 0.75f;
inline constexpr float kSnowballWalkAfterThrow = 350.0f;
inline constexpr float kSnowballChargeMaxSeconds = 10.0f;
inline constexpr float kSnowballPathTolerance = 24.0f;
inline constexpr float kSnowballWidth = 80.0f;
inline constexpr float kSnowballTargetRange = 625.0f;
inline constexpr float kAbsoluteZeroRadius = 650.0f;
inline constexpr float kAbsoluteZeroChannelSeconds = 3.0f;
inline constexpr float kAbsoluteZeroStartSlow = 0.50f;
inline constexpr float kAbsoluteZeroMaxSlow = 0.95f;
inline constexpr float kAbsoluteZeroMinDamage = 0.5f;
inline constexpr float kAbsoluteZeroBaseDamage = 625.0f;
inline constexpr float kAbsoluteZeroApRatio = 3.0f;

inline int SnowballChargeTier(float seconds) {
    if (!std::isfinite(seconds) || seconds < 0.0f) return 0;
    if (seconds >= kSnowballLargeTime) return 2;
    if (seconds >= kSnowballMediumTime) return 1;
    return 0;
}

inline float SnowballChargeFraction(float seconds) {
    if (!std::isfinite(seconds) || seconds <= 0.0f) return 0.0f;
    return std::clamp(seconds / kSnowballLargeTime, 0.0f, 1.0f);
}

inline float SnowballDistance(const Vec3& origin, const Vec3& endpoint) {
    if (!origin.IsValid() || !endpoint.IsValid()) {
        return 0.0f;
    }
    return origin.Distance2D(endpoint);
}

inline bool SnowballPathValid(const Vec3& origin,
                              const Vec3& endpoint,
                              float targetRadius = 0.0f) {
    const float distance = SnowballDistance(origin, endpoint);
    if (distance < kSnowballMinimumDistance ||
        distance > kSnowballMaximumDistance + std::max(0.0f, targetRadius)) {
        return false;
    }
    return SharedGeometry::Direction2D(origin, endpoint).IsValid();
}

inline float SnowballRadiusForCharge(float seconds) {
    const float fraction = SnowballChargeFraction(seconds);
    return kSnowballMinimumRadius +
           (kSnowballMaximumRadius - kSnowballMinimumRadius) * fraction;
}

inline float SnowballWidthForCharge(float seconds) {
    const float fraction = SnowballChargeFraction(seconds);
    return kSnowballMinimumWidth +
           (kSnowballMaximumWidth - kSnowballMinimumWidth) * fraction;
}

inline bool SnowballLineHits(const Vec3& origin,
                             const Vec3& endpoint,
                             const Vec3& target,
                             float targetRadius = 0.0f,
                             float chargeSeconds = kSnowballLargeTime) {
    if (!SnowballPathValid(origin, endpoint)) return false;
    if (!target.IsValid() || target.IsZero()) return false;
    const auto projection = SharedGeometry::ProjectPointToSegment2D(
        target, origin, endpoint);
    return projection.T > 0.0f && projection.T <= 1.0f &&
           projection.Distance <= SnowballWidthForCharge(chargeSeconds) * 0.5f +
               std::max(0.0f, targetRadius);
}

inline float SnowballImpactSeconds(float distance, float chargeSeconds) {
    if (!std::isfinite(distance) || distance < 0.0f) return 0.0f;
    const float speed = kSnowballSpeed +
        75.0f * SnowballChargeFraction(chargeSeconds);
    return distance / std::max(1.0f, speed);
}

inline float SnowballStunDuration(float chargeSeconds) {
    const float fraction = SnowballChargeFraction(chargeSeconds);
    return kSnowballStunBase + kSnowballStunAdditional * fraction;
}

inline bool ConsumeSecure(float consumeDamage,
                          float monsterHealth,
                          float smiteDamage,
                          bool epicMonster,
                          bool enemyContesting) {
    const float health = std::max(0.0f, monsterHealth);
    const float q = std::max(0.0f, consumeDamage);
    if (epicMonster && enemyContesting) {
        return q >= health || q + std::max(0.0f, smiteDamage) >= health;
    }
    return q >= health || (epicMonster && q + std::max(0.0f, smiteDamage) >= health);
}

inline bool ConsumeHealthy(float playerHealthPercent,
                           float objectiveHealthPercent,
                           bool objective) {
    if (!std::isfinite(playerHealthPercent) || !std::isfinite(objectiveHealthPercent)) {
        return false;
    }
    return objective || playerHealthPercent <= 72.0f || objectiveHealthPercent <= 35.0f;
}

inline bool ERootReady(int snowballStacks, bool recastAvailable) {
    return snowballStacks >= 3 || recastAvailable;
}

inline float ESlowPercent(int rank) {
    static constexpr float values[] = {0.0f, 25.0f, 30.0f, 35.0f, 40.0f, 45.0f, 50.0f};
    return values[std::clamp(rank, 0, 6)];
}

inline bool EStackWindowActive(int stacks, int nowTick, int lastStackTick) {
    return stacks > 0 && nowTick >= lastStackTick && nowTick - lastStackTick <= 3000;
}

inline float AbsoluteZeroDamage(float channelSeconds, float abilityPower) {
    const float fraction = std::clamp(channelSeconds / kAbsoluteZeroChannelSeconds, 0.0f, 1.0f);
    return (kAbsoluteZeroBaseDamage + kAbsoluteZeroApRatio * std::max(0.0f, abilityPower)) *
           (kAbsoluteZeroMinDamage + (1.0f - kAbsoluteZeroMinDamage) * fraction);
}

inline bool AbsoluteZeroCommitAllowed(float playerHealthPercent,
                                      int enemiesInRadius,
                                      bool rootedTarget,
                                      bool hardThreatIncoming,
                                      bool underEnemyTurret) {
    if (underEnemyTurret || enemiesInRadius <= 0 || hardThreatIncoming) return false;
    return rootedTarget || enemiesInRadius >= 2 || playerHealthPercent <= 38.0f;
}

inline bool AbsoluteZeroReleaseNeeded(float elapsedSeconds,
                                      bool hardThreatIncoming,
                                      bool noTargetsRemain,
                                      bool underEnemyTurret) {
    return elapsedSeconds >= 0.35f &&
           (hardThreatIncoming || noTargetsRemain || underEnemyTurret);
}

inline bool FleeSnowballSafe(const Vec3& origin,
                             const Vec3& endpoint,
                             bool endpointWall,
                             bool endpointTurret,
                             int endpointEnemies,
                             int maximumEnemies = 1) {
    return SnowballPathValid(origin, endpoint) && !endpointWall && !endpointTurret &&
           endpointEnemies <= std::max(0, maximumEnemies);
}

} // namespace Plugins::KuroAIO::AI::Controllers::Nunu::Geometry
