#pragma once

// Pure Pantheon mechanics and geometry used by both the live controller and
// the standalone test. No SDK objects or live memory are consulted here.

#include "../AIGeometry.h"

#include <algorithm>
#include <cmath>

namespace Plugins::KuroAIO::AI::Controllers::Pantheon::Geometry {

using SharedGeometry::Direction2D;
using SharedGeometry::ProjectPointToSegment2D;

inline constexpr float kTapQRange = 575.0f;
inline constexpr float kThrowQRange = 1200.0f;
inline constexpr float kQRadius = 60.0f;
inline constexpr float kMinimumThrowHoldSeconds = 0.35f;
inline constexpr float kMaximumChargeSeconds = 0.80f;
inline constexpr float kWRange = 600.0f;
inline constexpr float kESpearRadius = 525.0f;
inline constexpr float kEShieldSlamRadius = 375.0f;
inline constexpr float kRRange = 5500.0f;
inline constexpr float kRFullDamageRadius = 125.0f;
inline constexpr float kRDamageRadius = 450.0f;

inline float EffectiveDistance(float centerDistance, float targetRadius) {
    return std::max(0.0f, centerDistance - std::max(0.0f, targetRadius));
}

enum class QCastStyle {
    None,
    Tap,
    Throw,
};

inline QCastStyle QStyleForDistance(float centerDistance,
                                    float targetRadius = 0.0f) {
    const float distance = EffectiveDistance(centerDistance, targetRadius);
    if (!std::isfinite(distance)) return QCastStyle::None;
    if (distance <= kTapQRange) return QCastStyle::Tap;
    if (distance <= kThrowQRange) return QCastStyle::Throw;
    return QCastStyle::None;
}

inline float QBodyDamageMultiplier(int bodyIndex) {
    return bodyIndex <= 0 ? 1.0f : 0.5f;
}

inline bool QLineHits(const Vec3& source,
                      const Vec3& castPosition,
                      const Vec3& targetPosition,
                      float targetRadius,
                      float maximumRange) {
    if (!source.IsValid() || !castPosition.IsValid() ||
        !targetPosition.IsValid() || maximumRange <= 0.0f) {
        return false;
    }
    const Vec3 direction = Direction2D(source, castPosition);
    if (direction.IsZero()) return false;
    const Vec3 end = source + direction * maximumRange;
    const auto projection = ProjectPointToSegment2D(targetPosition, source, end);
    return projection.Distance <= kQRadius + std::max(0.0f, targetRadius) &&
           targetPosition.Distance2D(source) <= maximumRange +
               std::max(0.0f, targetRadius);
}

struct QTapContext {
    bool Ready = false;
    bool PredictionHigh = false;
    bool InTapRange = false;
    bool AttackWindingUp = false;
    bool Lethal = false;
    bool ManualOwnership = false;
};

inline bool ShouldTapQ(const QTapContext& context) {
    return context.Ready && context.PredictionHigh && context.InTapRange &&
           !context.ManualOwnership &&
           (!context.AttackWindingUp || context.Lethal);
}

struct QChargeStartContext {
    bool Ready = false;
    bool PredictionHigh = false;
    bool InThrowRange = false;
    bool OutsideTapRange = false;
    bool Execute = false;
    bool FirstBodyClear = false;
    bool AttackWindingUp = false;
    bool ManualOwnership = false;
};

inline bool ShouldStartQCharge(const QChargeStartContext& context) {
    if (!context.Ready || !context.PredictionHigh ||
        !context.InThrowRange || context.ManualOwnership) {
        return false;
    }
    if (context.AttackWindingUp && !context.Execute) return false;
    if (context.Execute && !context.FirstBodyClear) return false;
    return context.OutsideTapRange || context.Execute;
}

struct QChargeReleaseContext {
    bool Charging = false;
    float ElapsedSeconds = 0.0f;
    bool PredictionHigh = false;
    bool InThrowRange = false;
    bool ProjectileWall = false;
    bool FirstBodyClear = false;
    bool PreserveFullDamage = false;
};

inline bool ShouldReleaseQCharge(const QChargeReleaseContext& context) {
    if (!context.Charging ||
        context.ElapsedSeconds < kMinimumThrowHoldSeconds ||
        !context.PredictionHigh || !context.InThrowRange ||
        context.ProjectileWall) {
        return false;
    }
    return !context.PreserveFullDamage || context.FirstBodyClear ||
           context.ElapsedSeconds >= kMaximumChargeSeconds;
}

inline float FacingDot(const Vec3& defender,
                       const Vec3& facingPoint,
                       const Vec3& damageSource) {
    const Vec3 facing = Direction2D(defender, facingPoint);
    const Vec3 source = Direction2D(defender, damageSource);
    if (facing.IsZero() || source.IsZero()) return -1.0f;
    return facing.Dot(source);
}

// Aegis Assault blocks damage whose source lies in Pantheon's forward
// hemisphere. Boundary tolerance avoids oscillation on exactly lateral hits.
inline bool DirectionalShieldBlocks(const Vec3& defender,
                                    const Vec3& facingPoint,
                                    const Vec3& damageSource,
                                    float boundaryTolerance = -0.02f) {
    return FacingDot(defender, facingPoint, damageSource) >= boundaryTolerance;
}

struct EContext {
    bool Ready = false;
    bool ThreatCommitted = false;
    bool SourceInFront = false;
    bool PlayerLow = false;
    bool Fleeing = false;
    bool AttackWindingUp = false;
    bool ManualOwnership = false;
};

inline bool ShouldCastE(const EContext& context) {
    return context.Ready && !context.ManualOwnership &&
           (context.ThreatCommitted || context.PlayerLow || context.Fleeing) &&
           context.SourceInFront &&
           (!context.AttackWindingUp || context.ThreatCommitted ||
            context.PlayerLow);
}

struct VaultContext {
    bool Ready = false;
    bool TargetValid = false;
    bool InRange = false;
    bool EndpointWalkable = false;
    bool TargetSpellShield = false;
    bool EnemyTurret = false;
    bool Lethal = false;
    int NearbyEnemies = 0;
    int MaximumEnemies = 2;
    bool PlayerLow = false;
};

inline bool VaultSafe(const VaultContext& context) {
    if (!context.Ready || !context.TargetValid || !context.InRange ||
        !context.EndpointWalkable || context.TargetSpellShield) {
        return false;
    }
    if (context.EnemyTurret && !context.Lethal) return false;
    if (context.PlayerLow && !context.Lethal) return false;
    return context.NearbyEnemies <= std::max(1, context.MaximumEnemies) ||
           context.Lethal;
}

enum class EmpoweredSpell {
    None,
    Q,
    W,
    E,
};

struct PassiveContext {
    bool Empowered = false;
    bool DefensiveThreat = false;
    bool EReady = false;
    bool SafeWEngage = false;
    bool WReady = false;
    bool QReady = false;
    bool QReachable = false;
    bool QExecute = false;
};

inline EmpoweredSpell ChooseEmpoweredSpell(const PassiveContext& context) {
    if (!context.Empowered) return EmpoweredSpell::None;
    if (context.DefensiveThreat && context.EReady) return EmpoweredSpell::E;
    if (context.SafeWEngage && context.WReady && !context.QExecute) {
        return EmpoweredSpell::W;
    }
    if (context.QReady && context.QReachable) return EmpoweredSpell::Q;
    if (context.SafeWEngage && context.WReady) return EmpoweredSpell::W;
    return EmpoweredSpell::None;
}

inline float RDamageScale(float distanceFromCenter) {
    if (!std::isfinite(distanceFromCenter) || distanceFromCenter < 0.0f ||
        distanceFromCenter > kRDamageRadius) {
        return 0.0f;
    }
    if (distanceFromCenter <= kRFullDamageRadius) return 1.0f;
    const float progress = (distanceFromCenter - kRFullDamageRadius) /
        (kRDamageRadius - kRFullDamageRadius);
    return 1.0f - 0.5f * std::clamp(progress, 0.0f, 1.0f);
}

struct RLandingContext {
    bool ManualRequested = false;
    bool DestinationValid = false;
    bool DestinationWalkable = false;
    bool InRange = false;
    bool BeyondLocalCombat = false;
    bool TargetPredictedInside = false;
    bool EnemyTurret = false;
    bool EscapeRoute = false;
    bool Lethal = false;
    int NearbyEnemies = 0;
    int MaximumEnemies = 2;
    int AlliedFollowup = 0;
};

inline bool RLandingSafe(const RLandingContext& context) {
    if (!context.ManualRequested || !context.DestinationValid ||
        !context.DestinationWalkable || !context.InRange ||
        !context.BeyondLocalCombat || !context.TargetPredictedInside) {
        return false;
    }
    if (context.EnemyTurret && !context.Lethal) return false;
    if (context.NearbyEnemies > std::max(1, context.MaximumEnemies) &&
        !context.Lethal) {
        return false;
    }
    return context.Lethal || context.AlliedFollowup > 0 ||
           context.EscapeRoute;
}

struct AutomaticContext {
    bool Defensive = false;
    bool Interrupt = false;
    bool KillSecure = false;
    bool Engage = false;
};

inline bool AutomaticAllowed(const AutomaticContext& context) {
    return !context.Engage &&
           (context.Defensive || context.Interrupt || context.KillSecure);
}

} // namespace Plugins::KuroAIO::AI::Controllers::Pantheon::Geometry
