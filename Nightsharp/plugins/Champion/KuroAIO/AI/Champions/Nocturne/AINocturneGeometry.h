#pragma once

// Pure, live-memory-free mechanics used by Nocturne's controller and its
// standalone C++20 test. Runtime prediction, navmesh and spell state stay in
// the controller; this file owns only stable kit math and geometry.

#include "../../AIGeometry.h"

#include <algorithm>
#include <cmath>

namespace Plugins::KuroAIO::AI::Controllers::Nocturne::Geometry {

using SharedGeometry::ProjectPointToSegment2D;
using SharedGeometry::SegmentProjection;

inline constexpr float kPassiveCooldownSeconds = 13.0f;
inline constexpr float kPassiveCleaveRadius = 360.0f;
inline constexpr float kQRange = 1125.0f;
inline constexpr float kQMissileRadius = 60.0f;
inline constexpr float kETargetRange = 425.0f;
inline constexpr float kETetherRadius = 465.0f;

inline float PassiveCooldownAfterAttack(float remainingSeconds,
                                        bool championOrMonster,
                                        bool passiveWasConsumed) {
    if (passiveWasConsumed) return kPassiveCooldownSeconds;
    const float reduction = championOrMonster ? 3.0f : 1.0f;
    return std::max(0.0f, remainingSeconds - reduction);
}

inline float PassiveHealPerTarget(int level,
                                  float abilityPower,
                                  bool secondaryMinion) {
    const float base = 13.0f + static_cast<float>(std::clamp(level, 1, 18) - 1);
    const float heal = base + std::max(0.0f, abilityPower) * 0.30f;
    return heal * (secondaryMinion ? 0.5f : 1.0f);
}

inline float PassiveCleaveDamage(float totalAttackDamage,
                                 bool secondaryMinion) {
    const float damage = std::max(0.0f, totalAttackDamage) * 1.20f;
    return damage * (secondaryMinion ? 0.5f : 1.0f);
}

inline bool PassiveCleaveHits(const Vec3& attackCenter,
                              const Vec3& unitPosition,
                              float unitRadius = 0.0f) {
    if (!attackCenter.IsValid() || !unitPosition.IsValid()) return false;
    return attackCenter.Distance2D(unitPosition) <=
        kPassiveCleaveRadius + std::max(0.0f, unitRadius);
}

inline Vec3 ClampQEndpoint(const Vec3& source, const Vec3& requested) {
    if (!source.IsValid() || !requested.IsValid()) return {};
    Vec3 direction = SharedGeometry::Direction2D(source, requested);
    if (direction.IsZero()) return {};
    const float distance = std::min(kQRange, source.Distance2D(requested));
    return source + direction * distance;
}

inline bool QPathHits(const Vec3& source,
                      const Vec3& endpoint,
                      const Vec3& unitPosition,
                      float unitRadius = 0.0f) {
    if (!source.IsValid() || !endpoint.IsValid() ||
        !unitPosition.IsValid()) return false;
    const SegmentProjection projection = ProjectPointToSegment2D(
        unitPosition, source, ClampQEndpoint(source, endpoint));
    return projection.Distance <=
        kQMissileRadius + std::max(0.0f, unitRadius);
}

inline bool OnDuskTrail(const Vec3& position,
                        const Vec3& trailStart,
                        const Vec3& trailEnd,
                        float bodyRadius = 35.0f,
                        float trailPadding = 55.0f) {
    if (!position.IsValid() || !trailStart.IsValid() ||
        !trailEnd.IsValid()) return false;
    const SegmentProjection projection = ProjectPointToSegment2D(
        position, trailStart, trailEnd);
    return projection.Distance <= std::max(0.0f, bodyRadius) +
                                  std::max(0.0f, trailPadding);
}

inline float TetherMargin(const Vec3& nocturnePosition,
                          const Vec3& targetPosition,
                          float targetRadius = 0.0f) {
    if (!nocturnePosition.IsValid() || !targetPosition.IsValid()) {
        return -kETetherRadius;
    }
    return kETetherRadius + std::max(0.0f, targetRadius) -
           nocturnePosition.Distance2D(targetPosition);
}

inline bool TetherMaintained(const Vec3& nocturnePosition,
                             const Vec3& targetPosition,
                             float targetRadius = 0.0f) {
    return TetherMargin(nocturnePosition, targetPosition, targetRadius) >= 0.0f;
}

inline float ParanoiaRange(int rank) {
    constexpr float values[] = { 0.0f, 2500.0f, 3250.0f, 4000.0f };
    return values[std::clamp(rank, 0, 3)];
}

struct ParanoiaSafety {
    bool DestinationValid = false;
    bool InRange = false;
    bool TargetDamageable = false;
    bool Wall = false;
    bool Turret = false;
    bool PointClickLockdown = false;
    bool DashHazard = false;
    bool Lethal = false;
    bool Manual = false;
    int EnemiesAtLanding = 0;
    int AlliesAtLanding = 0;
    int MaximumEnemies = 2;
};

inline bool SafeParanoiaCommit(const ParanoiaSafety& context) {
    if (!context.DestinationValid || !context.InRange ||
        !context.TargetDamageable || context.Wall || context.Turret ||
        context.PointClickLockdown || context.DashHazard) {
        return false;
    }
    const int enemies = std::max(0, context.EnemiesAtLanding);
    const int alliesAfterArrival = std::max(0, context.AlliesAtLanding) + 1;
    if (enemies > std::max(1, context.MaximumEnemies)) return false;
    if (enemies > alliesAfterArrival && !context.Lethal) return false;
    return context.Manual || context.Lethal || enemies <= alliesAfterArrival;
}

} // namespace Plugins::KuroAIO::AI::Controllers::Nocturne::Geometry
