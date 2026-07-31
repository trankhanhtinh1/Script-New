#pragma once

#include "../AIGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace Plugins::KuroAIO::AI::Controllers::Urgot::Geometry {

using Vec3 = ::Vec3;
using SharedGeometry::Direction2D;

inline constexpr float kQRange = 800.0f;
inline constexpr float kQWidth = 210.0f;
inline constexpr float kQDelay = 0.30f;
inline constexpr float kQSpeed = 1600.0f;
inline constexpr float kWRange = 490.0f;
inline constexpr float kWShotInterval = 0.333f;
inline constexpr float kERange = 475.0f;
inline constexpr float kEWidth = 120.0f;
inline constexpr float kEDelay = 0.45f;
inline constexpr float kESpeed = 1750.0f;
inline constexpr float kEShieldDuration = 4.0f;
inline constexpr int kEStunDurationMs = 1000;
inline constexpr float kRRange = 2500.0f;
inline constexpr float kRWidth = 80.0f;
inline constexpr float kRDelay = 0.50f;
inline constexpr float kRSpeed = 3200.0f;
inline constexpr float kRExecutePercent = 25.0f;
inline constexpr int kRFearDurationMs = 1500;
inline constexpr int kRRecastWindowMs = 4000;
inline constexpr int kLegCount = 6;

inline constexpr float RankValue(int rank, const std::array<float, 5>& values) {
    return values[static_cast<std::size_t>(std::clamp(rank, 1, 5) - 1)];
}

inline constexpr float QRawDamage(int rank, float bonusAttackDamage,
                                  float targetMaximumHealth = 0.0f) {
    return RankValue(rank, {25.0f, 70.0f, 115.0f, 160.0f, 205.0f}) +
        0.70f * std::max(0.0f, bonusAttackDamage) +
        0.05f * std::max(0.0f, targetMaximumHealth);
}

inline constexpr float WPurgeShotRawDamage(int rank, float totalAttackDamage) {
    return RankValue(rank, {12.0f, 14.0f, 16.0f, 18.0f, 20.0f}) +
        0.50f * std::max(0.0f, totalAttackDamage);
}

inline constexpr float ERawDamage(int rank, float bonusAttackDamage) {
    return RankValue(rank, {60.0f, 90.0f, 120.0f, 150.0f, 180.0f}) +
        0.50f * std::max(0.0f, bonusAttackDamage);
}

inline constexpr float EShieldRaw(int rank, float bonusAttackDamage,
                                  float maximumHealth) {
    return RankValue(rank, {60.0f, 90.0f, 120.0f, 150.0f, 180.0f}) +
        0.50f * std::max(0.0f, bonusAttackDamage) +
        0.12f * std::max(0.0f, maximumHealth);
}

inline constexpr float RRawDamage(int rank, float bonusAttackDamage) {
    return RankValue(rank, {100.0f, 175.0f, 250.0f, 250.0f, 250.0f}) +
        0.50f * std::max(0.0f, bonusAttackDamage);
}

inline bool RExecuteThreshold(float healthPercent,
                              float threshold = kRExecutePercent) {
    return std::isfinite(healthPercent) && healthPercent <= threshold;
}

inline bool RCanRecast(bool hooked, bool recastReady,
                       float healthPercent,
                       float threshold = kRExecutePercent) {
    return hooked && recastReady && RExecuteThreshold(healthPercent, threshold);
}

inline bool RInitialCastAllowed(float healthPercent, float damage,
                                float currentHealth, float shield = 0.0f,
                                float threshold = 35.0f) {
    if (!std::isfinite(healthPercent) || healthPercent > threshold) return false;
    const float postHit = std::max(0.0f, currentHealth + std::max(0.0f, shield) -
        std::max(0.0f, damage));
    return healthPercent <= threshold || RExecuteThreshold(healthPercent) ||
           postHit <= 0.25f * std::max(1.0f, currentHealth + shield);
}

inline Vec3 ClampDashEndpoint(const Vec3& origin, const Vec3& aim,
                              float range = kERange) {
    if (!origin.IsValid() || !aim.IsValid() || origin.IsZero() || aim.IsZero()) return {};
    const Vec3 direction = Direction2D(origin, aim);
    if (direction.IsZero()) return {};
    return origin + direction * std::min(std::max(0.0f, range), origin.Distance2D(aim));
}

