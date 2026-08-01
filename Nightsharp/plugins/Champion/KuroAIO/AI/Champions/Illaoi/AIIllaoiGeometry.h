#pragma once

// Pure Illaoi geometry and patch values. Runtime object ownership, prediction,
// and spell casts remain in AIIllaoiController.h so this header is standalone-testable.
#include "../../AIGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace Plugins::KuroAIO::AI::Controllers::Illaoi::Geometry {

using Vec3 = ::Vec3;
using SharedGeometry::Direction2D;
using SharedGeometry::ProjectPointToSegment2D;

inline constexpr float kQRange = 825.0f;
inline constexpr float kQHalfWidth = 52.5f;
inline constexpr float kQDelay = 0.75f;
inline constexpr float kQTentacleLength = 800.0f;
inline constexpr float kQTentacleWidth = 200.0f;
inline constexpr float kWRange = 350.0f;
inline constexpr float kWDurationSeconds = 6.0f;
inline constexpr float kWResetLockoutSeconds = 0.25f;
inline constexpr float kWTentacleRadius = 350.0f;
inline constexpr float kERange = 950.0f;
inline constexpr float kEHalfWidth = 25.0f;
inline constexpr float kESpeed = 1900.0f;
inline constexpr float kEDelay = 0.25f;
inline constexpr float kEVesselSeconds = 4.0f;
inline constexpr float kESpiritSeconds = 7.0f;
inline constexpr float kESlowSeconds = 1.5f;
inline constexpr float kESpiritLeashRange = 1500.0f;
inline constexpr float kRRange = 450.0f;
inline constexpr float kRRadius = 500.0f;
inline constexpr float kRDurationSeconds = 8.0f;
inline constexpr float kPassiveDisabledLifetimeSeconds = 30.0f;
inline constexpr float kPassiveSpawnRadius = 1200.0f;
inline constexpr float kPassiveSpawnDensity = 1000.0f;
inline constexpr float kPassiveTentacleLength = 925.0f;
inline constexpr float kPassiveTentacleWidth = 200.0f;

inline constexpr float Rank5(int rank, const std::array<float, 5>& values) {
    return values[static_cast<std::size_t>(std::clamp(rank, 1, 5) - 1)];
}
inline constexpr float Rank3(int rank, const std::array<float, 3>& values) {
    return values[static_cast<std::size_t>(std::clamp(rank, 1, 3) - 1)];
}

inline constexpr float QTentacleBonus(int rank) {
    return Rank5(rank, {0.10f, 0.15f, 0.20f, 0.25f, 0.30f});
}
inline constexpr float QDamage(int rank, int championLevel, float totalAttackDamage,
                               float abilityPower, bool slam = true) {
    const float levelBase = 9.0f + (180.0f - 9.0f) *
        (static_cast<float>(std::clamp(championLevel, 1, 18) - 1) / 17.0f);
    const float raw = levelBase + 1.10f * std::max(0.0f, totalAttackDamage) +
        0.40f * std::max(0.0f, abilityPower);
    return raw * (slam ? 1.0f + QTentacleBonus(rank) : 1.0f);
}
inline constexpr float WMinimumDamage(int rank) {
    return Rank5(rank, {10.0f, 20.0f, 30.0f, 40.0f, 50.0f});
}
inline constexpr float WTargetMaxHealthPercent(int rank) {
    return Rank5(rank, {0.025f, 0.030f, 0.035f, 0.040f, 0.045f});
}
inline constexpr float WDamage(int rank, float targetMaxHealth, float totalAttackDamage) {
    const float ratio = WTargetMaxHealthPercent(rank) + 0.00035f *
        std::max(0.0f, totalAttackDamage);
    return WMinimumDamage(rank) + std::max(0.0f, targetMaxHealth) * ratio;
}
inline constexpr float ETransferPercent(int rank) {
    return Rank5(rank, {0.25f, 0.30f, 0.35f, 0.40f, 0.45f});
}
inline constexpr float RBaseDamage(int rank) {
    return Rank3(rank, {150.0f, 350.0f, 550.0f});
}
inline constexpr float RDamage(int rank, float totalAttackDamage) {
    return RBaseDamage(rank) + 0.50f * std::max(0.0f, totalAttackDamage);
}
inline constexpr int RSpawnCount(int enemyChampionsInRadius) {
    return std::clamp(enemyChampionsInRadius, 1, 6);
}
inline constexpr float PassiveSpawnCooldownSeconds(int championLevel) {
    const float level = static_cast<float>(std::clamp(championLevel, 1, 18));
    return 18.0f - 11.0f * ((level - 1.0f) / 17.0f);
}

