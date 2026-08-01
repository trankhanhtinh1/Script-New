#pragma once

// Pure Ekko mechanics. No SDK objects or live state are required here so
// passive sequencing, Q flight, delayed W, E safety, and Chronobreak policy
// remain deterministic and boundary-testable.
#include "../../AIGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace Plugins::KuroAIO::AI::Controllers::Ekko::Geometry {

using SharedGeometry::Direction2D;
using SharedGeometry::ProjectPointToSegment2D;

inline constexpr float kQRange = 1075.0f;
inline constexpr float kQRadius = 60.0f;
inline constexpr float kQSpeed = 1650.0f;
inline constexpr float kQReturnSpeed = 2300.0f;
inline constexpr int kQCastDelayMs = 250;
inline constexpr int kQReturnWindowMs = 2200;
inline constexpr float kWRange = 1600.0f;
inline constexpr float kWRadius = 375.0f;
inline constexpr int kWDelayMs = 3000;
inline constexpr int kWStunMs = 2250;
inline constexpr float kERange = 325.0f;
inline constexpr float kEAttackRange = 250.0f;
inline constexpr float kRRange = 4000.0f;
inline constexpr float kRRadius = 375.0f;
inline constexpr int kRRewindWindowMs = 4000;

struct PassiveState {
    int TargetId = 0;
    int Hits = 0;
    int LastHitTick = 0;
};

inline constexpr int kPassiveWindowMs = 4000;

inline PassiveState ObservePassiveHit(PassiveState state, int targetId,
                                       int nowTick) {
    if (targetId == 0) return state;
    if (state.TargetId != targetId || state.LastHitTick <= 0 ||
        nowTick - state.LastHitTick > kPassiveWindowMs) {
        state.TargetId = targetId;
        state.Hits = 0;
    }
    state.Hits = std::clamp(state.Hits + 1, 1, 3);
    state.LastHitTick = nowTick;
    return state;
}

inline bool PassiveReady(const PassiveState& state, int targetId,
                         int nowTick) {
    return targetId != 0 && state.TargetId == targetId && state.Hits >= 3 &&
           state.LastHitTick > 0 && nowTick - state.LastHitTick <= kPassiveWindowMs;
}

inline PassiveState ReconcilePassive(const PassiveState& state, int nowTick) {
    if (state.LastHitTick <= 0 || nowTick - state.LastHitTick <= kPassiveWindowMs)
        return state;
    return {};
}

inline float PassiveRawDamage(int rank, float abilityPower) {
    static constexpr std::array<float, 6> base{0.0f, 30.0f, 50.0f, 70.0f, 90.0f, 110.0f};
    return base[std::clamp(rank, 0, 5)] + 0.8f * std::max(0.0f, abilityPower);
}

inline float QOutboundRawDamage(int rank, float abilityPower) {
    static constexpr std::array<float, 6> base{0.0f, 45.0f, 60.0f, 75.0f, 90.0f, 105.0f};
    return base[std::clamp(rank, 0, 5)] + 0.1f * std::max(0.0f, abilityPower);
}

inline float QReturnRawDamage(int rank, float abilityPower) {
    static constexpr std::array<float, 6> base{0.0f, 45.0f, 60.0f, 75.0f, 90.0f, 105.0f};
    return base[std::clamp(rank, 0, 5)] + 0.6f * std::max(0.0f, abilityPower);
}

inline float WRawDamage(int rank, float abilityPower) {
    static constexpr std::array<float, 6> base{0.0f, 70.0f, 100.0f, 130.0f, 160.0f, 190.0f};
    return base[std::clamp(rank, 0, 5)] + 0.3f * std::max(0.0f, abilityPower);
}

inline float WShieldAmount(int rank, float abilityPower) {
    static constexpr std::array<float, 6> base{0.0f, 100.0f, 120.0f, 140.0f, 160.0f, 180.0f};
    return base[std::clamp(rank, 0, 5)] + 0.4f * std::max(0.0f, abilityPower);
}

inline float ERawDamage(int rank, float abilityPower) {
    static constexpr std::array<float, 6> base{0.0f, 50.0f, 75.0f, 100.0f, 125.0f, 150.0f};
    return base[std::clamp(rank, 0, 5)] + 0.4f * std::max(0.0f, abilityPower);
}

inline float RRawDamage(int rank, float abilityPower, float missingHealthPercent) {
    static constexpr std::array<float, 4> base{0.0f, 150.0f, 300.0f, 450.0f};
    const float missing = std::clamp(missingHealthPercent, 0.0f, 100.0f);
    return (base[std::clamp(rank, 0, 3)] + 1.5f * std::max(0.0f, abilityPower)) *
           (1.0f + 0.03f * missing);
}

inline Vec3 ClampQEndpoint(const Vec3& origin, const Vec3& requested) {
    const Vec3 direction = Direction2D(origin, requested);
    if (direction.IsZero()) return {};
    return origin + direction * std::min(kQRange, origin.Distance2D(requested));
}

