#pragma once

#include "../../AIGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace Plugins::KuroAIO::AI::Controllers::Nautilus::Geometry {

using SharedGeometry::Direction2D;
using SharedGeometry::ProjectPointToSegment2D;
using Vec3 = ::Vec3;

inline constexpr float kQRange = 1100.0f;
inline constexpr float kQHalfWidth = 45.0f;
inline constexpr float kQCastSeconds = 0.25f;
inline constexpr float kQMissileSpeed = 2000.0f;
inline constexpr float kQDashSpeed = 1500.0f;
inline constexpr float kQTerrainTolerance = 18.0f;
inline constexpr float kWShieldDuration = 6.0f;
inline constexpr float kEWaveRange = 600.0f;
inline constexpr float kEWaveRadius = 350.0f;
inline constexpr float kEWaveDelay = 0.25f;
inline constexpr float kRRange = 825.0f;
inline constexpr float kRChannelSeconds = 1.0f;
inline constexpr float kRTargetRadius = 90.0f;

struct Collision {
    bool Hit = false;
    int NetworkId = 0;
    float EntryDistance = 0.0f;
};

inline bool SegmentContact(const Vec3& origin, const Vec3& endpoint,
                           const Vec3& center, float radius,
                           float halfWidth = kQHalfWidth) {
    if (origin.IsZero() || endpoint.IsZero() || center.IsZero()) return false;
    const auto projection = ProjectPointToSegment2D(center, origin, endpoint);
    return projection.T >= 0.0f && projection.T <= 1.0f &&
           projection.Distance <= std::max(0.0f, halfWidth) +
                                      std::max(0.0f, radius);
}

template <std::size_t N>
inline Collision FirstCollision(const Vec3& origin, const Vec3& endpoint,
                               const Vec3& target, float targetRadius,
                               int targetId,
                               const std::array<Vec3, N>& blockers,
                               const std::array<float, N>& blockerRadii) {
    Collision result{};
    if (!SegmentContact(origin, endpoint, target, targetRadius)) return result;
    const float targetDistance = origin.Distance2D(target);
    result = {true, targetId, targetDistance};
    for (std::size_t i = 0; i < N; ++i) {
        if (!SegmentContact(origin, endpoint, blockers[i], blockerRadii[i])) continue;
        const float distance = origin.Distance2D(blockers[i]);
        if (distance + 0.01f < result.EntryDistance)
            result = {true, 0, distance};
    }
    return result;
}

inline Vec3 ClampDashEndpoint(const Vec3& origin, const Vec3& requested) {
    const Vec3 direction = Direction2D(origin, requested);
    if (direction.IsZero()) return origin;
    return origin + direction * std::min(kQRange, origin.Distance2D(requested));
}

inline bool DashEndpointSafe(const Vec3& endpoint, bool terrainBlocked,
                             bool endpointUnderTurret, bool originUnderTurret,
                             int enemiesAtEndpoint, int maximumEnemies = 2) {
    if (endpoint.IsZero() || terrainBlocked) return false;
    if (endpointUnderTurret && !originUnderTurret) return false;
    return enemiesAtEndpoint <= std::max(0, maximumEnemies);
}

inline bool DashCollision(const Vec3& origin, const Vec3& endpoint,
                          const Vec3& target, float targetRadius) {
    return SegmentContact(origin, endpoint, target, targetRadius, 110.0f);
}

inline bool PassiveRootReady(int nowMs, int lastRootMs,
                            int cooldownMs = 6000) {
    return lastRootMs <= 0 || nowMs - lastRootMs >= cooldownMs;
}

inline bool PassiveRootApplies(bool targetChampion, bool targetImmune,
                               bool rootReady) {
    return targetChampion && !targetImmune && rootReady;
}

inline float WShieldValue(int rank, float maxHealth, float abilityPower) {
    const int clampedRank = std::clamp(rank, 1, 5);
    const float base = 30.0f + 10.0f * static_cast<float>(clampedRank);
    const float healthRatio = 0.06f + 0.01f * static_cast<float>(clampedRank);
    return std::max(0.0f, base + maxHealth * healthRatio + abilityPower * 0.4f);
}

inline bool EWaveHits(const Vec3& origin, const Vec3& target,
                      float targetRadius = 0.0f) {
    if (origin.IsZero() || target.IsZero()) return false;
    return origin.Distance2D(target) <=
           kEWaveRadius + std::max(0.0f, targetRadius);
}

inline float EDamage(int rank, float abilityPower) {
    const int clampedRank = std::clamp(rank, 1, 5);
    constexpr float base[] = {0.0f, 55.0f, 75.0f, 95.0f, 115.0f, 135.0f};
    return base[clampedRank] + std::max(0.0f, abilityPower) * 0.5f;
}

struct DepthChargeTrack {
    int TargetId = 0;
    Vec3 LastPosition = {};
    int StartedAtMs = 0;
    int ExpectedImpactAtMs = 0;
    bool Active = false;
};

inline bool BeginDepthCharge(DepthChargeTrack& track, int targetId,
                             const Vec3& targetPosition, int nowMs,
                             float travelSeconds = kRChannelSeconds) {
    if (targetId <= 0 || targetPosition.IsZero()) return false;
    track = {targetId, targetPosition, nowMs,
             nowMs + static_cast<int>(std::max(0.0f, travelSeconds) * 1000.0f),
             true};
    return true;
}

inline bool UpdateDepthCharge(DepthChargeTrack& track, int targetId,
                              const Vec3& targetPosition, int nowMs) {
    if (!track.Active || targetId != track.TargetId || targetPosition.IsZero())
        return false;
    track.LastPosition = targetPosition;
    return nowMs < track.ExpectedImpactAtMs;
}

inline bool ChannelSafe(bool targetValid, bool targetSpellShielded,
                        bool playerImmobile, bool underEnemyTurret,
                        bool enemyDiveThreat, bool alliedFollowup) {
    if (!targetValid || targetSpellShielded || playerImmobile) return false;
    if (underEnemyTurret && enemyDiveThreat && !alliedFollowup) return false;
    return true;
}

inline bool RTargetInRange(const Vec3& origin, const Vec3& target,
                           float targetRadius = kRTargetRadius) {
    if (origin.IsZero() || target.IsZero()) return false;
    return origin.Distance2D(target) <= kRRange + std::max(0.0f, targetRadius);
}

} // namespace Plugins::KuroAIO::AI::Controllers::Nautilus::Geometry