inline Vec3 ClampQEndpoint(const Vec3& origin, const Vec3& requested) {
    if (!origin.IsValid() || !requested.IsValid()) return {};
    const Vec3 direction = Direction2D(origin, requested);
    if (direction.IsZero()) return {};
    return origin + direction * std::min(kQRange, origin.Distance2D(requested));
}
inline bool QPathHits(const Vec3& origin, const Vec3& endpoint, const Vec3& target,
                      float targetRadius = 0.0f) {
    if (!origin.IsValid() || !endpoint.IsValid() || !target.IsValid()) return false;
    const auto projection = ProjectPointToSegment2D(target, origin,
        ClampQEndpoint(origin, endpoint));
    return projection.T >= 0.0f && projection.T <= 1.0f &&
        projection.Distance <= kQHalfWidth + std::max(0.0f, targetRadius);
}
inline bool WInRange(const Vec3& player, const Vec3& target, float targetRadius = 0.0f) {
    return player.IsValid() && target.IsValid() &&
        player.Distance2D(target) <= kWRange + std::max(0.0f, targetRadius);
}
inline bool ELineHits(const Vec3& origin, const Vec3& endpoint, const Vec3& target,
                      float targetRadius = 0.0f) {
    if (!origin.IsValid() || !endpoint.IsValid() || !target.IsValid()) return false;
    const auto projection = ProjectPointToSegment2D(target, origin, endpoint);
    return projection.T > 0.0f && projection.T <= 1.0f &&
        projection.Distance <= kEHalfWidth + std::max(0.0f, targetRadius);
}
inline bool Reachable(const Vec3& origin, const Vec3& target, float range,
                      float targetRadius = 0.0f) {
    return origin.IsValid() && target.IsValid() &&
        origin.Distance2D(target) <= range + std::max(0.0f, targetRadius);
}
inline bool SafeCommit(float healthPercent, int nearbyEnemies, bool endpointUnderTurret,
                       bool playerUnderTurret, bool lethal, bool reactive,
                       float minimumHealthPercent = 24.0f, int maximumEnemies = 3) {
    if (!reactive && endpointUnderTurret && !playerUnderTurret && !lethal) return false;
    if (!reactive && nearbyEnemies > maximumEnemies && !lethal) return false;
    if (!reactive && healthPercent <= minimumHealthPercent && !lethal) return false;
    return true;
}
inline bool SafeUltimate(float healthPercent, int nearbyEnemies, bool underTurret,
                         bool lethal, bool reactive, float lowHealth = 42.0f) {
    if (!reactive && !lethal && underTurret && healthPercent > lowHealth) return false;
    if (!reactive && !lethal && nearbyEnemies == 0 && healthPercent > lowHealth) return false;
    return reactive || lethal || healthPercent <= lowHealth || nearbyEnemies >= 2;
}

struct TentacleState {
    int Id = 0;
    Vec3 Position = {};
    int SpawnTick = 0;
    int DisabledUntil = 0;
    int LastSlamTick = 0;
    bool Alive = false;
    bool SpawnedByR = false;
};
inline TentacleState ObserveTentacle(const TentacleState& prior, int id, const Vec3& position,
                                     int now, bool spawnedByR) {
    TentacleState next = prior;
    next.Id = id;
    next.Position = position;
    next.SpawnTick = prior.Alive && prior.Id == id ? prior.SpawnTick : now;
    next.DisabledUntil = 0;
    next.LastSlamTick = prior.Id == id ? prior.LastSlamTick : 0;
    next.Alive = id != 0 && position.IsValid();
    next.SpawnedByR = spawnedByR || prior.SpawnedByR;
    return next;
}
inline TentacleState DisableTentacle(const TentacleState& prior, int now) {
    TentacleState next = prior;
    next.Alive = false;
    next.DisabledUntil = now + static_cast<int>(kPassiveDisabledLifetimeSeconds * 1000.0f);
    return next;
}
inline bool TentacleUsable(const TentacleState& tentacle, int now) {
    return tentacle.Id != 0 && tentacle.Alive && tentacle.Position.IsValid() &&
        (tentacle.DisabledUntil == 0 || tentacle.DisabledUntil <= now);
}
inline bool TentacleCanSlam(const TentacleState& tentacle, int now,
                            int minimumDelayMs = 900) {
    return TentacleUsable(tentacle, now) &&
        (tentacle.LastSlamTick == 0 || now - tentacle.LastSlamTick >= minimumDelayMs);
}
inline bool SpiritActive(int now, int targetId, int expiresTick) {
    return targetId != 0 && expiresTick > now;
}

} // namespace Plugins::KuroAIO::AI::Controllers::Illaoi::Geometry
