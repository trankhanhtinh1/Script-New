#pragma once

#include "../../AIGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace Plugins::KuroAIO::AI::Controllers::Kennen::Geometry {

using Vec3 = ::Vec3;
using SharedGeometry::Direction2D;

inline constexpr int kMarkMaximumStacks = 3;
inline constexpr int kMarkDurationMs = 6000;
inline constexpr float kQRange = 1050.0f;
inline constexpr float kQWidth = 50.0f;
inline constexpr float kQDelay = 0.18f;
inline constexpr float kQSpeed = 1700.0f;
inline constexpr float kWRange = 775.0f;
inline constexpr float kWRadius = 300.0f;
inline constexpr float kERange = 2000.0f;
inline constexpr float kERadius = 200.0f;
inline constexpr float kRRadius = 550.0f;
inline constexpr float kRStrikeInterval = 500.0f;
inline constexpr int kRMaximumStrikes = 3;

inline constexpr int ClampMarkStacks(int stacks) {
    return std::clamp(stacks, 0, kMarkMaximumStacks);
}

struct MarkState {
    int Stacks = 0;
    int ExpireTick = 0;
    bool StunReady = false;
};

inline constexpr bool MarkActive(const MarkState& state, int now) {
    return state.Stacks > 0 && state.ExpireTick > now;
}

// A mark that reaches three stacks is consumed by the stun. The caller may
// retain the returned state as an event-reconciled snapshot until the next
// mark is observed; no SDK objects are needed for this transition.
inline constexpr MarkState ApplyMark(MarkState state, int now,
                                     int durationMs = kMarkDurationMs) {
    if (state.ExpireTick <= now) state.Stacks = 0;
    state.Stacks = std::min(kMarkMaximumStacks, state.Stacks + 1);
    state.ExpireTick = now + std::max(1, durationMs);
    state.StunReady = state.Stacks >= kMarkMaximumStacks;
    if (state.StunReady) state.Stacks = 0;
    return state;
}

inline constexpr MarkState ReconcileMark(MarkState state, int observedStacks,
                                         int now, int durationMs = kMarkDurationMs) {
    state.Stacks = ClampMarkStacks(observedStacks);
    state.ExpireTick = state.Stacks > 0 ? now + std::max(1, durationMs) : 0;
    state.StunReady = state.Stacks >= kMarkMaximumStacks;
    return state;
}

inline constexpr MarkState ExpireMark(MarkState state, int now) {
    if (state.ExpireTick <= now) {
        state.Stacks = 0;
        state.ExpireTick = 0;
        state.StunReady = false;
    }
    return state;
}

inline constexpr bool HasMark(const MarkState& state) {
    return state.Stacks > 0 || state.StunReady;
}

inline bool LineHits(const Vec3& origin, const Vec3& aim, const Vec3& target,
                     float halfWidth = kQWidth, float targetRadius = 0.0f) {
    const Vec3 direction = Direction2D(origin, aim);
    if (direction.IsZero()) return false;
    const Vec3 end = origin + direction * std::min(kQRange, origin.Distance2D(aim));
    const auto projection = SharedGeometry::ProjectPointToSegment2D(target, origin, end);
    return projection.T >= 0.0f && projection.T <= 1.0f &&
           projection.Distance <= std::max(0.0f, halfWidth) + std::max(0.0f, targetRadius);
}

inline bool InArea(const Vec3& center, const Vec3& target,
                   float radius, float targetRadius = 0.0f) {
    if (!center.IsValid() || !target.IsValid()) return false;
    return center.Distance2D(target) <= std::max(0.0f, radius) +
        std::max(0.0f, targetRadius);
}

inline Vec3 ClampRush(const Vec3& origin, const Vec3& requested,
                      float range = kERange) {
    const Vec3 direction = Direction2D(origin, requested);
    if (direction.IsZero() || !origin.IsValid() || !requested.IsValid()) return {};
    return origin + direction * std::min(std::max(0.0f, range),
                                         origin.Distance2D(requested));
}

inline int CountAreaTargets(const Vec3& center, const std::array<Vec3, 16>& targets,
                            float radius, float targetRadius = 0.0f) {
    int count = 0;
    for (const Vec3& target : targets) {
        if (target.IsZero()) continue;
        if (InArea(center, target, radius, targetRadius)) ++count;
    }
    return count;
}

struct RushContext {
    bool Ready = false;
    bool DirectionValid = false;
    bool Defensive = false;
    bool CursorTowardThreat = false;
    bool EndpointUnderTurret = false;
    bool EndpointWall = false;
    int EnemiesAtEndpoint = 0;
    int MaximumEndpointEnemies = 1;
};

inline constexpr bool ShouldLightningRush(const RushContext& context) {
    if (!context.Ready || !context.DirectionValid || context.EndpointWall) return false;
    if (context.EndpointUnderTurret && !context.Defensive) return false;
    return context.Defensive ||
           context.CursorTowardThreat ||
           context.EnemiesAtEndpoint <= std::max(0, context.MaximumEndpointEnemies);
}

struct UltimateContext {
    bool Ready = false;
    bool AreaValid = false;
    bool Lethal = false;
    bool Defensive = false;
    bool Manual = false;
    bool AttackWindingUp = false;
    bool UnderEnemyTurret = false;
    int PredictedTargets = 0;
    int MinimumTargets = 2;
};

inline constexpr bool ShouldCastUltimate(const UltimateContext& context) {
    if (!context.Ready || !context.AreaValid || context.UnderEnemyTurret) return false;
    if (context.AttackWindingUp && !context.Lethal && !context.Defensive && !context.Manual)
        return false;
    return context.Lethal || context.Defensive || context.Manual ||
        context.PredictedTargets >= std::max(1, context.MinimumTargets);
}

inline constexpr int PredictedStrikeCount(int markStacks, bool appliesFirstStrike = true) {
    const int startingMarks = ClampMarkStacks(markStacks);
    const int strikes = std::max(0, kRMaximumStrikes -
        (appliesFirstStrike && startingMarks >= kMarkMaximumStacks ? 1 : 0));
    return strikes;
}

struct EnergyContext {
    float Current = 0.0f;
    float Required = 0.0f;
    float Reserve = 0.0f;
};
inline constexpr bool HasEnergy(const EnergyContext& context) {
    return context.Current + 0.5f >= std::max(0.0f, context.Required) +
        std::max(0.0f, context.Reserve);
}

} // namespace Plugins::KuroAIO::AI::Controllers::Kennen::Geometry
