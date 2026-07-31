#pragma once

#include "../AIGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace Plugins::KuroAIO::AI::Controllers::Draven::Geometry {

using SharedGeometry::Direction2D;
using Vec3 = ::Vec3;

inline constexpr float kAxeCatchRadius = 145.0f;
inline constexpr float kAxeMaximumLandingRange = 1000.0f;
inline constexpr float kAxeLifetimeSeconds = 1.6f;
inline constexpr float kWMovementWindowSeconds = 1.5f;
inline constexpr float kERange = 1050.0f;
inline constexpr float kEHalfWidth = 65.0f;
inline constexpr float kRRange = 25000.0f;
inline constexpr float kRHalfWidth = 80.0f;
inline constexpr float kReturnSafetyRadius = 175.0f;

inline Vec3 ClampAxeLanding(const Vec3& origin,
                            const Vec3& requested,
                            float maximumRange = kAxeMaximumLandingRange) {
    if (!origin.IsValid() || origin.IsZero() ||
        !requested.IsValid() || requested.IsZero()) return {};
    const Vec3 direction = Direction2D(origin, requested);
    if (direction.IsZero()) return {};
    const float distance = std::max(0.0f, origin.Distance2D(requested));
    return origin + direction * std::min(maximumRange, distance);
}

inline bool AxeLandingValid(const Vec3& position,
                            const Vec3& owner,
                            float maximumRange = kAxeMaximumLandingRange) {
    return position.IsValid() && !position.IsZero() && owner.IsValid() &&
           !owner.IsZero() && owner.Distance2D(position) <= maximumRange + 0.01f;
}

inline bool AxeCatchReachable(const Vec3& player,
                              const Vec3& landing,
                              float movementBudget,
                              float catchRadius = kAxeCatchRadius) {
    if (!player.IsValid() || !landing.IsValid() || player.IsZero() ||
        landing.IsZero() || !std::isfinite(movementBudget)) return false;
    return player.Distance2D(landing) <=
           std::max(0.0f, movementBudget) + std::max(0.0f, catchRadius);
}

inline bool AxeCatchSafe(const Vec3& player,
                         const Vec3& landing,
                         int enemiesAtLanding,
                         int maximumEnemies,
                         bool underEnemyTurret,
                         bool wallAtLanding) {
    if (!player.IsValid() || !landing.IsValid() || player.IsZero() ||
        landing.IsZero()) return false;
    return enemiesAtLanding <= std::max(0, maximumEnemies) &&
           !underEnemyTurret && !wallAtLanding;
}

struct AxeState {
    int NetworkId = 0;
    Vec3 Position{};
    int SpawnTick = 0;
    int ExpireTick = 0;
    bool Active = false;
};

inline void RecordAxe(AxeState& axe,
                      int networkId,
                      const Vec3& position,
                      int spawnTick,
                      int lifetimeMs = 1600) {
    axe.NetworkId = networkId;
    axe.Position = position;
    axe.SpawnTick = spawnTick;
    axe.ExpireTick = spawnTick + std::max(1, lifetimeMs);
    axe.Active = AxeLandingValid(position, position, kAxeMaximumLandingRange);
}

inline bool AxeActive(const AxeState& axe, int now) {
    return axe.Active && axe.NetworkId != 0 && axe.Position.IsValid() &&
           !axe.Position.IsZero() && now >= axe.SpawnTick && now <= axe.ExpireTick;
}

inline void ClearAxe(AxeState& axe) { axe = {}; }

inline bool SegmentHits(const Vec3& start,
                        const Vec3& end,
                        const Vec3& target,
                        float halfWidth,
                        float targetRadius = 0.0f) {
    if (!start.IsValid() || !end.IsValid() || !target.IsValid()) return false;
    return SharedGeometry::ProjectPointToSegment2D(target, start, end).Distance <=
           std::max(0.0f, halfWidth) + std::max(0.0f, targetRadius);
}

inline Vec3 ReturnPosition(const Vec3& start,
                           const Vec3& end,
                           float progress) {
    if (!start.IsValid() || !end.IsValid()) return {};
    const float t = std::clamp(progress, 0.0f, 1.0f);
    return start + (end - start) * t;
}

inline bool ReturnPathHits(const Vec3& start,
                           const Vec3& end,
                           const Vec3& target,
                           float targetRadius = 0.0f) {
    return SegmentHits(start, end, target, kRHalfWidth, targetRadius);
}

struct ReturnContext {
    bool OutgoingHit = false;
    bool ReturnHit = false;
    bool LethalOnEitherPass = false;
    bool MultiTarget = false;
    bool Manual = false;
    bool CashInSafe = false;
    bool ProjectileWall = false;
    bool TargetEscaping = false;
};

inline bool ShouldCastReturn(const ReturnContext& context) {
    if (context.ProjectileWall) return false;
    if (context.Manual) return context.OutgoingHit || context.ReturnHit;
    return context.LethalOnEitherPass ||
           (context.MultiTarget && (context.OutgoingHit || context.ReturnHit)) ||
           (context.TargetEscaping && context.OutgoingHit && context.CashInSafe);
}

struct CashInContext {
    bool PassiveStacks = false;
    bool TargetLethal = false;
    bool SafeCatch = false;
    bool SelectedTarget = false;
    bool EnemyNearBase = false;
    bool Manual = false;
};

inline bool ShouldCashIn(const CashInContext& context) {
    if (!context.PassiveStacks || !context.TargetLethal || !context.SafeCatch) return false;
    if (context.EnemyNearBase && !context.Manual) return false;
    return context.SelectedTarget || context.Manual;
}

struct AttackPolicyContext {
    bool Windup = false;
    bool CatchReachable = false;
    bool CatchSafe = false;
    bool TargetKillable = false;
    bool SelectedTarget = false;
    bool AxeWouldExpire = false;
    bool ManualCastPending = false;
};

inline bool AllowAttackDuringWindup(const AttackPolicyContext& context) {
    if (context.ManualCastPending) return false;
    if (!context.Windup) return true;
    if (!context.CatchReachable || !context.CatchSafe) return true;
    return context.TargetKillable || context.SelectedTarget || context.AxeWouldExpire;
}

inline bool ReturnIntersectsTarget(const Vec3& outgoingStart,
                                   const Vec3& outgoingEnd,
                                   const Vec3& target,
                                   float radius = 0.0f) {
    return ReturnPathHits(outgoingEnd, outgoingStart, target, radius);
}

} // namespace Plugins::KuroAIO::AI::Controllers::Draven::Geometry
