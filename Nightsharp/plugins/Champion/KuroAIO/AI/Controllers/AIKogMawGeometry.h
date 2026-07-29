#pragma once

#include <algorithm>

namespace Plugins::KuroAIO::AI::Controllers::KogMaw::Geometry {

inline float BarrageBonusRange(int rank) {
    return rank <= 0 ? 0.0f
                     : 130.0f + 20.0f * static_cast<float>(
                           std::clamp(rank, 1, 5) - 1);
}

inline float ArtilleryRange(int rank) {
    static constexpr float ranges[] = {0.0f, 1300.0f, 1550.0f, 1800.0f};
    return ranges[std::clamp(rank, 0, 3)];
}

struct BarrageContext {
    bool Ready = false;
    bool AlreadyActive = false;
    bool TargetInEmpoweredRange = false;
    bool AttackIntent = false;
    bool TargetValid = false;
    bool TargetKillableByAttack = false;
};

inline bool ShouldActivateBarrage(const BarrageContext& context) {
    return context.Ready && !context.AlreadyActive && context.TargetValid &&
           !context.TargetKillableByAttack &&
           context.TargetInEmpoweredRange && context.AttackIntent;
}

struct SpittleContext {
    bool PredictionHits = false;
    bool Collision = false;
    bool ProjectileWall = false;
    bool AttackAvailable = false;
    bool Lethal = false;
    bool Immobilized = false;
    bool OutsideAttackRange = false;
};

inline bool ShouldCastSpittle(const SpittleContext& context) {
    if (!context.PredictionHits || context.Collision ||
        context.ProjectileWall) return false;
    return true;
}

struct OozeContext {
    bool PredictionHits = false;
    bool ProjectileWall = false;
    bool AttackAvailable = false;
    bool Lethal = false;
    bool Escaping = false;
    bool Gapcloser = false;
    bool Immobilized = false;
};

inline bool ShouldCastOoze(const OozeContext& context) {
    if (!context.PredictionHits || context.ProjectileWall) return false;
    if (context.Gapcloser || context.Lethal) return true;
    return context.Escaping || context.Immobilized;
}

struct ArtilleryContext {
    bool PredictionVeryHigh = false;
    bool InRange = false;
    bool AttackAvailable = false;
    bool Lethal = false;
    bool LowHealth = false;
    bool SlowedOrImmobile = false;
    bool Escaping = false;
    int CostStacks = 0;
    int MaximumStacks = 2;
};

inline bool ShouldCastArtillery(const ArtilleryContext& context) {
    if (!context.PredictionVeryHigh || !context.InRange) return false;
    if (context.Lethal) return true;
    if (context.CostStacks >= std::max(0, context.MaximumStacks)) return false;
    return context.LowHealth &&
           (context.SlowedOrImmobile || context.Escaping);
}

} // namespace Plugins::KuroAIO::AI::Controllers::KogMaw::Geometry
