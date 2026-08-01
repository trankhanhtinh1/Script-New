#pragma once

#include "../../AIGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace Plugins::KuroAIO::AI::Controllers::Ziggs::Geometry {

using Vec3 = ::Vec3;
using SharedGeometry::Direction2D;

// Riot 26.15 / CommunityDragon 16.15 Summoner's Rift spell geometry.
inline constexpr float kQRange = 850.0f;
inline constexpr float kQWidth = 140.0f;
inline constexpr float kQExplosionRadius = 150.0f;
inline constexpr float kQDelay = 0.25f;
inline constexpr float kQSpeed = 1700.0f;
inline constexpr float kWRange = 1000.0f;
inline constexpr float kWSatchelRadius = 275.0f;
inline constexpr float kWSelfDisplacement = 400.0f;
inline constexpr float kWEnemyDisplacement = 325.0f;
inline constexpr float kWDelay = 0.25f;
inline constexpr float kERange = 900.0f;
inline constexpr float kEMinefieldRadius = 250.0f;
inline constexpr float kEMineRadius = 75.0f;
inline constexpr int kEMineCount = 11;
inline constexpr float kEDelay = 0.25f;
inline constexpr float kEArmingSeconds = 0.4f;
inline constexpr float kEDurationSeconds = 10.0f;
inline constexpr float kRRange = 5300.0f;
inline constexpr float kRRadius = 500.0f;
inline constexpr float kRCenterRadius = 250.0f;
inline constexpr float kRDelay = 0.375f;
inline constexpr float kRSpeed = 1550.0f;

inline constexpr float RankValue(int rank, const std::array<float, 5>& values) {
    return values[std::clamp(rank, 1, 5) - 1];
}
inline constexpr float RankValue(int rank, const std::array<float, 3>& values) {
    return values[std::clamp(rank, 1, 3) - 1];
}
inline constexpr float QRawDamage(int rank, float abilityPower) {
    return RankValue(rank, {85.0f, 135.0f, 185.0f, 235.0f, 285.0f}) +
           0.65f * std::max(0.0f, abilityPower);
}
inline constexpr float WRawDamage(int rank, float abilityPower) {
    return RankValue(rank, {70.0f, 105.0f, 140.0f, 175.0f, 210.0f}) +
           0.35f * std::max(0.0f, abilityPower);
}
inline constexpr float ERawDamage(int rank, float abilityPower) {
    return RankValue(rank, {30.0f, 70.0f, 110.0f, 150.0f, 190.0f}) +
           0.30f * std::max(0.0f, abilityPower);
}
inline constexpr float RRawDamage(int rank, float abilityPower) {
    return RankValue(rank, std::array<float, 3>{200.0f, 300.0f, 400.0f}) +
           0.73f * std::max(0.0f, abilityPower);
}

inline bool SegmentHits(const Vec3& start, const Vec3& end, const Vec3& target,
                        float halfWidth, float targetRadius = 0.0f) {
    if (start.IsZero() || end.IsZero() || target.IsZero()) return false;
    const auto projection = SharedGeometry::ProjectPointToSegment2D(target, start, end);
    return projection.T >= 0.0f && projection.T <= 1.0f &&
           projection.Distance <= std::max(0.0f, halfWidth) +
               std::max(0.0f, targetRadius);
}

inline bool QHits(const Vec3& origin, const Vec3& aim, const Vec3& target,
                  float targetRadius = 0.0f) {
    const Vec3 direction = Direction2D(origin, aim);
    if (direction.IsZero() || origin.Distance2D(aim) > kQRange + 0.01f) return false;
    const Vec3 end = origin + direction * std::min(kQRange, origin.Distance2D(aim));
    return SegmentHits(origin, end, target, kQWidth * 0.5f, targetRadius) ||
           target.Distance2D(end) <= kQExplosionRadius + std::max(0.0f, targetRadius);
}

inline bool CircleContains(const Vec3& center, const Vec3& point, float radius,
                           float pointRadius = 0.0f) {
    return !center.IsZero() && !point.IsZero() &&
           center.Distance2D(point) <= std::max(0.0f, radius) +
               std::max(0.0f, pointRadius);
}

inline Vec3 ClampPosition(const Vec3& origin, const Vec3& requested, float range) {
    const Vec3 direction = Direction2D(origin, requested);
    if (direction.IsZero()) return {};
    return origin + direction * std::min(std::max(0.0f, range), origin.Distance2D(requested));
}

inline Vec3 SatchelDisplacement(const Vec3& player, const Vec3& center,
                                bool towardCenter) {
    const Vec3 direction = Direction2D(center, player);
    if (direction.IsZero()) return {};
    return player + direction * (towardCenter ? -kWSelfDisplacement : kWSelfDisplacement);
}

struct SatchelSafetyContext {
    bool Ready = false;
    bool PositionValid = false;
    bool ProjectileBlocked = false;
    bool EndpointWall = false;
    bool EndpointTurret = false;
    int EnemiesAtEndpoint = 0;
    int MaximumEnemies = 1;
    bool Defensive = false;
    bool Lethal = false;
};
inline bool ShouldSatchel(const SatchelSafetyContext& c) {
    if (!c.Ready || !c.PositionValid || c.ProjectileBlocked || c.EndpointWall ||
        (c.EndpointTurret && !c.Defensive && !c.Lethal)) return false;
    return c.Defensive || c.Lethal || c.EnemiesAtEndpoint <= std::max(0, c.MaximumEnemies);
}

struct MinefieldContext {
    bool Ready = false;
    bool PositionValid = false;
    bool ProjectileBlocked = false;
    bool UnderEnemyTurret = false;
    bool Defensive = false;
    bool Objective = false;
    int PredictedTargets = 0;
    int MinimumTargets = 1;
};
inline bool ShouldMinefield(const MinefieldContext& c) {
    if (!c.Ready || !c.PositionValid || c.ProjectileBlocked) return false;
    if (c.UnderEnemyTurret && !c.Defensive && !c.Objective) return false;
    return c.Defensive || c.Objective || c.PredictedTargets >= std::max(1, c.MinimumTargets);
}

struct UltimateContext {
    bool Ready = false;
    bool TargetValid = false;
    bool PredictionHits = false;
    bool ProjectileBlocked = false;
    bool Lethal = false;
    bool Defensive = false;
    bool Manual = false;
    bool AttackWindingUp = false;
    int PredictedTargets = 0;
    int MinimumTargets = 2;
};
inline bool ShouldCastMega(const UltimateContext& c) {
    if (!c.Ready || !c.TargetValid || !c.PredictionHits || c.ProjectileBlocked) return false;
    if (c.AttackWindingUp && !c.Lethal && !c.Defensive && !c.Manual) return false;
    return c.Lethal || c.Defensive || c.Manual ||
           c.PredictedTargets >= std::max(1, c.MinimumTargets);
}
inline bool RCenterHit(const Vec3& center, const Vec3& target, float radius = 0.0f) {
    return CircleContains(center, target, kRCenterRadius, radius);
}
inline bool ROuterHit(const Vec3& center, const Vec3& target, float radius = 0.0f) {
    return CircleContains(center, target, kRRadius, radius);
}

struct AutomaticContext {
    bool Defensive = false;
    bool Interrupt = false;
    bool KillSecure = false;
    bool ManualOwnership = false;
};
inline bool AutomaticAllowed(const AutomaticContext& c) {
    return !c.ManualOwnership && (c.Defensive || c.Interrupt || c.KillSecure);
}

} // namespace Plugins::KuroAIO::AI::Controllers::Ziggs::Geometry
