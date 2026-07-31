#pragma once

#include "../AIGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace Plugins::KuroAIO::AI::Controllers::Shen::Geometry {

using SharedGeometry::Direction2D;
using SharedGeometry::ProjectPointToSegment2D;

struct ShieldResult {
    float Amount = 0.0f;
    bool Valid = false;
};

inline ShieldResult PassiveShield(int level, float bonusHealth) {
    if (level < 1 || !std::isfinite(bonusHealth) || bonusHealth < 0.0f) return {};
    constexpr std::array<float, 19> base = {
        60.0f, 63.0f, 66.0f, 69.0f, 72.0f, 75.0f, 78.0f, 81.0f,
        84.0f, 87.0f, 90.0f, 93.0f, 96.0f, 99.0f, 102.0f, 105.0f,
        108.0f, 111.0f, 114.0f,
    };
    const float value = base[static_cast<std::size_t>(
        std::clamp(level - 1, 0, 17))] + bonusHealth * 0.14f;
    return { std::max(0.0f, value), true };
}

inline Vec3 SpiritBladePosition(const Vec3& caster,
                                const Vec3& target,
                                float pullDistance = 60.0f) {
    if (!caster.IsValid() || !target.IsValid()) return {};
    const Vec3 direction = Direction2D(caster, target);
    if (direction.IsZero()) return target;
    Vec3 blade = target + direction * std::max(0.0f, pullDistance);
    blade.y = caster.y;
    return blade;
}

inline Vec3 TrackSpiritBlade(const Vec3& previous,
                             const Vec3& target,
                             float maxStep = 180.0f) {
    if (!target.IsValid()) return previous;
    if (!previous.IsValid()) return target;
    const float distance = previous.Distance2D(target);
    if (distance <= std::max(0.0f, maxStep)) return target;
    const Vec3 direction = Direction2D(previous, target);
    return direction.IsZero() ? previous : previous + direction * maxStep;
}

inline bool WZoneContains(const Vec3& zoneCenter,
                          const Vec3& unitPosition,
                          float zoneRadius = 60.0f,
                          float unitRadius = 0.0f) {
    if (!zoneCenter.IsValid() || !unitPosition.IsValid() ||
        !std::isfinite(zoneRadius) || !std::isfinite(unitRadius)) return false;
    return zoneCenter.Distance2D(unitPosition) <=
           std::max(0.0f, zoneRadius) + std::max(0.0f, unitRadius);
}

inline Vec3 DashEndpoint(const Vec3& start,
                         const Vec3& requested,
                         float maximumDistance = 600.0f) {
    if (!start.IsValid() || !requested.IsValid()) return {};
    const Vec3 direction = Direction2D(start, requested);
    if (direction.IsZero()) return start;
    return start + direction * std::clamp(
        start.Distance2D(requested), 0.0f, std::max(0.0f, maximumDistance));
}

inline bool DashEndpointSafe(const Vec3& endpoint,
                             bool underTurret,
                             int enemiesAtEndpoint,
                             int maximumEnemies,
                             float healthPercent,
                             float minimumHealthPercent = 25.0f,
                             bool lethal = false) {
    if (!endpoint.IsValid() || enemiesAtEndpoint < 0 || maximumEnemies < 0 ||
        !std::isfinite(healthPercent)) return false;
    if (underTurret && !lethal) return false;
    if (enemiesAtEndpoint > maximumEnemies && !lethal) return false;
    return lethal || healthPercent >= minimumHealthPercent;
}

inline float DashPathHitScore(const Vec3& start,
                              const Vec3& endpoint,
                              const Vec3& target,
                              float targetRadius,
                              float dashRadius = 55.0f) {
    if (!start.IsValid() || !endpoint.IsValid() || !target.IsValid()) return 0.0f;
    const auto projection = ProjectPointToSegment2D(target, start, endpoint);
    const float radius = std::max(0.0f, targetRadius) + std::max(0.0f, dashRadius);
    if (projection.Distance > radius) return 0.0f;
    return std::clamp(1.0f - projection.Distance / std::max(1.0f, radius), 0.0f, 1.0f);
}

enum class TeleportDecision : int {
    Hold = 0,
    Channel = 1,
    Interrupt = 2,
};

inline TeleportDecision EvaluateTeleport(float allyHealthPercent,
                                          float incomingDamage,
                                          float channelRemainingSeconds,
                                          bool hardCrowdControl,
                                          bool enemyNearAlly,
                                          bool objectiveContest) {
    if (!std::isfinite(allyHealthPercent) || !std::isfinite(incomingDamage) ||
        !std::isfinite(channelRemainingSeconds)) return TeleportDecision::Hold;
    if (channelRemainingSeconds <= 0.0f) return TeleportDecision::Hold;
    if (hardCrowdControl || incomingDamage >= allyHealthPercent * 0.01f ||
        (enemyNearAlly && allyHealthPercent < 35.0f)) return TeleportDecision::Interrupt;
    if (objectiveContest || allyHealthPercent < 58.0f) return TeleportDecision::Channel;
    return TeleportDecision::Hold;
}

inline bool CanReachTarget(const Vec3& source,
                           const Vec3& target,
                           float range,
                           float targetRadius = 0.0f) {
    return source.IsValid() && target.IsValid() && std::isfinite(range) &&
           source.Distance2D(target) <= std::max(0.0f, range) +
               std::max(0.0f, targetRadius);
}

} // namespace Plugins::KuroAIO::AI::Controllers::Shen::Geometry
