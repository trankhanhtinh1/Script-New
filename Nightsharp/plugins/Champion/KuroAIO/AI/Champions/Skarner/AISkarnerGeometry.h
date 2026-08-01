#pragma once

#include "../../AIGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace Plugins::KuroAIO::AI::Controllers::Skarner::Geometry {

using SharedGeometry::Direction2D;
using Vec3 = ::Vec3;

inline constexpr float kQAttackWindow = 5.0f;
inline constexpr float kQRange = 350.0f;
inline constexpr float kQThirdAttackRange = 800.0f;
inline constexpr float kQWidth = 110.0f;
inline constexpr float kQDelay = 0.25f;
inline constexpr float kWRadius = 350.0f;
inline constexpr float kWDelay = 0.25f;
inline constexpr float kERange = 1000.0f;
inline constexpr float kESpeed = 1500.0f;
inline constexpr float kEWidth = 120.0f;
inline constexpr float kEStunRadius = 120.0f;
inline constexpr int kEStunDurationMs = 750;
inline constexpr float kRRange = 350.0f;
inline constexpr float kRWidth = 160.0f;
inline constexpr float kRDelay = 0.50f;
inline constexpr float kRSpeed = 1500.0f;
inline constexpr int kRDurationMs = 1750;
inline constexpr int kRMaximumTargets = 3;

inline constexpr float RankValue(int rank, const std::array<float, 5>& values) {
    return values[static_cast<std::size_t>(std::clamp(rank, 1, 5) - 1)];
}
inline constexpr float QRawDamage(int rank, float bonusAttackDamage,
                                  float targetMaximumHealth = 0.0f) {
    return RankValue(rank, {10.0f, 20.0f, 30.0f, 40.0f, 50.0f}) +
        0.80f * std::max(0.0f, bonusAttackDamage) +
        RankValue(rank, {0.01f, 0.0125f, 0.015f, 0.0175f, 0.02f}) *
            std::max(0.0f, targetMaximumHealth);
}
inline constexpr float WRawDamage(int rank, float abilityPower) {
    return RankValue(rank, {50.0f, 75.0f, 100.0f, 125.0f, 150.0f}) +
        0.80f * std::max(0.0f, abilityPower);
}
inline constexpr float WShield(int rank, float maximumHealth) {
    return RankValue(rank, {80.0f, 110.0f, 140.0f, 170.0f, 200.0f}) +
        0.08f * std::max(0.0f, maximumHealth);
}
inline constexpr float ERawDamage(int rank, float bonusAttackDamage) {
    return RankValue(rank, {30.0f, 60.0f, 90.0f, 120.0f, 150.0f}) +
        0.40f * std::max(0.0f, bonusAttackDamage);
}
inline constexpr float RRawDamagePerPass(int rank, float bonusAttackDamage) {
    return RankValue(rank, {20.0f, 40.0f, 60.0f}) +
        0.30f * std::max(0.0f, bonusAttackDamage);
}

inline bool SegmentHits(const Vec3& start, const Vec3& end, const Vec3& target,
                        float halfWidth, float targetRadius = 0.0f) {
    if (!start.IsValid() || !end.IsValid() || !target.IsValid()) return false;
    const auto projection = SharedGeometry::ProjectPointToSegment2D(target, start, end);
    return projection.Distance <= std::max(0.0f, halfWidth) +
        std::max(0.0f, targetRadius);
}
inline Vec3 PredictedEndpoint(const Vec3& origin, const Vec3& aim,
                              float range = kERange) {
    const Vec3 direction = Direction2D(origin, aim);
    if (direction.IsZero() || !origin.IsValid() || !aim.IsValid()) return {};
    return origin + direction * std::min(std::max(0.0f, range), origin.Distance2D(aim));
}
// Pure terrain sampler: the controller supplies a NavMesh callback when live data exists.
inline bool TerrainBlocks(const Vec3& origin, const Vec3& endpoint,
                          bool (*isWall)(const Vec3&) = nullptr,
                          float sampleSpacing = 30.0f) {
    if (!origin.IsValid() || !endpoint.IsValid())
        return true;
    if (!isWall) return false;
    const Vec3 direction = Direction2D(origin, endpoint);
    const float distance = origin.Distance2D(endpoint);
    if (direction.IsZero() || !std::isfinite(distance)) return true;
    for (float d = std::max(1.0f, sampleSpacing); d < distance;
         d += std::max(8.0f, sampleSpacing))
        if (isWall(origin + direction * d)) return true;
    return false;
}
inline bool DashEndpointSafe(const Vec3& origin, const Vec3& endpoint,
                             bool terrainClear, bool endpointWalkable,
                             bool startedUnderEnemyTurret,
                             bool endpointUnderEnemyTurret,
                             int enemiesAtEndpoint, int maximumEnemies,
                             bool lethal, bool defensive) {
    if (!origin.IsValid() || !endpoint.IsValid() || endpoint.IsZero() ||
        !endpointWalkable || !terrainClear) return false;
    if (endpointUnderEnemyTurret && !startedUnderEnemyTurret && !lethal && !defensive)
        return false;
    return defensive || lethal || enemiesAtEndpoint <= std::max(0, maximumEnemies);
}

