#pragma once

#include "../../AIGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace Plugins::KuroAIO::AI::Controllers::Morgana::Geometry {

using Vec3 = ::Vec3;
using SharedGeometry::Direction2D;

inline constexpr float kQRange = 1250.0f;
inline constexpr float kQWidth = 70.0f;
inline constexpr float kQDelay = 0.25f;
inline constexpr float kQSpeed = 1200.0f;
inline constexpr float kWRange = 900.0f;
inline constexpr float kWRadius = 325.0f;
inline constexpr float kWDuration = 5.0f;
inline constexpr float kERange = 800.0f;
inline constexpr float kRRadius = 625.0f;
inline constexpr float kRTetherDuration = 3.0f;
inline constexpr float kRStunDelay = 3.0f;
inline constexpr float kRSlowPercent = 20.0f;

inline float PointSegmentDistance(const Vec3& point, const Vec3& start,
                                  const Vec3& end) {
    const float dx = end.x - start.x;
    const float dz = end.z - start.z;
    const float lengthSquared = dx * dx + dz * dz;
    if (lengthSquared <= 0.001f) return point.Distance2D(start);
    const float t = std::clamp(
        ((point.x - start.x) * dx + (point.z - start.z) * dz) /
            lengthSquared, 0.0f, 1.0f);
    return point.Distance2D({start.x + dx * t, start.y,
                             start.z + dz * t});
}

inline bool QLineHits(const Vec3& start, const Vec3& end, const Vec3& target,
                      float targetRadius = 0.0f) {
    return PointSegmentDistance(target, start, end) <=
        std::max(0.0f, targetRadius) + kQWidth * 0.5f;
}

// Q stops on the first unit collision.  The caller supplies the first
// collision distance when the prediction service reports one; a negative
// distance means no collision was found.
inline bool QCollisionFree(float targetDistance, float firstCollisionDistance,
                           float targetRadius = 0.0f) {
    if (targetDistance < 0.0f || firstCollisionDistance < 0.0f) return true;
    return targetDistance + std::max(0.0f, targetRadius) <
        firstCollisionDistance;
}

inline bool ZoneContains(const Vec3& center, const Vec3& target,
                         float targetRadius = 0.0f) {
    return center.Distance2D(target) <= kWRadius +
        std::max(0.0f, targetRadius);
}

// Tormented Shadow scales from its base damage to double damage at 100%
// missing health.  Keep this pure so W placement and tests share the same
// missing-health gate without depending on runtime hero objects.
inline float WMissingHealthMultiplier(float healthPercent) {
    return 1.0f + std::clamp(100.0f - healthPercent, 0.0f, 100.0f) / 100.0f;
}

inline bool WZoneWorthwhile(float targetHealthPercent, bool targetRooted,
                            bool killable, int nearbyTargets,
                            int minimumTargets = 1) {
    if (nearbyTargets < std::max(1, minimumTargets)) return false;
    if (killable || targetRooted) return true;
    return targetHealthPercent <= 72.0f;
}

inline Vec3 ClampCastPoint(const Vec3& origin, const Vec3& requested,
                           float range) {
    const Vec3 direction = Direction2D(origin, requested);
    if (direction.IsZero()) return {};
    return origin + direction *
        std::min(std::max(0.0f, range), origin.Distance2D(requested));
}

inline bool AllyInShieldRange(const Vec3& origin, const Vec3& ally,
                              float extraRadius = 0.0f) {
    return origin.Distance2D(ally) <= kERange + std::max(0.0f, extraRadius);
}

enum class ThreatKind : unsigned char {
    None,
    Damage,
    HardCrowdControl,
    Immobilize,
    GapCloser,
};

struct ShieldThreatContext {
    ThreatKind Threat = ThreatKind::None;
    bool AllyValid = false;
    bool SpellReady = false;
    bool AllyAlreadyShielded = false;
    bool AllyLowHealth = false;
    bool Lethal = false;
    bool Manual = false;
    int NearbyEnemies = 0;
};

inline bool ShouldBlackShield(const ShieldThreatContext& context) {
    if (!context.AllyValid || !context.SpellReady ||
        context.AllyAlreadyShielded || context.Threat == ThreatKind::None)
        return false;
    if (context.Manual || context.Lethal ||
        context.Threat == ThreatKind::HardCrowdControl ||
        context.Threat == ThreatKind::Immobilize)
        return true;
    return context.AllyLowHealth && context.NearbyEnemies > 0;
}

inline bool RInsideTether(const Vec3& player, const Vec3& enemy,
                         float enemyRadius = 0.0f) {
    return player.Distance2D(enemy) <= kRRadius +
        std::max(0.0f, enemyRadius);
}

inline bool RStunWillLand(const Vec3& player, const Vec3& enemy,
                          float tetherRemainingSeconds,
                          bool targetDashingAway, bool targetUntargetable) {
    if (!RInsideTether(player, enemy) || targetUntargetable) return false;
    if (targetDashingAway && tetherRemainingSeconds < 0.45f) return false;
    return tetherRemainingSeconds >= 0.0f;
}

inline bool SafeUltimatePosition(const Vec3& position, bool wall,
                                 bool underEnemyTurret, int enemies,
                                 int allies, int maximumEnemies,
                                 bool defensive) {
    if (position.IsZero() || wall || underEnemyTurret) return false;
    if (defensive) return allies > 0 || enemies > 0;
    return enemies >= 1 && enemies <= std::max(1, maximumEnemies) &&
        allies + 1 >= enemies;
}

} // namespace Plugins::KuroAIO::AI::Controllers::Morgana::Geometry
