#pragma once

#include "../../AIGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace Plugins::KuroAIO::AI::Controllers::Cassiopeia::Geometry {

using SharedGeometry::Direction2D;
using SharedGeometry::ProjectPointToSegment2D;
using SharedGeometry::RankValue;

inline constexpr float kQRange = 850.0f;
inline constexpr float kWRange = 700.0f;
inline constexpr float kWRadius = 160.0f;
inline constexpr float kERange = 700.0f;
inline constexpr float kRRange = 825.0f;
inline constexpr float kRWidth = 80.0f;
inline constexpr float kRHalfAngleDegrees = 40.0f;
inline constexpr int kPoisonDurationMs = 3000;

inline float QRawDamage(int rank, float abilityPower) {
    static constexpr std::array<float, 6> base{ 0, 75, 110, 145, 180, 215 };
    return RankValue(base, rank) + 0.90f * std::max(0.0f, abilityPower);
}
inline float WRawDamage(int rank, float abilityPower, float seconds) {
    static constexpr std::array<float, 6> base{ 0, 20, 25, 30, 35, 40 };
    return RankValue(base, rank) * std::clamp(seconds, 0.0f, 5.0f) +
           0.10f * std::max(0.0f, abilityPower) * std::clamp(seconds, 0.0f, 5.0f);
}
inline float ERawDamage(int rank, float abilityPower, float targetHealthPercent) {
    static constexpr std::array<float, 6> base{ 0, 55, 80, 105, 130, 155 };
    const float execute = targetHealthPercent < 40.0f ? 0.35f : 0.0f;
    return RankValue(base, rank) + 0.65f * std::max(0.0f, abilityPower) + execute *
        (RankValue(base, rank) + 0.65f * std::max(0.0f, abilityPower));
}
inline float RRawDamage(int rank, float abilityPower) {
    static constexpr std::array<float, 4> base{ 0, 150, 250, 350 };
    return RankValue(base, rank) + 0.50f * std::max(0.0f, abilityPower);
}
inline int PassiveStacksAfterKill(int stacks, int monstersKilled) {
    return std::max(0, stacks) + std::max(0, monstersKilled);
}
inline bool ECanReset(bool targetPoisoned, bool targetMarked, bool eReady) {
    return eReady && (targetPoisoned || targetMarked);
}
inline bool RStuns(float facingCosine) { return facingCosine >= 0.0f; }
inline bool RHits(float distance, float lateralDistance, float targetRadius) {
    return distance <= kRRange + targetRadius && lateralDistance <= kRWidth * 0.5f + targetRadius;
}
struct ZoneContext {
    bool Valid = false;
    bool Walkable = false;
    bool UnderTurret = false;
    bool EnemyInside = false;
    bool PoisonNeeded = false;
    bool Fleeing = false;
};
inline bool ZoneSafe(const ZoneContext& context) {
    return context.Valid && context.Walkable &&
           (!context.UnderTurret || context.Fleeing) &&
           (context.EnemyInside || context.PoisonNeeded || context.Fleeing);
}
struct RContext {
    bool Ready = false;
    bool PredictionAccepted = false;
    bool Hit = false;
    bool WallBlocked = false;
    bool Lethal = false;
    bool MultiTarget = false;
    bool Defensive = false;
};
inline bool MayCastR(const RContext& context) {
    if (!context.Ready || !context.PredictionAccepted || !context.Hit || context.WallBlocked) return false;
    return context.Lethal || context.MultiTarget || context.Defensive;
}
struct AutomaticContext { bool Defensive = false; bool Interrupt = false; bool KillSecure = false; bool Engage = false; };
inline bool AutomaticAllowed(const AutomaticContext& c) {
    return !c.Engage && (c.Defensive || c.Interrupt || c.KillSecure);
}
} // namespace Plugins::KuroAIO::AI::Controllers::Cassiopeia::Geometry
