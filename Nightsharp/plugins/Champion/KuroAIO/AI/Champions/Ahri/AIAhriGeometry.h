#pragma once

// Pure, live-memory-free geometry shared by Ahri's controller and standalone
// tests. The return Orb follows the segment from its current position to
// Ahri's post-dash position, so a correct R is a segment-interception problem.

#include "../../AIGeometry.h"

#include <algorithm>
#include <cmath>

namespace Plugins::KuroAIO::AI::Controllers::Ahri::Geometry {

using SharedGeometry::Direction2D;
using SharedGeometry::ProjectPointToSegment2D;
using SharedGeometry::SegmentProjection;

inline float ReturnHitScore(const Vec3& orbPosition,
                            const Vec3& ahriDestination,
                            const Vec3& targetPosition,
                            float targetRadius,
                            float orbRadius = 100.0f) {
    if (!orbPosition.IsValid() || !ahriDestination.IsValid() ||
        !targetPosition.IsValid()) {
        return 0.0f;
    }
    const SegmentProjection projection = ProjectPointToSegment2D(
        targetPosition, orbPosition, ahriDestination);
    const float hitRadius = std::max(20.0f, orbRadius) +
                            std::clamp(targetRadius, 25.0f, 100.0f);
    if (projection.Distance > hitRadius) {
        return 0.0f;
    }
    const float centered = 1.0f - projection.Distance / hitRadius;
    // Prefer an actual path crossing. A target sitting at an endpoint can
    // still be hit, but the timing is less robust under movement/ping.
    const float interior = 0.65f +
        0.35f * (1.0f - std::min(1.0f, std::fabs(projection.T - 0.5f) * 2.0f));
    return std::clamp(centered * interior, 0.0f, 1.0f);
}

inline float ReturnTravelSecondsToTarget(const Vec3& orbPosition,
                                         const Vec3& ahriDestination,
                                         const Vec3& targetPosition,
                                         float averageReturnSpeed = 2050.0f) {
    const SegmentProjection projection = ProjectPointToSegment2D(
        targetPosition, orbPosition, ahriDestination);
    const float pathDistance = orbPosition.Distance2D(projection.Closest);
    return pathDistance / std::max(500.0f, averageReturnSpeed);
}

inline float TipDoubleHitScore(const Vec3& source,
                               const Vec3& castEnd,
                               const Vec3& targetPosition,
                               float targetRadius,
                               float maximumRange = 970.0f,
                               float orbRadius = 100.0f) {
    const Vec3 direction = Direction2D(source, castEnd);
    if (direction.IsZero()) {
        return 0.0f;
    }
    Vec3 relative = targetPosition - source;
    relative.y = 0.0f;
    const float forward = relative.Dot(direction);
    const float lateral = std::fabs(direction.x * relative.z -
                                    direction.z * relative.x);
    const float hitRadius = orbRadius + std::clamp(targetRadius, 25.0f, 100.0f);
    if (forward < maximumRange - 190.0f ||
        forward > maximumRange + targetRadius || lateral > hitRadius) {
        return 0.0f;
    }
    const float radial = 1.0f - std::min(
        1.0f, std::fabs(forward - (maximumRange - 55.0f)) / 150.0f);
    const float centered = 1.0f - std::min(1.0f, lateral / hitRadius);
    return std::clamp(radial * 0.72f + centered * 0.28f, 0.0f, 1.0f);
}

} // namespace Plugins::KuroAIO::AI::Controllers::Ahri::Geometry
