#pragma once

// Deterministic Jarvan IV mechanics. The live controller owns prediction,
// NavMesh queries and target selection; this file owns the 26.15 / 16.15
// passive, flag-line, dash, shield and Cataclysm geometry.

#include "../AIGeometry.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <vector>

namespace Plugins::KuroAIO::AI::Controllers::JarvanIV::Geometry {

using SharedGeometry::Direction2D;
using SharedGeometry::ProjectPointToSegment2D;
using SharedGeometry::RankValue;

inline constexpr float kQRange = 770.0f;
inline constexpr float kQLineWidth = 70.0f;
inline constexpr float kQCastDelaySeconds = 0.25f;
inline constexpr float kFlagCastRange = 860.0f;
inline constexpr float kFlagRadius = 175.0f;
inline constexpr float kFlagMissileSpeed = 1450.0f;
inline constexpr float kFlagCastDelaySeconds = 0.25f;
inline constexpr float kFlagLifetimeSeconds = 8.0f;
inline constexpr float kFlagPickupRadius = 82.0f;
inline constexpr float kEQDashSpeed = 1400.0f;
inline constexpr float kEQKnockupRadius = 180.0f;
inline constexpr float kWRadius = 625.0f;
inline constexpr float kRCastRange = 650.0f;
inline constexpr float kArenaRadius = 340.0f;
inline constexpr float kArenaWallDurationSeconds = 3.5f;

inline float PassiveCooldownSeconds(int championLevel) {
    const int level = std::clamp(championLevel, 1, 18);
    return level >= 16 ? 3.0f : level >= 11 ? 4.0f : level >= 6 ? 5.0f : 6.0f;
}

inline float PassiveRawDamage(float targetCurrentHealth,
                              bool monster = false) {
    const float scaled = std::max(0.0f, targetCurrentHealth) * 0.08f;
    const float damage = std::max(20.0f, scaled);
    return monster ? std::min(400.0f, damage) : damage;
}

inline float QRawDamage(int rank, float bonusAttackDamage) {
    static constexpr std::array<float, 6> base{
        0.0f, 90.0f, 130.0f, 170.0f, 210.0f, 250.0f,
    };
    return RankValue(base, rank) +
           std::max(0.0f, bonusAttackDamage) * 1.45f;
}

inline float QArmorReduction(int rank) {
    static constexpr std::array<float, 6> reduction{
        0.0f, 0.10f, 0.14f, 0.18f, 0.22f, 0.26f,
    };
    return RankValue(reduction, rank);
}

inline float WSlowPercent(int rank) {
    static constexpr std::array<float, 6> slow{
        0.0f, 15.0f, 20.0f, 25.0f, 30.0f, 35.0f,
    };
    return RankValue(slow, rank);
}

inline float WShield(int rank,
                     float bonusAttackDamage,
                     float maximumHealth,
                     int nearbyEnemyChampions) {
    static constexpr std::array<float, 6> base{
        0.0f, 60.0f, 80.0f, 100.0f, 120.0f, 140.0f,
    };
    const float perChampion = std::max(0.0f, maximumHealth) * 0.013f;
    return RankValue(base, rank) +
           std::max(0.0f, bonusAttackDamage) * 0.70f +
           perChampion * static_cast<float>(
               std::clamp(nearbyEnemyChampions, 0, 5));
}

inline float ERawDamage(int rank, float abilityPower) {
    static constexpr std::array<float, 6> base{
        0.0f, 80.0f, 120.0f, 160.0f, 200.0f, 240.0f,
    };
    return RankValue(base, rank) + std::max(0.0f, abilityPower) * 0.80f;
}

inline float EAttackSpeedPercent(int rank) {
    static constexpr std::array<float, 6> amount{
        0.0f, 20.0f, 22.5f, 25.0f, 27.5f, 30.0f,
    };
    return RankValue(amount, rank);
}

inline float RRawDamage(int rank, float bonusAttackDamage) {
    static constexpr std::array<float, 4> base{
        0.0f, 200.0f, 325.0f, 450.0f,
    };
    return RankValue(base, rank) +
           std::max(0.0f, bonusAttackDamage) * 1.80f;
}

inline float FlagArrivalSeconds(float castDistance) {
    return kFlagCastDelaySeconds +
           std::clamp(castDistance, 0.0f, kFlagCastRange) /
               kFlagMissileSpeed;
}

inline bool QHitsFlag(const Vec3& origin,
                      const Vec3& aim,
                      const Vec3& flag,
                      float qRange = kQRange,
                      float pickupRadius = kFlagPickupRadius) {
    if (!origin.IsValid() || !aim.IsValid() || !flag.IsValid()) return false;
    const Vec3 direction = Direction2D(origin, aim);
    if (direction.IsZero()) return false;
    const Vec3 end = origin + direction * std::max(0.0f, qRange);
    const auto projection = ProjectPointToSegment2D(flag, origin, end);
    return projection.T > 0.0f && projection.Distance <=
        std::max(0.0f, pickupRadius) + kQLineWidth * 0.5f;
}

inline Vec3 QEndpointThroughFlag(const Vec3& origin,
                                 const Vec3& flag,
                                 float qRange = kQRange) {
    const Vec3 direction = Direction2D(origin, flag);
    if (direction.IsZero()) return {};
    Vec3 result = origin + direction * std::max(0.0f, qRange);
    result.y = origin.y;
    return result;
}

inline float EQDashSeconds(const Vec3& origin,
                           const Vec3& flag,
                           float dashSpeed = kEQDashSpeed) {
    if (!origin.IsValid() || !flag.IsValid()) return FLT_MAX;
    return origin.Distance2D(flag) / std::max(1.0f, dashSpeed);
}

inline bool EQKnocksUp(const Vec3& origin,
                       const Vec3& flag,
                       const Vec3& target,
                       float targetRadius = 0.0f) {
    if (!origin.IsValid() || !flag.IsValid() || !target.IsValid()) return false;
    const auto projection = ProjectPointToSegment2D(target, origin, flag);
    return projection.T >= 0.0f && projection.T <= 1.0f &&
           projection.Distance <= kEQKnockupRadius +
               std::clamp(targetRadius, 0.0f, 150.0f);
}

inline Vec3 ClampFlagPosition(const Vec3& origin,
                              const Vec3& desired,
                              float maximumDistance = kFlagCastRange) {
    const Vec3 direction = Direction2D(origin, desired);
    if (direction.IsZero()) return origin;
    const float distance = std::min(
        origin.Distance2D(desired), std::max(0.0f, maximumDistance));
    Vec3 result = origin + direction * distance;
    result.y = desired.y;
    return result;
}

inline Vec3 CataclysmLandingEndpoint(const Vec3& source,
                                     const Vec3& arenaCenter,
                                     float jarvanRadius = 65.0f,
                                     float targetRadius = 65.0f) {
    const Vec3 direction = Direction2D(source, arenaCenter);
    if (direction.IsZero()) return source;
    const float separation = std::clamp(
        std::max(0.0f, jarvanRadius) + std::max(0.0f, targetRadius),
        70.0f, 190.0f);
    Vec3 endpoint = arenaCenter - direction * separation;
    endpoint.y = arenaCenter.y;
    return endpoint;
}

inline bool ArenaContains(const Vec3& center,
                          const Vec3& position,
                          float unitRadius = 0.0f,
                          float arenaRadius = kArenaRadius) {
    return center.IsValid() && position.IsValid() &&
           center.Distance2D(position) <= std::max(0.0f, arenaRadius) +
               std::clamp(unitRadius, 0.0f, 150.0f);
}

inline int ArenaHitCount(const Vec3& center,
                         const std::vector<Vec3>& positions,
                         float unitRadius = 0.0f) {
    int count = 0;
    for (const auto& position : positions) {
        if (ArenaContains(center, position, unitRadius)) ++count;
    }
    return count;
}

struct ArenaSafetyContext {
    bool CenterValid = false;
    bool LandingWalkable = false;
    bool LandingUnderEnemyTurret = false;
    bool StartingUnderEnemyTurret = false;
    bool PointClickThreat = false;
    bool DashHazard = false;
    bool Lethal = false;
    bool Defensive = false;
    int NearbyEnemies = 0;
    int NearbyAllies = 0;
    int TrappedAllies = 0;
    int WalkableInteriorSamples = 0;
    int MaximumEnemies = 3;
};

inline bool CataclysmSafe(const ArenaSafetyContext& context) {
    if (!context.CenterValid || !context.LandingWalkable) return false;
    if (context.WalkableInteriorSamples < 4) return false;
    if (context.LandingUnderEnemyTurret &&
        !context.StartingUnderEnemyTurret && !context.Lethal) return false;
    if ((context.PointClickThreat || context.DashHazard) &&
        !context.Lethal && !context.Defensive) return false;
    if (context.NearbyEnemies > std::max(1, context.MaximumEnemies) &&
        context.NearbyAllies + (context.Lethal ? 1 : 0) <
            context.NearbyEnemies) return false;
    if (context.TrappedAllies > 0 && !context.Lethal &&
        context.NearbyEnemies < 2 && !context.Defensive) return false;
    return true;
}

inline bool ShouldCollapseArena(bool active,
                                int elapsedMs,
                                bool fleeing,
                                bool allyNeedsExit,
                                bool targetStillContained,
                                bool targetKillable,
                                int nearbyEnemies) {
    if (!active || elapsedMs < 250) return false;
    if (fleeing || allyNeedsExit) return true;
    if (!targetStillContained && !targetKillable) return true;
    return nearbyEnemies >= 4 && !targetKillable;
}

} // namespace Plugins::KuroAIO::AI::Controllers::JarvanIV::Geometry
