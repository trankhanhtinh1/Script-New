#pragma once

// Pure geometry and deterministic damage helpers for Akshan. Live terrain
// lookup, target selection and cast ownership stay in AIAkshanController;
// this file models the kit pieces that are easy to get subtly wrong:
// Avengerang's per-hit extension and return line, Heroic Swing's orbit and
// cursor-selected direction, and Comeuppance's blockers/ammo ramp.

#include "../../AIGeometry.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstddef>
#include <vector>

namespace Plugins::KuroAIO::AI::Controllers::Akshan::Geometry {

using SharedGeometry::Cross2D;
using SharedGeometry::Direction2D;
using SharedGeometry::ProjectPointToSegment2D;
using SharedGeometry::Rotate2D;
using SharedGeometry::SegmentProjection;
using SharedGeometry::kPi;

struct QPathUnit {
    Vec3 Position = {};
    float Radius = 35.0f;
    int NetworkId = 0;
    bool ExtendsRange = true;
};

struct QUnitIntersection {
    bool Hits = false;
    float Forward = 0.0f;
    float Lateral = FLT_MAX;
};

inline QUnitIntersection QLineIntersection(const Vec3& source,
                                           const Vec3& direction,
                                           const QPathUnit& unit,
                                           float missileWidth = 70.0f) {
    const Vec3 normalized = Direction2D({}, direction);
    if (normalized.IsZero() || !source.IsValid() ||
        !unit.Position.IsValid()) {
        return {};
    }
    Vec3 relative = unit.Position - source;
    relative.y = 0.0f;
    const float forward = relative.Dot(normalized);
    const float lateral = std::fabs(Cross2D(normalized, relative));
    const float hitRadius = std::max(1.0f, missileWidth * 0.5f) +
                            std::clamp(unit.Radius, 0.0f, 150.0f);
    return {
        forward >= -unit.Radius && lateral <= hitRadius,
        forward,
        lateral,
    };
}

struct QOutboundResult {
    float Reach = 0.0f;
    Vec3 End = {};
    int ExtensionHits = 0;
    int TotalHits = 0;
    bool TargetHit = false;
    float TargetForward = FLT_MAX;
};

// The outbound missile starts at its script range and gains ExtensionDistance
// once per unit struck. Sorting intersections before growing the reach makes
// chained minion lines deterministic and prevents a far target from granting
// its own range before the boomerang can physically reach it.
inline QOutboundResult SimulateQOutbound(
    const Vec3& source,
    const Vec3& castDirection,
    const std::vector<QPathUnit>& units,
    int targetNetworkId,
    float baseRange = 750.0f,
    float extensionPerHit = 500.0f,
    float missileWidth = 70.0f,
    float maximumModeledRange = 5000.0f) {
    QOutboundResult result{};
    const Vec3 direction = Direction2D({}, castDirection);
    if (direction.IsZero() || !source.IsValid()) return result;

    struct Candidate {
        QPathUnit Unit = {};
        QUnitIntersection Intersection = {};
    };
    std::vector<Candidate> candidates;
    candidates.reserve(units.size());
    for (const auto& unit : units) {
        const auto intersection = QLineIntersection(
            source, direction, unit, missileWidth);
        if (intersection.Hits && intersection.Forward >= 0.0f) {
            candidates.push_back({ unit, intersection });
        }
    }
    std::sort(candidates.begin(), candidates.end(),
        [](const Candidate& left, const Candidate& right) {
            return left.Intersection.Forward < right.Intersection.Forward;
        });

    float reach = std::max(0.0f, baseRange);
    for (const auto& candidate : candidates) {
        const float radius = std::clamp(candidate.Unit.Radius, 0.0f, 150.0f);
        if (candidate.Intersection.Forward > reach + radius) break;
        ++result.TotalHits;
        if (candidate.Unit.NetworkId != 0 &&
            candidate.Unit.NetworkId == targetNetworkId) {
            result.TargetHit = true;
            result.TargetForward = candidate.Intersection.Forward;
        }
        if (candidate.Unit.ExtendsRange) {
            reach = std::min(
                std::max(reach, candidate.Intersection.Forward) +
                    std::max(0.0f, extensionPerHit),
                std::max(baseRange, maximumModeledRange));
            ++result.ExtensionHits;
        }
    }
    result.Reach = reach;
    result.End = source + direction * reach;
    result.End.y = source.y;
    return result;
}

struct QReturnHit {
    bool Hits = false;
    float Distance = FLT_MAX;
    float TravelFraction = 0.0f;
    Vec3 Closest = {};
};

inline QReturnHit QReturnIntersection(const Vec3& outwardEnd,
                                      const Vec3& akshanReturnPosition,
                                      const Vec3& targetPosition,
                                      float targetRadius,
                                      float missileWidth = 70.0f) {
    if (!outwardEnd.IsValid() || !akshanReturnPosition.IsValid() ||
        !targetPosition.IsValid()) {
        return {};
    }
    const SegmentProjection projection = ProjectPointToSegment2D(
        targetPosition, outwardEnd, akshanReturnPosition);
    const float hitRadius = std::max(1.0f, missileWidth * 0.5f) +
                            std::clamp(targetRadius, 0.0f, 150.0f);
    return {
        projection.Distance <= hitRadius,
        projection.Distance,
        projection.T,
        projection.Closest,
    };
}

enum class SwingDirection : int {
    Clockwise = -1,
    CounterClockwise = 1,
};

inline SwingDirection DirectionFromCursor(const Vec3& akshanPosition,
                                          const Vec3& anchor,
                                          const Vec3& cursorPosition) {
    const Vec3 facing = Direction2D(akshanPosition, anchor);
    const Vec3 cursor = Direction2D(akshanPosition, cursorPosition);
    // Live Heroic Swing maps the cursor on Akshan's left side to clockwise.
    // Cross2D's sign is used only as a side test; the enum then supplies the
    // actual signed rotation used by SwingPoint.
    return Cross2D(facing, cursor) >= 0.0f
        ? SwingDirection::Clockwise
        : SwingDirection::CounterClockwise;
}

inline float SwingRadius(const Vec3& akshanPosition, const Vec3& anchor) {
    return akshanPosition.Distance2D(anchor);
}

inline float SwingAngularVelocity(float radius,
                                  float linearVelocity = 1200.0f) {
    return std::max(0.0f, linearVelocity) / std::max(1.0f, radius);
}

inline Vec3 SwingPoint(const Vec3& anchor,
                       const Vec3& startPosition,
                       SwingDirection direction,
                       float elapsedSeconds,
                       float linearVelocity = 1200.0f) {
    Vec3 radial = startPosition - anchor;
    radial.y = 0.0f;
    const float radius = radial.Length2D();
    if (radius <= 1.0f || !std::isfinite(radius)) return startPosition;
    const float sign = static_cast<float>(static_cast<int>(direction));
    const float radians = sign * SwingAngularVelocity(radius, linearVelocity) *
                          std::max(0.0f, elapsedSeconds);
    const Vec3 rotated = Rotate2D(radial, radians);
    Vec3 result = anchor + rotated * radius;
    result.y = startPosition.y;
    return result;
}

struct SwingApproach {
    float MinimumDistance = FLT_MAX;
    float TimeSeconds = 0.0f;
    Vec3 Position = {};
    bool Collides = false;
};

inline SwingApproach ClosestSwingApproach(
    const Vec3& anchor,
    const Vec3& startPosition,
    SwingDirection direction,
    const Vec3& targetPosition,
    float targetRadius,
    float horizonSeconds = 2.5f,
    float sampleSeconds = 0.04f,
    float collisionPadding = 50.0f,
    float linearVelocity = 1200.0f) {
    SwingApproach best{};
    const float step = std::clamp(sampleSeconds, 0.01f, 0.20f);
    const float horizon = std::clamp(horizonSeconds, 0.0f, 25.0f);
    for (float time = 0.0f; time <= horizon + 0.001f; time += step) {
        const Vec3 position = SwingPoint(
            anchor, startPosition, direction, time, linearVelocity);
        const float distance = position.Distance2D(targetPosition);
        if (distance < best.MinimumDistance) {
            best.MinimumDistance = distance;
            best.TimeSeconds = time;
            best.Position = position;
        }
    }
    best.Collides = best.MinimumDistance <=
        std::max(0.0f, targetRadius) + std::max(0.0f, collisionPadding);
    return best;
}

inline int EstimatedSwingShots(float uninterruptedSeconds,
                               float attackFrequencySeconds = 0.20f,
                               bool includeInitialShot = true,
                               bool includeDismountShot = true) {
    const float duration = std::max(0.0f, uninterruptedSeconds);
    const float frequency = std::max(0.05f, attackFrequencySeconds);
    int shots = static_cast<int>(std::floor(duration / frequency));
    if (includeInitialShot) ++shots;
    if (includeDismountShot) ++shots;
    return std::max(0, shots);
}

inline Vec3 SwingDismountPoint(const Vec3& swingPosition,
                               const Vec3& cursorPosition,
                               float maximumDistance = 350.0f) {
    const Vec3 direction = Direction2D(swingPosition, cursorPosition);
    if (direction.IsZero()) return swingPosition;
    const float requested = swingPosition.Distance2D(cursorPosition);
    Vec3 result = swingPosition + direction *
        std::min(std::max(0.0f, maximumDistance), requested);
    result.y = swingPosition.y;
    return result;
}

struct RBlocker {
    Vec3 Position = {};
    float Radius = 35.0f;
    int NetworkId = 0;
};

struct RBlockerResult {
    bool Blocked = false;
    int NetworkId = 0;
    float TravelFraction = 1.0f;
    float Clearance = FLT_MAX;
    Vec3 Intercept = {};
};

inline RBlockerResult FirstRBlocker(const Vec3& source,
                                    const Vec3& targetPosition,
                                    float targetRadius,
                                    const std::vector<RBlocker>& blockers,
                                    float missileWidth = 40.0f) {
    RBlockerResult best{};
    const float sourceTargetDistance = source.Distance2D(targetPosition);
    if (sourceTargetDistance <= 1.0f) return best;
    const float targetEntryFraction = std::clamp(
        (sourceTargetDistance - std::max(0.0f, targetRadius)) /
            sourceTargetDistance,
        0.0f, 1.0f);
    for (const auto& blocker : blockers) {
        if (!blocker.Position.IsValid()) continue;
        const SegmentProjection projection = ProjectPointToSegment2D(
            blocker.Position, source, targetPosition);
        const float hitRadius = std::max(1.0f, missileWidth * 0.5f) +
                                std::max(0.0f, blocker.Radius);
        const float clearance = projection.Distance - hitRadius;
        if (projection.T <= 0.001f || projection.T >= targetEntryFraction ||
            clearance > 0.0f) {
            continue;
        }
        if (!best.Blocked || projection.T < best.TravelFraction) {
            best.Blocked = true;
            best.NetworkId = blocker.NetworkId;
            best.TravelFraction = projection.T;
            best.Clearance = clearance;
            best.Intercept = projection.Closest;
        }
    }
    return best;
}

inline int MaximumRBullets(int ultimateRank) {
    return std::clamp(4 + ultimateRank, 0, 7);
}

inline int StoredRBullets(int ultimateRank,
                          float channelSeconds,
                          float fullChannelSeconds = 2.5f) {
    const int maximum = MaximumRBullets(ultimateRank);
    if (maximum <= 0 || channelSeconds < 0.0f) return 0;
    if (maximum == 1) return 1;
    const float interval = std::max(0.01f, fullChannelSeconds /
        static_cast<float>(maximum - 1));
    return std::clamp(
        1 + static_cast<int>(std::floor(channelSeconds / interval)),
        1, maximum);
}

inline float RMissingHealthMultiplier(float targetHealthPercent) {
    const float missing = 1.0f -
        std::clamp(targetHealthPercent, 0.0f, 100.0f) / 100.0f;
    return 1.0f + 2.0f * missing;
}

inline float RRawDamagePerBullet(int ultimateRank,
                                 float totalAttackDamage,
                                 float criticalStrikeChance,
                                 float totalCriticalDamageMultiplier,
                                 float targetHealthPercent) {
    static constexpr float bases[] = { 0.0f, 25.0f, 35.0f, 45.0f };
    const int rank = std::clamp(ultimateRank, 0, 3);
    if (rank == 0) return 0.0f;
    const float critScalar = 1.0f +
        std::clamp(criticalStrikeChance, 0.0f, 1.0f) * 0.30f *
        std::max(0.0f, totalCriticalDamageMultiplier - 1.0f);
    return (bases[rank] + 0.15f * std::max(0.0f, totalAttackDamage)) *
           critScalar * RMissingHealthMultiplier(targetHealthPercent);
}

} // namespace Plugins::KuroAIO::AI::Controllers::Akshan::Geometry
