#pragma once

#include "../AIGeometry.h"

#include <algorithm>
#include <cmath>

namespace Plugins::KuroAIO::AI::Controllers::Jhin::Geometry {

inline constexpr float kCurtainHalfAngleRadians =
    30.0f * SharedGeometry::kPi / 180.0f;

inline bool InsideCurtainCone(const Vec3& origin,
                              const Vec3& direction,
                              const Vec3& point,
                              float range = 3400.0f,
                              float halfAngle = kCurtainHalfAngleRadians) {
    const Vec3 toPoint = SharedGeometry::Direction2D(origin, point);
    const float distance = origin.Distance2D(point);
    if (direction.IsZero() || toPoint.IsZero() || distance > range) return false;
    const float dot = std::clamp(direction.Dot(toPoint), -1.0f, 1.0f);
    return dot >= std::cos(std::max(0.0f, halfAngle));
}

struct GrenadeContext {
    bool InRange = false;
    bool AttackWindingUp = false;
    bool AttackAvailable = false;
    bool AfterAttack = false;
    bool Reloading = false;
    bool Lethal = false;
};

inline bool ShouldCastGrenade(const GrenadeContext& context) {
    if (!context.InRange || context.AttackWindingUp) return false;
    return context.Reloading || context.AfterAttack ||
           !context.AttackAvailable || context.Lethal;
}

struct FlourishContext {
    bool InRange = false;
    bool PredictionHits = false;
    bool FirstChampionIsTarget = false;
    bool ProjectileWall = false;
    bool Marked = false;
    bool Immobilized = false;
    bool Lethal = false;
    bool AttackAvailable = false;
    bool Reloading = false;
};

inline bool ShouldCastFlourish(const FlourishContext& context) {
    if (!context.InRange || !context.PredictionHits ||
        !context.FirstChampionIsTarget || context.ProjectileWall) {
        return false;
    }
    if (context.AttackAvailable && !context.Reloading &&
        !context.Lethal && !context.Immobilized) {
        return false;
    }
    return context.Marked || context.Immobilized || context.Lethal;
}

struct TrapContext {
    bool InRange = false;
    bool AmmoReady = false;
    bool ExistingTrapNear = false;
    bool Immobilized = false;
    bool Dashing = false;
    bool Gapcloser = false;
    bool Committed = false;
};

inline bool ShouldPlaceTrap(const TrapContext& context) {
    return context.InRange && context.AmmoReady &&
           !context.ExistingTrapNear &&
           (context.Immobilized || context.Dashing ||
            context.Gapcloser || context.Committed);
}

struct CurtainShotContext {
    bool InCone = false;
    bool PredictionVeryHigh = false;
    bool FirstChampionIsTarget = false;
    bool ProjectileWall = false;
    bool TargetDamageable = false;
    bool Lethal = false;
    bool Marked = false;
    float HealthPercent = 100.0f;
};

inline float CurtainShotScore(const CurtainShotContext& context) {
    if (!context.InCone || !context.PredictionVeryHigh ||
        !context.FirstChampionIsTarget || context.ProjectileWall ||
        !context.TargetDamageable) {
        return -100000.0f;
    }
    float score = 100.0f - std::clamp(context.HealthPercent, 0.0f, 100.0f);
    if (context.Lethal) score += 500.0f;
    if (context.Marked) score += 35.0f;
    return score;
}

} // namespace Plugins::KuroAIO::AI::Controllers::Jhin::Geometry
