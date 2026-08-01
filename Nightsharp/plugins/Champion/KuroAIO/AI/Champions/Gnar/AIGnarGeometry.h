#pragma once

#include "../../AIGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace Plugins::KuroAIO::AI::Controllers::Gnar::Geometry {

using SharedGeometry::Direction2D;
using SharedGeometry::ProjectPointToSegment2D;
using SharedGeometry::RankValue;

inline constexpr float kMiniQRange = 1100.0f;
inline constexpr float kMiniQMissileRange = 1125.0f;
inline constexpr float kMiniQReturnRange = 3000.0f;
inline constexpr float kMiniQWidth = 55.0f;
inline constexpr float kMiniQSpeed = 2500.0f;
inline constexpr float kMegaQRange = 1125.0f;
inline constexpr float kMegaQWidth = 90.0f;
inline constexpr float kMegaQSpeed = 2100.0f;
inline constexpr float kBoulderPickupRadius = 200.0f;
inline constexpr float kBoulderLifetimeSeconds = 6.0f;
inline constexpr float kWallopRange = 550.0f;
inline constexpr float kWallopWidth = 200.0f;
inline constexpr float kHopRange = 475.0f;
inline constexpr float kHopBounceRange = 525.0f;
inline constexpr float kCrunchRange = 675.0f;
inline constexpr float kGnarRadius = 475.0f;
inline constexpr float kGnarDisplayRange = 590.0f;
inline constexpr float kGnarKnockbackDistance = 500.0f;
inline constexpr float kGnarWallDamageMultiplier = 1.50f;
inline constexpr int kMegaDurationMs = 15000;
inline constexpr int kTiredDurationMs = 15000;

inline float MiniQDamage(int rank, float totalAttackDamage,
                         int previousChampionHits = 0) {
    static constexpr std::array<float, 6> base{
        0.0f, 5.0f, 45.0f, 85.0f, 125.0f, 165.0f
    };
    const float first = RankValue(base, rank) +
                        1.25f * std::max(0.0f, totalAttackDamage);
    return first * (previousChampionHits > 0 ? 0.50f : 1.0f);
}

inline float MegaQDamage(int rank, float totalAttackDamage) {
    static constexpr std::array<float, 6> base{
        0.0f, 45.0f, 90.0f, 135.0f, 180.0f, 225.0f
    };
    return RankValue(base, rank) +
           1.40f * std::max(0.0f, totalAttackDamage);
}

inline float HyperDamage(int rank, float totalAttackDamage,
                         float targetMaximumHealth) {
    static constexpr std::array<float, 6> base{
        0.0f, 10.0f, 20.0f, 30.0f, 40.0f, 50.0f
    };
    static constexpr std::array<float, 6> maximumHealthRatio{
        0.0f, 0.06f, 0.08f, 0.10f, 0.12f, 0.14f
    };
    return RankValue(base, rank) + std::max(0.0f, totalAttackDamage) +
           RankValue(maximumHealthRatio, rank) *
               std::max(0.0f, targetMaximumHealth);
}

inline float WallopDamage(int rank, float bonusAttackDamage) {
    static constexpr std::array<float, 6> base{
        0.0f, 45.0f, 75.0f, 105.0f, 135.0f, 165.0f
    };
    return RankValue(base, rank) + std::max(0.0f, bonusAttackDamage);
}

inline float HopDamage(int rank, float maximumHealth) {
    static constexpr std::array<float, 6> base{
        0.0f, 50.0f, 85.0f, 120.0f, 155.0f, 190.0f
    };
    return RankValue(base, rank) + 0.06f * std::max(0.0f, maximumHealth);
}

inline float CrunchDamage(int rank, float maximumHealth) {
    static constexpr std::array<float, 6> base{
        0.0f, 80.0f, 115.0f, 150.0f, 185.0f, 220.0f
    };
    return RankValue(base, rank) + 0.06f * std::max(0.0f, maximumHealth);
}

