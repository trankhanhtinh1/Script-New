#pragma once

#include "../AIGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace Plugins::KuroAIO::AI::Controllers::Sejuani::Geometry {

using SharedGeometry::Direction2D;
using Vec3 = ::Vec3;

inline constexpr int kPassiveArmorReadyAtLevelOneMs = 12000;
inline constexpr int kPassiveArmorReadyAtLevelEighteenMs = 6000;
inline constexpr int kFrostArmorDurationMs = 3000;
inline constexpr float kQRange = 650.0f;
inline constexpr float kQWidth = 75.0f;
inline constexpr float kQRadius = 150.0f;
inline constexpr float kQDelay = 0.25f;
inline constexpr float kQSpeed = 1000.0f;
inline constexpr float kWRange = 600.0f;
inline constexpr float kWHalfWidth = 65.0f;
inline constexpr float kWFirstDelay = 0.25f;
inline constexpr float kWSecondDelay = 0.75f;
inline constexpr float kERange = 600.0f;
inline constexpr float kEAuraRange = 1100.0f;
inline constexpr float kEStunDuration = 1.0f;
inline constexpr int kEStunDurationMs = 1000;
inline constexpr int kEMaxStacks = 4;
inline constexpr int kEStackDurationMs = 5000;
inline constexpr float kRRange = 1300.0f;
inline constexpr float kRWidth = 120.0f;
inline constexpr float kRSpeed = 1600.0f;
inline constexpr float kRExplosionRadius = 550.0f;
inline constexpr float kRStunDuration = 1.0f;
inline constexpr float kREmpoweredStunDuration = 1.5f;

inline constexpr float RankValue(int rank, const std::array<float, 5>& values) {
    return values[static_cast<std::size_t>(std::clamp(rank, 1, 5) - 1)];
}
inline constexpr float RankValue3(int rank, const std::array<float, 3>& values) {
    return values[static_cast<std::size_t>(std::clamp(rank, 1, 3) - 1)];
}
inline constexpr int PassiveReadyDelayMs(int level) {
    const float t = static_cast<float>(std::clamp(level, 1, 18) - 1) / 17.0f;
    return static_cast<int>(static_cast<float>(kPassiveArmorReadyAtLevelOneMs) +
        static_cast<float>(kPassiveArmorReadyAtLevelEighteenMs -
            kPassiveArmorReadyAtLevelOneMs) * t);
}
inline constexpr bool FrostArmorReady(int now, int lastDamageTick, int level) {
    return now >= lastDamageTick + PassiveReadyDelayMs(level);
}
inline constexpr bool FrostArmorActive(int now, int lastDamageTick, int level,
                                       int armorBrokenTick = 0) {
    if (armorBrokenTick > 0 && now < armorBrokenTick + kFrostArmorDurationMs)
        return true;
    return FrostArmorReady(now, lastDamageTick, level);
}
inline constexpr float FrostArmorBonus(float bonusResist) {
    return 10.0f + 0.75f * std::max(0.0f, bonusResist);
}
inline constexpr float QRawDamage(int rank, float abilityPower) {
    return RankValue(rank, {40.0f, 90.0f, 140.0f, 190.0f, 240.0f}) +
        0.75f * std::max(0.0f, abilityPower);
}
inline constexpr float WFirstRawDamage(int rank, float abilityPower, float maxHealth) {
    return RankValue(rank, {5.0f, 15.0f, 25.0f, 35.0f, 45.0f}) +
        0.30f * std::max(0.0f, abilityPower) +
        0.04f * std::max(0.0f, maxHealth);
}
inline constexpr float WSecondRawDamage(int rank, float abilityPower, float maxHealth) {
    return RankValue(rank, {5.0f, 25.0f, 45.0f, 65.0f, 85.0f}) +
        0.60f * std::max(0.0f, abilityPower) +
        0.08f * std::max(0.0f, maxHealth);
}
inline constexpr float ERawDamage(int rank, float abilityPower) {
    return RankValue(rank, {55.0f, 105.0f, 155.0f, 205.0f, 255.0f}) +
        0.70f * std::max(0.0f, abilityPower);
}
inline constexpr float RRawDamage(int rank, float abilityPower, bool empowered) {
    return (empowered ? RankValue3(rank, {200.0f, 300.0f, 400.0f}) :
        RankValue3(rank, {125.0f, 150.0f, 175.0f})) +
        (empowered ? 0.80f : 0.40f) * std::max(0.0f, abilityPower);
}