inline bool SegmentHits(const Vec3& start, const Vec3& end, const Vec3& target,
                        float halfWidth, float targetRadius = 0.0f) {
    if (!start.IsValid() || !end.IsValid() || !target.IsValid()) return false;
    const auto projection = SharedGeometry::ProjectPointToSegment2D(target, start, end);
    return projection.Distance <= std::max(0.0f, halfWidth) + std::max(0.0f, targetRadius);
}

inline bool TerrainBlocks(const Vec3& origin, const Vec3& endpoint,
                          bool (*isWall)(const Vec3&) = nullptr,
                          float spacing = 30.0f) {
    if (!origin.IsValid() || !endpoint.IsValid() || origin.IsZero() || endpoint.IsZero()) return true;
    if (!isWall) return false;
    const Vec3 direction = Direction2D(origin, endpoint);
    const float distance = origin.Distance2D(endpoint);
    if (direction.IsZero() || !std::isfinite(distance)) return true;
    for (float travelled = std::max(1.0f, spacing); travelled < distance;
         travelled += std::max(8.0f, spacing)) {
        if (isWall(origin + direction * travelled)) return true;
    }
    return false;
}

struct CollisionResult {
    bool Hit = false;
    int NetworkId = 0;
    Vec3 Contact{};
};

inline CollisionResult FirstCollision(const Vec3& origin, const Vec3& endpoint,
                                      const std::array<CollisionResult, 16>& candidates) {
    CollisionResult best{};
    float bestT = 2.0f;
    for (const auto& candidate : candidates) {
        if (!candidate.Hit || !candidate.Contact.IsValid()) continue;
        const auto projection = SharedGeometry::ProjectPointToSegment2D(candidate.Contact, origin, endpoint);
        if (projection.T < bestT) { best = candidate; bestT = projection.T; }
    }
    return best;
}

inline bool DashEndpointSafe(const Vec3& origin, const Vec3& endpoint,
                             bool terrainClear, bool endpointWalkable,
                             bool underEnemyTurret, bool lethal,
                             bool defensive, int enemiesAtEndpoint,
                             int maximumEnemies) {
    if (!origin.IsValid() || !endpoint.IsValid() || endpoint.IsZero() ||
        !terrainClear || !endpointWalkable) return false;
    if (underEnemyTurret && !lethal && !defensive) return false;
    return defensive || lethal || enemiesAtEndpoint <= std::max(0, maximumEnemies);
}

inline bool LegReadyAt(int now, int readyTick) { return readyTick <= now; }
inline bool AnyLegReady(const std::array<int, kLegCount>& readyTicks, int now) {
    for (const int tick : readyTicks) if (LegReadyAt(now, tick)) return true;
    return false;
}

inline int DirectionalLeg(const Vec3& origin, const Vec3& target,
                          int readyTick = 0, int now = 0) {
    if (!origin.IsValid() || !target.IsValid() || !LegReadyAt(now, readyTick)) return -1;
    const Vec3 delta = target - origin;
    if (delta.IsZero()) return -1;
    float angle = std::atan2(delta.z, delta.x);
    if (angle < 0.0f) angle += 2.0f * 3.14159265358979323846f;
    const int leg = static_cast<int>(std::floor((angle + 3.14159265358979323846f / 6.0f) /
        (2.0f * 3.14159265358979323846f / static_cast<float>(kLegCount)))) % kLegCount;
    return leg;
}

inline bool MarkActive(int targetId, int markedTargetId, int now, int expiryTick) {
    return targetId != 0 && targetId == markedTargetId && expiryTick > now;
}

inline bool QCollisionOwned(const CollisionResult& first, int intendedTargetId) {
    return first.Hit && intendedTargetId != 0 && first.NetworkId == intendedTargetId;
}

inline bool PreserveAttackWindup(bool windingUp, bool reactive, bool lethal,
                                 bool manualOwnership) {
    return windingUp && !reactive && !lethal && !manualOwnership;
}

inline bool ShouldUseW(bool active, bool targetInRange, bool marked,
                       bool incomingThreat, bool lowHealth, bool farmValue) {
    if (active) return targetInRange || incomingThreat || farmValue;
    return targetInRange && (marked || incomingThreat || lowHealth || farmValue);
}

struct RContext {
    bool Ready = false;
    bool PredictionAccepted = false;
    bool TargetValid = false;
    bool InitialGate = false;
    bool Recast = false;
    bool Manual = false;
    bool Lethal = false;
};
inline bool ShouldCastR(const RContext& context) {
    if (!context.Ready || !context.PredictionAccepted || !context.TargetValid) return false;
    return context.Recast || context.InitialGate || context.Lethal || context.Manual;
}

} // namespace Plugins::KuroAIO::AI::Controllers::Urgot::Geometry