struct CollisionResult {
    bool Hit = false;
    int NetworkId = 0;
    Vec3 Contact{};
};
inline CollisionResult FirstCollision(const Vec3& origin, const Vec3& endpoint,
                                      const std::array<CollisionResult, 8>& candidates) {
    CollisionResult best{};
    float bestT = 2.0f;
    for (const auto& candidate : candidates) {
        if (!candidate.Hit || !candidate.Contact.IsValid()) continue;
        const auto projection = SharedGeometry::ProjectPointToSegment2D(
            candidate.Contact, origin, endpoint);
        if (projection.T < bestT) {
            best = candidate;
            bestT = projection.T;
        }
    }
    return best;
}
inline int CountLineHits(const Vec3& origin, const Vec3& endpoint,
                         const std::array<Vec3, 8>& positions,
                         const std::array<float, 8>& radii, int count,
                         float width = kRWidth) {
    int hits = 0;
    const int bounded = std::clamp(count, 0, 8);
    for (int i = 0; i < bounded; ++i)
        if (SegmentHits(origin, endpoint, positions[i], width * 0.5f, radii[i])) ++hits;
    return hits;
}

enum class QState { Ready, Empowered, RockReady };
inline QState QStateAfterCast() { return QState::Empowered; }
inline QState QStateAfterAttack(QState state) {
    if (state == QState::Empowered) return QState::RockReady;
    if (state == QState::RockReady) return QState::Ready;
    return state;
}

struct EContext {
    bool Ready = false;
    bool TargetValid = false;
    bool PredictionAccepted = false;
    bool CollisionOwned = false;
    bool EndpointSafe = false;
    bool Defensive = false;
    bool Lethal = false;
    int StunnedEnemies = 0;
};
inline bool ShouldCastE(const EContext& context) {
    return context.Ready && context.TargetValid && context.PredictionAccepted &&
        context.CollisionOwned && context.EndpointSafe &&
        (context.Defensive || context.Lethal || context.StunnedEnemies <= 1);
}

enum class ImpaleState { Idle, CastPending, Active, Released };
struct RContext {
    bool Ready = false;
    bool TargetValid = false;
    bool PredictionAccepted = false;
    bool TerrainClear = false;
    bool AttackWindingUp = false;
    bool Lethal = false;
    bool Defensive = false;
    bool Manual = false;
    int PredictedHits = 0;
    int MinimumTargets = 2;
};
inline bool ShouldImpale(const RContext& context) {
    if (!context.Ready || !context.TargetValid || !context.PredictionAccepted ||
        !context.TerrainClear) return false;
    if (context.AttackWindingUp && !context.Lethal && !context.Defensive && !context.Manual)
        return false;
    return context.Lethal || context.Defensive || context.Manual ||
        context.PredictedHits >= std::max(1, context.MinimumTargets);
}

struct AutomaticContext {
    bool Defensive = false;
    bool Interrupt = false;
    bool KillSecure = false;
    bool Engage = false;
    bool ManualOwnership = false;
};
inline bool AutomaticAllowed(const AutomaticContext& context) {
    return !context.ManualOwnership && !context.Engage &&
        (context.Defensive || context.Interrupt || context.KillSecure);
}

} // namespace Plugins::KuroAIO::AI::Controllers::Skarner::Geometry
