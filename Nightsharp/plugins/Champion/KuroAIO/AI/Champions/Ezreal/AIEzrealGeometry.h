#pragma once

namespace Plugins::KuroAIO::AI::Controllers::Ezreal::Geometry {

struct MysticShotContext {
    bool InRange = false;
    bool PredictionHits = false;
    bool Collision = false;
    bool ProjectileWall = false;
    bool AttackWindingUp = false;
    bool AttackAvailable = false;
    bool AfterAttack = false;
    bool Marked = false;
    bool Lethal = false;
};

inline bool ShouldCastMysticShot(const MysticShotContext& context) {
    if (!context.InRange || !context.PredictionHits || context.Collision ||
        context.ProjectileWall || context.AttackWindingUp) {
        return false;
    }
    return !context.AttackAvailable || context.AfterAttack ||
           context.Marked || context.Lethal;
}

struct FluxContext {
    bool InRange = false;
    bool PredictionHits = false;
    bool ProjectileWall = false;
    bool AlreadyMarked = false;
    bool AttackWindingUp = false;
    bool QCanDetonate = false;
    bool AutoCanDetonate = false;
    bool LethalSequence = false;
};

inline bool ShouldCastFlux(const FluxContext& context) {
    return context.InRange && context.PredictionHits &&
           !context.ProjectileWall && !context.AlreadyMarked &&
           !context.AttackWindingUp &&
           (context.QCanDetonate || context.AutoCanDetonate ||
            context.LethalSequence);
}

struct BlinkContext {
    bool DestinationSafe = false;
    bool DestinationWalkable = false;
    bool Defensive = false;
    bool Lethal = false;
    bool CreatesFollowup = false;
    bool AttackAlreadyAvailable = false;
    int EnemiesAtDestination = 0;
};

inline bool ShouldBlink(const BlinkContext& context) {
    if (!context.DestinationSafe || !context.DestinationWalkable ||
        context.EnemiesAtDestination > 1) {
        return false;
    }
    if (context.Defensive) return true;
    return context.Lethal && context.CreatesFollowup &&
           !context.AttackAlreadyAvailable;
}

struct BarrageContext {
    bool Lethal = false;
    bool PredictionVeryHigh = false;
    bool ProjectileWall = false;
    bool LocalEnemyNearby = false;
    bool BetterLocalAction = false;
    float Distance = 0.0f;
};

inline bool ShouldCastBarrage(const BarrageContext& context) {
    if (!context.PredictionVeryHigh || context.ProjectileWall ||
        context.BetterLocalAction) {
        return false;
    }
    return context.Lethal && !context.LocalEnemyNearby &&
           context.Distance >= 850.0f;
}

} // namespace Plugins::KuroAIO::AI::Controllers::Ezreal::Geometry
