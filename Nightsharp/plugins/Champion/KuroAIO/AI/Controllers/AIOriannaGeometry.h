#pragma once

#include "../AIGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace Plugins::KuroAIO::AI::Controllers::Orianna::Geometry {

using SharedGeometry::Direction2D;
using SharedGeometry::ProjectPointToSegment2D;
using SharedGeometry::RankValue;

inline constexpr float kQRange = 815.0f;
inline constexpr float kQWidth = 80.0f;
inline constexpr float kQSpeed = 1400.0f;
inline constexpr float kWRadius = 225.0f;
inline constexpr float kERange = 1095.0f;
inline constexpr float kEWidth = 85.0f;
inline constexpr float kESpeed = 1850.0f;
inline constexpr float kRRadius = 415.0f;
inline constexpr float kRDelaySeconds = 0.75f;
inline constexpr float kBallLeashRange = 1300.0f;

inline float QRawDamage(int rank, float abilityPower, int contactIndex = 0) {
    static constexpr std::array<float, 6> base{
        0.0f, 60.0f, 90.0f, 120.0f, 150.0f, 180.0f,
    };
    const float full = RankValue(base, rank) +
        std::max(0.0f, abilityPower) * 0.55f;
    return full * (contactIndex > 0 ? 0.70f : 1.0f);
}

inline float WRawDamage(int rank, float abilityPower) {
    static constexpr std::array<float, 6> base{
        0.0f, 70.0f, 110.0f, 150.0f, 190.0f, 230.0f,
    };
    return RankValue(base, rank) + std::max(0.0f, abilityPower) * 0.80f;
}

inline float WSlowAndHaste(int rank) {
    static constexpr std::array<float, 6> amount{
        0.0f, 0.20f, 0.25f, 0.30f, 0.35f, 0.40f,
    };
    return RankValue(amount, rank);
}

inline float EShield(int rank, float abilityPower) {
    static constexpr std::array<float, 6> base{
        0.0f, 55.0f, 90.0f, 125.0f, 160.0f, 195.0f,
    };
    return RankValue(base, rank) + std::max(0.0f, abilityPower) * 0.45f;
}

inline float ERawDamage(int rank, float abilityPower) {
    static constexpr std::array<float, 6> base{
        0.0f, 60.0f, 90.0f, 120.0f, 150.0f, 180.0f,
    };
    return RankValue(base, rank) + std::max(0.0f, abilityPower) * 0.30f;
}

inline float EResists(int rank) {
    static constexpr std::array<float, 6> amount{
        0.0f, 6.0f, 12.0f, 18.0f, 24.0f, 30.0f,
    };
    return RankValue(amount, rank);
}

inline float RRawDamage(int rank, float abilityPower) {
    static constexpr std::array<float, 4> base{
        0.0f, 225.0f, 350.0f, 475.0f,
    };
    return RankValue(base, rank) + std::max(0.0f, abilityPower) * 0.70f;
}

inline float PassiveRawDamage(int championLevel,
                              float abilityPower,
                              int repeatedAttackStacks) {
    const int level = std::clamp(championLevel, 1, 18);
    const float base = 10.0f + 2.0f * static_cast<float>(level - 1);
    const float first = base + std::max(0.0f, abilityPower) * 0.15f;
    return first * (1.0f + 0.20f * static_cast<float>(
        std::clamp(repeatedAttackStacks, 0, 2)));
}

inline float TravelSeconds(const Vec3& start,
                           const Vec3& end,
                           float speed) {
    if (!start.IsValid() || !end.IsValid()) return 0.0f;
    return start.Distance2D(end) / std::max(1.0f, speed);
}

inline Vec3 ClampQDestination(const Vec3& player,
                              const Vec3& desired) {
    if (!player.IsValid() || !desired.IsValid()) return {};
    const Vec3 direction = Direction2D(player, desired);
    if (direction.IsZero()) return player;
    Vec3 result = player + direction * std::min(kQRange,
        player.Distance2D(desired));
    result.y = desired.y;
    return result;
}

