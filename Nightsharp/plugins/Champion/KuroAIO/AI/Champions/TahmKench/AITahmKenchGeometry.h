#pragma once

#include "../../AIGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace Plugins::KuroAIO::AI::Controllers::TahmKench::Geometry {

using SharedGeometry::ProjectPointToSegment2D;
using Vec3 = ::Vec3;

inline constexpr float kQRange = 900.0f;
inline constexpr float kQWidth = 70.0f;
inline constexpr float kQDelay = 0.25f;
inline constexpr float kQSpeed = 2000.0f;
inline constexpr float kWRangeRank1 = 1000.0f;
inline constexpr float kWRangePerRank = 50.0f;
inline constexpr float kWRadius = 250.0f;
inline constexpr float kWChannelSeconds = 1.35f;
inline constexpr float kWKnockupSeconds = 1.0f;
inline constexpr float kEShieldSeconds = 2.5f;
inline constexpr float kRRange = 300.0f;
inline constexpr float kRChannelSeconds = 3.0f;
inline constexpr int kPassiveMaxStacks = 3;
inline constexpr int kPassiveDurationMs = 5000;
inline constexpr int kPassiveDecayMs = 1000;

inline constexpr float RankValue(int rank, const std::array<float, 5>& values) {
    return values[std::clamp(rank, 1, 5) - 1];
}

inline constexpr int ClampStacks(int stacks) {
    return std::clamp(stacks, 0, kPassiveMaxStacks);
}

inline constexpr int AddPassiveStack(int stacks, int amount = 1) {
    return ClampStacks(stacks + std::max(0, amount));
}

inline constexpr bool PassiveReady(int stacks) {
    return ClampStacks(stacks) >= kPassiveMaxStacks;
}

inline constexpr bool PassiveStacksActive(int stacks, int nowTick, int lastAppliedTick) {
    return ClampStacks(stacks) > 0 && nowTick >= lastAppliedTick &&
        nowTick - lastAppliedTick < kPassiveDurationMs;
}

inline constexpr bool PassiveDecayDue(int nowTick, int lastAppliedTick) {
    return nowTick >= lastAppliedTick + kPassiveDecayMs;
}

inline constexpr float QSlowPercent(int rank) {
    return RankValue(rank, {0.50f, 0.50f, 0.50f, 0.50f, 0.50f});
}

inline constexpr float QRawDamage(int rank, float abilityPower) {
    return RankValue(rank, {75.0f, 120.0f, 165.0f, 210.0f, 255.0f}) +
        std::max(0.0f, abilityPower);
}

inline constexpr float PassiveRawDamage(int level, float bonusHealth, float abilityPower) {
    const float levelPart = 5.0f + (std::clamp(level, 1, 18) >= 12 ? 5.0f : 0.0f);
    return levelPart + 0.04f * std::max(0.0f, bonusHealth) +
        0.000125f * std::max(0.0f, abilityPower) * std::max(0.0f, bonusHealth);
}

inline constexpr float WRawDamage(int rank, float abilityPower) {
    return RankValue(rank, {100.0f, 135.0f, 170.0f, 205.0f, 240.0f}) +
        1.5f * std::max(0.0f, abilityPower);
}

inline constexpr float GreyHealthRatio(int rank) {
    return RankValue(rank, {0.15f, 0.23f, 0.31f, 0.39f, 0.47f});
}

inline constexpr float EnhancedGreyHealthRatio(int rank) {
    return RankValue(rank, {0.42f, 0.44f, 0.46f, 0.48f, 0.50f});
}

inline constexpr float GreyHealthValue(float greyHealth, int rank, bool enhanced = false) {
    return std::max(0.0f, greyHealth) * (enhanced ? EnhancedGreyHealthRatio(rank) : GreyHealthRatio(rank));
}

inline constexpr float EShieldRaw(int rank, float greyHealth, float abilityPower, bool enhanced = false) {
    return GreyHealthValue(greyHealth, rank, enhanced) + std::max(0.0f, abilityPower);
}

