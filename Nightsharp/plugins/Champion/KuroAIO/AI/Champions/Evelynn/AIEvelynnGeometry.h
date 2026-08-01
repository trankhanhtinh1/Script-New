#pragma once

#include "../../AIGeometry.h"

#include <algorithm>
#include <cmath>

namespace Plugins::KuroAIO::AI::Controllers::Evelynn::Geometry {

using SharedGeometry::Direction2D;
using SharedGeometry::ProjectPointToSegment2D;
using Vec3 = ::Vec3;

inline constexpr float kQInitialRange = 800.0f;
inline constexpr float kQRecastRange = 550.0f;
inline constexpr float kQWidth = 60.0f;
inline constexpr float kQRecastWidth = 90.0f;
inline constexpr float kQMissileSpeed = 2400.0f;
inline constexpr float kWRange = 1600.0f;
inline constexpr float kWCharmArmSeconds = 2.5f;
inline constexpr float kWProjectileWidth = 100.0f;
inline constexpr float kWProjectileSpeed = 2400.0f;
inline constexpr float kERange = 210.0f;
inline constexpr float kEDashDistance = 450.0f;
inline constexpr float kRRange = 700.0f;
inline constexpr float kRRadius = 350.0f;
inline constexpr float kREscapeDistance = 700.0f;
inline constexpr float kRExecutePercent = 30.0f;
inline bool InRange(const Vec3& origin, const Vec3& target, float range,
                   float targetRadius = 0.0f) {
    return origin.IsValid() && target.IsValid() &&
           origin.Distance2D(target) <=
               std::max(0.0f, range) + std::max(0.0f, targetRadius);
}

inline Vec3 ClampToRange(const Vec3& origin, const Vec3& requested,
                        float range) {
    const Vec3 direction = Direction2D(origin, requested);
    if (direction.IsZero()) return origin;
    return origin + direction * std::min(std::max(0.0f, range),
                                         origin.Distance2D(requested));
}

inline bool LineHits(const Vec3& origin, const Vec3& aim, const Vec3& target,
                     float width, float targetRadius = 0.0f,
                     float range = kQInitialRange) {
    const Vec3 direction = Direction2D(origin, aim);
    if (direction.IsZero() || target.IsZero() || !std::isfinite(width)) return false;
    const Vec3 end = origin + direction * std::min(std::max(0.0f, range),
                                                    origin.Distance2D(aim));
    const auto projection = ProjectPointToSegment2D(target, origin, end);
    return projection.T >= 0.0f && projection.T <= 1.0f &&
           projection.Distance <= std::max(0.0f, width) * 0.5f +
               std::max(0.0f, targetRadius);
}

inline bool QInitialHits(const Vec3& origin, const Vec3& aim,
                         const Vec3& target, float targetRadius = 0.0f) {
    return LineHits(origin, aim, target, kQWidth, targetRadius, kQInitialRange);
}

inline bool QRecastHits(const Vec3& origin, const Vec3& target,
                        float targetRadius = 0.0f) {
    return InRange(origin, target, kQRecastRange, targetRadius);
}

inline bool WCharmReady(float markAgeSeconds,
                        float requiredSeconds = kWCharmArmSeconds) {
    return std::isfinite(markAgeSeconds) && std::isfinite(requiredSeconds) &&
           markAgeSeconds >= std::max(0.0f, requiredSeconds);
}

inline bool WTargetReachable(const Vec3& origin, const Vec3& target,
                             float range = kWRange, float targetRadius = 0.0f) {
    return InRange(origin, target, range, targetRadius);
}

inline bool EEntryReachable(const Vec3& origin, const Vec3& target,
                           float targetRadius = 0.0f) {
    return InRange(origin, target, kERange, targetRadius);
}

inline Vec3 EEntryPoint(const Vec3& origin, const Vec3& target) {
    return ClampToRange(origin, target, kERange);
}

inline bool RExecuteReady(float targetHealthPercent,
                          float threshold = kRExecutePercent) {
    return std::isfinite(targetHealthPercent) && std::isfinite(threshold) &&
           targetHealthPercent <= std::clamp(threshold, 0.0f, 100.0f);
}

inline bool RDamageZoneHits(const Vec3& origin, const Vec3& target,
                            float targetRadius = 0.0f) {
    return InRange(origin, target, kRRange, targetRadius) &&
           origin.Distance2D(target) <= kRRadius + std::max(0.0f, targetRadius);
}

inline Vec3 EscapePoint(const Vec3& origin, const Vec3& threat,
                        float distance = kREscapeDistance) {
    const Vec3 away = Direction2D(threat, origin);
    if (away.IsZero()) return origin;
    return origin + away * std::max(0.0f, distance);
}

inline bool SafeEscapeVector(const Vec3& origin, const Vec3& threat,
                             const Vec3& endpoint,
                             float minimumDistance = 350.0f) {
    if (!threat.IsValid() || threat.IsZero() || !endpoint.IsValid() ||
        endpoint.IsZero()) return false;
    const Vec3 away = Direction2D(threat, origin);
    const Vec3 move = Direction2D(origin, endpoint);
    return !away.IsZero() && !move.IsZero() &&
           away.Dot(move) >= 0.45f &&
           origin.Distance2D(endpoint) >= std::max(0.0f, minimumDistance);
}

} // namespace Plugins::KuroAIO::AI::Controllers::Evelynn::Geometry
