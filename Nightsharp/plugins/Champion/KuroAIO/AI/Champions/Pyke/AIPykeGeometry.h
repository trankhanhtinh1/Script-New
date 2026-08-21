#pragma once

#include "../../AIGeometry.h"

#include <algorithm>
#include <cmath>

namespace Plugins::KuroAIO::AI::Controllers::Pyke::Geometry {

using SharedGeometry::Direction2D;
using SharedGeometry::ProjectPointToSegment2D;
using Vec3 = ::Vec3;

inline constexpr float kQTapRange = 400.0f;
inline constexpr float kQMaxRange = 1100.0f;
inline constexpr float kQWidth = 70.0f;
inline constexpr float kQChargeSeconds = 1.0f;
inline constexpr float kQMaximumHoldSeconds = 1.15f;
inline constexpr float kEDashRange = 550.0f;
inline constexpr float kEReturnDelay = 0.95f;
inline constexpr float kEStunRadius = 110.0f;
inline constexpr float kWDetectionRadius = 600.0f;
inline constexpr float kWStealthRadius = 600.0f;
inline constexpr float kWChaseRange = 2000.0f;
inline constexpr float kRRange = 750.0f;
inline constexpr float kRHalfWidth = 180.0f;

inline float QRangeFromCharge(float chargeSeconds) {
    const float progress = std::clamp(
        chargeSeconds / kQChargeSeconds, 0.0f, 1.0f);
    return kQTapRange + (kQMaxRange - kQTapRange) * progress;
}
inline bool QIsCharged(float chargeSeconds) {
    return chargeSeconds >= kQChargeSeconds;
}
inline bool QMustRelease(float chargeSeconds) {
    return chargeSeconds >= kQMaximumHoldSeconds;
}
struct QReleaseContext {
    bool Charging = false;
    bool TargetValid = false;
    bool PredictionHits = false;
    bool CollisionFree = false;
    bool InCurrentRange = false;
    bool FullPullOnly = false;
    float ChargeSeconds = 0.0f;
};
inline bool ShouldReleaseQ(const QReleaseContext& context) {
    if (!context.Charging) return false;
    if (QMustRelease(context.ChargeSeconds)) return true;
    return context.TargetValid && context.PredictionHits &&
           context.CollisionFree && context.InCurrentRange &&
           (!context.FullPullOnly || QIsCharged(context.ChargeSeconds));
}
inline bool QLineHits(const Vec3& origin, const Vec3& aim, const Vec3& target,
                      float targetRadius = 0.0f, float range = kQMaxRange) {
    const Vec3 direction = Direction2D(origin, aim);
    if (direction.IsZero() || target.IsZero()) return false;
    const Vec3 end = origin + direction * std::min(std::max(0.0f, range),
                                                    origin.Distance2D(aim));
    const auto projection = ProjectPointToSegment2D(target, origin, end);
    return projection.T >= 0.0f && projection.T <= 1.0f &&
           projection.Distance <= kQWidth + std::max(0.0f, targetRadius);
}
inline Vec3 ClampDashEndpoint(const Vec3& origin, const Vec3& requested) {
    const Vec3 direction = Direction2D(origin, requested);
    if (direction.IsZero()) return origin;
    return origin + direction * std::min(kEDashRange, origin.Distance2D(requested));
}
inline bool ETrailStuns(const Vec3& origin, const Vec3& endpoint,
                        const Vec3& target, float targetRadius = 0.0f) {
    if (endpoint.IsZero() || target.IsZero()) return false;
    const auto projection = ProjectPointToSegment2D(target, origin, endpoint);
    return projection.T >= 0.0f && projection.T <= 1.0f &&
           projection.Distance <= kEStunRadius + std::max(0.0f, targetRadius);
}
inline bool EFollowupReachable(const Vec3& endpoint,
                               const Vec3& predictedTarget,
                               float followupRange = kQMaxRange) {
    return endpoint.IsValid() && !endpoint.IsZero() &&
           predictedTarget.IsValid() && !predictedTarget.IsZero() &&
           endpoint.Distance2D(predictedTarget) <= std::max(0.0f, followupRange);
}
inline bool WStealthTargetAllowed(float distanceToEnemy, bool attackedRecently,
                                  bool targetValid = true) {
    if (!targetValid || attackedRecently) return false;
    return distanceToEnemy >= kWStealthRadius;
}
inline float GreyHealthRecovered(float greyHealth, float missingHealth,
                                 float recoveryPercent = 0.80f) {
    return std::clamp(std::min(std::max(0.0f, greyHealth),
                               std::max(0.0f, missingHealth)) *
                      std::clamp(recoveryPercent, 0.0f, 1.0f),
                      0.0f, std::max(0.0f, missingHealth));
}
inline bool ShouldRecoverGreyHealth(float greyHealth, float missingHealth,
                                    float dangerDistance, bool enemyNearby) {
    return greyHealth > 0.0f && missingHealth > 0.0f &&
           (!enemyNearby || dangerDistance >= kWDetectionRadius);
}
inline bool TurretEndpointSafe(bool endpointUnderEnemyTurret,
                               bool originUnderEnemyTurret,
                               bool defensive, int enemiesAtEndpoint,
                               int maximumEnemies = 2) {
    if (!defensive && endpointUnderEnemyTurret && !originUnderEnemyTurret) return false;
    return enemiesAtEndpoint <= std::max(0, maximumEnemies);
}

struct ExecuteContext {
    bool Ready = false;
    bool TargetValid = false;
    bool Lethal = false;
    bool Marked = false;
    bool AllyNearby = false;
    bool ShareEnabled = true;
    bool Protected = false;
};
inline bool ShouldExecuteR(const ExecuteContext& context) {
    if (!context.Ready || !context.TargetValid || !context.Lethal || context.Protected)
        return false;
    if (context.Marked) return true;
    return context.ShareEnabled || !context.AllyNearby;
}
inline bool ShouldShareExecute(const ExecuteContext& context) {
    return context.Ready && context.TargetValid && context.Lethal &&
           !context.Protected && context.AllyNearby && context.ShareEnabled;
}

} // namespace Plugins::KuroAIO::AI::Controllers::Pyke::Geometry
