#pragma once

#include "../AIGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace Plugins::KuroAIO::AI::Controllers::Rakan::Geometry {

using SharedGeometry::Direction2D;
using SharedGeometry::ProjectPointToSegment2D;

inline constexpr float kQRange = 900.0f;
inline constexpr float kQWidth = 65.0f;
inline constexpr float kQSpeed = 1450.0f;
inline constexpr float kWRange = 650.0f;
inline constexpr float kWRadius = 275.0f;
inline constexpr float kEDashRange = 700.0f;
inline constexpr float kRRadius = 450.0f;
inline constexpr float kRDurationSeconds = 4.0f;

inline bool FinitePoint(const Vec3& point) {
    return point.IsValid();
}

inline bool QHits(const Vec3& origin, const Vec3& endpoint,
                  const Vec3& target, float targetRadius = 0.0f,
                  float width = kQWidth) {
    if (!FinitePoint(origin) || !FinitePoint(endpoint) ||
        !FinitePoint(target)) return false;
    const auto projection = ProjectPointToSegment2D(target, origin, endpoint);
    return projection.Distance <= std::max(0.0f, width) * 0.5f +
        std::clamp(targetRadius, 0.0f, 150.0f);
}

inline float QTravelSeconds(float distance, float delay = 0.25f,
                            float speed = kQSpeed) {
    if (!std::isfinite(distance) || !std::isfinite(delay) ||
        !std::isfinite(speed) || distance < 0.0f) return 0.0f;
    return std::max(0.0f, delay) + distance / std::max(1.0f, speed);
}

inline bool QHealWindowOpen(float elapsedSeconds, float windowSeconds = 3.0f) {
    return std::isfinite(elapsedSeconds) && elapsedSeconds >= 0.0f &&
        elapsedSeconds <= std::max(0.0f, windowSeconds);
}

inline Vec3 WLandingPoint(const Vec3& origin, const Vec3& requested,
                          float range = kWRange) {
    if (!FinitePoint(origin) || !FinitePoint(requested)) return {};
    const Vec3 direction = Direction2D(origin, requested);
    if (direction.IsZero()) return {};
    return origin + direction * std::min(std::max(0.0f, range),
                                         origin.Distance2D(requested));
}

inline bool WKnockupHits(const Vec3& origin, const Vec3& landingPoint,
                         const Vec3& target, float targetRadius = 0.0f,
                         float radius = kWRadius) {
    if (!FinitePoint(origin) || !FinitePoint(landingPoint) ||
        !FinitePoint(target)) return false;
    return landingPoint.Distance2D(target) <= std::max(0.0f, radius) +
        std::clamp(targetRadius, 0.0f, 150.0f);
}

inline Vec3 AllyDashEndpoint(const Vec3& caster, const Vec3& ally,
                             float dashRange = kEDashRange) {
    if (!FinitePoint(caster) || !FinitePoint(ally)) return {};
    const Vec3 direction = Direction2D(caster, ally);
    if (direction.IsZero()) return ally;
    return caster + direction * std::min(std::max(0.0f, dashRange),
                                         caster.Distance2D(ally));
}

inline bool LandingSafe(const Vec3& endpoint, int enemiesAtLanding,
                        bool underEnemyTurret, bool wall,
                        int maximumEnemies = 2) {
    return FinitePoint(endpoint) && !wall && !underEnemyTurret &&
        enemiesAtLanding >= 0 && enemiesAtLanding <=
        std::max(0, maximumEnemies);
}

inline bool ReturnSafe(const Vec3& caster, const Vec3& ally,
                       int enemiesAtLanding, bool underEnemyTurret,
                       bool wall, int maximumEnemies = 2) {
    const Vec3 endpoint = AllyDashEndpoint(caster, ally);
    return LandingSafe(endpoint, enemiesAtLanding, underEnemyTurret, wall,
                       maximumEnemies);
}

inline int CharmCount(const Vec3& center, const std::vector<Vec3>& enemies,
                      float radius = kRRadius) {
    if (!FinitePoint(center)) return 0;
    int count = 0;
    for (const Vec3& enemy : enemies) {
        if (FinitePoint(enemy) && center.Distance2D(enemy) <=
            std::max(0.0f, radius)) ++count;
    }
    return count;
}

inline float CharmMovementSeconds(float distance, float moveSpeed,
                                  float duration = kRDurationSeconds) {
    if (!std::isfinite(distance) || !std::isfinite(moveSpeed) ||
        !std::isfinite(duration) || distance < 0.0f) return 0.0f;
    return std::min(std::max(0.0f, duration),
                    distance / std::max(1.0f, moveSpeed));
}

inline bool DefensiveQRequired(float allyHealthPercent,
                               float thresholdPercent,
                               bool enemyInLine) {
    return enemyInLine && std::isfinite(allyHealthPercent) &&
        allyHealthPercent <= std::clamp(thresholdPercent, 0.0f, 100.0f);
}

} // namespace Plugins::KuroAIO::AI::Controllers::Rakan::Geometry
