#pragma once

#include "../../AIGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace Plugins::KuroAIO::AI::Controllers::KSante::Geometry {

using SharedGeometry::Direction2D;
using SharedGeometry::ProjectPointToSegment2D;
using SharedGeometry::RankValue;

inline constexpr float kQRange = 450.0f;
inline constexpr float kQ3Range = 825.0f;
inline constexpr float kQHalfWidth = 60.0f;
inline constexpr float kWMinimumRange = 100.0f;
inline constexpr float kWMaximumRange = 600.0f;
inline constexpr float kWHalfWidth = 55.0f;
inline constexpr int kWMinimumChargeMs = 400;
inline constexpr int kWAllOutMinimumChargeMs = 750;
inline constexpr int kWFullChargeMs = 900;
inline constexpr int kWMaximumHoldMs = 1000;
inline constexpr float kESelfRange = 250.0f;
inline constexpr float kEAllOutSelfRange = 400.0f;
inline constexpr float kEAllyRange = 550.0f;
inline constexpr float kRRange = 350.0f;
inline constexpr int kAllOutDurationMs = 15000;
inline constexpr int kQStackDurationMs = 6000;

enum class Stance { Tank, AllOut };
enum class WPurpose { Combo, Peel, Interrupt, Flee };
enum class EPurpose { Reposition, Chase, Peel, Flee, ShieldAlly };
enum class RPurpose { None, TerrainIsolation, SafetyIsolation, Lethal };

inline int ReconcileQStacks(int observedStacks, bool q3Buff, int eventStacks,
                            bool expired) {
    if (expired) return 0;
    if (q3Buff) return 2;
    if (observedStacks >= 0) return std::clamp(observedStacks, 0, 2);
    return std::clamp(eventStacks, 0, 2);
}

inline int QStacksAfterCast(int currentStacks, bool castWasQ3) {
    return castWasQ3 ? 0 : std::min(2, std::max(0, currentStacks) + 1);
}

inline float QRange(int stacks) { return stacks >= 2 ? kQ3Range : kQRange; }
inline bool IsQ3(int stacks) { return stacks >= 2; }

inline float QRawDamage(int rank, float totalAttackDamage, Stance stance) {
    static constexpr std::array<float, 6> base{ 0.0f, 70.0f, 100.0f, 130.0f, 160.0f, 190.0f };
    const float stanceScale = stance == Stance::AllOut ? 1.10f : 1.0f;
    return (RankValue(base, rank) + 0.40f * std::max(0.0f, totalAttackDamage)) * stanceScale;
}

inline float WChargeProgress(int elapsedMs, Stance stance) {
    const int minimum = stance == Stance::AllOut
        ? kWAllOutMinimumChargeMs : kWMinimumChargeMs;
    if (elapsedMs <= minimum) return 0.0f;
    return std::clamp(static_cast<float>(elapsedMs - minimum) /
        static_cast<float>(std::max(1, kWFullChargeMs - minimum)), 0.0f, 1.0f);
}

inline float WRange(int elapsedMs, Stance stance) {
    return kWMinimumRange + (kWMaximumRange - kWMinimumRange) *
        WChargeProgress(elapsedMs, stance);
}

inline Vec3 ClampDashEndpoint(const Vec3& origin, const Vec3& requested,
                              float range) {
    const Vec3 direction = Direction2D(origin, requested);
    if (direction.IsZero()) return {};
    return origin + direction * std::min(std::max(0.0f, range),
                                         origin.Distance2D(requested));
}

inline bool LineHits(const Vec3& origin, const Vec3& endpoint,
                     const Vec3& target, float targetRadius,
                     float halfWidth) {
    if (origin.IsZero() || endpoint.IsZero() || target.IsZero()) return false;
    const auto projection = ProjectPointToSegment2D(target, origin, endpoint);
    return projection.Distance <= std::max(0.0f, halfWidth) +
                                  std::max(0.0f, targetRadius);
}

struct WReleaseContext {
    bool Charging = false;
    bool PredictionAccepted = false;
    bool TargetInCurrentRange = false;
    bool EndpointSafe = false;
    bool Interrupt = false;
    bool Peel = false;
    bool Lethal = false;
    bool Expiring = false;
    bool AttackWindingUp = false;
    int ElapsedMs = 0;
    Stance CurrentStance = Stance::Tank;
};

inline bool MayReleaseW(const WReleaseContext& context) {
    if (!context.Charging || !context.PredictionAccepted ||
        !context.TargetInCurrentRange || !context.EndpointSafe) return false;
    const int minimum = context.CurrentStance == Stance::AllOut
        ? kWAllOutMinimumChargeMs : kWMinimumChargeMs;
    if (context.ElapsedMs < minimum) return false;
    if (context.AttackWindingUp && !context.Interrupt && !context.Peel &&
        !context.Lethal && !context.Expiring) return false;
    return context.Interrupt || context.Peel || context.Lethal ||
           context.Expiring || context.ElapsedMs >= kWFullChargeMs;
}

