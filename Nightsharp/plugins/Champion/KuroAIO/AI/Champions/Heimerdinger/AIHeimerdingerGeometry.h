#pragma once

#include "../../AIGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace Plugins::KuroAIO::AI::Controllers::Heimerdinger::Geometry {

using Vec3 = ::Vec3;
using SharedGeometry::Direction2D;

inline constexpr float kQPlacementRange = 350.0f;
inline constexpr float kQTurretZoneRadius = 900.0f;
inline constexpr float kQTurretAttackRadius = 625.0f;
inline constexpr float kQTurretCollisionRadius = 42.0f;
inline constexpr float kQRechargeSeconds = 20.0f;
inline constexpr int kMaximumTurrets = 3;
inline constexpr float kWRange = 1325.0f;
inline constexpr int kWRockets = 5;
inline constexpr float kWRocketWidth = 60.0f;
inline constexpr float kWRocketSpeed = 902.0f;
inline constexpr float kWDelay = 0.25f;
inline constexpr float kERange = 925.0f;
inline constexpr float kEGrenadeRadius = 100.0f;
inline constexpr float kEStunRadius = 100.0f;
inline constexpr float kEStunSeconds = 1.5f;
inline constexpr float kESpeed = 2500.0f;
inline constexpr float kEDelay = 0.25f;
inline constexpr float kRPlacementRange = 450.0f;
inline constexpr float kRTurretRadius = 400.0f;
inline constexpr float kRTurretAttackRadius = 625.0f;
inline constexpr int kRMaximumTurrets = 1;
inline constexpr float RankValue(int rank, const std::array<float, 5>& values) {
    return values[std::clamp(rank, 1, 5) - 1];
}
inline constexpr float RankValue(int rank, const std::array<float, 3>& values) {
    return values[std::clamp(rank, 1, 3) - 1];
}

inline constexpr float ERawDamage(int rank, float abilityPower) {
    return RankValue(rank, {60.0f, 100.0f, 140.0f, 180.0f, 220.0f}) +
           0.60f * std::max(0.0f, abilityPower);
}

inline constexpr float WRawSingleRocketDamage(int rank, float abilityPower) {
    return RankValue(rank, {50.0f, 75.0f, 100.0f, 125.0f, 150.0f}) +
           0.55f * std::max(0.0f, abilityPower);
}

inline constexpr float WRawFiveRocketDamage(int rank, float abilityPower) {
    return WRawSingleRocketDamage(rank, abilityPower) +
           4.0f * (RankValue(rank, {10.0f, 15.0f, 20.0f, 25.0f, 30.0f}) +
                   0.12f * std::max(0.0f, abilityPower));
}

inline constexpr float RRawTurretDamage(int rank, float abilityPower) {
    return RankValue(rank, std::array<float, 3>{150.0f, 250.0f, 350.0f}) +
           0.45f * std::max(0.0f, abilityPower);
}

inline constexpr float RRawGrenadeDamage(int rank, float abilityPower) {
    return RankValue(rank, std::array<float, 3>{100.0f, 200.0f, 300.0f}) +
           0.60f * std::max(0.0f, abilityPower);
}

enum class UpgradeChoice : std::uint8_t { None, Turret, Rockets, Grenade };

struct TurretObject {
    int NetworkId = 0;
    Vec3 Position = {};
    int CreatedTick = 0;
    int LastSeenTick = 0;
    int ExpireTick = 0;
    bool Valid = false;
    bool Super = false;
};

struct TurretZone {
    Vec3 Center = {};
    int TurretCount = 0;
    int ReadyTurretCount = 0;
    bool HasSuperTurret = false;
};

struct PlacementContext {
    bool Ready = false;
    bool PositionValid = false;
    bool InRange = false;
    bool OnWall = false;
    bool UnderEnemyTurret = false;
    bool UnsafeEnemyDensity = false;
    bool PreserveAutoAttack = true;
    int TurretCount = 0;
    int MaximumTurrets = kMaximumTurrets;
};

inline bool ShouldPlaceTurret(const PlacementContext& c, bool defensive = false,
                              bool lethal = false) {
    if (!c.Ready || !c.PositionValid || !c.InRange || c.OnWall) return false;
    if (c.TurretCount >= std::max(1, c.MaximumTurrets) && !lethal) return false;
    if (c.UnsafeEnemyDensity && !defensive && !lethal) return false;
    if (c.UnderEnemyTurret && !defensive && !lethal) return false;
    return true;
}

inline bool PointInTurretZone(const TurretZone& zone, const Vec3& point,
                              float padding = 0.0f) {
    return zone.Center.IsValid() && !zone.Center.IsZero() && point.IsValid() &&
           !point.IsZero() && zone.Center.Distance2D(point) <=
               kQTurretZoneRadius + std::max(0.0f, padding);
}