inline float GnarDamage(int rank, float totalAttackDamage,
                        float abilityPower, bool wallCollision) {
    static constexpr std::array<float, 4> base{
        0.0f, 200.0f, 300.0f, 400.0f
    };
    const float raw = RankValue(base, rank) +
                      std::max(0.0f, totalAttackDamage) +
                      0.50f * std::max(0.0f, abilityPower);
    return raw * (wallCollision ? kGnarWallDamageMultiplier : 1.0f);
}

inline float QBaseCooldown(int rank) {
    static constexpr std::array<float, 6> seconds{
        0.0f, 16.0f, 14.5f, 13.0f, 11.5f, 10.0f
    };
    return RankValue(seconds, rank);
}

inline float QCooldownAfterCatch(int rank, bool mega) {
    const float refund = mega ? 0.70f : 0.40f;
    return QBaseCooldown(rank) * (1.0f - refund);
}

inline float EBaseCooldown(int rank) {
    static constexpr std::array<float, 6> seconds{
        0.0f, 22.0f, 19.5f, 17.0f, 14.5f, 12.0f
    };
    return RankValue(seconds, rank);
}

enum class FormState : std::uint8_t {
    Mini,
    TransformReady,
    Mega,
    Tired,
};

struct TransformObservation {
    bool MegaBuff = false;
    bool TransformSoonBuff = false;
    bool TiredBuff = false;
    float Fury = 0.0f;
    FormState Previous = FormState::Mini;
    bool MegaGraceActive = false;
};

inline FormState ResolveTransformState(const TransformObservation& observation) {
    if (observation.TiredBuff) return FormState::Tired;
    if (observation.MegaBuff ||
        (observation.Previous == FormState::Mega &&
         observation.MegaGraceActive)) {
        return FormState::Mega;
    }
    if (observation.TransformSoonBuff || observation.Fury >= 100.0f) {
        return FormState::TransformReady;
    }
    return FormState::Mini;
}

inline bool IsMini(FormState state) {
    return state == FormState::Mini || state == FormState::TransformReady ||
           state == FormState::Tired;
}

struct RageContext {
    FormState Form = FormState::Mini;
    float Fury = 0.0f;
    bool ChampionNearby = false;
    bool ObjectiveFight = false;
    bool LaneClear = false;
    bool LastHit = false;
    bool DesiredMegaWindow = false;
};

inline bool MayGenerateRage(const RageContext& context) {
    if (context.Form == FormState::Mega) return true;
    if (context.Form == FormState::Tired) return true;
    if (context.ChampionNearby || context.ObjectiveFight ||
        context.DesiredMegaWindow) return true;
    if (context.LastHit) return true;
    if (!context.LaneClear) return true;
    return std::clamp(context.Fury, 0.0f, 100.0f) < 82.0f;
}

struct QReachContext {
    FormState Form = FormState::Mini;
    float Distance = 0.0f;
    float TargetRadius = 0.0f;
    bool PredictionAccepted = false;
    bool ProjectileWall = false;
    bool InterveningBody = false;
};

inline bool QCanReach(const QReachContext& context) {
    const float range = context.Form == FormState::Mega
        ? kMegaQRange : kMiniQRange;
    if (!context.PredictionAccepted || context.ProjectileWall ||
        context.Distance > range + std::max(0.0f, context.TargetRadius)) {
        return false;
    }
    return context.Form != FormState::Mega || !context.InterveningBody;
}

inline bool CanCatchQ(float playerDistanceToPickup,
                      bool pickupAvailable,
                      bool pathWalkable,
                      bool pathSafe) {
    return pickupAvailable && pathWalkable && pathSafe &&
           playerDistanceToPickup <= kBoulderPickupRadius;
}

inline bool WallopHits(const Vec3& origin, const Vec3& aim,
                       const Vec3& target, float targetRadius) {
    if (aim.IsZero() || target.IsZero()) return false;
    const Vec3 direction = Direction2D(origin, aim);
    if (direction.IsZero()) return false;
    const Vec3 endpoint = origin + direction * kWallopRange;
    const auto projection = ProjectPointToSegment2D(target, origin, endpoint);
    return projection.T > 0.0f &&
           projection.Distance <= kWallopWidth * 0.5f +
                                      std::max(0.0f, targetRadius);
}

