#pragma once

#include "../AIGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace Plugins::KuroAIO::AI::Controllers::Irelia::Geometry {

using SharedGeometry::Direction2D;
using SharedGeometry::ProjectPointToSegment2D;
using SharedGeometry::RankValue;
using SharedGeometry::Rotate2D;
using SharedGeometry::kPi;

inline constexpr int kPassiveMaximumStacks = 4;
inline constexpr int kPassiveDurationMs = 6000;
inline constexpr float kQRange = 600.0f;
inline constexpr float kWRange = 825.0f;
inline constexpr int kWMaximumChargeMs = 1500;
inline constexpr int kWMinimumReleaseMs = 350;
inline constexpr float kERange = 850.0f;
inline constexpr float kEWidth = 70.0f;
inline constexpr int kERecastWindowMs = 3500;
inline constexpr float kRRange = 950.0f;
inline constexpr float kRWidth = 160.0f;
inline constexpr float kRBladeWallHalfLength = 450.0f;
inline constexpr float kRBladeWallThickness = 95.0f;
inline constexpr int kMarkDurationMs = 5000;

inline int ClampPassiveStacks(int stacks) {
    return std::clamp(stacks, 0, kPassiveMaximumStacks);
}

inline bool PassiveEmpowered(int stacks) {
    return ClampPassiveStacks(stacks) == kPassiveMaximumStacks;
}

inline float PassiveAttackSpeedPercent(int level, int stacks) {
    const float perStack = 7.5f +
        static_cast<float>(std::clamp(level, 1, 18) - 1) * (17.5f / 17.0f);
    return perStack * static_cast<float>(ClampPassiveStacks(stacks));
}

inline float PassiveMagicRawDamage(int level, float bonusAttackDamage) {
    const int clamped = std::clamp(level, 1, 18);
    const float base = 10.0f + static_cast<float>(clamped - 1) * (51.0f / 17.0f);
    return base + 0.20f * std::max(0.0f, bonusAttackDamage);
}

inline float QRawDamage(int rank, float totalAttackDamage) {
    static constexpr std::array<float, 6> base{ 0.0f, 5.0f, 25.0f, 45.0f, 65.0f, 85.0f };
    return RankValue(base, rank) + 0.70f * std::max(0.0f, totalAttackDamage);
}

inline float QRawDamageToMinion(int rank, float totalAttackDamage) {
    static constexpr std::array<float, 6> bonus{ 0.0f, 55.0f, 70.0f, 85.0f, 100.0f, 115.0f };
    return QRawDamage(rank, totalAttackDamage) + RankValue(bonus, rank);
}

struct QResetContext {
    bool TargetMarked = false;
    bool TargetKillable = false;
    bool TargetAlreadyDead = false;
};

inline bool QWillReset(const QResetContext& context) {
    return !context.TargetAlreadyDead &&
           (context.TargetMarked || context.TargetKillable);
}

struct DashSafetyContext {
    bool EndpointValid = false;
    bool EndpointWall = false;
    bool NewEnemyTurret = false;
    bool ResetExpected = false;
    bool Lethal = false;
    bool Fleeing = false;
    bool CursorProgress = false;
    int EnemiesAtEndpoint = 0;
    int MaximumEnemies = 2;
};

inline bool QDashSafe(const DashSafetyContext& context) {
    if (!context.EndpointValid || context.EndpointWall) return false;
    if (context.NewEnemyTurret && !context.Lethal) return false;
    if (context.EnemiesAtEndpoint > std::max(0, context.MaximumEnemies) &&
        !context.Lethal) return false;
    if (context.Fleeing) return context.CursorProgress && context.ResetExpected;
    return context.ResetExpected || context.Lethal;
}

inline float WChargeRatio(int elapsedMs) {
    const float elapsed = static_cast<float>(std::clamp(elapsedMs, 0, kWMaximumChargeMs));
    return elapsed / static_cast<float>(kWMaximumChargeMs);
}

inline float WRawDamage(int rank, float totalAttackDamage, float abilityPower,
                        int elapsedMs) {
    static constexpr std::array<float, 6> minimum{ 0.0f, 10.0f, 20.0f, 30.0f, 40.0f, 50.0f };
    const float minDamage = RankValue(minimum, rank) +
        0.40f * std::max(0.0f, totalAttackDamage) +
        0.40f * std::max(0.0f, abilityPower);
    return minDamage * (1.0f + 2.0f * WChargeRatio(elapsedMs));
}

inline float WPhysicalReductionPercent(int level, float abilityPower) {
    const float levelScaling = static_cast<float>(std::clamp(level, 1, 18) - 1) *
        (30.0f / 17.0f);
    return std::clamp(40.0f + levelScaling +
        0.07f * std::max(0.0f, abilityPower), 0.0f, 90.0f);
}

inline float WMagicReductionPercent(int level, float abilityPower) {
    return WPhysicalReductionPercent(level, abilityPower) * 0.5f;
}

struct WReleaseContext {
    bool Charging = false;
    bool AimValid = false;
    bool TargetInRange = false;
    bool IncomingImpact = false;
    bool TargetLeaving = false;
    bool Lethal = false;
    int ElapsedMs = 0;
};

