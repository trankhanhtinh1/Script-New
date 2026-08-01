#pragma once

#include "../../AIGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace Plugins::KuroAIO::AI::Controllers::KhaZix::Geometry {

using SharedGeometry::Direction2D;
using SharedGeometry::ProjectPointToSegment2D;
using Vec3 = ::Vec3;

inline constexpr float kQRange = 325.0f;
inline constexpr float kWRange = 1000.0f;
inline constexpr float kWWidth = 70.0f;
inline constexpr float kWProjectileSpeed = 1700.0f;
inline constexpr float kERange = 700.0f;
inline constexpr float kRStealthSeconds = 1.25f;
inline constexpr float kIsolationRadius = 425.0f;
inline constexpr float kIsolationMultiplier = 2.0f;
inline constexpr float kQIsolationBonus = 1.0f;

inline bool InRange(const Vec3& origin, const Vec3& target, float range,
                   float targetRadius = 0.0f) {
    return origin.IsValid() && target.IsValid() &&
        origin.Distance2D(target) <= std::max(0.0f, range) +
            std::max(0.0f, targetRadius);
}

inline bool Isolated(int nearbyEnemies, int radius = static_cast<int>(kIsolationRadius)) {
    return radius > 0 && nearbyEnemies == 0;
}

inline float QRawDamage(int rank, float totalAttackDamage, bool isolated) {
    static constexpr std::array<float, 6> base{0.0f, 60.0f, 85.0f, 110.0f,
                                                135.0f, 160.0f};
    const int clamped = std::clamp(rank, 0, 5);
    const float raw = base[static_cast<std::size_t>(clamped)] +
        std::max(0.0f, totalAttackDamage) * 1.15f;
    return raw * (isolated ? kIsolationMultiplier : 1.0f);
}

inline bool QDamageLethal(float rawDamage, float targetHealth,
                          float targetShield) {
    return std::isfinite(rawDamage) && rawDamage > 0.0f &&
        rawDamage >= std::max(0.0f, targetHealth) + std::max(0.0f, targetShield);
}

inline bool WLineHits(const Vec3& origin, const Vec3& aim,
                      const Vec3& target, float targetRadius = 0.0f,
                      float range = kWRange) {
    const Vec3 direction = Direction2D(origin, aim);
    if (direction.IsZero() || !origin.IsValid() || !target.IsValid() ||
        !std::isfinite(range)) return false;
    const Vec3 end = origin + direction *
        std::min(std::max(0.0f, range), origin.Distance2D(aim));
    const auto projection = ProjectPointToSegment2D(target, origin, end);
    return projection.T >= 0.0f && projection.T <= 1.0f &&
        projection.Distance <= kWWidth * 0.5f + std::max(0.0f, targetRadius);
}

inline bool WProjectileClear(const Vec3& origin, const Vec3& aim,
                             const Vec3& blocker, float blockerRadius = 0.0f) {
    return !WLineHits(origin, aim, blocker, blockerRadius, kWRange);
}

inline Vec3 LeapEndpoint(const Vec3& origin, const Vec3& requested,
                         float range = kERange) {
    const Vec3 direction = Direction2D(origin, requested);
    if (direction.IsZero()) return origin;
    return origin + direction * std::min(std::max(0.0f, range),
                                         origin.Distance2D(requested));
}

inline bool LeapEndpointValid(const Vec3& origin, const Vec3& endpoint,
                              bool wall, bool endpointTurret,
                              bool originTurret, int enemiesAtEndpoint,
                              int maxEnemies, bool lethal = false) {
    if (!origin.IsValid() || !endpoint.IsValid() || endpoint.IsZero() || wall)
        return false;
    if (endpointTurret && !originTurret && !lethal) return false;
    return enemiesAtEndpoint <= std::max(0, maxEnemies);
}

inline bool StealthLive(int nowTick, int expireTick) {
    return nowTick >= 0 && expireTick > nowTick;
}

inline bool RRecastAvailable(bool evolved, bool stealthed, int castsUsed,
                             int nowTick, int expireTick) {
    return evolved && stealthed && castsUsed >= 1 && castsUsed < 3 &&
        StealthLive(nowTick, expireTick);
}

inline bool RTargetSafe(bool targetIsolated, bool targetUnderTurret,
                        bool playerUnderTurret, bool lethal) {
    return targetIsolated && (!targetUnderTurret || playerUnderTurret || lethal);
}

} // namespace Plugins::KuroAIO::AI::Controllers::KhaZix::Geometry
