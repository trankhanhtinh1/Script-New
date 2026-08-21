#pragma once

#include "../../AIGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace Plugins::KuroAIO::AI::Controllers::Trundle::Geometry {

using Vec3 = ::Vec3;
using SharedGeometry::Direction2D;

inline constexpr float kQRange = 300.0f;
inline constexpr float kQDelay = 0.0f;
inline constexpr float kWRange = 750.0f;
inline constexpr float kWRadius = 360.0f;
inline constexpr float kWDuration = 8.0f;
inline constexpr float kERange = 1000.0f;
inline constexpr float kEPillarRadius = 225.0f;
inline constexpr float kEDisplacementRadius = 225.0f;
inline constexpr float kRRange = 650.0f;

inline constexpr float RankValue(int rank, const std::array<float, 5>& values) {
    return values[std::clamp(rank, 1, 5) - 1];
}

inline constexpr float QRawDamage(int rank, float attackDamage) {
    return RankValue(rank, {10.0f, 20.0f, 30.0f, 40.0f, 50.0f}) +
        RankValue(rank, {0.15f, 0.25f, 0.35f, 0.45f, 0.55f}) *
            std::max(0.0f, attackDamage);
}

inline constexpr float QAttackDamageSteal(int rank) {
    return RankValue(rank, {20.0f, 25.0f, 30.0f, 35.0f, 40.0f});
}

inline constexpr float WAttackSpeedPercent(int rank) {
    return RankValue(rank, {30.0f, 50.0f, 70.0f, 90.0f, 110.0f});
}

inline constexpr float RCurrentHealthPercent(int rank) {
    return RankValue(rank, {20.0f, 27.5f, 35.0f, 42.5f, 50.0f}) / 100.0f;
}

inline constexpr float RRawDamage(int rank, float targetCurrentHealth) {
    return RCurrentHealthPercent(rank) * std::max(0.0f, targetCurrentHealth);
}

inline constexpr float RResistanceStealPercent() { return 0.40f; }

inline bool InRange(const Vec3& origin, const Vec3& point, float range) {
    return origin.IsValid() && point.IsValid() && !origin.IsZero() &&
        !point.IsZero() && origin.Distance2D(point) <= std::max(0.0f, range) + 0.01f;
}

inline bool ZoneContains(const Vec3& center, const Vec3& point,
                         float radius = kWRadius) {
    return center.IsValid() && point.IsValid() && !center.IsZero() &&
        !point.IsZero() && center.Distance2D(point) <= std::max(0.0f, radius) + 0.01f;
}

inline Vec3 ClampZoneCast(const Vec3& origin, const Vec3& requested) {
    if (!origin.IsValid() || origin.IsZero() || !requested.IsValid() ||
        requested.IsZero()) return {};
    const Vec3 direction = Direction2D(origin, requested);
    if (direction.IsZero()) return origin;
    return origin + direction * std::min(kWRange, origin.Distance2D(requested));
}

inline bool PillarPlacementValid(const Vec3& origin, const Vec3& requested,
                                 bool destinationIsWall,
                                 bool underEnemyTurret,
                                 int enemiesAtPlacement,
                                 int maximumEnemies = 2) {
    return InRange(origin, requested, kERange) && !destinationIsWall &&
        !underEnemyTurret && enemiesAtPlacement <= std::max(0, maximumEnemies);
}

inline bool PillarAffectsTarget(const Vec3& pillar, const Vec3& target,
                                float targetRadius = 0.0f) {
    return ZoneContains(pillar, target,
        kEPillarRadius + std::max(0.0f, targetRadius));
}

inline bool PillarBlocksPath(const Vec3& start, const Vec3& end,
                             const Vec3& pillar, float targetRadius = 0.0f) {
    if (!start.IsValid() || !end.IsValid() || !pillar.IsValid() ||
        start.IsZero() || end.IsZero() || pillar.IsZero()) return false;
    return SharedGeometry::ProjectPointToSegment2D(pillar, start, end).Distance <=
        kEPillarRadius + std::max(0.0f, targetRadius);
}

inline Vec3 DisplacementAwayFromPillar(const Vec3& pillar, const Vec3& target,
                                       float distance = 225.0f) {
    if (!pillar.IsValid() || !target.IsValid() || pillar.IsZero() ||
        target.IsZero()) return {};
    const Vec3 direction = Direction2D(pillar, target);
    if (direction.IsZero()) return {};
    return target + direction * std::max(0.0f, distance);
}

struct ZoneState {
    Vec3 Center{};
    int CastTick = 0;
    int ExpireTick = 0;
};

inline void RecordZone(ZoneState& zone, const Vec3& center, int castTick) {
    zone.Center = center;
    zone.CastTick = castTick;
    zone.ExpireTick = castTick + static_cast<int>(kWDuration * 1000.0f);
}

inline bool ZoneActive(const ZoneState& zone, int now) {
    return !zone.Center.IsZero() && zone.ExpireTick > 0 &&
        now >= zone.CastTick && now <= zone.ExpireTick;
}

struct RTargetPolicy {
    bool Ready = false;
    bool TargetValid = false;
    bool TargetProtected = false;
    bool Lethal = false;
    bool TargetLow = false;
    bool UnderEnemyTurret = false;
    bool Defensive = false;
    int NearbyEnemies = 0;
    int MaximumNearbyEnemies = 3;
};

inline bool ShouldCastR(const RTargetPolicy& policy) {
    if (!policy.Ready || !policy.TargetValid || policy.TargetProtected ||
        policy.UnderEnemyTurret) return false;
    return policy.Lethal || policy.TargetLow || policy.Defensive;
}

struct AutomaticContext {
    bool IncomingThreat = false;
    bool KillSecure = false;
    bool DefensiveR = false;
};

inline bool AutomaticAllowed(const AutomaticContext& context) {
    return context.IncomingThreat || context.KillSecure || context.DefensiveR;
}

} // namespace Plugins::KuroAIO::AI::Controllers::Trundle::Geometry
