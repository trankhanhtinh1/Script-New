#pragma once

#include "../AIGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace Plugins::KuroAIO::AI::Controllers::Lissandra::Geometry {

using SharedGeometry::Direction2D;
using SharedGeometry::ProjectPointToSegment2D;
using SharedGeometry::RankValue;
using SharedGeometry::Rotate2D;

inline constexpr float kQRange = 725.0f;
inline constexpr float kQWidth = 75.0f;
inline constexpr float kQSpeed = 2200.0f;
inline constexpr float kQSpreadHalfAngle = 0.22f;
inline constexpr float kWRadius = 225.0f;
inline constexpr float kERange = 1050.0f;
inline constexpr float kEWidth = 120.0f;
inline constexpr float kESpeed = 850.0f;
inline constexpr float kRRange = 550.0f;
inline constexpr float kRRadius = 450.0f;
inline constexpr float kRDelaySeconds = 0.10f;
inline constexpr float kThrallLifetimeSeconds = 8.0f;

inline float QRawDamage(int rank, float abilityPower) {
    static constexpr std::array<float, 6> base{0, 70, 100, 130, 160, 190};
    return RankValue(base, rank) + std::max(0.0f, abilityPower) * 0.80f;
}
inline float WRawDamage(int rank, float abilityPower) {
    static constexpr std::array<float, 6> base{0, 70, 100, 130, 160, 190};
    return RankValue(base, rank) + std::max(0.0f, abilityPower) * 0.70f;
}
inline float ERawDamage(int rank, float abilityPower) {
    static constexpr std::array<float, 6> base{0, 70, 100, 130, 160, 190};
    return RankValue(base, rank) + std::max(0.0f, abilityPower) * 0.60f;
}
inline float RRawDamage(int rank, float abilityPower) {
    static constexpr std::array<float, 4> base{0, 125, 200, 275};
    return RankValue(base, rank) + std::max(0.0f, abilityPower) * 0.75f;
}
inline float RMissingHealthMultiplier(float missingHealthPercent) {
    return 1.0f + std::clamp(missingHealthPercent, 0.0f, 100.0f) * 0.03f;
}
inline float TravelSeconds(const Vec3& start, const Vec3& end, float speed) {
    if (!start.IsValid() || !end.IsValid()) return 0.0f;
    return start.Distance2D(end) / std::max(1.0f, speed);
}
inline Vec3 ClampDestination(const Vec3& origin, const Vec3& desired, float range) {
    if (!origin.IsValid() || !desired.IsValid()) return {};
    const Vec3 direction = Direction2D(origin, desired);
    if (direction.IsZero()) return origin;
    Vec3 result = origin + direction * std::min(std::max(0.0f, range), origin.Distance2D(desired));
    result.y = desired.y;
    return result;
}
inline Vec3 QShardEndpoint(const Vec3& origin, const Vec3& desired, bool left) {
    if (!origin.IsValid() || !desired.IsValid()) return {};
    const Vec3 direction = Direction2D(origin, desired);
    if (direction.IsZero()) return origin;
    const Vec3 spread = Rotate2D(direction, left ? kQSpreadHalfAngle : -kQSpreadHalfAngle);
    Vec3 endpoint = origin + spread * std::min(kQRange, origin.Distance2D(desired));
    endpoint.y = desired.y;
    return endpoint;
}
inline bool SegmentHits(const Vec3& start, const Vec3& end, const Vec3& target,
                        float width, float targetRadius = 0.0f) {
    if (!start.IsValid() || !end.IsValid() || !target.IsValid()) return false;
    const auto projection = ProjectPointToSegment2D(target, start, end);
    return projection.T >= 0.0f && projection.T <= 1.0f &&
        projection.Distance <= std::max(0.0f, width) + std::clamp(targetRadius, 0.0f, 150.0f);
}
inline bool QShardHits(const Vec3& origin, const Vec3& desired, const Vec3& target,
                       float targetRadius = 0.0f) {
    if (!origin.IsValid() || !desired.IsValid() || !target.IsValid()) return false;
    const Vec3 center = ClampDestination(origin, desired, kQRange);
    return SegmentHits(origin, center, target, kQWidth * 0.5f, targetRadius) ||
        SegmentHits(origin, QShardEndpoint(origin, desired, true), target, kQWidth * 0.35f, targetRadius) ||
        SegmentHits(origin, QShardEndpoint(origin, desired, false), target, kQWidth * 0.35f, targetRadius);
}
inline bool RootContains(const Vec3& center, const Vec3& target, float targetRadius = 0.0f) {
    return center.IsValid() && target.IsValid() &&
        center.Distance2D(target) <= kWRadius + std::clamp(targetRadius, 0.0f, 150.0f);
}
inline bool ClawPathHits(const Vec3& start, const Vec3& end, const Vec3& target,
                         float targetRadius = 0.0f) {
    return SegmentHits(start, end, target, kEWidth * 0.5f, targetRadius);
}
inline Vec3 ReturnPosition(const Vec3& origin, const Vec3& clawEnd, float progress) {
    if (!origin.IsValid() || !clawEnd.IsValid()) return {};
    return origin + (clawEnd - origin) * std::clamp(progress, 0.0f, 1.0f);
}
inline bool SafeReturn(const Vec3& origin, const Vec3& clawEnd,
                       bool endpointWalkable, bool underEnemyTurret,
                       bool incomingHardCc) {
    return origin.IsValid() && clawEnd.IsValid() && endpointWalkable &&
        (!underEnemyTurret || incomingHardCc);
}

struct RPolicyContext {
    bool Ready = false;
    bool TargetValid = false;
    bool TargetProtected = false;
    bool TargetInsideRange = false;
    bool TargetLethal = false;
    bool SelfLow = false;
    bool IncomingBurst = false;
    bool EnemyInsideRadius = false;
    bool AttackWindingUp = false;
    bool Manual = false;
};
inline bool ShouldCastTargetR(const RPolicyContext& c) {
    if (!c.Ready || !c.TargetValid || c.TargetProtected || !c.TargetInsideRange) return false;
    if (c.AttackWindingUp && !c.TargetLethal && !c.Manual) return false;
    return c.TargetLethal || c.Manual || c.EnemyInsideRadius;
}
inline bool ShouldCastSelfR(const RPolicyContext& c) {
    if (!c.Ready || c.TargetProtected) return false;
    return c.Manual || c.SelfLow || c.IncomingBurst || c.EnemyInsideRadius;
}
struct AutomaticContext {
    bool IncomingBurst = false;
    bool IncomingHardCc = false;
    bool LowHealthStasis = false;
    bool KillSecure = false;
    bool FreshEngage = false;
};
inline bool AutomaticAllowed(const AutomaticContext& c) {
    return !c.FreshEngage && (c.IncomingBurst || c.IncomingHardCc ||
                              c.LowHealthStasis || c.KillSecure);
}
struct ThrallState {
    Vec3 Position = {};
    int NetworkId = 0;
    int SpawnTick = 0;
    bool Active = false;
};
inline bool ThrallActive(const ThrallState& state, int nowTick) {
    return state.Active && state.SpawnTick >= 0 &&
        nowTick - state.SpawnTick <= static_cast<int>(kThrallLifetimeSeconds * 1000.0f);
}
inline bool ThrallSafe(const Vec3& position, bool alliedNearby, bool turretDanger) {
    return position.IsValid() && !position.IsZero() && !alliedNearby && !turretDanger;
}

} // namespace Plugins::KuroAIO::AI::Controllers::Lissandra::Geometry