inline bool CircleContains(const Vec3& center,
                           const Vec3& target,
                           float radius,
                           float targetRadius = 0.0f) {
    return center.IsValid() && target.IsValid() &&
        center.Distance2D(target) <= std::max(0.0f, radius) +
            std::clamp(targetRadius, 0.0f, 150.0f);
}

inline bool BallPathHits(const Vec3& start,
                         const Vec3& end,
                         const Vec3& target,
                         float halfWidth,
                         float targetRadius = 0.0f) {
    if (!start.IsValid() || !end.IsValid() || !target.IsValid()) return false;
    const auto projection = ProjectPointToSegment2D(target, start, end);
    return projection.T >= 0.0f && projection.T <= 1.0f &&
        projection.Distance <= std::max(0.0f, halfWidth) +
            std::clamp(targetRadius, 0.0f, 150.0f);
}

inline int ShockwaveHitCount(const Vec3& ball,
                             const std::vector<Vec3>& predicted,
                             float targetRadius = 0.0f) {
    int count = 0;
    for (const auto& position : predicted) {
        if (CircleContains(ball, position, kRRadius, targetRadius)) ++count;
    }
    return count;
}

struct BallState {
    Vec3 Position = {};
    Vec3 PendingPosition = {};
    int AttachedNetworkId = 0;
    int PendingAttachedNetworkId = 0;
    int ArrivalTick = 0;
    bool InTransit = false;
};

inline void BeginBallTransit(BallState& state,
                             const Vec3& destination,
                             int attachedNetworkId,
                             int nowTick,
                             int travelMilliseconds) {
    state.PendingPosition = destination;
    state.PendingAttachedNetworkId = attachedNetworkId;
    state.ArrivalTick = nowTick + std::max(0, travelMilliseconds);
    state.InTransit = true;
}

inline void ReconcileBallTransit(BallState& state, int nowTick) {
    if (!state.InTransit || nowTick < state.ArrivalTick) return;
    state.Position = state.PendingPosition;
    state.AttachedNetworkId = state.PendingAttachedNetworkId;
    state.PendingPosition = {};
    state.PendingAttachedNetworkId = 0;
    state.ArrivalTick = 0;
    state.InTransit = false;
}

struct ShockwaveContext {
    bool Ready = false;
    bool BallKnown = false;
    bool BallInTransit = false;
    bool IntendedTargetInside = false;
    bool TargetProtected = false;
    bool AttackWindingUp = false;
    bool Lethal = false;
    bool Defensive = false;
    bool Manual = false;
    int PredictedHits = 0;
    int MinimumHits = 2;
};

inline bool ShouldCastShockwave(const ShockwaveContext& context) {
    if (!context.Ready || !context.BallKnown || context.BallInTransit ||
        !context.IntendedTargetInside || context.TargetProtected) return false;
    if (context.AttackWindingUp && !context.Lethal &&
        !context.Defensive && !context.Manual) return false;
    return context.Manual || context.Defensive || context.Lethal ||
        context.PredictedHits >= std::max(1, context.MinimumHits);
}

struct ProtectContext {
    bool Ready = false;
    bool AllyValid = false;
    bool AllyInRange = false;
    bool BallPathKnown = false;
    bool IncomingThreat = false;
    bool AllyLow = false;
    bool DeliversCombo = false;
    bool PathHitsEnemy = false;
    bool WouldAbandonBetterBall = false;
};

inline bool ShouldCastProtect(const ProtectContext& context) {
    if (!context.Ready || !context.AllyValid || !context.AllyInRange ||
        !context.BallPathKnown || context.WouldAbandonBetterBall) return false;
    return context.IncomingThreat || context.AllyLow || context.DeliversCombo ||
        context.PathHitsEnemy;
}

struct AutomaticContext {
    bool DefensiveShield = false;
    bool DefensiveShockwave = false;
    bool Interrupt = false;
    bool KillSecure = false;
    bool StartsEngage = false;
};

inline bool AutomaticAllowed(const AutomaticContext& context) {
    return !context.StartsEngage &&
        (context.DefensiveShield || context.DefensiveShockwave ||
         context.Interrupt || context.KillSecure);
}

} // namespace Plugins::KuroAIO::AI::Controllers::Orianna::Geometry