struct DashSafetyContext {
    bool EndpointValid = false;
    bool EndpointWalkable = false;
    bool EnemyTurret = false;
    bool StartedUnderEnemyTurret = false;
    bool PointClickThreat = false;
    bool DashHazard = false;
    bool CursorAgrees = false;
    bool AllyTarget = false;
    bool AllyAlive = false;
    bool AllyInRange = false;
    bool Lethal = false;
    bool Defensive = false;
    int EnemiesAtEndpoint = 0;
    int AlliesAtEndpoint = 0;
    int MaximumEnemies = 2;
};

inline bool DashSafe(const DashSafetyContext& context) {
    if (!context.EndpointValid || !context.EndpointWalkable) return false;
    if (context.AllyTarget && (!context.AllyAlive || !context.AllyInRange)) return false;
    if (context.EnemyTurret && !context.StartedUnderEnemyTurret && !context.Lethal) return false;
    if ((context.PointClickThreat || context.DashHazard) &&
        !context.Defensive && !context.Lethal) return false;
    if (context.EnemiesAtEndpoint > std::max(0, context.MaximumEnemies) &&
        context.EnemiesAtEndpoint > context.AlliesAtEndpoint + 1 &&
        !context.Defensive && !context.Lethal) return false;
    return context.CursorAgrees || context.Defensive || context.Lethal || context.AllyTarget;
}

struct IsolationContext {
    bool Ready = false;
    bool TargetValid = false;
    bool TargetUnstoppable = false;
    bool TargetSpellShielded = false;
    bool WallBehindTarget = false;
    bool LandingWalkable = false;
    bool LandingUnderEnemyTurret = false;
    bool PlayerUnderEnemyTurret = false;
    bool PlayerExitAvailable = false;
    bool CursorAgrees = false;
    bool Lethal = false;
    int EnemiesBefore = 0;
    int EnemiesAfter = 0;
    int AlliesAfter = 0;
    float SeparationGain = 0.0f;
};

struct IsolationResult {
    bool Cast = false;
    RPurpose Purpose = RPurpose::None;
    float Score = -100000.0f;
};

inline IsolationResult EvaluateIsolation(const IsolationContext& context) {
    IsolationResult result{};
    if (!context.Ready || !context.TargetValid || context.TargetUnstoppable ||
        context.TargetSpellShielded || !context.LandingWalkable ||
        !context.PlayerExitAvailable) return result;
    if (context.LandingUnderEnemyTurret && !context.PlayerUnderEnemyTurret &&
        !context.Lethal) return result;
    if (!context.CursorAgrees && !context.Lethal) return result;
    const bool isolates = context.EnemiesAfter < context.EnemiesBefore ||
        context.SeparationGain >= 250.0f;
    const bool numericallySafe = context.EnemiesAfter <= context.AlliesAfter + 1;
    if (!context.Lethal && !isolates && !numericallySafe) return result;
    result.Purpose = context.Lethal ? RPurpose::Lethal :
        context.WallBehindTarget ? RPurpose::TerrainIsolation :
        RPurpose::SafetyIsolation;
    result.Score = (context.WallBehindTarget ? 380.0f : 0.0f) +
        (context.Lethal ? 900.0f : 0.0f) + context.SeparationGain * 0.8f +
        static_cast<float>(context.EnemiesBefore - context.EnemiesAfter) * 180.0f +
        static_cast<float>(context.AlliesAfter - context.EnemiesAfter) * 90.0f;
    result.Cast = context.Lethal || (isolates && result.Score >= 250.0f);
    return result;
}

struct ModeContext {
    bool ManualOwnership = false;
    bool AttackWindingUp = false;
    bool SelectedTarget = false;
    bool OrbwalkerTarget = false;
    bool Defensive = false;
    bool Interrupt = false;
    bool KillSecure = false;
    bool Engage = false;
};

inline bool MayUseAbility(const ModeContext& context) {
    if (context.ManualOwnership) return false;
    if (context.AttackWindingUp && !context.Defensive &&
        !context.Interrupt && !context.KillSecure) return false;
    return context.SelectedTarget || context.OrbwalkerTarget ||
           context.Defensive || context.Interrupt;
}

inline bool AutomaticAllowed(const ModeContext& context) {
    return !context.ManualOwnership && !context.Engage &&
        (context.Defensive || context.Interrupt || context.KillSecure);
}

} // namespace Plugins::KuroAIO::AI::Controllers::KSante::Geometry
