#pragma once

#include "../AIGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace Plugins::KuroAIO::AI::Controllers::RekSai::Geometry {

using Vec3 = ::Vec3;
using SharedGeometry::Direction2D;

inline constexpr float kQBurrowedRange = 1625.0f;
inline constexpr float kQUnburrowedRange = 325.0f;
inline constexpr float kQWidth = 65.0f;
inline constexpr float kQDelay = 0.40f;
inline constexpr float kWKnockupRadius = 165.0f;
inline constexpr float kWKnockupDuration = 1.25f;
inline constexpr float kTunnelRange = 2500.0f;
inline constexpr float kTunnelRadius = 115.0f;
inline constexpr float kTunnelDuration = 10.0f;
inline constexpr float kERange = 325.0f;
inline constexpr float kRRange = 1500.0f;
inline constexpr float kRLandingRadius = 210.0f;

inline constexpr float RankValue(int rank, const std::array<float, 5>& values) {
    return values[std::clamp(rank, 1, 5) - 1];
}
inline constexpr float RankValue3(int rank, const std::array<float, 3>& values) {
    return values[std::clamp(rank, 1, 3) - 1];
}
inline constexpr float QRawDamage(int rank, float attackDamage) {
    return RankValue(rank, {15.0f, 20.0f, 25.0f, 30.0f, 35.0f}) +
           0.50f * std::max(0.0f, attackDamage);
}
inline constexpr float PreySeekerRawDamage(int rank, float bonusAttackDamage) {
    return RankValue(rank, {60.0f, 90.0f, 120.0f, 150.0f, 180.0f}) +
           0.50f * std::max(0.0f, bonusAttackDamage);
}
inline constexpr float EBaseDamage(int rank) {
    return RankValue(rank, {55.0f, 60.0f, 65.0f, 70.0f, 75.0f});
}
inline constexpr float FuryMultiplier(float fury) {
    return 1.0f + std::clamp(fury, 0.0f, 100.0f) / 100.0f;
}
inline constexpr bool IsFuryTrueDamage(float fury) { return fury >= 100.0f; }
inline constexpr float FuriousBiteRawDamage(int rank, float bonusAttackDamage,
                                            float fury) {
    return (EBaseDamage(rank) + 1.0f * std::max(0.0f, bonusAttackDamage)) *
           FuryMultiplier(fury);
}
inline constexpr float FuriousBiteDamageThreshold(float rankThreeBase,
                                                   float bonusAttackDamage) {
    return std::max(0.0f, rankThreeBase + bonusAttackDamage);
}
inline constexpr float VoidRushRawDamage(int rank, float bonusAttackDamage,
                                         float targetMissingHealthPercent) {
    const float missing = std::clamp(targetMissingHealthPercent, 0.0f, 100.0f);
    return RankValue3(rank, {100.0f, 250.0f, 400.0f}) +
           0.80f * std::max(0.0f, bonusAttackDamage) + 0.20f * missing;
}
inline constexpr float PassiveHealRaw(int level, float maxHealth, float fury) {
    const float t = static_cast<float>(std::clamp(level, 1, 18) - 1) / 17.0f;
    const float percent = 0.02f + 0.08f * t;
    return std::max(0.0f, maxHealth) * percent *
           (std::clamp(fury, 0.0f, 100.0f) / 100.0f);
}

inline bool SegmentHits(const Vec3& start, const Vec3& end, const Vec3& target,
                        float halfWidth, float targetRadius = 0.0f) {
    if (start.IsZero() || end.IsZero() || target.IsZero()) return false;
    const auto projection = SharedGeometry::ProjectPointToSegment2D(target, start, end);
    return projection.T >= 0.0f && projection.T <= 1.0f &&
           projection.Distance <= std::max(0.0f, halfWidth) +
                                      std::max(0.0f, targetRadius);
}
inline bool KnockupHits(const Vec3& origin, const Vec3& target,
                        float targetRadius = 0.0f) {
    return !origin.IsZero() && !target.IsZero() &&
           origin.Distance2D(target) <= kWKnockupRadius +
                                         std::max(0.0f, targetRadius);
}
inline Vec3 ClampTunnelEndpoint(const Vec3& origin, const Vec3& requested) {
    if (!origin.IsValid() || !requested.IsValid() || origin.IsZero() ||
        requested.IsZero()) return {};
    const Vec3 direction = Direction2D(origin, requested);
    if (direction.IsZero()) return {};
    return origin + direction * std::min(kTunnelRange,
                                         origin.Distance2D(requested));
}
inline bool TunnelPlacementSafe(const Vec3& origin, const Vec3& endpoint,
                                bool wall, bool enemyTurret,
                                int nearbyEnemies, int maximumEnemies) {
    if (origin.IsZero() || endpoint.IsZero() || wall || enemyTurret ||
        origin.Distance2D(endpoint) < 120.0f) return false;
    return nearbyEnemies <= std::max(0, maximumEnemies);
}

struct TunnelState {
    int NetworkId = 0;
    Vec3 Start{};
    Vec3 End{};
    int SpawnTick = 0;
    int ExpireTick = 0;
    bool Confirmed = false;
};
inline bool TunnelActive(const TunnelState& tunnel, int now) {
    return tunnel.NetworkId != 0 && !tunnel.Start.IsZero() &&
           !tunnel.End.IsZero() && tunnel.ExpireTick > now;
}
inline void RecordTunnel(TunnelState& tunnel, int id, const Vec3& start,
                         const Vec3& end, int now, bool confirmed = false) {
    tunnel.NetworkId = id;
    tunnel.Start = start;
    tunnel.End = end;
    tunnel.SpawnTick = now;
    tunnel.ExpireTick = now + static_cast<int>(kTunnelDuration * 1000.0f);
    tunnel.Confirmed = confirmed;
}
inline Vec3 NearestTunnelEndpoint(const std::array<TunnelState, 16>& tunnels,
                                  const Vec3& origin, int now) {
    Vec3 best{};
    float distance = 1.0e9f;
    for (const auto& tunnel : tunnels) {
        if (!TunnelActive(tunnel, now)) continue;
        const float startDistance = origin.Distance2D(tunnel.Start);
        const float endDistance = origin.Distance2D(tunnel.End);
        if (startDistance < distance) { distance = startDistance; best = tunnel.Start; }
        if (endDistance < distance) { distance = endDistance; best = tunnel.End; }
    }
    return best;
}
inline bool MarkedTargetRAllowed(bool targetMarked, bool targetValid,
                                 bool endpointWalkable, bool turretLanding,
                                 int enemiesAtLanding, int maximumEnemies,
                                 bool lethal) {
    if (!targetMarked || !targetValid || !endpointWalkable || turretLanding)
        return false;
    return lethal || enemiesAtLanding <= std::max(0, maximumEnemies);
}

} // namespace Plugins::KuroAIO::AI::Controllers::RekSai::Geometry
