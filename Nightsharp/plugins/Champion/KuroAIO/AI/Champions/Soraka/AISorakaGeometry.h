#pragma once

#include "../../AIGeometry.h"

#include <algorithm>
#include <cmath>

namespace Plugins::KuroAIO::AI::Controllers::Soraka::Geometry {

using SharedGeometry::Direction2D;
using SharedGeometry::ProjectPointToSegment2D;

inline constexpr float kQRange = 800.0f;
inline constexpr float kQRadius = 230.0f;
inline constexpr float kQSpeed = 1750.0f;
inline constexpr float kQDelay = 0.25f;
inline constexpr float kQRejuvenationSeconds = 2.5f;
inline constexpr float kQReturnRadius = 115.0f;
inline constexpr float kWRange = 550.0f;
inline constexpr float kWHealthCostPercent = 0.10f;
inline constexpr float kWMinimumHealthPercent = 0.05f;
inline constexpr float kERange = 875.0f;
inline constexpr float kERadius = 260.0f;
inline constexpr float kERootDelaySeconds = 1.5f;
inline constexpr float kRLowHealthAmpThreshold = 40.0f;

inline bool FinitePoint(const Vec3& point) {
    return point.IsValid() && std::isfinite(point.x) &&
        std::isfinite(point.y) && std::isfinite(point.z);
}

inline bool CircleHits(const Vec3& center, const Vec3& target,
                       float radius, float targetRadius = 0.0f) {
    return FinitePoint(center) && FinitePoint(target) &&
        center.Distance2D(target) <= std::max(0.0f, radius) +
            std::clamp(targetRadius, 0.0f, 150.0f);
}

inline Vec3 QImpactPoint(const Vec3& origin, const Vec3& requested,
                         float range = kQRange) {
    if (!FinitePoint(origin) || !FinitePoint(requested)) return {};
    const Vec3 direction = Direction2D(origin, requested);
    if (direction.IsZero()) return {};
    return origin + direction * std::min(std::max(0.0f, range),
                                         origin.Distance2D(requested));
}

inline bool QHits(const Vec3& origin, const Vec3& requested,
                  const Vec3& target, float targetRadius = 0.0f,
                  float radius = kQRadius) {
    const Vec3 impact = QImpactPoint(origin, requested);
    return CircleHits(impact, target, radius, targetRadius);
}

inline float QTravelSeconds(float distance, float delay = kQDelay,
                            float speed = kQSpeed) {
    if (!std::isfinite(distance) || !std::isfinite(delay) ||
        !std::isfinite(speed) || distance < 0.0f) return 0.0f;
    return std::max(0.0f, delay) + distance / std::max(1.0f, speed);
}

inline bool QRejuvenationOpen(float elapsedSeconds,
                              float window = kQRejuvenationSeconds) {
    return std::isfinite(elapsedSeconds) && elapsedSeconds >= 0.0f &&
        elapsedSeconds <= std::max(0.0f, window);
}

inline bool QReturnHits(const Vec3& caster, const Vec3& impact,
                        const Vec3& target, float targetRadius = 0.0f,
                        float returnRadius = kQReturnRadius) {
    if (!FinitePoint(caster) || !FinitePoint(impact) || !FinitePoint(target))
        return false;
    return ProjectPointToSegment2D(target, impact, caster).Distance <=
        std::max(0.0f, returnRadius) + std::clamp(targetRadius, 0.0f, 100.0f);
}

inline bool CanPayWHealthCost(float currentHealth, float maximumHealth,
                              float costPercent = kWHealthCostPercent,
                              float minimumPercent = kWMinimumHealthPercent,
                              bool emergency = false) {
    if (!std::isfinite(currentHealth) || !std::isfinite(maximumHealth) ||
        maximumHealth <= 0.0f || currentHealth <= 0.0f) return false;
    const float cost = maximumHealth * std::clamp(costPercent, 0.0f, 1.0f);
    const float floor = maximumHealth * std::clamp(minimumPercent, 0.0f, 1.0f);
    return emergency ? currentHealth > cost : currentHealth - cost >= floor;
}

inline bool EZoneHits(const Vec3& center, const Vec3& target,
                      float targetRadius = 0.0f, float radius = kERadius) {
    return CircleHits(center, target, radius, targetRadius);
}

inline bool ERootArmed(float elapsedSeconds,
                       float delay = kERootDelaySeconds) {
    return std::isfinite(elapsedSeconds) && elapsedSeconds >=
        std::max(0.0f, delay);
}

inline bool SafeSupportZone(const Vec3& center, int enemiesAtZone,
                            bool underTurret, bool wall, int maxEnemies = 2) {
    return FinitePoint(center) && !underTurret && !wall && enemiesAtZone >= 0 &&
        enemiesAtZone <= std::max(0, maxEnemies);
}

inline bool ShouldWish(float allyHealthPercent, bool predictedLethal,
                       bool enemyPresent, float threshold = 34.0f) {
    if (!std::isfinite(allyHealthPercent)) return false;
    return predictedLethal || (enemyPresent && allyHealthPercent <=
        std::clamp(threshold, 0.0f, 100.0f));
}

inline bool WishAmplified(float allyHealthPercent,
                         float threshold = kRLowHealthAmpThreshold) {
    return std::isfinite(allyHealthPercent) && allyHealthPercent <=
        std::clamp(threshold, 0.0f, 100.0f);
}

inline float HealthCost(float maximumHealth,
                        float costPercent = kWHealthCostPercent) {
    if (!std::isfinite(maximumHealth) || maximumHealth <= 0.0f) return 0.0f;
    return maximumHealth * std::clamp(costPercent, 0.0f, 1.0f);
}

} // namespace Plugins::KuroAIO::AI::Controllers::Soraka::Geometry
