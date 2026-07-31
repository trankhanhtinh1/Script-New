#pragma once

#include "../AIGeometry.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace Plugins::KuroAIO::AI::Controllers::Janna::Geometry {

using SharedGeometry::Direction2D;
using SharedGeometry::ProjectPointToSegment2D;
using Vec3 = ::Vec3;

inline constexpr float kQMinRange = 175.0f;
inline constexpr float kQMaxRange = 1075.0f;
inline constexpr float kQWidth = 120.0f;
inline constexpr float kQSpeed = 900.0f;
inline constexpr float kQMaxChargeSeconds = 3.0f;
inline constexpr float kWRange = 650.0f;
inline constexpr float kWSlowDurationSeconds = 3.0f;
inline constexpr float kERange = 800.0f;
inline constexpr float kRRange = 725.0f;
inline constexpr float kRDurationSeconds = 4.0f;

inline bool FinitePoint(const Vec3& point) {
    return point.IsValid();
}

inline float ChargeFraction(float elapsedSeconds,
                            float maxCharge = kQMaxChargeSeconds) {
    if (!std::isfinite(elapsedSeconds) || !std::isfinite(maxCharge) ||
        maxCharge <= 0.0f) return 0.0f;
    return std::clamp(elapsedSeconds / maxCharge, 0.0f, 1.0f);
}

inline float ChargeRange(float elapsedSeconds,
                         float minRange = kQMinRange,
                         float maxRange = kQMaxRange) {
    const float low = std::max(0.0f, minRange);
    const float high = std::max(low, maxRange);
    return low + (high - low) * ChargeFraction(elapsedSeconds);
}

inline Vec3 QEndpoint(const Vec3& origin, const Vec3& direction,
                      float elapsedSeconds,
                      float minRange = kQMinRange,
                      float maxRange = kQMaxRange) {
    if (!FinitePoint(origin) || !FinitePoint(direction)) return {};
    const Vec3 unit = Direction2D(origin, direction);
    if (unit.IsZero()) return {};
    return origin + unit * ChargeRange(elapsedSeconds, minRange, maxRange);
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

inline float QTravelSeconds(float distance, float chargeSeconds = 0.0f,
                            float speed = kQSpeed) {
    if (!std::isfinite(distance) || !std::isfinite(chargeSeconds) ||
        !std::isfinite(speed) || distance < 0.0f) return 0.0f;
    return std::max(0.0f, chargeSeconds) +
        distance / std::max(1.0f, speed);
}

inline bool QShouldRelease(float elapsedSeconds, float targetDistance,
                          bool targetThreatened = false) {
    if (!std::isfinite(targetDistance)) return false;
    const float range = ChargeRange(elapsedSeconds);
    return targetThreatened || targetDistance <= range ||
        elapsedSeconds >= kQMaxChargeSeconds;
}

inline bool WInRange(const Vec3& origin, const Vec3& target,
                     float range = kWRange, float targetRadius = 0.0f) {
    return FinitePoint(origin) && FinitePoint(target) &&
        origin.Distance2D(target) <= std::max(0.0f, range) +
            std::clamp(targetRadius, 0.0f, 150.0f);
}

inline bool WSlowWorthwhile(float targetHealthPercent,
                            bool targetThreatened,
                            float lowHealthThreshold = 65.0f) {
    return targetThreatened ||
        (std::isfinite(targetHealthPercent) &&
         targetHealthPercent <= std::clamp(lowHealthThreshold, 0.0f, 100.0f));
}

inline float AllyPriority(float healthPercent, float totalAttackDamage,
                          float abilityPower, int nearbyEnemies,
                          bool selected = false) {
    if (!std::isfinite(healthPercent) || !std::isfinite(totalAttackDamage) ||
        !std::isfinite(abilityPower)) return -1.0e9f;
    const float missingHealth = 100.0f - std::clamp(healthPercent, 0.0f, 100.0f);
    const float carry = std::max(0.0f, totalAttackDamage) * 0.85f +
        std::max(0.0f, abilityPower) * 0.62f;
    const float threat = static_cast<float>(std::max(0, nearbyEnemies)) * 240.0f;
    return missingHealth * 1.55f + carry + threat + (selected ? 520.0f : 0.0f);
}

inline bool ShieldWorthwhile(float allyHealthPercent, int nearbyEnemies,
                             bool offensiveBuff, float threshold = 92.0f) {
    if (!std::isfinite(allyHealthPercent) || allyHealthPercent < 0.0f) return false;
    return allyHealthPercent <= std::clamp(threshold, 0.0f, 100.0f) ||
        nearbyEnemies > 0 || offensiveBuff;
}

inline bool InMonsoonZone(const Vec3& center, const Vec3& point,
                          float radius = kRRange) {
    return FinitePoint(center) && FinitePoint(point) &&
        center.Distance2D(point) <= std::max(0.0f, radius);
}

inline Vec3 PeelDirection(const Vec3& caster, const Vec3& cursor) {
    if (!FinitePoint(caster) || !FinitePoint(cursor)) return {};
    return Direction2D(caster, cursor);
}

inline Vec3 KnockbackPoint(const Vec3& caster, const Vec3& enemy,
                           float distance = 375.0f) {
    if (!FinitePoint(caster) || !FinitePoint(enemy)) return {};
    const Vec3 away = Direction2D(caster, enemy);
    if (away.IsZero()) return enemy;
    return enemy + away * std::max(0.0f, distance);
}

inline int MonsoonEnemyCount(const Vec3& center,
                             const std::vector<Vec3>& enemies,
                             float radius = kRRange) {
    if (!FinitePoint(center)) return 0;
    int count = 0;
    for (const Vec3& enemy : enemies) {
        if (InMonsoonZone(center, enemy, radius)) ++count;
    }
    return count;
}

inline bool MonsoonSafe(const Vec3& center, int enemiesInZone,
                        int alliesInZone, bool underTurret, bool wall,
                        int maximumEnemies = 3) {
    return FinitePoint(center) && !wall &&
        (!underTurret || alliesInZone >= 2) && enemiesInZone >= 0 &&
        enemiesInZone <= std::max(0, maximumEnemies);
}

inline bool MonsoonWorthwhile(float playerHealthPercent,
                              float allyHealthPercent,
                              int enemiesInZone, bool hardThreat,
                              float playerThreshold = 38.0f,
                              float allyThreshold = 55.0f) {
    const bool playerNeeds = std::isfinite(playerHealthPercent) &&
        playerHealthPercent <= std::clamp(playerThreshold, 0.0f, 100.0f);
    const bool allyNeeds = std::isfinite(allyHealthPercent) &&
        allyHealthPercent <= std::clamp(allyThreshold, 0.0f, 100.0f);
    return hardThreat || (enemiesInZone > 0 && (playerNeeds || allyNeeds));
}

inline bool CursorPeelSafe(const Vec3& caster, const Vec3& cursor,
                           const Vec3& threat, float minimumDot = 0.0f) {
    const Vec3 desired = PeelDirection(caster, cursor);
    const Vec3 threatDirection = PeelDirection(caster, threat);
    return desired.IsZero() || threatDirection.IsZero() ||
        desired.Dot(threatDirection) >= minimumDot;
}

} // namespace Plugins::KuroAIO::AI::Controllers::Janna::Geometry