inline bool SegmentHits(const Vec3& start, const Vec3& end, const Vec3& target,
                        float halfWidth, float targetRadius = 0.0f) {
    if (!start.IsValid() || !end.IsValid() || !target.IsValid()) return false;
    const auto projection = SharedGeometry::ProjectPointToSegment2D(target, start, end);
    return projection.T >= 0.0f && projection.T <= 1.0f &&
        projection.Distance <= std::max(0.0f, halfWidth) + std::max(0.0f, targetRadius);
}
inline Vec3 DashEndpoint(const Vec3& origin, const Vec3& requested) {
    if (!origin.IsValid() || !requested.IsValid() || origin.IsZero()) return {};
    const Vec3 direction = Direction2D(origin, requested);
    if (direction.IsZero()) return {};
    return origin + direction * std::min(kQRange, origin.Distance2D(requested));
}
inline bool TerrainBlocks(const Vec3& origin, const Vec3& endpoint,
                          bool (*isWall)(const Vec3&) = nullptr,
                          float sampleSpacing = 30.0f) {
    if (!origin.IsValid() || !endpoint.IsValid() || origin.IsZero() || endpoint.IsZero()) return true;
    if (!isWall) return false;
    const Vec3 direction = Direction2D(origin, endpoint);
    const float distance = origin.Distance2D(endpoint);
    if (direction.IsZero() || !std::isfinite(distance)) return true;
    for (float d = std::max(1.0f, sampleSpacing); d < distance; d += std::max(8.0f, sampleSpacing))
        if (isWall(origin + direction * d)) return true;
    return false;
}
inline bool WFirstHit(const Vec3& origin, const Vec3& aim, const Vec3& target,
                      float radius = 0.0f) {
    const Vec3 direction = Direction2D(origin, aim);
    if (direction.IsZero()) return false;
    const Vec3 endpoint = origin + direction * kWRange;
    return SegmentHits(origin, endpoint, target, kWHalfWidth, radius);
}
inline bool WSecondHit(const Vec3& origin, const Vec3& aim, const Vec3& target,
                       float radius = 0.0f) {
    const Vec3 direction = Direction2D(origin, aim);
    if (direction.IsZero()) return false;
    const Vec3 first = origin + direction * std::min(kWRange, 350.0f);
    const Vec3 endpoint = origin + direction * kWRange;
    return SegmentHits(first, endpoint, target, kWHalfWidth, radius);
}
inline int AdvanceEStacks(int stacks) { return std::clamp(stacks + 1, 0, kEMaxStacks); }
inline bool ECanStun(int stacks, int now, int lastStackTick) {
    return stacks >= kEMaxStacks && now <= lastStackTick + kEStackDurationMs;
}
inline bool EStackActive(int now, int lastStackTick) {
    return now <= lastStackTick + kEStackDurationMs;
}
inline bool SafeDashEndpoint(const Vec3& origin, const Vec3& endpoint,
                            bool terrainClear, bool endpointWalkable,
                            bool startedUnderTurret, bool endpointUnderTurret,
                            int enemiesAtEndpoint, int maximumEnemies,
                            bool lethal, bool defensive) {
    if (!origin.IsValid() || !endpoint.IsValid() || endpoint.IsZero() ||
        !terrainClear || !endpointWalkable) return false;
    if (endpointUnderTurret && !startedUnderTurret && !lethal && !defensive) return false;
    return defensive || lethal || enemiesAtEndpoint <= std::max(0, maximumEnemies);
}
inline bool SafeObjectiveCommit(bool objectiveEpic, bool objectiveUnderThreat,
                               bool qSecures, bool allyNearby, bool playerLow) {
    if (playerLow && !qSecures) return false;
    if (objectiveEpic && objectiveUnderThreat && !qSecures) return false;
    return qSecures || !objectiveEpic || allyNearby;
}
inline bool SafeAllyPeel(bool allyValid, bool allyLow, bool enemyNearAlly,
                        bool lineClear, bool underEnemyTurret, bool playerCanReach) {
    return allyValid && allyLow && enemyNearAlly && lineClear && playerCanReach &&
        (!underEnemyTurret || enemyNearAlly);
}

struct CollisionResult { bool Hit = false; int NetworkId = 0; Vec3 Contact{}; };
inline CollisionResult FirstCollision(const Vec3& origin, const Vec3& endpoint,
                                      const std::array<CollisionResult, 8>& candidates) {
    CollisionResult best{};
    float bestT = 2.0f;
    for (const auto& candidate : candidates) {
        if (!candidate.Hit || !candidate.Contact.IsValid()) continue;
        const auto projection = SharedGeometry::ProjectPointToSegment2D(candidate.Contact, origin, endpoint);
        if (projection.T >= 0.0f && projection.T <= 1.0f && projection.T < bestT) {
            best = candidate; bestT = projection.T;
        }
    }
    return best;
}
inline int CountLineHits(const Vec3& origin, const Vec3& endpoint,
                         const std::array<Vec3, 8>& positions,
                         const std::array<float, 8>& radii, int count) {
    int hits = 0;
    for (int i = 0; i < std::clamp(count, 0, 8); ++i)
        if (SegmentHits(origin, endpoint, positions[static_cast<std::size_t>(i)],
                        kRWidth * 0.5f, radii[static_cast<std::size_t>(i)])) ++hits;
    return hits;
}
inline bool ShouldCastR(bool ready, bool targetValid, bool predictionAccepted,
                        bool projectileBlocked, bool lethal, bool defensive,
                        bool manual, int predictedHits, int minimumHits) {
    if (!ready || !targetValid || !predictionAccepted || projectileBlocked) return false;
    return lethal || defensive || manual || predictedHits >= std::max(1, minimumHits);
}
inline bool AutomaticAllowed(bool defensive, bool interrupt, bool killSecure,
                            bool freshEngage, bool manualOwnership) {
    return !freshEngage && !manualOwnership && (defensive || interrupt || killSecure);
}

} // namespace Plugins::KuroAIO::AI::Controllers::Sejuani::Geometry
