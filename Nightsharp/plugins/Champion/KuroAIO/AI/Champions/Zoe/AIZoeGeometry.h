#pragma once

// Zoe mechanics are intentionally represented as pure X/Z calculations. The
// controller supplies live wall, prediction and unit telemetry; tests can use
// the same collision, reach and timing rules without an SDK process.
#include "../../AIGeometry.h"

#include <algorithm>
#include <cmath>
#include <cfloat>
#include <cstddef>

namespace Plugins::KuroAIO::AI::Controllers::Zoe::Geometry {

using SharedGeometry::Direction2D;
using SharedGeometry::ProjectPointToSegment2D;
using SharedGeometry::SegmentProjection;
using Vec3 = ::Vec3;

inline constexpr float kQBaseRange = 800.0f;
inline constexpr float kQMaximumRange = 1600.0f;
inline constexpr float kQWidth = 50.0f;
inline constexpr float kQDelaySeconds = 0.25f;
inline constexpr float kQSpeed = 1200.0f;
inline constexpr float kERange = 800.0f;
inline constexpr float kEWallRange = 1400.0f;
inline constexpr float kEWidth = 55.0f;
inline constexpr float kESpeed = 1700.0f;
inline constexpr float kRRange = 575.0f;
inline constexpr float kRReturnSeconds = 1.0f;

inline float ClampRange(float requested, bool extended) {
    const float maximum = extended ? kQMaximumRange : kQBaseRange;
    return std::clamp(std::max(0.0f, requested), 0.0f, maximum);
}

inline Vec3 ReachEndpoint(const Vec3& origin, const Vec3& requested,
                          float maximumRange) {
    if (!origin.IsValid() || !requested.IsValid() || origin.IsZero() ||
        requested.IsZero() || !std::isfinite(maximumRange) || maximumRange <= 0.0f) {
        return {};
    }
    const Vec3 direction = Direction2D(origin, requested);
    if (direction.IsZero()) return {};
    Vec3 result = origin + direction * std::min(maximumRange,
                                                 origin.Distance2D(requested));
    result.y = origin.y;
    return result;
}

inline Vec3 QRecastEndpoint(const Vec3& source, const Vec3& requested,
                            bool recast, float requestedRange) {
    return ReachEndpoint(source, requested,
                         ClampRange(requestedRange, recast));
}

inline float QTravelSeconds(float distance, bool recast = false) {
    const float range = ClampRange(distance, recast);
    return kQDelaySeconds + range / std::max(1.0f, kQSpeed);
}

inline float BubbleRange(bool throughWall) {
    return throughWall ? kEWallRange : kERange;
}

inline bool LineHits(const Vec3& origin, const Vec3& endpoint,
                     const Vec3& target, float targetRadius,
                     float halfWidth = kQWidth * 0.5f) {
    if (!origin.IsValid() || !endpoint.IsValid() || !target.IsValid() ||
        origin.IsZero() || endpoint.IsZero()) return false;
    const SegmentProjection projection =
        ProjectPointToSegment2D(target, origin, endpoint);
    const float radius = std::max(0.0f, halfWidth) +
                         std::clamp(targetRadius, 0.0f, 180.0f);
    return projection.Distance <= radius &&
           origin.Distance2D(target) <= origin.Distance2D(endpoint) +
                                         std::max(0.0f, targetRadius);
}

struct CollisionUnit {
    Vec3 Position{};
    float Radius = 0.0f;
    bool Blocks = true;
};

inline int FirstBubbleCollision(const Vec3& origin, const Vec3& endpoint,
                                const CollisionUnit* units, std::size_t count,
                                float halfWidth = kEWidth * 0.5f) {
    if (!units || count == 0 || !origin.IsValid() || !endpoint.IsValid()) return -1;
    int first = -1;
    float firstT = FLT_MAX;
    for (std::size_t i = 0; i < count; ++i) {
        if (!units[i].Blocks || !units[i].Position.IsValid()) continue;
        const SegmentProjection projection = ProjectPointToSegment2D(
            units[i].Position, origin, endpoint);
        const float radius = std::max(0.0f, halfWidth) +
                             std::clamp(units[i].Radius, 0.0f, 180.0f);
        if (projection.Distance <= radius && projection.T < firstT) {
            firstT = projection.T;
            first = static_cast<int>(i);
        }
    }
    return first;
}

inline bool BubblePathSafe(const Vec3& origin, const Vec3& endpoint,
                           const CollisionUnit* units, std::size_t count,
                           bool wallOnPath, bool throughWall,
                           float halfWidth = kEWidth * 0.5f) {
    if (!origin.IsValid() || !endpoint.IsValid() || origin.IsZero() ||
        endpoint.IsZero() || origin.Distance2D(endpoint) > BubbleRange(throughWall) + 1.0f ||
        (!throughWall && wallOnPath)) return false;
    return FirstBubbleCollision(origin, endpoint, units, count, halfWidth) < 0;
}

inline float BubbleImpactSeconds(const Vec3& origin, const Vec3& impact,
                                 float speed = kESpeed,
                                 float castDelay = 0.30f) {
    if (!origin.IsValid() || !impact.IsValid() || !std::isfinite(speed) ||
        speed <= 0.0f) return FLT_MAX;
    return std::max(0.0f, castDelay) + origin.Distance2D(impact) / speed;
}

inline float QReturnHitScore(const Vec3& outboundEnd, const Vec3& returnOrigin,
                             const Vec3& target, float targetRadius,
                             float missileWidth = kQWidth) {
    if (!outboundEnd.IsValid() || !returnOrigin.IsValid() ||
        !target.IsValid() || outboundEnd.IsZero() || returnOrigin.IsZero()) return 0.0f;
    const SegmentProjection projection =
        ProjectPointToSegment2D(target, outboundEnd, returnOrigin);
    const float radius = std::max(1.0f, missileWidth * 0.5f) +
                         std::clamp(targetRadius, 0.0f, 180.0f);
    if (projection.Distance > radius) return 0.0f;
    const float centered = 1.0f - projection.Distance / radius;
    const float interior = 0.70f + 0.30f *
        (1.0f - std::min(1.0f, std::fabs(projection.T - 0.5f) * 2.0f));
    return std::clamp(centered * interior, 0.0f, 1.0f);
}

inline float QReturnTravelSeconds(const Vec3& outboundEnd,
                                  const Vec3& returnOrigin,
                                  const Vec3& target,
                                  float returnSpeed = 1900.0f) {
    if (!outboundEnd.IsValid() || !returnOrigin.IsValid() || !target.IsValid() ||
        returnOrigin.IsZero() || outboundEnd.IsZero()) return FLT_MAX;
    const auto projection = ProjectPointToSegment2D(target, outboundEnd, returnOrigin);
    return outboundEnd.Distance2D(projection.Closest) /
           std::max(500.0f, returnSpeed);
}

inline Vec3 PredictedPosition(const Vec3& position, const Vec3& velocity,
                              float seconds, float maximumDisplacement = 700.0f) {
    if (!position.IsValid() || !velocity.IsValid() || !std::isfinite(seconds) ||
        seconds < 0.0f) return {};
    Vec3 displacement = velocity * seconds;
    const float length = displacement.Length2D();
    if (length > std::max(0.0f, maximumDisplacement) && length > 0.001f)
        displacement = displacement / length * std::max(0.0f, maximumDisplacement);
    Vec3 result = position + displacement;
    result.y = position.y;
    return result;
}

inline float LineInterceptSeconds(const Vec3& origin, const Vec3& target,
                                  float delaySeconds, float speed) {
    if (!origin.IsValid() || !target.IsValid() || !std::isfinite(delaySeconds) ||
        !std::isfinite(speed) || speed <= 0.0f) return FLT_MAX;
    return std::max(0.0f, delaySeconds) + origin.Distance2D(target) / speed;
}

inline Vec3 PortalEndpoint(const Vec3& origin, const Vec3& requested,
                           bool wall, float maximumRange = kRRange) {
    if (wall) return {};
    return ReachEndpoint(origin, requested, std::min(kRRange, maximumRange));
}

inline bool PortalReturnSafe(const Vec3& endpoint, const Vec3& returnOrigin,
                             bool endpointWall, bool returnWall,
                             int enemiesAtEndpoint, int maximumEnemies,
                             bool underTurret, bool lethal) {
    if (!endpoint.IsValid() || endpoint.IsZero() || !returnOrigin.IsValid() ||
        returnOrigin.IsZero() || endpointWall || returnWall || underTurret) return false;
    return lethal || enemiesAtEndpoint <= std::max(0, maximumEnemies);
}

inline int PortalReturnTick(int castTick, int returnMilliseconds = 1000) {
    return castTick + std::max(1, returnMilliseconds);
}

enum class KillSecurePosture { Hold, Q, Bubble, StolenSpell, PortalSetup };

inline KillSecurePosture ChooseKillSecurePosture(float targetHealth,
                                                   float qDamage,
                                                   float bubbleDamage,
                                                   float stolenDamage,
                                                   bool qReachable,
                                                   bool bubbleReachable,
                                                   bool stolenReachable,
                                                   bool aaWindingUp,
                                                   bool protectedTarget,
                                                   bool wallSafe) {
    if (protectedTarget || !wallSafe || targetHealth <= 0.0f) return KillSecurePosture::Hold;
    if (bubbleReachable && bubbleDamage >= targetHealth) return KillSecurePosture::Bubble;
    if (qReachable && qDamage >= targetHealth && (!aaWindingUp || qDamage > targetHealth * 1.15f))
        return KillSecurePosture::Q;
    if (stolenReachable && stolenDamage >= targetHealth) return KillSecurePosture::StolenSpell;
    if (qReachable || bubbleReachable || stolenReachable) return KillSecurePosture::PortalSetup;
    return KillSecurePosture::Hold;
}

inline bool PreserveAaWindup(KillSecurePosture posture, bool aaWindingUp,
                             bool lethal) {
    return aaWindingUp && !lethal && posture != KillSecurePosture::Bubble;
}

} // namespace Plugins::KuroAIO::AI::Controllers::Zoe::Geometry