inline constexpr float REnemyRawDamage(int rank, float targetMaxHealth, float abilityPower) {
    return RankValue(rank, {100.0f, 250.0f, 400.0f, 550.0f, 700.0f}) +
        0.15f * std::max(0.0f, targetMaxHealth) + 0.70f * std::max(0.0f, abilityPower);
}

inline constexpr float RAllyShieldRaw(int rank, float greyHealth, float abilityPower) {
    return RankValue(rank, {650.0f, 800.0f, 950.0f, 1100.0f, 1250.0f}) +
        std::max(0.0f, greyHealth) + std::max(0.0f, abilityPower);
}

inline constexpr float WRange(int rank) {
    return kWRangeRank1 + 50.0f * static_cast<float>(std::clamp(rank, 1, 5) - 1);
}

inline bool FinitePoint(const Vec3& point) {
    return point.IsValid() && std::isfinite(point.x) && std::isfinite(point.y) && std::isfinite(point.z);
}

inline bool QLineHits(const Vec3& origin, const Vec3& endpoint, const Vec3& target,
                      float targetRadius = 0.0f) {
    if (!FinitePoint(origin) || !FinitePoint(endpoint) || !FinitePoint(target)) return false;
    const auto projection = ProjectPointToSegment2D(target, origin, endpoint);
    return projection.T >= 0.0f && projection.T <= 1.0f &&
        projection.Distance <= kQWidth * 0.5f + std::max(0.0f, targetRadius);
}

inline bool QReachable(const Vec3& origin, const Vec3& endpoint) {
    return FinitePoint(origin) && FinitePoint(endpoint) &&
        origin.Distance2D(endpoint) <= kQRange + 0.01f;
}

inline float QTravelSeconds(float distance) {
    if (!std::isfinite(distance) || distance < 0.0f) return 0.0f;
    return kQDelay + distance / kQSpeed;
}

inline bool WImpactHits(const Vec3& endpoint, const Vec3& target, float targetRadius = 0.0f) {
    return FinitePoint(endpoint) && FinitePoint(target) &&
        endpoint.Distance2D(target) <= kWRadius + std::max(0.0f, targetRadius);
}

inline bool WEndpointSafe(const Vec3& endpoint, bool wall, bool enemyTurret,
                          int enemiesAtEndpoint, int maxEnemies, bool lethal) {
    if (!FinitePoint(endpoint) || wall || enemyTurret) return false;
    return lethal || enemiesAtEndpoint <= std::max(0, maxEnemies);
}

inline float WTravelSeconds(float distance) {
    if (!std::isfinite(distance) || distance < 0.0f) return 0.0f;
    return kWChannelSeconds + distance / 1200.0f;
}

inline bool GreyHealthUsable(float greyHealth, float currentHealth, float maxHealth) {
    return std::isfinite(greyHealth) && std::isfinite(currentHealth) &&
        std::isfinite(maxHealth) && greyHealth > 0.0f && currentHealth > 0.0f && maxHealth > 0.0f;
}

inline bool REnemyEligible(int stacks, bool lethal, bool isolated, bool underEnemyTurret,
                           int enemiesNearby, int maxEnemies) {
    if (!PassiveReady(stacks) || underEnemyTurret) return false;
    return lethal || (isolated && enemiesNearby <= std::max(0, maxEnemies));
}

inline bool RAllyEligible(float allyHealthPercent, float threshold, bool threatened,
                          bool underEnemyTurret, int enemiesNearby, int alliesNearby) {
    if (allyHealthPercent > threshold || !threatened) return false;
    if (!underEnemyTurret) return true;
    return alliesNearby >= enemiesNearby;
}

inline bool RSpitEndpointSafe(const Vec3& origin, const Vec3& endpoint, bool wall,
                              bool enemyTurret, int enemies, int maxEnemies) {
    if (!FinitePoint(origin) || !FinitePoint(endpoint) || wall || enemyTurret ||
        origin.Distance2D(endpoint) > kRRange + 0.01f) return false;
    return enemies <= std::max(0, maxEnemies);
}

} // namespace Plugins::KuroAIO::AI::Controllers::TahmKench::Geometry
