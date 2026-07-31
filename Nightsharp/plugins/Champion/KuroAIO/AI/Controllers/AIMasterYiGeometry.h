#pragma once

#include "../AIGeometry.h"

#include <algorithm>
#include <cmath>

namespace Plugins::KuroAIO::AI::Controllers::MasterYi::Geometry {

using Vec3 = ::Vec3;
using SharedGeometry::Direction2D;

inline constexpr float kAlphaStrikeRange = 600.0f;
inline constexpr float kMeditateChannelSeconds = 4.0f;
inline constexpr float kMeditateLingerSeconds = 0.5f;
inline constexpr float kWujuAttackRangePadding = 25.0f;
inline constexpr float kHighlanderDurationSeconds = 7.0f;
inline constexpr int kHighlanderTakedownExtensionMs = 7000;
inline constexpr int kTargetMarkLifetimeMs = 1800;

inline bool AlphaStrikeReachable(const Vec3& origin,
                                 const Vec3& destination,
                                 float targetRadius = 0.0f) {
    if (!origin.IsValid() || !destination.IsValid() || origin.IsZero() ||
        destination.IsZero()) return false;
    return origin.Distance2D(destination) <=
        kAlphaStrikeRange + std::max(0.0f, targetRadius);
}

inline bool AlphaStrikeLandingSafe(const Vec3& landing,
                                   bool wall,
                                   bool enemyTurret,
                                   int enemiesAtLanding,
                                   int maximumEnemies,
                                   bool lethalDive) {
    if (!landing.IsValid() || landing.IsZero() || wall) return false;
    if (enemyTurret && !lethalDive) return false;
    return enemiesAtLanding <= std::max(1, maximumEnemies);
}

inline float MeditateDamageTakenMultiplier(float elapsedSeconds) {
    if (!std::isfinite(elapsedSeconds) || elapsedSeconds < 0.0f) return 1.0f;
    // The first half-second is the strong Meditation reduction; the channel
    // then retains the ordinary reduction until the lingering half-second.
    return elapsedSeconds <= 0.5f ? 0.10f : 0.40f;
}

inline bool MeditateStartAllowed(float playerHealthPercent,
                                 bool incomingThreat,
                                 int nearbyEnemies,
                                 bool underEnemyTurret,
                                 bool mobilityLocked,
                                 float lowHealthThreshold = 38.0f) {
    if (mobilityLocked) return false;
    const bool low = playerHealthPercent <= lowHealthThreshold;
    if (!low && !incomingThreat) return false;
    if (underEnemyTurret && !low) return false;
    return low || nearbyEnemies <= 0 || incomingThreat;
}

inline float WujuTrueDamage(float baseDamage, float bonusAttackDamage) {
    return std::max(0.0f, baseDamage) +
        std::max(0.0f, bonusAttackDamage) * 0.35f;
}

inline bool WujuLethal(float trueDamage,
                       float targetHealth,
                       float targetShield,
                       bool targetProtected = false) {
    return !targetProtected && trueDamage >=
        std::max(0.0f, targetHealth) + std::max(0.0f, targetShield);
}

inline bool HighlanderCommitAllowed(bool active,
                                    bool targetKillable,
                                    bool targetInReach,
                                    bool underEnemyTurret,
                                    int enemiesAtTarget,
                                    int maximumEnemies,
                                    bool escape) {
    if (active || (!targetKillable && !escape) || !targetInReach) return false;
    if (underEnemyTurret && !escape) return false;
    return enemiesAtTarget <= std::max(1, maximumEnemies);
}

inline int ExtendHighlander(int currentExpireTick,
                            int nowTick,
                            bool takedownObserved) {
    if (!takedownObserved) return currentExpireTick;
    return std::max(currentExpireTick, nowTick) + kHighlanderTakedownExtensionMs;
}

inline bool ResetChainTargetValid(int targetNetworkId,
                                  int markedNetworkId,
                                  bool targetAlive,
                                  int nowTick,
                                  int markExpireTick) {
    return targetNetworkId != 0 && targetAlive &&
        targetNetworkId == markedNetworkId && nowTick <= markExpireTick;
}

} // namespace Plugins::KuroAIO::AI::Controllers::MasterYi::Geometry
