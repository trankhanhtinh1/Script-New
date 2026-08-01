#pragma once

#include "../../AIGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace Plugins::KuroAIO::AI::Controllers::Gragas::Geometry {

using Vec3 = ::Vec3;
using SharedGeometry::Direction2D;

inline constexpr float kQRange = 850.0f;
inline constexpr float kQRadius = 250.0f;
inline constexpr float kQDelay = 0.25f;
inline constexpr float kQChargeSeconds = 4.0f;
inline constexpr float kQMaxDamageMultiplier = 1.5f;
inline constexpr float kWRange = 50.0f;
inline constexpr float kERange = 600.0f;
inline constexpr float kEDashRadius = 180.0f;
inline constexpr float kESpeed = 900.0f;
inline constexpr float kRRange = 1000.0f;
inline constexpr float kRRadius = 400.0f;
inline constexpr float kRDelay = 0.25f;
inline constexpr float kPassiveHealPercent = 0.055f;
inline constexpr int kPassiveBaseCooldownMs = 12000;

inline constexpr float RankValue(int rank, const std::array<float, 5>& values) {
    return values[std::clamp(rank, 1, 5) - 1];
}
inline constexpr float QBaseDamage(int rank) {
    return RankValue(rank, {80.0f, 120.0f, 160.0f, 200.0f, 240.0f});
}
inline constexpr float QRawDamage(int rank, float abilityPower,
                                  float chargeMultiplier = 1.0f) {
    return QBaseDamage(rank) + 0.80f * std::max(0.0f, abilityPower) *
        std::clamp(chargeMultiplier, 1.0f, kQMaxDamageMultiplier);
}
inline constexpr float WBaseDamage(int rank) {
    return RankValue(rank, {20.0f, 50.0f, 80.0f, 110.0f, 140.0f});
}
inline constexpr float WRawDamage(int rank, float targetMaximumHealth,
                                  float abilityPower) {
    return WBaseDamage(rank) + 0.07f * std::max(0.0f, targetMaximumHealth) +
        0.70f * std::max(0.0f, abilityPower);
}
inline constexpr float EBaseDamage(int rank) {
    return RankValue(rank, {80.0f, 125.0f, 170.0f, 215.0f, 260.0f});
}
inline constexpr float ERawDamage(int rank, float abilityPower) {
    return EBaseDamage(rank) + 0.60f * std::max(0.0f, abilityPower);
}
inline constexpr float RBaseDamage(int rank) {
    return RankValue(rank, {200.0f, 300.0f, 400.0f});
}
inline constexpr float RRawDamage(int rank, float abilityPower) {
    return RBaseDamage(rank) + 0.80f * std::max(0.0f, abilityPower);
}
inline constexpr float PassiveHeal(float maximumHealth) {
    return std::max(0.0f, maximumHealth) * kPassiveHealPercent;
}
inline constexpr int PassiveCooldownMs(int championLevel) {
    if (championLevel >= 16) return 6000;
    if (championLevel >= 11) return 8000;
    if (championLevel >= 6) return 10000;
    return kPassiveBaseCooldownMs;
}
inline bool PassiveReady(int now, int lastProcTick, int championLevel = 1) {
    return now >= lastProcTick + PassiveCooldownMs(championLevel);
}