inline bool TurretZoneSafe(const TurretZone& zone, const Vec3& playerPosition,
                           int nearbyEnemies, int maximumEnemies,
                           bool defensive, bool lethal) {
    if (!PointInTurretZone(zone, playerPosition, 0.0f)) return false;
    if (defensive || lethal) return true;
    return nearbyEnemies <= std::max(0, maximumEnemies);
}

inline Vec3 ClampPlacement(const Vec3& origin, const Vec3& requested,
                           float range = kQPlacementRange) {
    const Vec3 direction = Direction2D(origin, requested);
    if (direction.IsZero() || origin.IsZero() || !origin.IsValid()) return {};
    return origin + direction * std::min(std::max(0.0f, range),
                                         origin.Distance2D(requested));
}

inline bool SegmentHits(const Vec3& start, const Vec3& end, const Vec3& point,
                        float radius) {
    if (!start.IsValid() || !end.IsValid() || !point.IsValid() ||
        end.IsZero() || point.IsZero()) return false;
    const auto projection = SharedGeometry::ProjectPointToSegment2D(
        point, start, end);
    return projection.Distance <= std::max(0.0f, radius);
}

inline float RocketTravelSeconds(const Vec3& start, const Vec3& end) {
    return start.IsValid() && end.IsValid() && !end.IsZero()
        ? start.Distance2D(end) / kWRocketSpeed + kWDelay : 0.0f;
}

inline bool RocketLineClear(const Vec3& start, const Vec3& end,
                           const Vec3* blockers, std::size_t blockerCount,
                           float targetRadius = 0.0f) {
    if (!start.IsValid() || !end.IsValid() || end.IsZero() ||
        start.Distance2D(end) > kWRange + 0.01f) return false;
    for (std::size_t i = 0; i < blockerCount; ++i) {
        if (SegmentHits(start, end, blockers[i], kWRocketWidth * 0.5f +
                                                     std::max(0.0f, targetRadius)))
            return false;
    }
    return true;
}

inline bool GrenadeCenterHit(const Vec3& center, const Vec3& target,
                             float targetRadius = 0.0f) {
    return center.IsValid() && !center.IsZero() && target.IsValid() &&
           !target.IsZero() && center.Distance2D(target) <=
               kEStunRadius + std::max(0.0f, targetRadius);
}

inline bool GrenadeOuterHit(const Vec3& center, const Vec3& target,
                            float targetRadius = 0.0f) {
    return center.IsValid() && !center.IsZero() && target.IsValid() &&
           !target.IsZero() && center.Distance2D(target) <=
               kEGrenadeRadius + std::max(0.0f, targetRadius);
}

struct GrenadeContext {
    bool Ready = false;
    bool TargetValid = false;
    bool PredictionHigh = false;
    bool ProjectileBlocked = false;
    bool CenterHit = false;
    bool UnderEnemyTurret = false;
    bool Defensive = false;
    bool Lethal = false;
};

inline bool ShouldThrowGrenade(const GrenadeContext& c) {
    if (!c.Ready || !c.TargetValid || !c.PredictionHigh ||
        c.ProjectileBlocked || !c.CenterHit) return false;
    return c.Defensive || c.Lethal || !c.UnderEnemyTurret;
}

struct RocketContext {
    bool Ready = false;
    bool TargetValid = false;
    bool PredictionHigh = false;
    bool ProjectileBlocked = false;
    bool Lethal = false;
    bool Defensive = false;
    bool Manual = false;
    bool AttackWindingUp = false;
};

inline bool ShouldFireRockets(const RocketContext& c) {
    if (!c.Ready || !c.TargetValid || !c.PredictionHigh ||
        c.ProjectileBlocked) return false;
    return c.Lethal || c.Defensive || c.Manual || !c.AttackWindingUp;
}

inline UpgradeChoice ChooseUpgrade(bool grenadeStun, bool multiTarget,
                                   bool killable, bool defensive,
                                   int turretCount, int minimumTurrets) {
    if (defensive && turretCount < minimumTurrets) return UpgradeChoice::Turret;
    if (grenadeStun && killable) return UpgradeChoice::Grenade;
    if (multiTarget) return UpgradeChoice::Rockets;
    if (killable) return UpgradeChoice::Grenade;
    return turretCount < minimumTurrets ? UpgradeChoice::Turret : UpgradeChoice::Rockets;
}

inline bool FleeZoneSafe(const TurretZone& zone, const Vec3& playerPosition,
                         const Vec3& destination, int nearbyEnemies) {
    return PointInTurretZone(zone, playerPosition, 0.0f) &&
           PointInTurretZone(zone, destination, 80.0f) && nearbyEnemies <= 2;
}

} // namespace Plugins::KuroAIO::AI::Controllers::Heimerdinger::Geometry