inline bool ShouldReleaseW(const WReleaseContext& context) {
    if (!context.Charging) return false;
    if (context.ElapsedMs >= kWMaximumChargeMs - 40) return true;
    if (!context.AimValid || !context.TargetInRange) {
        return context.ElapsedMs >= kWMinimumReleaseMs;
    }
    if ((context.IncomingImpact || context.TargetLeaving || context.Lethal) &&
        context.ElapsedMs >= kWMinimumReleaseMs) return true;
    return false;
}

struct BladePlan {
    Vec3 First = {};
    Vec3 Second = {};
    Vec3 PredictedTarget = {};
    bool FirstInRange = false;
    bool SecondInRange = false;
    bool CrossesTarget = false;
};

inline bool ELineHits(const Vec3& first, const Vec3& second,
                      const Vec3& target, float targetRadius = 0.0f) {
    if (first.IsZero() || second.IsZero() || target.IsZero() ||
        first.DistanceSqr2D(second) <= 1.0f) return false;
    return ProjectPointToSegment2D(target, first, second).Distance <=
        kEWidth * 0.5f + std::max(0.0f, targetRadius);
}

inline Vec3 ClampPlacement(const Vec3& player, const Vec3& requested) {
    const Vec3 direction = Direction2D(player, requested);
    if (direction.IsZero()) return {};
    return player + direction * std::min(kERange, player.Distance2D(requested));
}

inline BladePlan BuildBladePlan(const Vec3& player, const Vec3& predictedTarget,
                                float targetRadius = 35.0f,
                                float bladeSeparation = 360.0f) {
    BladePlan result{};
    if (player.IsZero() || predictedTarget.IsZero()) return result;
    Vec3 axis = Direction2D(player, predictedTarget);
    if (axis.IsZero()) return result;
    const float half = std::max(120.0f, bladeSeparation * 0.5f);
    result.First = ClampPlacement(player, predictedTarget + axis * half);
    result.Second = ClampPlacement(player, predictedTarget - axis * half);
    result.PredictedTarget = predictedTarget;
    result.FirstInRange = !result.First.IsZero() &&
        player.Distance2D(result.First) <= kERange + 0.1f;
    result.SecondInRange = !result.Second.IsZero() &&
        player.Distance2D(result.Second) <= kERange + 0.1f;
    result.CrossesTarget = ELineHits(result.First, result.Second,
                                     predictedTarget, targetRadius);
    return result;
}

inline Vec3 BuildSecondBlade(const Vec3& player, const Vec3& first,
                             const Vec3& predictedTarget,
                             float separation = 260.0f) {
    Vec3 direction = Direction2D(first, predictedTarget);
    if (direction.IsZero()) direction = Direction2D(player, predictedTarget);
    if (direction.IsZero()) return {};
    return ClampPlacement(player, predictedTarget + direction *
        std::max(120.0f, separation));
}

struct ECastContext {
    bool Ready = false;
    bool PlacementValid = false;
    bool PlacementWall = false;
    bool PredictionAccepted = false;
    bool CrossesTarget = false;
    bool SpellShield = false;
    bool Recast = false;
    bool Interrupt = false;
    bool Defensive = false;
};

inline bool MayCastE(const ECastContext& context) {
    if (!context.Ready || !context.PlacementValid || context.PlacementWall ||
        context.SpellShield) return false;
    if (!context.Recast) return context.PredictionAccepted;
    return context.CrossesTarget &&
        (context.PredictionAccepted || context.Interrupt || context.Defensive);
}

struct BladeWall {
    Vec3 Center = {};
    Vec3 Start = {};
    Vec3 End = {};
    bool Valid = false;
};

inline BladeWall BuildBladeWall(const Vec3& caster, const Vec3& impact) {
    BladeWall wall{};
    const Vec3 direction = Direction2D(caster, impact);
    if (direction.IsZero()) return wall;
    const Vec3 perpendicular = Rotate2D(direction, kPi * 0.5f);
    wall.Center = impact;
    wall.Start = impact - perpendicular * kRBladeWallHalfLength;
    wall.End = impact + perpendicular * kRBladeWallHalfLength;
    wall.Valid = true;
    return wall;
}

inline bool PointTouchesBladeWall(const BladeWall& wall, const Vec3& point,
                                  float radius = 0.0f) {
    return wall.Valid && !point.IsZero() &&
        ProjectPointToSegment2D(point, wall.Start, wall.End).Distance <=
            kRBladeWallThickness + std::max(0.0f, radius);
}

struct RContext {
    bool Ready = false;
    bool PredictionAccepted = false;
    bool TargetHit = false;
    bool ProjectileWall = false;
    bool QReady = false;
    bool DashEndpointSafe = false;
    bool Lethal = false;
    bool MultiTarget = false;
    bool TargetEscaping = false;
    int PassiveStacks = 0;
};

inline bool MayCastR(const RContext& context) {
    if (!context.Ready || !context.PredictionAccepted || !context.TargetHit ||
        context.ProjectileWall) return false;
    if (context.QReady && !context.DashEndpointSafe && !context.Lethal) return false;
    return context.Lethal || context.MultiTarget || context.TargetEscaping ||
           ClampPassiveStacks(context.PassiveStacks) <= 2;
}

struct AutomaticContext {
    bool DefensiveW = false;
    bool InterruptE = false;
    bool KillSecureQ = false;
    bool Engage = false;
};

inline bool AutomaticAllowed(const AutomaticContext& context) {
    return !context.Engage &&
        (context.DefensiveW || context.InterruptE || context.KillSecureQ);
}

} // namespace Plugins::KuroAIO::AI::Controllers::Irelia::Geometry