inline Vec3 ClampEndpoint(const Vec3& origin, const Vec3& requested,
                          float maximumRange) {
    const Vec3 direction = Direction2D(origin, requested);
    if (direction.IsZero()) return {};
    return origin + direction *
        std::min(std::max(0.0f, maximumRange),
                 origin.Distance2D(requested));
}

inline Vec3 HopLanding(const Vec3& origin, const Vec3& requested) {
    return ClampEndpoint(origin, requested, kHopRange);
}

inline Vec3 HopBounceLanding(const Vec3& origin, const Vec3& firstLanding,
                             const Vec3& requestedDirection) {
    const Vec3 direction = Direction2D(origin, requestedDirection);
    if (direction.IsZero() || firstLanding.IsZero()) return {};
    return firstLanding + direction * kHopBounceRange;
}

inline Vec3 CrunchLanding(const Vec3& origin, const Vec3& requested) {
    return ClampEndpoint(origin, requested, kCrunchRange);
}

struct MobilityContext {
    bool EndpointValid = false;
    bool EndpointWalkable = false;
    bool EndpointEnemyTurret = false;
    bool EndpointDashHazard = false;
    bool EndpointPointClickThreat = false;
    bool BouncePossible = false;
    bool BounceEndpointSafe = true;
    bool Defensive = false;
    bool Lethal = false;
    bool EscapesThreat = false;
    bool CursorAgrees = true;
    int EnemiesAtEndpoint = 0;
    int AlliesAtEndpoint = 0;
    int MaximumCommitEnemies = 2;
};

inline bool MobilitySafe(const MobilityContext& context) {
    if (!context.EndpointValid || !context.EndpointWalkable ||
        context.EndpointDashHazard || context.EndpointPointClickThreat) {
        return false;
    }
    if (context.BouncePossible && !context.BounceEndpointSafe) return false;
    if (context.EndpointEnemyTurret &&
        !(context.Lethal || context.Defensive || context.EscapesThreat)) {
        return false;
    }
    if (!context.Defensive && !context.Lethal && !context.CursorAgrees) {
        return false;
    }
    const int allowed = std::max(
        std::max(0, context.MaximumCommitEnemies),
        std::max(0, context.AlliesAtEndpoint) + 1);
    return context.Defensive || context.EscapesThreat ||
           context.EnemiesAtEndpoint <= allowed;
}

inline Vec3 KnockbackEndpoint(const Vec3& caster,
                              const Vec3& target) {
    const Vec3 direction = Direction2D(caster, target);
    return direction.IsZero()
        ? Vec3{}
        : target + direction * kGnarKnockbackDistance;
}

struct UltimateContext {
    bool Mega = false;
    bool Ready = false;
    bool TargetInRadius = false;
    bool TargetDamageable = false;
    bool TargetDisplacementImmune = false;
    bool WallCollision = false;
    bool Lethal = false;
    bool Defensive = false;
    bool Interrupt = false;
    bool EndpointSafe = false;
    bool FollowupReady = false;
    int ChampionHits = 0;
    int MinimumChampionHits = 2;
};

inline bool MayCastGnar(const UltimateContext& context) {
    if (!context.Mega || !context.Ready || !context.TargetInRadius ||
        !context.TargetDamageable || context.TargetDisplacementImmune ||
        !context.EndpointSafe) {
        return false;
    }
    const bool multiTarget = context.ChampionHits >=
        std::max(1, context.MinimumChampionHits);
    if (context.Defensive || context.Interrupt) return true;
    if (context.Lethal) return true;
    return multiTarget && (context.WallCollision || context.FollowupReady);
}

struct AutomaticContext {
    bool ManualOwnership = false;
    bool Defensive = false;
    bool Interrupt = false;
    bool KillSecure = false;
    bool ProactiveEngage = false;
    bool EndpointKnownSafe = true;
};

inline bool AutomaticAllowed(const AutomaticContext& context) {
    return !context.ManualOwnership && !context.ProactiveEngage &&
           context.EndpointKnownSafe &&
           (context.Defensive || context.Interrupt || context.KillSecure);
}

} // namespace Plugins::KuroAIO::AI::Controllers::Gnar::Geometry
