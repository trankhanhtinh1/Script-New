#pragma once

#include "../../AIGeometry.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace Plugins::KuroAIO::AI::Controllers::Yuumi::Geometry {

using SharedGeometry::Direction2D;
using SharedGeometry::ProjectPointToSegment2D;
using Vec3 = ::Vec3;

inline constexpr float kQRange = 1050.0f;
inline constexpr float kQWidth = 35.0f;
inline constexpr float kQInitialSpeed = 100.0f;
inline constexpr float kQAcceleratedSpeed = 850.0f;
inline constexpr float kQSteerSeconds = 1.35f;
inline constexpr float kWRange = 1100.0f;
inline constexpr float kERange = 1100.0f;
inline constexpr float kRRange = 1100.0f;
inline constexpr float kRWidth = 100.0f;
inline constexpr float kRWaveRadius = 55.0f;
inline constexpr int kRWaveCount = 5;

inline bool FinitePoint(const Vec3& point) { return point.IsValid(); }

inline Vec3 QEndpoint(const Vec3& origin, const Vec3& aim,
                      float range = kQRange) {
    if (!FinitePoint(origin) || !FinitePoint(aim)) return {};
    const Vec3 direction = Direction2D(origin, aim);
    if (direction.IsZero()) return {};
    return origin + direction * std::max(0.0f, range);
}

inline bool QHits(const Vec3& origin, const Vec3& endpoint, const Vec3& target,
                  float targetRadius = 0.0f, float width = kQWidth) {
    if (!FinitePoint(origin) || !FinitePoint(endpoint) || !FinitePoint(target))
        return false;
    const auto projection = ProjectPointToSegment2D(target, origin, endpoint);
    return projection.Distance <= std::max(0.0f, width) * 0.5f +
        std::clamp(targetRadius, 0.0f, 150.0f);
}

inline bool QSteerAllowed(bool attached, float elapsedSeconds,
                          const Vec3& cursor, const Vec3& origin) {
    return attached && FinitePoint(cursor) && FinitePoint(origin) &&
        std::isfinite(elapsedSeconds) && elapsedSeconds >= 0.0f &&
        elapsedSeconds <= kQSteerSeconds;
}

inline float QTravelSeconds(float distance, bool accelerated = false) {
    if (!std::isfinite(distance) || distance < 0.0f) return 0.0f;
    const float initial = std::min(distance, kQInitialSpeed * 0.2f);
    const float remainder = std::max(0.0f, distance - initial);
    return initial / kQInitialSpeed + remainder /
        (accelerated ? kQAcceleratedSpeed : kQInitialSpeed);
}

inline bool WAttachAllowed(const Vec3& ally, bool allyValid, bool underTurret,
                           int nearbyEnemies, int enemyLimit = 4) {
    return allyValid && FinitePoint(ally) && nearbyEnemies >= 0 &&
        nearbyEnemies <= std::max(0, enemyLimit) && !underTurret;
}

inline float AllyPriority(float healthPercent, float attackDamage,
                          float abilityPower, int nearbyEnemies,
                          bool selected = false, bool bestFriend = false) {
    if (!std::isfinite(healthPercent) || !std::isfinite(attackDamage) ||
        !std::isfinite(abilityPower)) return -1.0e9f;
    const float missing = 100.0f - std::clamp(healthPercent, 0.0f, 100.0f);
    const float carry = std::max(0.0f, attackDamage) * 0.60f +
        std::max(0.0f, abilityPower) * 0.45f;
    const float threat = static_cast<float>(std::max(0, nearbyEnemies)) * 230.0f;
    return missing * 1.8f + carry + threat + (selected ? 380.0f : 0.0f) +
        (bestFriend ? 520.0f : 0.0f);
}

inline bool EShieldWorthwhile(float healthPercent, int nearbyEnemies,
                              bool lethalThreat, bool attached,
                              float threshold = 88.0f) {
    if (!std::isfinite(healthPercent) || healthPercent < 0.0f) return false;
    return lethalThreat || nearbyEnemies > 0 || attached ||
        healthPercent <= std::clamp(threshold, 0.0f, 100.0f);
}

inline int RWaveEnemyCount(const Vec3& origin, const Vec3& direction,
                           const std::vector<Vec3>& enemies,
                           float range = kRRange, float width = kRWidth) {
    if (!FinitePoint(origin) || !FinitePoint(direction)) return 0;
    const Vec3 endpoint = QEndpoint(origin, direction, range);
    int count = 0;
    for (const Vec3& enemy : enemies)
        if (QHits(origin, endpoint, enemy, 0.0f, width)) ++count;
    return count;
}

inline bool RChannelSafe(const Vec3& origin, const Vec3& aim, int enemies,
                         bool underTurret, bool wall, int maximumEnemies = 4) {
    return FinitePoint(origin) && FinitePoint(aim) && !wall && !underTurret &&
        enemies >= 0 && enemies <= std::max(0, maximumEnemies);
}

inline bool RHealWorthwhile(float allyHealthPercent, int nearbyEnemies,
                            bool bestFriend, bool hardThreat,
                            float threshold = 78.0f) {
    if (!std::isfinite(allyHealthPercent)) return hardThreat;
    return hardThreat || nearbyEnemies > 0 || bestFriend ||
        allyHealthPercent <= std::clamp(threshold, 0.0f, 100.0f);
}

} // namespace Plugins::KuroAIO::AI::Controllers::Yuumi::Geometry
