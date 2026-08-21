#pragma once

#include "../../AIGeometry.h"

#include <algorithm>

namespace Plugins::KuroAIO::AI::Controllers::Caitlyn::Geometry {

using SharedGeometry::Direction2D;

inline constexpr float kHeadshotTrapRange = 1300.0f;
inline constexpr float kNetRecoilDistance = 390.0f;

inline Vec3 RecoilPosition(const Vec3& source,
                           const Vec3& castPosition,
                           float distance = kNetRecoilDistance) {
    const Vec3 direction = Direction2D(source, castPosition);
    return direction.IsZero() ? Vec3{} : source - direction * distance;
}

struct NetContext {
    bool PredictionHits = false;
    bool LandingSafe = false;
    bool Defensive = false;
    bool TargetStillReachable = false;
    bool LethalFollowup = false;
    bool AttackAvailable = false;
};

inline bool ShouldCastNet(const NetContext& context) {
    if (!context.PredictionHits || !context.LandingSafe) return false;
    if (context.Defensive) return true;
    if (context.AttackAvailable && !context.LethalFollowup) return false;
    return context.TargetStillReachable || context.LethalFollowup;
}

struct PeacemakerContext {
    bool InRange = false;
    bool PredictionHits = false;
    bool ProjectileWall = false;
    bool AttackAvailable = false;
    bool AttackWindingUp = false;
    bool TargetImmobile = false;
    bool Lethal = false;
};

inline bool ShouldCastPeacemaker(const PeacemakerContext& context) {
    if (!context.InRange || !context.PredictionHits ||
        context.ProjectileWall || context.AttackWindingUp) {
        return false;
    }
    return !context.AttackAvailable || context.TargetImmobile || context.Lethal;
}

struct TrapContext {
    bool InRange = false;
    bool AmmoReady = false;
    bool AlreadyTrapped = false;
    bool TrapAlreadyNear = false;
    bool Immobilized = false;
    bool Dashing = false;
    bool Committed = false;
    bool NetFollowup = false;
};

inline bool ShouldPlaceTrap(const TrapContext& context) {
    return context.InRange && context.AmmoReady &&
           !context.AlreadyTrapped && !context.TrapAlreadyNear &&
           (context.Immobilized || context.Dashing ||
            context.Committed || context.NetFollowup);
}

struct UltimateContext {
    bool Lethal = false;
    bool InRange = false;
    bool ChannelSafe = false;
    bool TargetCanBeDamaged = false;
    bool BetterLocalAction = false;
    bool ProjectileWall = false;
};

inline bool ShouldCastUltimate(const UltimateContext& context) {
    return context.InRange && context.ChannelSafe &&
           context.TargetCanBeDamaged && !context.BetterLocalAction &&
           !context.ProjectileWall && context.Lethal;
}

} // namespace Plugins::KuroAIO::AI::Controllers::Caitlyn::Geometry
