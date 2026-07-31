#pragma once

#include "../AIGeometry.h"

#include <algorithm>
#include <cmath>

namespace Plugins::KuroAIO::AI::Controllers::Swain::Geometry {

using Vec3 = ::Vec3;
using SharedGeometry::Direction2D;

inline constexpr float kQRange = 750.0f;
inline constexpr float kQHalfAngleDegrees = 10.0f;
inline constexpr float kQDelay = 0.25f;
inline constexpr float kWRange = 7500.0f;
inline constexpr float kWEffectRadius = 325.0f;
inline constexpr float kWVisionRadius = 420.0f;
inline constexpr float kWDelay = 0.25f;
inline constexpr float kERange = 850.0f;
inline constexpr float kERecastRange = 1150.0f;
inline constexpr float kEWidth = 85.0f;
inline constexpr float kESpeed = 935.0f;
inline constexpr float kEExplosionRadius = 205.0f;
inline constexpr float kEPullDistance = 290.0f;
inline constexpr float kERootDuration = 1.5f;
inline constexpr float kRRadius = 600.0f;
inline constexpr float kRDrainPerSecond = 10.0f;
inline constexpr float kRRegenPerSecond = 20.0f;
inline constexpr float kRMaxDemonPower = 50.0f;
inline constexpr float kRMinDemonflareDelaySeconds = 2.0f;
inline constexpr float kRDamageAPRatio = 0.04f;
inline constexpr float kRHealAPRatio = 0.05f;
inline constexpr float kRHealMaxHealthRatio = 0.015f;

inline constexpr float DemonDamagePerSecond(float baseDamage, float ap) {
    return std::max(0.0f, baseDamage) + std::max(0.0f, ap) * kRDamageAPRatio;
}

inline constexpr float DemonHealPerSecond(float baseHeal, float ap,
                                          float maxHealth) {
    return std::max(0.0f, baseHeal) + std::max(0.0f, ap) * kRHealAPRatio +
        std::max(0.0f, maxHealth) * kRHealMaxHealthRatio;
}
inline constexpr float kRMaxDemonflareDelaySeconds = 5.0f;
inline constexpr float kSoulCollectionRange = 1100.0f;
inline constexpr int kSoulHealthIncrement = 15;

struct SoulState {
    int Fragments = 0;
    int BonusHealth = 0;
    int LastCollectedTick = 0;
};

inline constexpr SoulState CollectSoul(SoulState state, int now,
                                        int healthIncrement = kSoulHealthIncrement) {
    state.Fragments = std::max(0, state.Fragments) + 1;
    state.BonusHealth = state.Fragments * std::max(0, healthIncrement);
    state.LastCollectedTick = now;
    return state;
}

inline constexpr SoulState ReconcileSouls(SoulState state, int observedFragments,
                                          int now,
                                          int healthIncrement = kSoulHealthIncrement) {
    state.Fragments = std::max(0, observedFragments);
    state.BonusHealth = state.Fragments * std::max(0, healthIncrement);
    state.LastCollectedTick = now;
    return state;
}

inline bool SoulPickupInRange(const Vec3& player, const Vec3& soul,
                              float range = kSoulCollectionRange) {
    return player.IsValid() && soul.IsValid() && !soul.IsZero() &&
        player.Distance2D(soul) <= std::max(0.0f, range);
}

struct TetherState {
    bool Outbound = false;
    bool PullReady = false;
    int TargetId = 0;
    int CastTick = 0;
    int PullExpireTick = 0;
    int RootExpireTick = 0;
};

inline constexpr TetherState BeginNevermove(TetherState state, int now,
                                             int targetId) {
    state.Outbound = true;
    state.PullReady = false;
    state.TargetId = targetId;
    state.CastTick = now;
    state.PullExpireTick = 0;
    state.RootExpireTick = 0;
    return state;
}

inline constexpr TetherState ArmPull(TetherState state, int now, int targetId,
                                     int windowMs = 1150) {
    state.Outbound = false;
    state.PullReady = true;
    state.TargetId = targetId;
    state.PullExpireTick = now + std::max(1, windowMs);
    state.RootExpireTick = now + static_cast<int>(kERootDuration * 1000.0f);
    return state;
}

inline constexpr TetherState ReconcileTether(TetherState state, bool outbound,
                                             bool pullReady, int targetId,
                                             int now) {
    state.Outbound = outbound;
    state.PullReady = pullReady;
    state.TargetId = targetId;
    if (pullReady && state.PullExpireTick <= now) state.PullExpireTick = now + 900;
    if (!pullReady) state.PullExpireTick = 0;
    return state;
}

inline constexpr TetherState ExpireTether(TetherState state, int now) {
    if (state.PullExpireTick > 0 && state.PullExpireTick <= now) {
        state.PullReady = false;
        state.TargetId = 0;
        state.PullExpireTick = 0;
    }
    if (state.RootExpireTick > 0 && state.RootExpireTick <= now) state.RootExpireTick = 0;
    return state;
}

inline Vec3 PullPosition(const Vec3& swain, const Vec3& target,
                         float distance = kEPullDistance) {
    if (!swain.IsValid() || !target.IsValid() || target.IsZero()) return {};
    const Vec3 direction = Direction2D(target, swain);
    if (direction.IsZero()) return target;
    return target + direction * std::min(std::max(0.0f, distance), target.Distance2D(swain));
}

inline bool LineHits(const Vec3& origin, const Vec3& aim, const Vec3& target,
                     float range = kERange, float width = kEWidth,
                     float targetRadius = 0.0f) {
    if (!origin.IsValid() || !aim.IsValid() || !target.IsValid()) return false;
    const Vec3 direction = Direction2D(origin, aim);
    if (direction.IsZero()) return false;
    const Vec3 end = origin + direction * std::min(std::max(0.0f, range),
                                                    origin.Distance2D(aim));
    const auto projection = SharedGeometry::ProjectPointToSegment2D(target, origin, end);
    return projection.T >= 0.0f && projection.T <= 1.0f &&
        projection.Distance <= std::max(0.0f, width * 0.5f) + std::max(0.0f, targetRadius);
}

inline bool ConeContains(const Vec3& origin, const Vec3& aim, const Vec3& target,
                         float range = kQRange,
                         float halfAngleDegrees = kQHalfAngleDegrees,
                         float targetRadius = 0.0f) {
    if (!origin.IsValid() || !aim.IsValid() || !target.IsValid()) return false;
    const Vec3 forward = Direction2D(origin, aim);
    const Vec3 toTarget = Direction2D(origin, target);
    const float distance = origin.Distance2D(target);
    if (forward.IsZero() || toTarget.IsZero() ||
        distance > std::max(0.0f, range) + std::max(0.0f, targetRadius)) return false;
    const float cosine = std::clamp(forward.Dot(toTarget), -1.0f, 1.0f);
    const float angle = std::acos(cosine) * 180.0f / 3.14159265358979323846f;
    const float radiusAllowance = distance > 0.0f
        ? std::asin(std::clamp(targetRadius / distance, 0.0f, 1.0f)) * 180.0f /
            3.14159265358979323846f
        : 90.0f;
    return angle <= std::max(0.0f, halfAngleDegrees) + radiusAllowance;
}

inline bool CircleHits(const Vec3& center, const Vec3& target, float radius,
                       float targetRadius = 0.0f) {
    return center.IsValid() && target.IsValid() && !target.IsZero() &&
        center.Distance2D(target) <= std::max(0.0f, radius) + std::max(0.0f, targetRadius);
}

inline Vec3 ClampRange(const Vec3& origin, const Vec3& requested, float range) {
    if (!origin.IsValid() || !requested.IsValid()) return {};
    const Vec3 direction = Direction2D(origin, requested);
    if (direction.IsZero()) return {};
    return origin + direction * std::min(std::max(0.0f, range), origin.Distance2D(requested));
}

struct DemonState {
    bool Active = false;
    bool RecastReady = false;
    float Power = 0.0f;
    int StartedTick = 0;
    int LastTick = 0;
    int LastEnemyCount = 0;
};

inline constexpr DemonState StartDemon(DemonState state, int now, float power = 0.0f) {
    state.Active = true;
    state.RecastReady = false;
    state.Power = std::clamp(power, 0.0f, kRMaxDemonPower);
    state.StartedTick = now;
    state.LastTick = now;
    state.LastEnemyCount = 0;
    return state;
}

inline constexpr DemonState AdvanceDemon(DemonState state, int now, int enemies,
                                         float maxPower = kRMaxDemonPower) {
    if (!state.Active) return state;
    const int elapsed = std::max(0, now - state.LastTick);
    const float seconds = static_cast<float>(elapsed) / 1000.0f;
    state.Power = std::clamp(state.Power - seconds * kRDrainPerSecond +
        static_cast<float>(std::max(0, enemies)) * seconds * kRRegenPerSecond,
        0.0f, std::max(0.0f, maxPower));
    state.LastTick = now;
    state.LastEnemyCount = std::max(0, enemies);
    state.RecastReady = state.Power >= 0.0f &&
        now - state.StartedTick >= static_cast<int>(kRMinDemonflareDelaySeconds * 1000.0f);
    return state;
}

inline constexpr bool DemonflareWindow(const DemonState& state, int now) {
    return state.Active && state.RecastReady && state.LastTick <= now &&
        now - state.StartedTick >= static_cast<int>(kRMinDemonflareDelaySeconds * 1000.0f);
}

struct UltimateContext {
    bool Ready = false;
    bool Active = false;
    bool RecastReady = false;
    bool AreaValid = false;
    bool Lethal = false;
    bool Defensive = false;
    bool Manual = false;
    bool AttackWindingUp = false;
    bool UnderEnemyTurret = false;
    int NearbyEnemies = 0;
    int MinimumEnemies = 2;
};

inline constexpr bool ShouldCastDemon(const UltimateContext& context) {
    if (!context.Ready || context.Active || context.UnderEnemyTurret || !context.AreaValid) return false;
    if (context.AttackWindingUp && !context.Lethal && !context.Defensive && !context.Manual) return false;
    return context.Lethal || context.Defensive || context.Manual ||
        context.NearbyEnemies >= std::max(1, context.MinimumEnemies);
}

inline constexpr bool ShouldDemonflare(const UltimateContext& context) {
    if (!context.Active || !context.RecastReady || !context.AreaValid) return false;
    if (context.UnderEnemyTurret) return false;
    return context.Lethal || context.Defensive || context.Manual ||
        context.NearbyEnemies >= std::max(1, context.MinimumEnemies);
}

} // namespace Plugins::KuroAIO::AI::Controllers::Swain::Geometry