inline Vec3 ClampBarrel(const Vec3& origin, const Vec3& requested) {
    if (!origin.IsValid() || !requested.IsValid()) return {};
    const Vec3 direction = Direction2D(origin, requested);
    if (direction.IsZero()) return origin;
    return origin + direction * std::min(kQRange, origin.Distance2D(requested));
}
inline bool BarrelHits(const Vec3& barrel, const Vec3& target,
                       float targetRadius = 0.0f) {
    return barrel.IsValid() && target.IsValid() &&
           barrel.Distance2D(target) <= kQRadius + std::max(0.0f, targetRadius);
}
inline bool SegmentHits(const Vec3& start, const Vec3& end,
                        const Vec3& target, float halfWidth,
                        float targetRadius = 0.0f) {
    if (!start.IsValid() || !end.IsValid() || !target.IsValid()) return false;
    return SharedGeometry::ProjectPointToSegment2D(target, start, end).Distance <=
        std::max(0.0f, halfWidth) + std::max(0.0f, targetRadius);
}
inline Vec3 ClampDash(const Vec3& origin, const Vec3& requested) {
    if (!origin.IsValid() || !requested.IsValid()) return {};
    const Vec3 direction = Direction2D(origin, requested);
    if (direction.IsZero()) return origin;
    return origin + direction * std::min(kERange, origin.Distance2D(requested));
}
inline bool BodySlamHits(const Vec3& origin, const Vec3& requested,
                         const Vec3& target, float targetRadius = 0.0f) {
    return SegmentHits(origin, ClampDash(origin, requested), target,
                       kEDashRadius, targetRadius);
}
inline Vec3 DisplacementEndpoint(const Vec3& target, const Vec3& awayFrom,
                                 float distance = 900.0f) {
    if (!target.IsValid() || !awayFrom.IsValid()) return {};
    const Vec3 direction = Direction2D(awayFrom, target);
    return direction.IsZero() ? target : target + direction * std::max(0.0f, distance);
}
inline bool CaskHits(const Vec3& origin, const Vec3& target,
                     float targetRadius = 0.0f) {
    return origin.IsValid() && target.IsValid() &&
        origin.Distance2D(target) <= kRRange + std::max(0.0f, targetRadius);
}
inline bool CaskKillSecure(float damage, float health, float shields = 0.0f) {
    return std::isfinite(damage) && damage >= std::max(0.0f, health) +
        std::max(0.0f, shields);
}
inline bool SafeDashEndpoint(const Vec3& endpoint, bool endpointWall,
                             bool endpointTurret, bool currentlyUnderTurret,
                             int enemies, int maximumEnemies, bool lethal) {
    if (!endpoint.IsValid() || endpoint.IsZero() || endpointWall) return false;
    if (endpointTurret && !currentlyUnderTurret && !lethal) return false;
    return lethal || enemies <= std::max(0, maximumEnemies);
}
inline float ResourceAfter(float resource, float cost) {
    return std::max(0.0f, resource) - std::max(0.0f, cost);
}

struct BarrelState {
    Vec3 Position{};
    int CastTick = 0;
    int DetonateAfterTick = 0;
    int ExpireTick = 0;
    bool Active = false;
};
inline void RecordBarrel(BarrelState& state, const Vec3& position, int now) {
    state.Position = position;
    state.CastTick = now;
    state.DetonateAfterTick = now + static_cast<int>(kQDelay * 1000.0f);
    state.ExpireTick = now + static_cast<int>(kQChargeSeconds * 1000.0f);
    state.Active = true;
}
inline bool BarrelCanDetonate(const BarrelState& state, int now) {
    return state.Active && state.Position.IsValid() &&
        now >= state.DetonateAfterTick && now <= state.ExpireTick;
}
inline void ExpireBarrel(BarrelState& state, int now) {
    if (state.Active && now > state.ExpireTick) state = {};
}

struct BodySlamContext {
    bool Ready = false;
    bool Collision = false;
    bool EndpointSafe = false;
    bool EndpointUnderNewTurret = false;
    int EnemiesAtEndpoint = 0;
    int MaximumEnemies = 2;
    bool Defensive = false;
    bool Lethal = false;
};
inline bool ShouldBodySlam(const BodySlamContext& context) {
    if (!context.Ready || !context.Collision || !context.EndpointSafe ||
        context.EndpointUnderNewTurret) return false;
    return context.Defensive || context.Lethal ||
        context.EnemiesAtEndpoint <= std::max(0, context.MaximumEnemies);
}
struct CaskContext {
    bool Ready = false;
    bool TargetValid = false;
    bool AimValid = false;
    bool ProjectileBlocked = false;
    bool KillSecure = false;
    bool Defensive = false;
    bool Manual = false;
    int PredictedHits = 0;
    int MinimumHits = 2;
};
inline bool ShouldCastCask(const CaskContext& context) {
    if (!context.Ready || !context.TargetValid || !context.AimValid ||
        context.ProjectileBlocked) return false;
    return context.KillSecure || context.Defensive || context.Manual ||
        context.PredictedHits >= std::max(1, context.MinimumHits);
}
struct AutomaticContext {
    bool Defensive = false;
    bool Interrupt = false;
    bool KillSecure = false;
    bool FreshEngage = false;
    bool ManualOwnership = false;
};
inline bool AutomaticAllowed(const AutomaticContext& context) {
    return !context.ManualOwnership && !context.FreshEngage &&
        (context.Defensive || context.Interrupt || context.KillSecure);
}

} // namespace Plugins::KuroAIO::AI::Controllers::Gragas::Geometry
