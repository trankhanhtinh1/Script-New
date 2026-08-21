#pragma once

#include "../../AIGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace Plugins::KuroAIO::AI::Controllers::Zaahen::Geometry {

using SharedGeometry::Direction2D;
using Vec3 = ::Vec3;

inline constexpr int kPassiveMaximumStacks = 12;
inline constexpr float kPassiveStackSeconds = 5.0f;
inline constexpr float kPassiveFalloffIntervalSeconds = 0.5f;
inline constexpr float kPassivePerStackMin = 0.015f;
inline constexpr float kPassivePerStackMax = 0.028f;
inline constexpr float kPassiveMaximumMultiplier = 2.0f;
inline constexpr float kQRange = 25.0f;
inline constexpr float kQBonusRange = 25.0f;
inline constexpr float kQRecastDelaySeconds = 1.5f;
inline constexpr float kQWindowSeconds = 5.0f;
inline constexpr float kQRecastWindowSeconds = 4.0f;
inline constexpr float kWRange = 850.0f;
inline constexpr float kWHalfWidth = 35.0f;
inline constexpr float kWPullDistance = 225.0f;
inline constexpr float kWDelay = 0.50f;
inline constexpr float kERange = 350.0f;
inline constexpr float kEDashSpeed = 900.0f;
inline constexpr float kEMinimumRadius = 200.0f;
inline constexpr float kEMaximumRadius = 375.0f;
inline constexpr float kESweetMultiplier = 1.5f;
inline constexpr float kRRange = 600.0f;
inline constexpr float kRRadius = 550.0f;
inline constexpr float kRDashSpeed = 2800.0f;
inline constexpr float kRDelay = 0.50f;
inline constexpr float kRDamageReduction = 0.50f;
inline constexpr float kRChampionHealPercent = 0.33f;

inline constexpr float RankValue(int rank, const std::array<float, 5>& values) {
    return values[std::clamp(rank, 1, 5) - 1];
}
inline constexpr float UltimateRankValue(int rank, const std::array<float, 3>& values) {
    return values[std::clamp(rank, 1, 3) - 1];
}
inline constexpr float PassivePerStackRatio(int level) {
    const float t = static_cast<float>(std::clamp(level, 1, 18) - 1) / 17.0f;
    return kPassivePerStackMin + (kPassivePerStackMax - kPassivePerStackMin) * t;
}
inline constexpr int ClampPassiveStacks(int stacks) {
    return std::clamp(stacks, 0, kPassiveMaximumStacks);
}
inline constexpr float PassiveBonusADRatio(int level, int stacks) {
    const int normalized = ClampPassiveStacks(stacks);
    const float base = PassivePerStackRatio(level) * static_cast<float>(normalized);
    return normalized == kPassiveMaximumStacks ? base * kPassiveMaximumMultiplier : base;
}
inline int PassiveStacksAfterElapsed(int stacks, float elapsedSeconds) {
    const int initial = ClampPassiveStacks(stacks);
    if (!std::isfinite(elapsedSeconds) || elapsedSeconds <= kPassiveStackSeconds) return initial;
    const int lost = static_cast<int>(std::floor((elapsedSeconds - kPassiveStackSeconds) /
                                                  kPassiveFalloffIntervalSeconds));
    return std::max(0, initial - lost);
}

inline constexpr float QInitialRawDamage(int rank, float attackDamage) {
    return RankValue(rank, {15.0f, 30.0f, 45.0f, 60.0f, 75.0f}) +
           attackDamage * RankValue(rank, {0.15f, 0.20f, 0.25f, 0.30f, 0.35f});
}
inline constexpr float QRecastRawDamage(int rank, float attackDamage) {
    return RankValue(rank, {25.0f, 50.0f, 75.0f, 100.0f, 125.0f}) +
           attackDamage * RankValue(rank, {0.15f, 0.20f, 0.25f, 0.30f, 0.35f});
}
inline constexpr float QHealPercent(int rank) {
    return RankValue(rank, {0.05f, 0.06f, 0.07f, 0.08f, 0.09f});
}
inline constexpr float WInitialRawDamage(int rank, float bonusAttackDamage) {
    return RankValue(rank, {40.0f, 60.0f, 80.0f, 100.0f, 120.0f}) +
           0.50f * std::max(0.0f, bonusAttackDamage);
}
inline constexpr float WSecondaryRawDamage(int rank, float bonusAttackDamage) {
    return RankValue(rank, {30.0f, 50.0f, 70.0f, 90.0f, 110.0f}) +
           0.30f * std::max(0.0f, bonusAttackDamage);
}
inline constexpr float ERawDamage(int rank, float bonusAttackDamage, bool sweetSpot = false) {
    const float base = RankValue(rank, {40.0f, 60.0f, 80.0f, 100.0f, 120.0f}) +
        0.50f * std::max(0.0f, bonusAttackDamage);
    return base * (sweetSpot ? kESweetMultiplier : 1.0f);
}
inline constexpr float EBonusMagicDamagePercent(int rank) {
    return RankValue(rank, {0.04f, 0.05f, 0.06f, 0.07f, 0.08f});
}
inline constexpr float RRawDamage(int rank, float bonusAttackDamage) {
    return UltimateRankValue(rank, {250.0f, 400.0f, 550.0f}) +
           2.0f * std::max(0.0f, bonusAttackDamage);
}
inline constexpr float RArmorPenetration(int rank) {
    return UltimateRankValue(rank, {0.10f, 0.20f, 0.30f});
}