inline Vec3 PredictLinear(const Vec3& position, const Vec3& velocity,
                          float delaySeconds, float maximumDistance = 350.0f) {
    if (!position.IsValid() || !velocity.IsValid() || !std::isfinite(delaySeconds) ||
        delaySeconds < 0.0f) return {};
    Vec3 result = position + velocity * delaySeconds;
    result.y = position.y;
    const float travel = result.Distance2D(position);
    if (travel > maximumDistance && travel > 0.001f)
        result = position + Direction2D(position, result) * maximumDistance;
    return result;
}

inline float QOutboundTravelSeconds(const Vec3& origin, const Vec3& endpoint) {
    return origin.IsValid() && endpoint.IsValid() && kQSpeed > 0.0f
        ? origin.Distance2D(endpoint) / kQSpeed + kQCastDelayMs / 1000.0f : 0.0f;
}

inline float QReturnTravelSeconds(const Vec3& from, const Vec3& to) {
    return from.IsValid() && to.IsValid() && kQReturnSpeed > 0.0f
        ? from.Distance2D(to) / kQReturnSpeed : 0.0f;
}

inline bool QPathClear(const Vec3& origin, const Vec3& endpoint,
                      bool wallBlocked, bool collisionBlocked) {
    return origin.IsValid() && endpoint.IsValid() &&
           !endpoint.IsZero() && origin.Distance2D(endpoint) > 1.0f &&
           !wallBlocked && !collisionBlocked;
}

struct WZone {
    Vec3 Center = {};
    int CastTick = 0;
    bool Armed = false;
};

inline bool WZoneArrived(const WZone& zone, int nowTick) {
    return zone.Armed && zone.CastTick > 0 && nowTick - zone.CastTick >= kWDelayMs;
}

inline bool WTargetInside(const WZone& zone, const Vec3& target,
                          float targetRadius = 0.0f) {
    return zone.Center.IsValid() && target.IsValid() &&
           zone.Center.Distance2D(target) <= kWRadius + std::max(0.0f, targetRadius);
}

inline bool QReturnIntersects(const Vec3& missile, const Vec3& caster,
                              const Vec3& target, float targetRadius = 0.0f) {
    if (!missile.IsValid() || !caster.IsValid() || !target.IsValid()) return false;
    const auto projection = ProjectPointToSegment2D(target, missile, caster);
    return projection.T > 0.0f && projection.T < 1.0f &&
           projection.Distance <= kQRadius + std::max(0.0f, targetRadius);
}

inline bool WCastAllowed(const Vec3& center, const Vec3& target,
                         float targetRadius, bool protectedTarget,
                         bool predictedInside, bool manualConsent = false) {
    if (!center.IsValid() || center.IsZero() || protectedTarget) return false;
    if (!predictedInside && !manualConsent) return false;
    return center.Distance2D(target) <= kWRange + kWRadius + std::max(0.0f, targetRadius);
}

inline Vec3 ClampEEndpoint(const Vec3& origin, const Vec3& requested) {
    const Vec3 direction = Direction2D(origin, requested);
    if (direction.IsZero()) return {};
    return origin + direction * std::min(kERange, origin.Distance2D(requested));
}

inline bool EEndpointSafe(bool endpointWalkable, bool endpointUnderTurret,
                          bool playerUnderTurret, int enemiesAtEndpoint,
                          int maximumEnemies, bool lethal, bool fleeing,
                          bool manualConsent = false) {
    if (!endpointWalkable) return false;
    if (!fleeing && endpointUnderTurret && !playerUnderTurret && !lethal && !manualConsent)
        return false;
    return fleeing || lethal || manualConsent || enemiesAtEndpoint <= std::max(0, maximumEnemies);
}

struct RContext {
    bool Ready = false;
    bool ReturnPositionValid = false;
    bool EndpointWalkable = false;
    bool EndpointUnderTurret = false;
    bool PlayerUnderTurret = false;
    int EnemiesAtEndpoint = 0;
    int MaximumEnemies = 1;
    bool Lethal = false;
    bool Defensive = false;
    bool Fleeing = false;
    float PlayerHealthPercent = 100.0f;
    float MinimumHealthPercent = 20.0f;
};

inline bool RewindEndpointSafe(const RContext& context) {
    if (!context.Ready || !context.ReturnPositionValid || !context.EndpointWalkable)
        return false;
    if (!context.Lethal && !context.Defensive && !context.Fleeing)
        return false;
    if (context.EndpointUnderTurret && !context.PlayerUnderTurret &&
        !context.Lethal && !context.Defensive && !context.Fleeing) return false;
    if (!context.Fleeing && !context.Lethal && !context.Defensive &&
        context.EnemiesAtEndpoint > std::max(0, context.MaximumEnemies)) return false;
    return context.PlayerHealthPercent >= context.MinimumHealthPercent ||
           context.Defensive || context.Fleeing;
}

inline bool RDamageWorthwhile(const RContext& context) {
    return context.Lethal || context.Defensive || context.Fleeing;
}

struct ModeContext {
    bool SelectedTarget = false;
    bool OrbwalkerTarget = false;
    bool AttackWindingUp = false;
    bool Lethal = false;
    bool ManualAssist = false;
};

inline bool MayUseAbility(const ModeContext& context) {
    if (context.ManualAssist) return false;
    if (context.AttackWindingUp && !context.Lethal) return false;
    return context.SelectedTarget || context.OrbwalkerTarget;
}

} // namespace Plugins::KuroAIO::AI::Controllers::Ekko::Geometry
