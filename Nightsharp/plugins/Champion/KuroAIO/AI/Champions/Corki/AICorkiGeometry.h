#pragma once

#include <algorithm>
#include <cmath>

namespace Plugins::KuroAIO::AI::Controllers::Corki::Geometry {

inline constexpr float kQRange = 825.0f;
inline constexpr float kQRadius = 250.0f;
inline constexpr float kQMissileSpeed = 1100.0f;
inline constexpr float kWMinimumRange = 300.0f;
inline constexpr float kWRange = 600.0f;
inline constexpr float kPackageWRange = 900.0f;
inline constexpr float kERange = 600.0f;
inline constexpr float kEConeHalfAngleDegrees = 28.0f;
inline constexpr float kRNormalRange = 1300.0f;
inline constexpr float kRBigRange = 1500.0f;
inline constexpr int kRMaximumAmmo = 4;

struct Point2 {
    float X = 0.0f;
    float Y = 0.0f;
};

inline float Distance(Point2 a, Point2 b) {
    const float x = b.X - a.X;
    const float y = b.Y - a.Y;
    return std::sqrt(x * x + y * y);
}

inline float QImpactSeconds(float distance) {
    return 0.25f + std::clamp(distance, 0.0f, kQRange) / kQMissileSpeed;
}

struct PhosphorusContext {
    bool InRange = false;
    bool PredictionHigh = false;
    bool ProjectileWall = false;
    bool AttackWindingUp = false;
    bool AttackAvailable = false;
    bool AfterAttack = false;
    bool Lethal = false;
    bool TargetOutsideAttackRange = false;
};

inline bool ShouldCastPhosphorus(const PhosphorusContext& context) {
    if (!context.InRange || !context.PredictionHigh ||
        context.ProjectileWall || context.AttackWindingUp) {
        return false;
    }
    return !context.AttackAvailable || context.AfterAttack ||
           context.Lethal || context.TargetOutsideAttackRange;
}

inline bool PointInCone(Point2 origin,
                        Point2 aim,
                        Point2 target,
                        float range = kERange,
                        float halfAngleDegrees = kEConeHalfAngleDegrees,
                        float targetRadius = 0.0f) {
    const float aimX = aim.X - origin.X;
    const float aimY = aim.Y - origin.Y;
    const float aimLength = std::sqrt(aimX * aimX + aimY * aimY);
    const float targetX = target.X - origin.X;
    const float targetY = target.Y - origin.Y;
    const float targetLength = std::sqrt(targetX * targetX + targetY * targetY);
    if (aimLength <= 0.001f || targetLength <= 0.001f ||
        targetLength > std::max(0.0f, range) + std::max(0.0f, targetRadius)) {
        return false;
    }
    const float cosine = std::clamp(
        (aimX * targetX + aimY * targetY) / (aimLength * targetLength),
        -1.0f, 1.0f);
    constexpr float pi = 3.14159265358979323846f;
    const float allowance = targetLength > 0.001f
        ? std::asin(std::clamp(
              std::max(0.0f, targetRadius) / targetLength, 0.0f, 1.0f))
        : 0.0f;
    const float threshold =
        std::max(0.0f, halfAngleDegrees) * pi / 180.0f + allowance;
    return std::acos(cosine) <= threshold;
}

struct GatlingContext {
    bool InCone = false;
    bool TargetStable = false;
    bool AttackWindingUp = false;
    bool HasFollowup = false;
    bool Lethal = false;
    bool Farm = false;
    int FarmHits = 0;
};

inline bool ShouldStartGatling(const GatlingContext& context) {
    if (!context.InCone || context.AttackWindingUp) return false;
    if (context.Farm) return context.FarmHits >= 3;
    return context.TargetStable && (context.HasFollowup || context.Lethal);
}

inline float ValkyrieReach(bool packageLoaded) {
    return packageLoaded ? kPackageWRange : kWRange;
}

struct ValkyrieContext {
    bool EndpointValid = false;
    bool Walkable = false;
    bool UnderEnemyTurret = false;
    bool PointClickThreat = false;
    bool DashThreat = false;
    bool Defensive = false;
    bool Emergency = false;
    bool Lethal = false;
    bool CreatesFollowup = false;
    bool AttackAvailable = false;
    bool PackageLoaded = false;
    bool PreservePackage = true;
    float TravelDistance = 0.0f;
    int EnemiesAtEndpoint = 0;
    int MaximumEnemies = 1;
};

inline bool ShouldValkyrie(const ValkyrieContext& context) {
    if (!context.EndpointValid || !context.Walkable ||
        context.UnderEnemyTurret || context.PointClickThreat ||
        context.DashThreat || context.EnemiesAtEndpoint >
            std::max(0, context.MaximumEnemies)) {
        return false;
    }
    if (context.TravelDistance > ValkyrieReach(context.PackageLoaded) + 1.0f) {
        return false;
    }
    if (context.PackageLoaded && context.PreservePackage &&
        !context.Emergency && !context.Lethal) {
        return false;
    }
    if (context.Defensive) return true;
    return context.Lethal && context.CreatesFollowup &&
           !context.AttackAvailable;
}

struct BarrageState {
    int Ammo = 0;
    int NormalShotsSinceBig = 0;
    bool BigOneReady = false;
    bool AmmoObserved = false;
    bool BigOneObserved = false;
};

inline int ValidatedAmmo(int ammo, int maximum) {
    return maximum == kRMaximumAmmo && ammo >= 0 && ammo <= maximum
        ? ammo : -1;
}

inline void ObserveAmmo(BarrageState& state, int ammo, int maximum) {
    const int observed = ValidatedAmmo(ammo, maximum);
    if (observed < 0) return;
    state.Ammo = observed;
    state.AmmoObserved = true;
}

inline void ObserveBigOne(BarrageState& state,
                          bool bigBuff,
                          bool normalCounterBuff,
                          int normalCounter) {
    if (bigBuff) {
        state.BigOneReady = true;
        state.NormalShotsSinceBig = 2;
        state.BigOneObserved = true;
        return;
    }
    if (normalCounterBuff && normalCounter >= 0 && normalCounter <= 2) {
        state.BigOneReady = false;
        state.NormalShotsSinceBig = normalCounter;
        state.BigOneObserved = true;
    }
}

inline void ConsumeBarrage(BarrageState& state) {
    if (state.Ammo > 0) --state.Ammo;
    if (state.BigOneReady) {
        state.BigOneReady = false;
        state.NormalShotsSinceBig = 0;
    } else {
        state.NormalShotsSinceBig =
            std::min(2, state.NormalShotsSinceBig + 1);
        state.BigOneReady = state.NormalShotsSinceBig >= 2;
    }
}

inline float BarrageReach(const BarrageState& state) {
    return state.BigOneReady ? kRBigRange : kRNormalRange;
}

struct BarrageContext {
    int Ammo = 0;
    int ReserveAmmo = 1;
    bool BigOne = false;
    bool PreserveBigOne = true;
    bool InReach = false;
    bool PredictionHigh = false;
    bool Collision = false;
    bool ProjectileWall = false;
    bool AttackWindingUp = false;
    bool AttackAvailable = false;
    bool Lethal = false;
    bool KillSecure = false;
    bool Combo = false;
    bool Harass = false;
};

inline bool ShouldCastBarrage(const BarrageContext& context) {
    if (context.Ammo <= 0 || !context.InReach ||
        !context.PredictionHigh || context.Collision ||
        context.ProjectileWall || context.AttackWindingUp) {
        return false;
    }
    if (context.Lethal || context.KillSecure) return true;
    if (context.BigOne && context.PreserveBigOne) return false;
    if (context.AttackAvailable) return false;
    const int reserve = std::clamp(context.ReserveAmmo, 0, kRMaximumAmmo);
    return (context.Combo || context.Harass) && context.Ammo > reserve;
}

inline bool ResourcePolicy(float manaPercent,
                           float threshold,
                           bool lethalOrEmergency) {
    return lethalOrEmergency ||
           manaPercent + 0.001f >= std::max(0.0f, threshold);
}

} // namespace Plugins::KuroAIO::AI::Controllers::Corki::Geometry