inline bool SegmentHits(const Vec3& start, const Vec3& end, const Vec3& target,
                        float halfWidth, float targetRadius = 0.0f) {
    return SharedGeometry::ProjectPointToSegment2D(target, start, end).Distance <=
        std::max(0.0f, halfWidth) + std::max(0.0f, targetRadius);
}
inline bool WHits(const Vec3& origin, const Vec3& aim, const Vec3& target,
                  float targetRadius = 0.0f) {
    const Vec3 direction = Direction2D(origin, aim);
    if (direction.IsZero()) return false;
    const Vec3 endpoint = origin + direction * kWRange;
    const auto projection = SharedGeometry::ProjectPointToSegment2D(target, origin, endpoint);
    return projection.T >= 0.0f && projection.T <= 1.0f &&
           projection.Distance <= kWHalfWidth + std::max(0.0f, targetRadius);
}
inline Vec3 ClampDash(const Vec3& origin, const Vec3& requested, float range = kERange) {
    const Vec3 direction = Direction2D(origin, requested);
    if (direction.IsZero()) return {};
    return origin + direction * std::min(std::max(0.0f, range), origin.Distance2D(requested));
}
inline Vec3 RLanding(const Vec3& origin, const Vec3& requested) {
    return ClampDash(origin, requested, kRRange);
}
inline bool InSweetSpot(const Vec3& center, const Vec3& target, float targetRadius = 0.0f) {
    return center.Distance2D(target) >= std::max(0.0f, kEMinimumRadius - targetRadius) &&
           center.Distance2D(target) <= kEMaximumRadius + std::max(0.0f, targetRadius);
}
inline int CircleHitCount(const Vec3& center, const Vec3& target, float radius,
                          float targetRadius = 0.0f) {
    return center.Distance2D(target) <= radius + std::max(0.0f, targetRadius) ? 1 : 0;
}

struct DashContext {
    bool Ready = false;
    bool EndpointValid = false;
    bool EndpointWalkable = false;
    bool TargetInOuterZone = false;
    bool EndpointUnderNewTurret = false;
    bool PlayerUnderEnemyTurret = false;
    int EnemiesAtEndpoint = 0;
    int MaximumEnemies = 2;
    bool Defensive = false;
    bool Lethal = false;
};
inline bool ShouldRush(const DashContext& context) {
    if (!context.Ready || !context.EndpointValid || !context.EndpointWalkable ||
        !context.TargetInOuterZone || context.EndpointUnderNewTurret) return false;
    if (context.PlayerUnderEnemyTurret && !context.Defensive && !context.Lethal) return false;
    return context.Defensive || context.Lethal ||
           context.EnemiesAtEndpoint <= std::max(0, context.MaximumEnemies);
}

struct UltimateContext {
    bool Ready = false;
    bool TargetValid = false;
    bool LandingValid = false;
    bool LandingSafe = false;
    bool PlayerLow = false;
    bool TargetLethal = false;
    bool Defensive = false;
    int EnemyHits = 0;
    int MinimumHits = 2;
};
inline bool ShouldDeliver(const UltimateContext& context) {
    if (!context.Ready || !context.TargetValid || !context.LandingValid || !context.LandingSafe) return false;
    return context.TargetLethal || context.Defensive || context.PlayerLow ||
           context.EnemyHits >= std::max(1, context.MinimumHits);
}

struct AutomaticContext {
    bool Defensive = false;
    bool Interrupt = false;
    bool KillSecure = false;
    bool Engage = false;
};
inline bool AutomaticAllowed(const AutomaticContext& context) {
    return !context.Engage &&
           (context.Defensive || context.Interrupt || context.KillSecure);
}

} // namespace Plugins::KuroAIO::AI::Controllers::Zaahen::Geometry
