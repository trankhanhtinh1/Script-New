#pragma once

#include "../AIGeometry.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace Plugins::KuroAIO::AI::Controllers::Lulu::Geometry {

using SharedGeometry::Direction2D;
using SharedGeometry::ProjectPointToSegment2D;
using Vec3 = ::Vec3;

inline constexpr float kQRange = 950.0f;
inline constexpr float kQWidth = 60.0f;
inline constexpr float kQSpeed = 1450.0f;
inline constexpr float kWRange = 650.0f;
inline constexpr float kERange = 650.0f;
inline constexpr float kRRange = 900.0f;
inline constexpr float kRKnockupRadius = 350.0f;

inline bool FinitePoint(const Vec3& point) {
    return point.IsValid();
}

inline Vec3 BoltEndpoint(const Vec3& origin, const Vec3& target,
                         float range = kQRange) {
    if (!FinitePoint(origin) || !FinitePoint(target)) return {};
    const Vec3 direction = Direction2D(origin, target);
    if (direction.IsZero()) return {};
    return origin + direction * std::max(0.0f, range);
}

inline bool BoltHits(const Vec3& origin, const Vec3& endpoint,
                     const Vec3& target, float targetRadius = 0.0f,
                     float width = kQWidth) {
    if (!FinitePoint(origin) || !FinitePoint(endpoint) || !FinitePoint(target)) return false;
    const auto projection = ProjectPointToSegment2D(target, origin, endpoint);
    return projection.Distance <= std::max(0.0f, width) * 0.5f +
        std::clamp(targetRadius, 0.0f, 150.0f);
}

inline bool DualBoltHits(const Vec3& playerOrigin, const Vec3& pixOrigin,
                         const Vec3& target, float targetRadius = 0.0f,
                         float width = kQWidth) {
    const Vec3 playerEnd = BoltEndpoint(playerOrigin, target);
    const Vec3 pixEnd = BoltEndpoint(pixOrigin, target);
    return BoltHits(playerOrigin, playerEnd, target, targetRadius, width) ||
        BoltHits(pixOrigin, pixEnd, target, targetRadius, width);
}

inline float QTravelSeconds(float distance, float speed = kQSpeed) {
    if (!std::isfinite(distance) || !std::isfinite(speed) || distance < 0.0f) return 0.0f;
    return distance / std::max(1.0f, speed);
}

inline bool WInRange(const Vec3& origin, const Vec3& target,
                     float range = kWRange, float targetRadius = 0.0f) {
    return FinitePoint(origin) && FinitePoint(target) &&
        origin.Distance2D(target) <= std::max(0.0f, range) +
            std::clamp(targetRadius, 0.0f, 150.0f);
}

enum class WPosture {
    Polymorph,
    Speed,
};

inline WPosture ChooseWPosture(bool enemyThreat, bool allyThreat,
                               bool fleeing, bool hardCrowdControl) {
    if (enemyThreat || hardCrowdControl) return WPosture::Polymorph;
    if (fleeing || allyThreat) return WPosture::Speed;
    return WPosture::Polymorph;
}

inline float AllyPriority(float healthPercent, float totalAttackDamage,
                          float abilityPower, int nearbyEnemies,
                          bool selected = false) {
    if (!std::isfinite(healthPercent) || !std::isfinite(totalAttackDamage) ||
        !std::isfinite(abilityPower)) return -1.0e9f;
    const float missing = 100.0f - std::clamp(healthPercent, 0.0f, 100.0f);
    const float carry = std::max(0.0f, totalAttackDamage) * 0.78f +
        std::max(0.0f, abilityPower) * 0.70f;
    const float threat = static_cast<float>(std::max(0, nearbyEnemies)) * 250.0f;
    return missing * 1.7f + carry + threat + (selected ? 500.0f : 0.0f);
}

inline bool EShieldWorthwhile(float allyHealthPercent, int nearbyEnemies,
                              bool lethalThreat, bool offensiveDamage,
                              float threshold = 92.0f) {
    if (!std::isfinite(allyHealthPercent) || allyHealthPercent < 0.0f) return false;
    return lethalThreat || nearbyEnemies > 0 || offensiveDamage ||
        allyHealthPercent <= std::clamp(threshold, 0.0f, 100.0f);
}

inline bool EDamageWorthwhile(float enemyHealthPercent, bool marked,
                              int nearbyAllies) {
    if (!std::isfinite(enemyHealthPercent)) return marked;
    return marked || enemyHealthPercent <= 78.0f || nearbyAllies >= 2;
}

inline Vec3 PixTransferPosition(const Vec3& player, const Vec3& ally,
                                bool toAlly) {
    if (!FinitePoint(player) || !FinitePoint(ally)) return {};
    return toAlly ? ally : player;
}

inline bool InWildGrowthZone(const Vec3& center, const Vec3& point,
                             float radius = kRKnockupRadius) {
    return FinitePoint(center) && FinitePoint(point) &&
        center.Distance2D(point) <= std::max(0.0f, radius);
}

inline int WildGrowthEnemyCount(const Vec3& center,
                                const std::vector<Vec3>& enemies,
                                float radius = kRKnockupRadius) {
    if (!FinitePoint(center)) return 0;
    int count = 0;
    for (const Vec3& enemy : enemies)
        if (InWildGrowthZone(center, enemy, radius)) ++count;
    return count;
}

inline bool WildGrowthSafe(const Vec3& center, int enemiesNearby,
                           int alliesNearby, bool underTurret, bool wall,
                           int maximumEnemies = 3) {
    return FinitePoint(center) && !wall && enemiesNearby >= 0 &&
        enemiesNearby <= std::max(0, maximumEnemies) &&
        (!underTurret || alliesNearby >= 2);
}

inline bool WildGrowthWorthwhile(float playerHealthPercent,
                                float allyHealthPercent, int enemiesNearby,
                                bool hardThreat, float playerThreshold = 44.0f,
                                float allyThreshold = 62.0f) {
    const bool playerNeeds = std::isfinite(playerHealthPercent) &&
        playerHealthPercent <= std::clamp(playerThreshold, 0.0f, 100.0f);
    const bool allyNeeds = std::isfinite(allyHealthPercent) &&
        allyHealthPercent <= std::clamp(allyThreshold, 0.0f, 100.0f);
    return hardThreat || (enemiesNearby > 0 && (playerNeeds || allyNeeds));
}

} // namespace Plugins::KuroAIO::AI::Controllers::Lulu::Geometry
