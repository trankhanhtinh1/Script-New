#pragma once

#include "../../../Core/CoreNavGrid.h"
#include "../../../Core/Vector.h"
#include "../EvadeSpells/EvadeSpellData.h"
#include "EvadeMath.h"

#include <algorithm>
#include <cmath>
#include <cfloat>
#include <cstdint>
#include <limits>
#include <vector>

namespace ZDEvade {

inline constexpr float kDefaultEndpointMargin = 10.0f;
inline constexpr float kNumericalOutwardEpsilon = 0.25f;
inline constexpr float kZeroRadiusNavValidationEpsilon = 0.001f;

struct SweptCircleGridGeometry {
    float minX = 0.0f;
    float minY = 0.0f;
    float maxX = 0.0f;
    float maxY = 0.0f;
    float cellSize = 0.0f;
    int width = 0;
    int height = 0;

    bool IsValid() const {
        if (!std::isfinite(minX) ||
            !std::isfinite(minY) ||
            !std::isfinite(maxX) ||
            !std::isfinite(maxY) ||
            !std::isfinite(cellSize) ||
            maxX <= minX ||
            maxY <= minY ||
            cellSize <= 0.0f ||
            width <= 0 ||
            height <= 0 ||
            width >= 10000 ||
            height >= 10000) {
            return false;
        }
        const float cellDomainMaxX =
            minX + static_cast<float>(width) * cellSize;
        const float cellDomainMaxY =
            minY + static_cast<float>(height) * cellSize;
        const float tolerance = std::max(0.01f, cellSize * 0.01f);
        return std::isfinite(cellDomainMaxX) &&
            std::isfinite(cellDomainMaxY) &&
            maxX <= cellDomainMaxX + tolerance &&
            maxY <= cellDomainMaxY + tolerance;
    }
};

inline bool SweptCircleCellWalkable(std::uint16_t rawFlags) {
    if (rawFlags == CoreNavGrid::kInvalidRawFlags) return false;
    if (CoreNavGrid::RawHasBrush(rawFlags)) return true;
    return !CoreNavGrid::RawHasWall(rawFlags);
}

template <typename RawCellFlags>
inline bool SweptCirclePointWalkable(
    const Vec2& point,
    float heroRadius,
    const SweptCircleGridGeometry& grid,
    RawCellFlags&& rawCellFlags) {
    if (!point.IsValid() ||
        !std::isfinite(heroRadius) ||
        heroRadius <= 0.0f ||
        !grid.IsValid()) {
        return false;
    }

    const float expandedMinX = point.x - heroRadius;
    const float expandedMinY = point.y - heroRadius;
    const float expandedMaxX = point.x + heroRadius;
    const float expandedMaxY = point.y + heroRadius;
    const float gridMaxX = std::min(
        grid.maxX,
        grid.minX + static_cast<float>(grid.width) * grid.cellSize);
    const float gridMaxY = std::min(
        grid.maxY,
        grid.minY + static_cast<float>(grid.height) * grid.cellSize);
    if (!std::isfinite(expandedMinX) ||
        !std::isfinite(expandedMinY) ||
        !std::isfinite(expandedMaxX) ||
        !std::isfinite(expandedMaxY) ||
        expandedMinX < grid.minX ||
        expandedMinY < grid.minY ||
        expandedMaxX > gridMaxX ||
        expandedMaxY > gridMaxY) {
        return false;
    }

    const int minCellX = std::clamp(
        static_cast<int>(std::ceil(
            (expandedMinX - grid.minX) / grid.cellSize - 1.0f)),
        0,
        grid.width - 1);
    const int minCellY = std::clamp(
        static_cast<int>(std::ceil(
            (expandedMinY - grid.minY) / grid.cellSize - 1.0f)),
        0,
        grid.height - 1);
    const int maxCellX = std::clamp(
        static_cast<int>(std::floor(
            (expandedMaxX - grid.minX) / grid.cellSize)),
        0,
        grid.width - 1);
    const int maxCellY = std::clamp(
        static_cast<int>(std::floor(
            (expandedMaxY - grid.minY) / grid.cellSize)),
        0,
        grid.height - 1);
    const float radiusSquared = heroRadius * heroRadius;

    for (int y = minCellY; y <= maxCellY; ++y) {
        const float cellMinY =
            grid.minY + static_cast<float>(y) * grid.cellSize;
        const float cellMaxY = cellMinY + grid.cellSize;
        for (int x = minCellX; x <= maxCellX; ++x) {
            const std::uint16_t flags = rawCellFlags(x, y);
            if (SweptCircleCellWalkable(flags)) continue;

            const float cellMinX =
                grid.minX + static_cast<float>(x) * grid.cellSize;
            const float cellMaxX = cellMinX + grid.cellSize;
            const float dx = std::max({
                cellMinX - point.x,
                0.0f,
                point.x - cellMaxX,
            });
            const float dy = std::max({
                cellMinY - point.y,
                0.0f,
                point.y - cellMaxY,
            });
            const float distanceSquared = dx * dx + dy * dy;
            if (!std::isfinite(distanceSquared) ||
                distanceSquared <= radiusSquared) {
                return false;
            }
        }
    }
    return true;
}

inline std::vector<Vec2> NormalizeObservedWaypoints(
    const Vec2& start,
    const std::vector<Vec2>& waypoints,
    const Vec2& fallbackEnd,
    float duplicateTolerance = 1.0f) {
    std::vector<Vec2> path;
    if (!start.IsValid() || start.IsZero()) return path;

    const float tolerance = std::max(0.0f, duplicateTolerance);
    path.reserve(waypoints.size() + 2);
    path.push_back(start);
    for (const Vec2& point : waypoints) {
        if (!point.IsValid() ||
            point.IsZero() ||
            path.back().Distance(point) <= tolerance) {
            continue;
        }
        path.push_back(point);
    }

    if (fallbackEnd.IsValid() && !fallbackEnd.IsZero()) {
        const bool alreadyPresent = std::any_of(
            path.begin(),
            path.end(),
            [&](const Vec2& point) {
                return point.Distance(fallbackEnd) <= tolerance;
            });
        if (!alreadyPresent) path.push_back(fallbackEnd);
    }
    return path;
}

struct ObservedRouteEvaluation {
    bool evaluated = false;
    bool valid = false;
    bool walkable = false;
    bool pathSafe = false;
    bool endpointSafe = false;
};

inline bool IsNavigationInterventionArmed(
    bool controlActive,
    bool exactDanger,
    bool pathAcquisitionDanger,
    bool releaseMarginDanger) {
    return exactDanger ||
        (controlActive ? releaseMarginDanger : pathAcquisitionDanger);
}

inline bool IsObservedThreatRouteUnsafe(
    std::size_t pointCount,
    const ObservedRouteEvaluation& evaluation) {
    return pointCount >= 2 &&
        evaluation.evaluated &&
        evaluation.valid &&
        evaluation.walkable &&
        (!evaluation.pathSafe || !evaluation.endpointSafe);
}

inline bool IsObservedRouteUnsafe(
    std::size_t pointCount,
    const ObservedRouteEvaluation& evaluation,
    bool navInterventionArmed) {
    if (pointCount < 2 || !evaluation.evaluated) return false;
    if (!evaluation.valid || !evaluation.walkable)
        return navInterventionArmed;
    return !evaluation.pathSafe || !evaluation.endpointSafe;
}

inline float SegmentCellAabbDistance(const Vec2& from,
                                     const Vec2& to,
                                     float minX,
                                     float minY,
                                     float maxX,
                                     float maxY) {
    const auto inside = [&](const Vec2& point) {
        return point.x >= minX &&
            point.x <= maxX &&
            point.y >= minY &&
            point.y <= maxY;
    };
    if (inside(from) || inside(to)) return 0.0f;

    const Vec2 bottomLeft(minX, minY);
    const Vec2 bottomRight(maxX, minY);
    const Vec2 topRight(maxX, maxY);
    const Vec2 topLeft(minX, maxY);
    return std::min({
        EvadeGeometryMath::SegmentSegmentDistance(
            from, to, bottomLeft, bottomRight),
        EvadeGeometryMath::SegmentSegmentDistance(
            from, to, bottomRight, topRight),
        EvadeGeometryMath::SegmentSegmentDistance(
            from, to, topRight, topLeft),
        EvadeGeometryMath::SegmentSegmentDistance(
            from, to, topLeft, bottomLeft),
    });
}

template <typename Path, typename RawCellFlags>
inline bool SweptCirclePathWalkable(const Path& path,
                                    float heroRadius,
                                    const SweptCircleGridGeometry& grid,
                                    RawCellFlags&& rawCellFlags) {
    if (path.size() < 2 ||
        !std::isfinite(heroRadius) ||
        heroRadius <= 0.0f ||
        !grid.IsValid()) {
        return false;
    }

    for (std::size_t index = 0; index < path.size(); ++index) {
        if (!path[index].IsValid()) return false;
        if (index == 0) continue;
        const float segmentLength = path[index - 1].Distance(path[index]);
        if (!std::isfinite(segmentLength) || segmentLength <= 0.001f)
            return false;
    }

    // O(sum over segments of expanded-cell-AABB area), O(1) memory.
    // Every blocked/invalid cell in that conservative enumeration receives an
    // exact segment-to-cell-AABB distance test, so no angular ray gaps exist.
    const float gridMaxX = std::min(
        grid.maxX,
        grid.minX + static_cast<float>(grid.width) * grid.cellSize);
    const float gridMaxY = std::min(
        grid.maxY,
        grid.minY + static_cast<float>(grid.height) * grid.cellSize);
    for (std::size_t index = 1; index < path.size(); ++index) {
        const Vec2& from = path[index - 1];
        const Vec2& to = path[index];
        const float expandedMinX =
            std::min(from.x, to.x) - heroRadius;
        const float expandedMinY =
            std::min(from.y, to.y) - heroRadius;
        const float expandedMaxX =
            std::max(from.x, to.x) + heroRadius;
        const float expandedMaxY =
            std::max(from.y, to.y) + heroRadius;
        if (!std::isfinite(expandedMinX) ||
            !std::isfinite(expandedMinY) ||
            !std::isfinite(expandedMaxX) ||
            !std::isfinite(expandedMaxY) ||
            expandedMinX < grid.minX ||
            expandedMinY < grid.minY ||
            expandedMaxX > gridMaxX ||
            expandedMaxY > gridMaxY) {
            return false;
        }

        const int minCellX = std::clamp(
            static_cast<int>(std::ceil(
                (expandedMinX - grid.minX) / grid.cellSize - 1.0f)),
            0,
            grid.width - 1);
        const int minCellY = std::clamp(
            static_cast<int>(std::ceil(
                (expandedMinY - grid.minY) / grid.cellSize - 1.0f)),
            0,
            grid.height - 1);
        const int maxCellX = std::clamp(
            static_cast<int>(std::floor(
                (expandedMaxX - grid.minX) / grid.cellSize)),
            0,
            grid.width - 1);
        const int maxCellY = std::clamp(
            static_cast<int>(std::floor(
                (expandedMaxY - grid.minY) / grid.cellSize)),
            0,
            grid.height - 1);

        for (int y = minCellY; y <= maxCellY; ++y) {
            const float cellMinY =
                grid.minY + static_cast<float>(y) * grid.cellSize;
            const float cellMaxY = cellMinY + grid.cellSize;
            for (int x = minCellX; x <= maxCellX; ++x) {
                const std::uint16_t flags = rawCellFlags(x, y);
                if (SweptCircleCellWalkable(flags)) continue;
                const float cellMinX =
                    grid.minX + static_cast<float>(x) * grid.cellSize;
                const float cellMaxX = cellMinX + grid.cellSize;
                const float distance = SegmentCellAabbDistance(
                    from,
                    to,
                    cellMinX,
                    cellMinY,
                    cellMaxX,
                    cellMaxY);
                if (!std::isfinite(distance) ||
                    distance <= heroRadius) {
                    return false;
                }
            }
        }
    }
    return true;
}

struct NavigationProbe {
    float clearance = 0.0f;
    Vec2 escapeDirection = {};
    int blockedRays = 0;
};

inline NavigationProbe ProbeNavigation(const Vec2& point,
                                       float planeY,
                                       float maxDistance,
                                       int rayCount = 8,
                                       int radialSteps = 4) {
    NavigationProbe result;
    maxDistance = std::max(20.0f, maxDistance);
    rayCount = std::clamp(rayCount, 4, 16);
    radialSteps = std::clamp(radialSteps, 2, 6);
    result.clearance = maxDistance;
    const auto navigable = [&](const Vec2& value) {
        return value.IsValid() && !value.IsZero() &&
            CoreNavGrid::IsWalkable(Vec3::From2D(value, planeY));
    };
    if (!navigable(point)) result.clearance = 0.0f;

    Vec2 escape;
    for (int ray = 0; ray < rayCount; ++ray) {
        const float angle = 2.0f * 3.14159265358979323846f *
            static_cast<float>(ray) / static_cast<float>(rayCount);
        const Vec2 direction(std::cos(angle), std::sin(angle));
        float openDistance = 0.0f;
        for (int step = 1; step <= radialSteps; ++step) {
            const float distance = maxDistance * static_cast<float>(step) /
                static_cast<float>(radialSteps);
            if (navigable(point + direction * distance)) {
                openDistance = distance;
                continue;
            }
            ++result.blockedRays;
            result.clearance = std::min(result.clearance, openDistance);
            const float proximity = 1.0f -
                std::clamp(openDistance / maxDistance, 0.0f, 1.0f);
            escape = escape - direction * (proximity * proximity);
            break;
        }
    }
    result.escapeDirection = escape.Normalized();
    return result;
}

struct ThreatCoverage {
    int collisionCount = 0;
    int endpointDanger = 0;
    int pathDanger = 0;
    int maxDanger = 0;
    float dangerExposureMs = 0.0f;
    float firstCollisionTimeMs = FLT_MAX;
    int summedExposureDanger = 0;
};

inline bool ImprovesThreatCoverage(const ThreatCoverage& candidate,
                                   const ThreatCoverage& baseline) {
    if (candidate.endpointDanger != baseline.endpointDanger)
        return candidate.endpointDanger < baseline.endpointDanger;
    if (candidate.maxDanger != baseline.maxDanger)
        return candidate.maxDanger < baseline.maxDanger;
    if (candidate.collisionCount != baseline.collisionCount)
        return candidate.collisionCount < baseline.collisionCount;
    if (candidate.pathDanger != baseline.pathDanger)
        return candidate.pathDanger < baseline.pathDanger;
    if (candidate.dangerExposureMs != baseline.dangerExposureMs)
        return candidate.dangerExposureMs < baseline.dangerExposureMs;
    return candidate.firstCollisionTimeMs > baseline.firstCollisionTimeMs;
}

inline bool ThreatCoverageNoWorse(const ThreatCoverage& candidate,
                                  const ThreatCoverage& baseline) {
    return !ImprovesThreatCoverage(baseline, candidate);
}

inline bool EquivalentThreatCoverage(const ThreatCoverage& left,
                                     const ThreatCoverage& right) {
    return !ImprovesThreatCoverage(left, right) &&
        !ImprovesThreatCoverage(right, left);
}

inline float NormalizeMetricBucketSize(float bucketSize,
                                       float fallback = 25.0f) {
    const float normalizedFallback =
        std::isfinite(fallback) && fallback > 0.0f
        ? fallback
        : 25.0f;
    return std::clamp(
        std::isfinite(bucketSize) && bucketSize > 0.0f
            ? bucketSize
            : normalizedFallback,
        1.0f,
        1000000.0f);
}

inline std::uint64_t TemporalMetricBucketId(
    float value,
    float bucketSize,
    bool nanMapsHigh) {
    constexpr std::uint64_t maximum =
        std::numeric_limits<std::uint64_t>::max();
    if (!std::isfinite(value)) return nanMapsHigh ? maximum : 0;
    if (value <= 0.0f) return 0;

    const long double normalized =
        static_cast<long double>(value) /
        static_cast<long double>(
            NormalizeMetricBucketSize(bucketSize));
    const long double finiteMaximum =
        static_cast<long double>(maximum - 1);
    if (!std::isfinite(normalized) ||
        normalized >= finiteMaximum) {
        return maximum - 1;
    }
    return static_cast<std::uint64_t>(std::floor(normalized));
}

inline int AggregateExposureDanger(
    const ThreatCoverage& coverage) {
    if (coverage.summedExposureDanger > 0)
        return coverage.summedExposureDanger;
    const std::int64_t collisionAggregate =
        static_cast<std::int64_t>(
            std::max(0, coverage.collisionCount)) *
        std::max(0, coverage.maxDanger);
    const std::int64_t safeAggregate = std::max<std::int64_t>({
        1,
        std::max(0, coverage.pathDanger),
        std::max(0, coverage.endpointDanger),
        collisionAggregate,
    });
    return static_cast<int>(std::min<std::int64_t>(
        safeAggregate,
        1000000));
}

inline float DangerExposureBucketSize(
    const ThreatCoverage& coverage,
    float temporalResolutionMs = 25.0f) {
    const float temporalBucket =
        NormalizeMetricBucketSize(temporalResolutionMs);
    return std::min(
        1000000.0f,
        temporalBucket *
            static_cast<float>(
                AggregateExposureDanger(coverage)));
}

inline std::uint64_t DangerExposureBucketId(
    float dangerExposureMs,
    const ThreatCoverage& coverage,
    float temporalResolutionMs = 25.0f) {
    return TemporalMetricBucketId(
        dangerExposureMs,
        DangerExposureBucketSize(
            coverage,
            temporalResolutionMs),
        true);
}

inline int CompareThreatCoverageAtResolution(
    const ThreatCoverage& candidate,
    const ThreatCoverage& baseline,
    float temporalResolutionMs = 25.0f) {
    if (candidate.endpointDanger != baseline.endpointDanger)
        return candidate.endpointDanger < baseline.endpointDanger ? -1 : 1;
    if (candidate.maxDanger != baseline.maxDanger)
        return candidate.maxDanger < baseline.maxDanger ? -1 : 1;
    if (candidate.collisionCount != baseline.collisionCount)
        return candidate.collisionCount < baseline.collisionCount ? -1 : 1;
    if (candidate.pathDanger != baseline.pathDanger)
        return candidate.pathDanger < baseline.pathDanger ? -1 : 1;

    const std::uint64_t candidateExposure =
        DangerExposureBucketId(
        candidate.dangerExposureMs,
        candidate,
        temporalResolutionMs);
    const std::uint64_t baselineExposure =
        DangerExposureBucketId(
            baseline.dangerExposureMs,
            baseline,
            temporalResolutionMs);
    if (candidateExposure != baselineExposure)
        return candidateExposure < baselineExposure ? -1 : 1;

    const float temporalBucket =
        NormalizeMetricBucketSize(temporalResolutionMs);
    const std::uint64_t candidateFirstContact =
        TemporalMetricBucketId(
            candidate.firstCollisionTimeMs,
            temporalBucket,
            false);
    const std::uint64_t baselineFirstContact =
        TemporalMetricBucketId(
            baseline.firstCollisionTimeMs,
            temporalBucket,
            false);
    if (candidateFirstContact == baselineFirstContact) return 0;
    return candidateFirstContact > baselineFirstContact ? -1 : 1;
}

inline bool MateriallyImprovesThreatCoverage(
    const ThreatCoverage& candidate,
    const ThreatCoverage& baseline,
    float temporalResolutionMs = 25.0f) {
    return CompareThreatCoverageAtResolution(
        candidate,
        baseline,
        temporalResolutionMs) < 0;
}

inline bool ThreatCoverageNoWorseAtResolution(
    const ThreatCoverage& candidate,
    const ThreatCoverage& baseline,
    float temporalResolutionMs = 25.0f) {
    return CompareThreatCoverageAtResolution(
        candidate,
        baseline,
        temporalResolutionMs) <= 0;
}

inline bool EquivalentThreatCoverageAtResolution(
    const ThreatCoverage& left,
    const ThreatCoverage& right,
    float temporalResolutionMs = 25.0f) {
    return CompareThreatCoverageAtResolution(
               left,
               right,
               temporalResolutionMs) == 0 &&
        CompareThreatCoverageAtResolution(
               right,
               left,
               temporalResolutionMs) == 0;
}

inline bool HasDiscreteThreatCoverageImprovement(
    const ThreatCoverage& candidate,
    const ThreatCoverage& baseline) {
    if (candidate.endpointDanger != baseline.endpointDanger)
        return candidate.endpointDanger < baseline.endpointDanger;
    if (candidate.maxDanger != baseline.maxDanger)
        return candidate.maxDanger < baseline.maxDanger;
    if (candidate.collisionCount != baseline.collisionCount)
        return candidate.collisionCount < baseline.collisionCount;
    if (candidate.pathDanger != baseline.pathDanger)
        return candidate.pathDanger < baseline.pathDanger;
    return false;
}

enum class LockedRouteSafety {
    Unsafe,
    FallbackNoWorse,
    StrictSafe,
};

struct LockedRouteValidationInput {
    ThreatCoverage coverage;
    ThreatCoverage baselineCoverage;
    bool hasLock = false;
    bool evaluationValid = false;
    bool walkable = false;
    bool reached = false;
    bool hardMoveFailure = false;
    bool strictSafe = false;
    float temporalResolutionMs = 25.0f;
};

struct LockedRouteValidation {
    bool hardValid = false;
    LockedRouteSafety safety = LockedRouteSafety::Unsafe;
};

inline LockedRouteValidation ClassifyLockedRoute(
    const LockedRouteValidationInput& input) {
    LockedRouteValidation result;
    result.hardValid =
        input.hasLock &&
        input.evaluationValid &&
        input.walkable &&
        !input.reached &&
        !input.hardMoveFailure;
    if (!result.hardValid) return result;
    if (input.strictSafe) {
        result.safety = LockedRouteSafety::StrictSafe;
    } else if (ThreatCoverageNoWorseAtResolution(
                   input.coverage,
                   input.baselineCoverage,
                   input.temporalResolutionMs)) {
        result.safety = LockedRouteSafety::FallbackNoWorse;
    }
    return result;
}

inline int DegradationCommitWindowMs(int targetLockMs) {
    return std::clamp(targetLockMs, 90, 160);
}

inline constexpr float kRouteTargetReachDistance = 18.0f;

inline bool IsRouteTargetReached(float distance) {
    return std::isfinite(distance) &&
        distance < kRouteTargetReachDistance;
}

enum class UnavoidableAction {
    MoveFallback,
    KeepNative,
    Hold,
};

struct UnavoidableDecisionInput {
    ThreatCoverage holdCoverage;
    ThreatCoverage nativeCoverage;
    ThreatCoverage candidateCoverage;
    ThreatCoverage lockCoverage;
    bool nativeAvailable = false;
    bool candidateAvailable = false;
    bool candidateValid = false;
    bool candidateWalkable = false;
    bool candidateStrictSafe = false;
    bool candidateMakesProgress = false;
    bool fallbackLockActive = false;
    bool lockValid = false;
    bool lockWalkable = false;
    bool lockReached = false;
    bool lockHardFailure = false;
    std::uint64_t currentManualEpoch = 0;
    std::uint64_t lockManualEpoch = 0;
};

struct UnavoidableDecision {
    UnavoidableAction action = UnavoidableAction::Hold;
    bool useNativeBaseline = false;
    bool retainLockedFallback = false;
};

inline UnavoidableDecision DecideUnavoidableAction(
    const UnavoidableDecisionInput& input) {
    UnavoidableDecision result;
    result.useNativeBaseline =
        input.nativeAvailable &&
        ThreatCoverageNoWorseAtResolution(
            input.nativeCoverage,
            input.holdCoverage);
    const ThreatCoverage& baseline = result.useNativeBaseline
        ? input.nativeCoverage
        : input.holdCoverage;

    const bool candidateUsable =
        input.candidateAvailable &&
        input.candidateValid &&
        input.candidateWalkable;
    if (candidateUsable && input.candidateStrictSafe) {
        result.action = UnavoidableAction::MoveFallback;
        return result;
    }
    const bool candidateAccepted = candidateUsable &&
        (MateriallyImprovesThreatCoverage(
             input.candidateCoverage,
             baseline) ||
         (EquivalentThreatCoverageAtResolution(
              input.candidateCoverage,
              baseline) &&
          input.candidateMakesProgress));
    const bool lockUsable =
        input.fallbackLockActive &&
        input.lockValid &&
        input.lockWalkable &&
        !input.lockReached &&
        !input.lockHardFailure &&
        input.currentManualEpoch == input.lockManualEpoch &&
        ThreatCoverageNoWorseAtResolution(input.lockCoverage, baseline);
    if (lockUsable &&
        (!candidateAccepted ||
         !MateriallyImprovesThreatCoverage(
             input.candidateCoverage,
             input.lockCoverage))) {
        result.action = UnavoidableAction::MoveFallback;
        result.retainLockedFallback = true;
        return result;
    }
    if (candidateAccepted) {
        result.action = UnavoidableAction::MoveFallback;
        return result;
    }
    result.action = result.useNativeBaseline
        ? UnavoidableAction::KeepNative
        : UnavoidableAction::Hold;
    return result;
}

inline bool ShouldPromoteFallbackEvaluation(bool fallbackState,
                                            bool valid,
                                            bool walkable,
                                            bool strictSafe) {
    return fallbackState && valid && walkable && strictSafe;
}

struct StableRouteMetrics {
    ThreatCoverage coverage;
    bool strictSafe = false;
    float minimumClearance = 0.0f;
    float timeMarginMs = 0.0f;
    float exitDistance = 0.0f;
    float travelDistance = 0.0f;
    float cursorDistance = 0.0f;
    float turretPenalty = 0.0f;
};

namespace StabilityBranch {
inline constexpr int Unknown = 0;
inline constexpr int LineAnalyticalLeft = 101;
inline constexpr int LineAnalyticalRight = 102;
inline constexpr int LineDetourLeft = 111;
inline constexpr int LineDetourRight = 112;
inline constexpr int LineStartCap = 113;
inline constexpr int LineEndCap = 114;
inline constexpr int ConeLeft = 201;
inline constexpr int ConeRight = 202;
inline constexpr int ConeRadialCap = 203;
inline constexpr int CircleCounterClockwise = 301;
inline constexpr int CircleClockwise = 302;
inline constexpr int CircleRadial = 303;
inline constexpr int RingOuterCounterClockwise = 311;
inline constexpr int RingOuterClockwise = 312;
inline constexpr int RingOuterRadial = 313;
inline constexpr int RingInnerCounterClockwise = 321;
inline constexpr int RingInnerClockwise = 322;
inline constexpr int RingInnerRadial = 323;
inline constexpr int CursorCoarseBase = 1000;
inline constexpr int RingCoarseBase = 1100;
inline constexpr int IntersectionCoarseBase = 1200;
inline constexpr int CoarseDirectionBins = 16;
}

inline constexpr float kContinuousChallengerBaseDrift = 18.0f;
inline constexpr float kContinuousChallengerMaximumDrift = 140.0f;
// A dot product of 0.8 keeps targets within the same broad route branch
// while making left/right or other opposing branches reset immediately.
inline constexpr float kContinuousChallengerDirectionDot = 0.80f;
inline constexpr int kContinuousChallengerMinimumWins = 2;
inline constexpr int kContinuousChallengerMaximumWaitMs = 90;

inline std::uint64_t StableThreatSetFingerprint(
    const std::vector<int>& threatIds) {
    std::vector<int> initializedIds;
    initializedIds.reserve(threatIds.size());
    std::size_t uninitializedCount = 0;
    for (const int id : threatIds) {
        if (id >= 0) {
            initializedIds.push_back(id);
        } else {
            ++uninitializedCount;
        }
    }
    std::sort(initializedIds.begin(), initializedIds.end());
    initializedIds.erase(
        std::unique(initializedIds.begin(), initializedIds.end()),
        initializedIds.end());

    constexpr std::uint64_t offset = 1469598103934665603ull;
    constexpr std::uint64_t prime = 1099511628211ull;
    std::uint64_t fingerprint = offset;
    const auto mix = [&](std::uint64_t value) {
        fingerprint ^= value;
        fingerprint *= prime;
    };
    mix(static_cast<std::uint64_t>(initializedIds.size()));
    for (const int id : initializedIds)
        mix(static_cast<std::uint64_t>(
            static_cast<std::uint32_t>(id)));
    mix(std::numeric_limits<std::uint64_t>::max());
    mix(static_cast<std::uint64_t>(uninitializedCount));
    return fingerprint;
}

struct ContinuousChallengerState {
    Vec2 target = {};
    Vec2 direction = {};
    int source = -1;
    int sourceThreatId = -1;
    int stabilityBranchKey = 0;
    std::uint64_t manualEpoch = 0;
    std::uint64_t threatSetFingerprint = 0;
    int firstWinTick = 0;
    int lastWinTick = 0;
    int consecutiveWins = 0;
    bool active = false;
};

struct ContinuousChallengerDecision {
    ContinuousChallengerState state;
    bool switchReady = false;
};

inline bool EquivalentContinuousChallengerTarget(
    const ContinuousChallengerState& state,
    const Vec2& origin,
    const Vec2& target,
    int source,
    int sourceThreatId,
    int stabilityBranchKey,
    std::uint64_t manualEpoch,
    std::uint64_t threatSetFingerprint,
    int now,
    float movementSpeed) {
    if (!origin.IsValid() ||
        !target.IsValid() ||
        target.IsZero()) {
        return false;
    }
    const Vec2 direction = (target - origin).Normalized();
    if (direction.IsZero() || state.direction.IsZero()) return false;
    const float safeSpeed = std::clamp(
        std::isfinite(movementSpeed) ? movementSpeed : 0.0f,
        0.0f,
        5000.0f);
    const float elapsedSeconds =
        static_cast<float>(std::max<std::int64_t>(
            0,
            TickDifference(now, state.lastWinTick))) /
        1000.0f;
    const float driftAllowance = std::clamp(
        kContinuousChallengerBaseDrift +
            safeSpeed * elapsedSeconds,
        kContinuousChallengerBaseDrift,
        kContinuousChallengerMaximumDrift);
    return state.active &&
        state.source == source &&
        state.sourceThreatId == sourceThreatId &&
        state.stabilityBranchKey == stabilityBranchKey &&
        state.manualEpoch == manualEpoch &&
        state.threatSetFingerprint == threatSetFingerprint &&
        state.direction.Dot(direction) >=
            kContinuousChallengerDirectionDot &&
        state.target.DistanceSqr(target) <=
            driftAllowance * driftAllowance;
}

inline ContinuousChallengerDecision AdvanceContinuousChallenger(
    const ContinuousChallengerState& current,
    const Vec2& origin,
    const Vec2& target,
    int source,
    int sourceThreatId,
    int stabilityBranchKey,
    std::uint64_t manualEpoch,
    std::uint64_t threatSetFingerprint,
    int now,
    float movementSpeed,
    bool challengerWon) {
    ContinuousChallengerDecision result;
    if (!challengerWon ||
        !origin.IsValid() ||
        !target.IsValid() ||
        target.IsZero()) {
        return result;
    }
    const Vec2 direction = (target - origin).Normalized();
    if (direction.IsZero()) return result;

    if (EquivalentContinuousChallengerTarget(
            current,
            origin,
            target,
            source,
            sourceThreatId,
            stabilityBranchKey,
            manualEpoch,
            threatSetFingerprint,
            now,
            movementSpeed)) {
        result.state = current;
        result.state.target = target;
        result.state.direction = direction;
        result.state.lastWinTick = now;
        result.state.consecutiveWins =
            current.consecutiveWins < std::numeric_limits<int>::max()
            ? current.consecutiveWins + 1
            : current.consecutiveWins;
    } else {
        result.state.target = target;
        result.state.direction = direction;
        result.state.source = source;
        result.state.sourceThreatId = sourceThreatId;
        result.state.stabilityBranchKey =
            stabilityBranchKey;
        result.state.manualEpoch = manualEpoch;
        result.state.threatSetFingerprint =
            threatSetFingerprint;
        result.state.firstWinTick = now;
        result.state.lastWinTick = now;
        result.state.consecutiveWins = 1;
        result.state.active = true;
    }
    const std::int64_t elapsedMs = std::max<std::int64_t>(
        0,
        TickDifference(now, result.state.firstWinTick));
    result.switchReady =
        result.state.consecutiveWins >=
            kContinuousChallengerMinimumWins ||
        elapsedMs >= kContinuousChallengerMaximumWaitMs;
    return result;
}

inline bool RequiresContinuousSwitchHysteresis(
    bool incumbentHardValid,
    bool strictOverFallback,
    bool discreteCoverageImprovement,
    bool manualEpochChanged,
    bool threatSetChanged = false) {
    return incumbentHardValid &&
        !strictOverFallback &&
        !discreteCoverageImprovement &&
        !manualEpochChanged &&
        !threatSetChanged;
}

inline bool KeepStableRoute(const StableRouteMetrics& current,
                            const StableRouteMetrics& proposed,
                            float alignment,
                            bool targetLockActive,
                            bool degradationCommitActive = false) {
    if (current.strictSafe) return true;
    if (proposed.strictSafe) return false;
    if (MateriallyImprovesThreatCoverage(
            proposed.coverage,
            current.coverage)) {
        return false;
    }
    if (MateriallyImprovesThreatCoverage(
            current.coverage,
            proposed.coverage)) {
        return true;
    }
    if (degradationCommitActive) return true;
    const auto higherBucket = [](float value, float bucketSize) {
        return TemporalMetricBucketId(
            value,
            bucketSize,
            false);
    };
    const auto lowerBucket = [](float value, float bucketSize) {
        return TemporalMetricBucketId(
            value,
            bucketSize,
            true);
    };
    const std::uint64_t currentUrgentClearance =
        higherBucket(current.minimumClearance, 20.0f);
    const std::uint64_t proposedUrgentClearance =
        higherBucket(proposed.minimumClearance, 20.0f);
    const std::uint64_t currentTurret =
        lowerBucket(current.turretPenalty, 20.0f);
    const std::uint64_t proposedTurret =
        lowerBucket(proposed.turretPenalty, 20.0f);
    const bool urgentClearance =
        (current.minimumClearance < 20.0f &&
         proposedUrgentClearance >
             currentUrgentClearance + 1) ||
        proposedTurret < currentTurret;
    if (urgentClearance) return false;

    const float clearanceBucketSize =
        targetLockActive ? 35.0f : 20.0f;
    const float marginBucketSize =
        targetLockActive ? 140.0f : 25.0f;
    const float exitBucketSize =
        targetLockActive ? 90.0f : 20.0f;
    const float travelBucketSize =
        targetLockActive ? 140.0f : 20.0f;
    const bool clearanceGain =
        higherBucket(
            proposed.minimumClearance,
            clearanceBucketSize) >
        higherBucket(
            current.minimumClearance,
            clearanceBucketSize);
    const bool marginGain =
        higherBucket(
            proposed.timeMarginMs,
            marginBucketSize) >
        higherBucket(
            current.timeMarginMs,
            marginBucketSize);
    const bool exitGain =
        lowerBucket(
            proposed.exitDistance,
            exitBucketSize) <
        lowerBucket(
            current.exitDistance,
            exitBucketSize);
    const bool travelGain =
        lowerBucket(
            proposed.travelDistance,
            travelBucketSize) <
        lowerBucket(
            current.travelDistance,
            travelBucketSize);
    const bool cursorGain =
        lowerBucket(
            proposed.cursorDistance,
            90.0f) <
        lowerBucket(
            current.cursorDistance,
            90.0f);
    const bool materialGain =
        clearanceGain ||
        marginGain ||
        exitGain ||
        travelGain ||
        (!targetLockActive &&
         alignment >= 0.70f &&
         cursorGain);
    if (!materialGain) return true;
    const bool strongClearanceGain =
        higherBucket(proposed.minimumClearance, 45.0f) >
        higherBucket(current.minimumClearance, 45.0f);
    const bool strongMarginGain =
        higherBucket(proposed.timeMarginMs, 170.0f) >
        higherBucket(current.timeMarginMs, 170.0f);
    return alignment < 0.70f &&
        !strongClearanceGain &&
        !strongMarginGain;
}

inline float ExitCollisionDistance(float spellRadius,
                                   float heroRadius,
                                   float endpointMargin,
                                   float uncertainty) {
    return std::max(0.0f, spellRadius) +
           std::max(0.0f, heroRadius) +
           std::max(0.0f, endpointMargin) +
           std::max(0.0f, uncertainty);
}

inline float ExitCenterDistance(float spellRadius,
                                float heroRadius,
                                float endpointMargin,
                                float uncertainty) {
    return ExitCollisionDistance(
               spellRadius,
               heroRadius,
               endpointMargin,
               uncertainty) +
           kNumericalOutwardEpsilon;
}

enum class DetourGeometry {
    Line,
    Circular,
    Ring,
    Cone,
    Arc,
};

struct DetourEnvelope {
    DetourGeometry geometry = DetourGeometry::Circular;
    Vec2 start = {};
    Vec2 end = {};
    Vec2 center = {};
    Vec2 direction = {};
    float range = 0.0f;
    float innerRadius = 0.0f;
    float outerRadius = 0.0f;
    float halfAngle = 0.0f;
};

inline float DetourSignedClearance(const DetourEnvelope& envelope,
                                   const Vec2& point) {
    const float outer = std::max(0.0f, envelope.outerRadius);
    switch (envelope.geometry) {
    case DetourGeometry::Line:
    case DetourGeometry::Arc:
        return EvadeGeometryMath::DistanceToSegment(
                   point,
                   envelope.start,
                   envelope.end) -
               outer;
    case DetourGeometry::Circular:
        return point.Distance(envelope.center) - outer;
    case DetourGeometry::Ring: {
        const float inner = std::clamp(
            envelope.innerRadius,
            0.0f,
            outer);
        const float distance = point.Distance(envelope.center);
        if (distance < inner) return inner - distance;
        if (distance > outer) return distance - outer;
        return -std::min(distance - inner, outer - distance);
    }
    case DetourGeometry::Cone:
        return EvadeGeometryMath::SignedDistanceToSector(
                   point,
                   envelope.center,
                   envelope.direction,
                   envelope.range,
                   envelope.halfAngle) -
               outer;
    default:
        return -FLT_MAX;
    }
}

inline float DetourFirstContactParameter(const DetourEnvelope& envelope,
                                         const Vec2& hero,
                                         const Vec2& goal) {
    return EvadeGeometryMath::FirstContactMovingPointBySignedDistance(
        hero,
        goal,
        [&](const Vec2& point) {
            return DetourSignedClearance(envelope, point);
        },
        0.01f,
        32);
}

inline bool RouteNeedsDetour(const DetourEnvelope& envelope,
                             const Vec2& hero,
                             const Vec2& goal) {
    if (!hero.IsValid() || !goal.IsValid() || hero.Distance(goal) < 1.0f)
        return false;
    if (DetourSignedClearance(envelope, hero) <= 0.0f) return false;
    return DetourFirstContactParameter(envelope, hero, goal) != FLT_MAX;
}

inline void AddUniqueDetourCandidate(std::vector<Vec2>& output,
                                     const Vec2& candidate) {
    if (!candidate.IsValid() || candidate.IsZero()) return;
    for (const Vec2& existing : output) {
        if (existing.DistanceSqr(candidate) <= 0.25f) return;
    }
    output.push_back(candidate);
}

inline void AddCircleDetourCandidates(std::vector<Vec2>& output,
                                      const Vec2& center,
                                      float collisionRadius,
                                      const Vec2& hero,
                                      const Vec2& goal) {
    const float radius = std::max(
        kNumericalOutwardEpsilon,
        collisionRadius + kNumericalOutwardEpsilon);
    const Vec2 heroOffset = hero - center;
    const float heroDistance = heroOffset.Length();
    if (heroDistance > radius + 0.001f) {
        const Vec2 radial = heroOffset * (1.0f / heroDistance);
        const Vec2 tangent(-radial.y, radial.x);
        const float along = radius * radius / heroDistance;
        const float across = radius * std::sqrt(std::max(
            0.0f,
            heroDistance * heroDistance - radius * radius)) /
            heroDistance;
        AddUniqueDetourCandidate(
            output,
            center + radial * along + tangent * across);
        AddUniqueDetourCandidate(
            output,
            center + radial * along - tangent * across);
    }
    Vec2 heroDirection = heroOffset.Normalized();
    if (heroDirection.IsZero()) heroDirection = Vec2(1.0f, 0.0f);
    Vec2 goalDirection = (goal - center).Normalized();
    if (goalDirection.IsZero()) goalDirection = heroDirection * -1.0f;
    AddUniqueDetourCandidate(output, center + heroDirection * radius);
    AddUniqueDetourCandidate(output, center + goalDirection * radius);
}

inline std::vector<Vec2> BuildDetourCandidates(
    const DetourEnvelope& envelope,
    const Vec2& hero,
    const Vec2& goal) {
    std::vector<Vec2> output;
    if (!RouteNeedsDetour(envelope, hero, goal)) return output;

    const float contact = DetourFirstContactParameter(
        envelope,
        hero,
        goal);
    const Vec2 contactPoint = contact == FLT_MAX
        ? hero
        : hero + (goal - hero) * contact;
    const float safeOuter = std::max(
        kNumericalOutwardEpsilon,
        envelope.outerRadius + kNumericalOutwardEpsilon);
    switch (envelope.geometry) {
    case DetourGeometry::Line:
    case DetourGeometry::Arc: {
        Vec2 axis = (envelope.end - envelope.start).Normalized();
        if (axis.IsZero()) {
            AddCircleDetourCandidates(
                output,
                envelope.start,
                envelope.outerRadius,
                hero,
                goal);
            break;
        }
        const Vec2 normal(-axis.y, axis.x);
        Vec2 contactProjection;
        EvadeGeometryMath::DistanceToSegment(
            contactPoint,
            envelope.start,
            envelope.end,
            nullptr,
            &contactProjection);
        for (int side = -1; side <= 1; side += 2) {
            const Vec2 offset =
                normal * (safeOuter * static_cast<float>(side));
            AddUniqueDetourCandidate(output, contactProjection + offset);
            AddUniqueDetourCandidate(output, envelope.start + offset);
            AddUniqueDetourCandidate(output, envelope.end + offset);
        }
        AddUniqueDetourCandidate(
            output,
            envelope.start - axis * safeOuter);
        AddUniqueDetourCandidate(
            output,
            envelope.end + axis * safeOuter);
        AddCircleDetourCandidates(
            output,
            envelope.start,
            envelope.outerRadius,
            hero,
            goal);
        AddCircleDetourCandidates(
            output,
            envelope.end,
            envelope.outerRadius,
            hero,
            goal);
        break;
    }
    case DetourGeometry::Circular:
        AddCircleDetourCandidates(
            output,
            envelope.center,
            envelope.outerRadius,
            hero,
            goal);
        break;
    case DetourGeometry::Ring: {
        const float heroDistance = hero.Distance(envelope.center);
        if (heroDistance > envelope.outerRadius) {
            AddCircleDetourCandidates(
                output,
                envelope.center,
                envelope.outerRadius,
                hero,
                goal);
            break;
        }
        const float safeInner = std::max(
            0.0f,
            envelope.innerRadius - kNumericalOutwardEpsilon);
        if (safeInner <= 0.0f) break;
        Vec2 radial = (hero - envelope.center).Normalized();
        if (radial.IsZero()) radial = (goal - envelope.center).Normalized();
        if (radial.IsZero()) radial = Vec2(1.0f, 0.0f);
        constexpr float offsets[] = {0.0f, -0.35f, 0.35f, -0.7f, 0.7f};
        for (float offset : offsets) {
            AddUniqueDetourCandidate(
                output,
                envelope.center +
                    EvadeGeometryMath::Rotate(radial, offset) * safeInner);
        }
        break;
    }
    case DetourGeometry::Cone: {
        Vec2 axis = envelope.direction.Normalized();
        if (axis.IsZero()) axis = (goal - envelope.center).Normalized();
        if (axis.IsZero()) break;
        const Vec2 relative = contactPoint - envelope.center;
        for (int side = -1; side <= 1; side += 2) {
            const Vec2 boundary = EvadeGeometryMath::Rotate(
                axis,
                envelope.halfAngle * static_cast<float>(side));
            const Vec2 outward = side > 0
                ? Vec2(-boundary.y, boundary.x)
                : Vec2(boundary.y, -boundary.x);
            const float projection = std::clamp(
                relative.Dot(boundary),
                0.0f,
                std::max(0.0f, envelope.range));
            AddUniqueDetourCandidate(
                output,
                envelope.center + boundary * projection +
                    outward * safeOuter);
            AddUniqueDetourCandidate(
                output,
                envelope.center +
                    boundary * std::max(0.0f, envelope.range) +
                    outward * safeOuter);
        }
        AddUniqueDetourCandidate(
            output,
            envelope.center +
                axis * (std::max(0.0f, envelope.range) + safeOuter));
        AddCircleDetourCandidates(
            output,
            envelope.center,
            envelope.outerRadius,
            hero,
            goal);
        break;
    }
    }
    return output;
}

inline bool ShouldCancelUnsafeMovement(bool walkingControlActive,
                                       bool hasUsableLockedPlan) {
    return walkingControlActive && !hasUsableLockedPlan;
}

enum class MoveIntentSource {
    Manual,
    Orbwalker,
    ObservedPath,
    Controller,
};

enum class ThreatFreeDecisionSite {
    Update,
    MoveRequest,
};

struct ThreatFreeActionDecision {
    bool applies = false;
    bool releaseControl = false;
    bool clearIntents = false;
    bool allowNativeInput = false;
    bool stopMovement = false;
    bool replan = false;
    bool deferInput = false;
};

inline ThreatFreeActionDecision DecideThreatFreeAction(
    bool actionableThreatContext,
    ThreatFreeDecisionSite site) {
    if (actionableThreatContext) return {};
    return {
        true,
        true,
        true,
        site == ThreatFreeDecisionSite::MoveRequest,
        false,
        false,
        false,
    };
}

enum class MoveIssueResult {
    Issued,
    AlreadyFollowing,
    Throttled,
    RetryableFailure,
    HardFailure,
};

struct TargetCommitDecision {
    bool commitProposed = false;
    bool retainCommitted = false;
    bool retryProposed = false;
};

inline TargetCommitDecision DecideTargetCommit(
    MoveIssueResult result,
    bool committedHardValid) {
    if (result == MoveIssueResult::Issued ||
        result == MoveIssueResult::AlreadyFollowing) {
        return {true, false, false};
    }
    if (result == MoveIssueResult::Throttled ||
        result == MoveIssueResult::RetryableFailure) {
        return {false, committedHardValid, true};
    }
    return {false, committedHardValid, false};
}

enum class MoveCadenceAction {
    AlreadyFollowing,
    Throttled,
    Issue,
};

inline MoveCadenceAction DecideMoveCadence(bool pathMatches,
                                           bool stuck,
                                           bool targetChanged,
                                           int now,
                                           int lastSuccessTick,
                                           int lastAttemptTick,
                                           int minimumIntervalMs,
                                           int refreshIntervalMs) {
    const int refreshInterval = std::max(0, refreshIntervalMs);
    if (pathMatches &&
        !stuck &&
        !targetChanged &&
        lastSuccessTick > 0 &&
        TickDifference(now, lastSuccessTick) < refreshInterval) {
        return MoveCadenceAction::AlreadyFollowing;
    }
    const int minimumInterval = std::max(20, minimumIntervalMs);
    if (lastAttemptTick > 0 &&
        TickDifference(now, lastAttemptTick) < minimumInterval) {
        return MoveCadenceAction::Throttled;
    }
    return MoveCadenceAction::Issue;
}

struct MoveFailureClassification {
    MoveIssueResult result = MoveIssueResult::RetryableFailure;
    int consecutiveFailures = 0;
};

inline MoveFailureClassification ClassifyMoveFailure(
    int previousConsecutiveFailures) {
    const int failures = std::max(0, previousConsecutiveFailures) + 1;
    return {
        failures >= 3
            ? MoveIssueResult::HardFailure
            : MoveIssueResult::RetryableFailure,
        failures,
    };
}

inline int NextMoveFailureStreak(MoveIssueResult result,
                                 int currentConsecutiveFailures) {
    if (result == MoveIssueResult::Issued ||
        result == MoveIssueResult::AlreadyFollowing) {
        return 0;
    }
    if (result == MoveIssueResult::RetryableFailure ||
        result == MoveIssueResult::HardFailure) {
        return ClassifyMoveFailure(currentConsecutiveFailures)
            .consecutiveFailures;
    }
    return std::max(0, currentConsecutiveFailures);
}

enum class StopIssueResult {
    Issued,
    Throttled,
    Failed,
};

enum class StopThrottleResetMode {
    ImmediateRelease,
    FullReset,
};

inline int StopThrottleTickAfterReset(StopThrottleResetMode mode,
                                      int now,
                                      int lastStopTick,
                                      int safeGraceMs = 180) {
    if (lastStopTick <= 0) return 0;
    if (mode == StopThrottleResetMode::ImmediateRelease)
        return lastStopTick;
    return TickDifference(now, lastStopTick) <
            std::max(0, safeGraceMs)
        ? lastStopTick
        : 0;
}

struct LegacyControlRestoreInput {
    bool controlActive = false;
    bool sameOrbwalkerImplementation = false;
    bool currentMoveEnabled = false;
    bool imposedMoveEnabled = false;
    bool currentAttackEnabled = false;
    bool imposedAttackEnabled = false;
    bool interventionStateMatches = false;
    bool comboBlockMatches = false;
    bool ownerStateReleaseSucceeded = false;
    bool otherOwnerActiveAfterRelease = false;
};

struct LegacyControlRestoreDecision {
    bool restoreMoveEnabled = false;
    bool restoreAttackEnabled = false;
    bool restoreInterventionState = false;
    bool restoreComboBlock = false;
};

struct AttackControlDecision {
    bool baselineAttackEnabled = false;
    bool imposedAttackEnabled = false;
};

inline AttackControlDecision DecideAttackControl(
        bool controlActive,
        bool previousAttackEnabled,
        bool currentAttackEnabled,
        bool allowAttacks) {
    const bool baseline =
        controlActive ? previousAttackEnabled : currentAttackEnabled;
    return {baseline, allowAttacks && baseline};
}

enum class LegacyControlExitMode {
    NormalRestore,
    ExternalOwnerHandoff,
};

inline LegacyControlRestoreDecision DecideLegacyControlRestore(
    const LegacyControlRestoreInput& input,
    LegacyControlExitMode mode = LegacyControlExitMode::NormalRestore) {
    if (!input.controlActive ||
        !input.ownerStateReleaseSucceeded) {
        return {};
    }
    if (mode == LegacyControlExitMode::ExternalOwnerHandoff &&
        input.otherOwnerActiveAfterRelease) {
        return {};
    }
    return {
        input.sameOrbwalkerImplementation &&
            input.currentMoveEnabled == input.imposedMoveEnabled,
        input.sameOrbwalkerImplementation &&
            input.currentAttackEnabled == input.imposedAttackEnabled,
        input.interventionStateMatches,
        input.comboBlockMatches,
    };
}

struct ReleaseDecisionInput {
    bool hasUsablePlan = false;
    bool currentPathUnsafe = false;
    int currentThreatSerial = 0;
    int releaseThreatSerial = 0;
    std::uint64_t currentMoveRequestGeneration = 0;
    std::uint64_t releaseMoveRequestGeneration = 0;
};

inline bool MustBreakReleaseCooldown(const ReleaseDecisionInput& input) {
    return input.currentThreatSerial != input.releaseThreatSerial ||
           input.currentMoveRequestGeneration !=
               input.releaseMoveRequestGeneration;
}

inline bool MustStopBeforeRelease(const ReleaseDecisionInput& input) {
    return input.currentPathUnsafe && !input.hasUsablePlan;
}

inline bool MustPreserveDeferredDestination(
    const ReleaseDecisionInput& input) {
    return !input.hasUsablePlan;
}

inline bool CanReleaseAfterStop(bool playerMoving,
                                StopIssueResult stopResult) {
    return !playerMoving || stopResult == StopIssueResult::Issued;
}

inline int NoPlanRetryDelayMs(float firstCollisionTimeMs,
                              float minimumTimeMarginMs) {
    constexpr int kMinimumRetryMs = 20;
    constexpr int kMaximumRetryMs = 120;
    if (firstCollisionTimeMs == FLT_MAX) {
        return kMaximumRetryMs;
    }
    if (!std::isfinite(firstCollisionTimeMs) ||
        firstCollisionTimeMs <= 0.0f) {
        return 0;
    }

    const double margin = std::isnan(minimumTimeMarginMs)
        ? 0.0
        : std::max(0.0, static_cast<double>(minimumTimeMarginMs));
    const double retryDeadline =
        static_cast<double>(firstCollisionTimeMs) - margin;
    if (!std::isfinite(retryDeadline) ||
        retryDeadline <= static_cast<double>(kMinimumRetryMs)) {
        return 0;
    }

    const double strictlyBeforeDeadline =
        std::ceil(retryDeadline) - 1.0;
    const double boundedRetry = std::clamp(
        strictlyBeforeDeadline,
        static_cast<double>(kMinimumRetryMs),
        static_cast<double>(kMaximumRetryMs));
    return static_cast<int>(boundedRetry);
}

enum class NoPlanHoldAction {
    RetryStop,
    Hold,
};

inline NoPlanHoldAction DecideNoPlanHoldAction(
    bool playerMoving,
    StopIssueResult stopResult) {
    return CanReleaseAfterStop(playerMoving, stopResult)
        ? NoPlanHoldAction::Hold
        : NoPlanHoldAction::RetryStop;
}

struct NoPlanRetrySchedule {
    bool pending = false;
    int retryTick = 0;
    int threatSerial = -1;
    std::uint64_t manualRequestGeneration = 0;
};

struct RequestGenerationState {
    std::uint64_t moveRequestGeneration = 0;
    std::uint64_t manualRequestGeneration = 0;
};

inline RequestGenerationState AdvanceRequestGenerations(
    RequestGenerationState current,
    MoveIntentSource source) {
    if (source == MoveIntentSource::Manual ||
        source == MoveIntentSource::Orbwalker) {
        ++current.moveRequestGeneration;
    }
    if (source == MoveIntentSource::Manual) {
        ++current.manualRequestGeneration;
    }
    return current;
}

inline NoPlanRetrySchedule ScheduleNoPlanRetry(
    int now,
    float firstCollisionTimeMs,
    float minimumTimeMarginMs,
    int threatSerial,
    std::uint64_t manualRequestGeneration) {
    return {
        true,
        SaturatingTickAdd(
            now,
            NoPlanRetryDelayMs(
                firstCollisionTimeMs,
                minimumTimeMarginMs)),
        threatSerial,
        manualRequestGeneration,
    };
}

inline bool ShouldRetryNoPlan(
    const NoPlanRetrySchedule& schedule,
    int now,
    int currentThreatSerial,
    std::uint64_t currentManualRequestGeneration) {
    return schedule.pending &&
        (currentThreatSerial != schedule.threatSerial ||
         currentManualRequestGeneration !=
             schedule.manualRequestGeneration ||
         TickDifference(now, schedule.retryTick) >= 0);
}

inline bool ShouldReplanRoute(
    bool lockedValid,
    int now,
    int lastPlanTick,
    int planIntervalMs,
    int currentThreatSerial,
    int lastThreatSerial,
    std::uint64_t currentManualRequestGeneration,
    const NoPlanRetrySchedule& noPlanRetry) {
    if (noPlanRetry.pending) {
        return ShouldRetryNoPlan(
            noPlanRetry,
            now,
            currentThreatSerial,
            currentManualRequestGeneration);
    }
    return !lockedValid ||
        currentThreatSerial != lastThreatSerial ||
        lastPlanTick == 0 ||
        TickDifference(now, lastPlanTick) >=
            std::max(20, planIntervalMs);
}

enum class ReleaseHysteresisAction {
    Release,
    Plan,
    HoldAtStrictEndpoint,
};

struct ReleaseHysteresisInput {
    bool controlActive = false;
    bool pathAcquisitionDanger = false;
    bool releaseMarginDanger = false;
    bool exactDanger = false;
    bool currentPathUnsafe = false;
    bool strictEndpointReached = false;
};

inline float ControlThreatBuffer(bool controlActive,
                                 float pathBuffer,
                                 float releaseBuffer) {
    return std::max(
        0.0f,
        controlActive ? releaseBuffer : pathBuffer);
}

inline ReleaseHysteresisAction DecideReleaseHysteresis(
    const ReleaseHysteresisInput& input) {
    if (input.exactDanger || input.currentPathUnsafe)
        return ReleaseHysteresisAction::Plan;
    if (!input.controlActive) {
        return input.pathAcquisitionDanger
            ? ReleaseHysteresisAction::Plan
            : ReleaseHysteresisAction::Release;
    }
    if (!input.releaseMarginDanger)
        return ReleaseHysteresisAction::Release;
    return input.strictEndpointReached
        ? ReleaseHysteresisAction::HoldAtStrictEndpoint
        : ReleaseHysteresisAction::Plan;
}

inline bool HoldMaySuppressPlanning(HoldProtectionKind protection,
                                    bool holdActive) {
    return holdActive &&
        (protection == HoldProtectionKind::Untargetable ||
         protection == HoldProtectionKind::Invulnerable);
}

inline bool VerifiedHoldMaySuppressPlanning(
    HoldProtectionKind protection,
    bool holdActive,
    bool observedInvulnerable,
    bool observedUntargetable) {
    if (!HoldMaySuppressPlanning(protection, holdActive)) return false;
    if (protection == HoldProtectionKind::Invulnerable)
        return observedInvulnerable;
    return observedUntargetable || observedInvulnerable;
}

inline bool ShouldClearEstimatedHold(
    HoldProtectionKind protection,
    bool holdActive,
    bool activationReached,
    bool observedInvulnerable,
    bool observedUntargetable) {
    if (!holdActive) return true;
    if (!activationReached ||
        !HoldMaySuppressPlanning(protection, holdActive)) {
        return false;
    }
    return !VerifiedHoldMaySuppressPlanning(
        protection,
        holdActive,
        observedInvulnerable,
        observedUntargetable);
}

inline bool MoveResultInvalidatesLock(MoveIssueResult result) {
    return result == MoveIssueResult::HardFailure;
}

inline bool ManualIntentWins(int manualGeneration, int orbGeneration) {
    (void)orbGeneration;
    return manualGeneration > 0;
}

struct StrictRouteRank {
    float turretPenalty = 0.0f;
    float exitDistance = 0.0f;
    float travelDistance = 0.0f;
    float timeMarginMs = 0.0f;
    float minimumClearance = 0.0f;
    float enemyDistance = 0.0f;
    float cursorDistance = 0.0f;
};

struct FallbackRouteRank {
    int endpointDanger = 0;
    int maxDanger = 0;
    int collisionCount = 0;
    int pathDanger = 0;
    float dangerExposureMs = 0.0f;
    bool reenteredDanger = false;
    float firstCollisionTimeMs = FLT_MAX;
    float timeMarginMs = 0.0f;
    float exitDistance = 0.0f;
    float travelDistance = 0.0f;
    float turretPenalty = 0.0f;
    float enemyDistance = 0.0f;
    float cursorDistance = 0.0f;
};

inline bool RankDifferent(float left, float right, float epsilon = 0.5f) {
    return std::fabs(left - right) > epsilon;
}

inline bool PreferFallbackRoute(const FallbackRouteRank& left,
                                const FallbackRouteRank& right) {
    if (left.endpointDanger != right.endpointDanger)
        return left.endpointDanger < right.endpointDanger;
    if (left.maxDanger != right.maxDanger)
        return left.maxDanger < right.maxDanger;
    if (left.collisionCount != right.collisionCount)
        return left.collisionCount < right.collisionCount;
    if (left.pathDanger != right.pathDanger)
        return left.pathDanger < right.pathDanger;
    if (RankDifferent(left.dangerExposureMs, right.dangerExposureMs))
        return left.dangerExposureMs < right.dangerExposureMs;
    if (left.reenteredDanger != right.reenteredDanger)
        return !left.reenteredDanger;
    if (RankDifferent(
            left.firstCollisionTimeMs,
            right.firstCollisionTimeMs)) {
        return left.firstCollisionTimeMs > right.firstCollisionTimeMs;
    }
    if (RankDifferent(left.timeMarginMs, right.timeMarginMs))
        return left.timeMarginMs > right.timeMarginMs;
    if (RankDifferent(left.exitDistance, right.exitDistance))
        return left.exitDistance < right.exitDistance;
    if (RankDifferent(left.travelDistance, right.travelDistance))
        return left.travelDistance < right.travelDistance;
    if (RankDifferent(left.turretPenalty, right.turretPenalty))
        return left.turretPenalty < right.turretPenalty;
    if (RankDifferent(left.enemyDistance, right.enemyDistance))
        return left.enemyDistance > right.enemyDistance;
    return left.cursorDistance < right.cursorDistance;
}

inline bool PreferStrictRoute(const StrictRouteRank& left,
                              const StrictRouteRank& right) {
    if (RankDifferent(left.turretPenalty, right.turretPenalty))
        return left.turretPenalty < right.turretPenalty;
    if (RankDifferent(left.exitDistance, right.exitDistance))
        return left.exitDistance < right.exitDistance;
    if (RankDifferent(left.travelDistance, right.travelDistance))
        return left.travelDistance < right.travelDistance;
    if (RankDifferent(left.timeMarginMs, right.timeMarginMs))
        return left.timeMarginMs > right.timeMarginMs;
    if (RankDifferent(left.minimumClearance, right.minimumClearance))
        return left.minimumClearance > right.minimumClearance;
    if (RankDifferent(left.enemyDistance, right.enemyDistance))
        return left.enemyDistance > right.enemyDistance;
    return left.cursorDistance < right.cursorDistance;
}

struct LockedRouteStatus {
    bool hasLock = false;
    bool valid = false;
    bool walkable = false;
    bool pathSafe = false;
    bool endpointSafe = false;
    bool reachedTarget = false;
};

inline bool KeepStrictRoute(const LockedRouteStatus& status) {
    return status.hasLock &&
           status.valid &&
           status.walkable &&
           status.pathSafe &&
           status.endpointSafe &&
           !status.reachedTarget;
}

inline bool ShouldCommitStrictState(bool strictEvadeState,
                                    bool reroutingPathState,
                                    bool rerouteRequired) {
    return strictEvadeState ||
           (reroutingPathState && rerouteRequired);
}

struct StrictCommitmentInput {
    LockedRouteStatus route;
    bool committedState = false;
    bool deferredResumeReady = false;
    bool replanTimerExpired = false;
    bool threatSerialChanged = false;
    bool targetLockExpired = false;
    bool materialClearanceGain = false;
    bool materialTimeGain = false;
    bool materialCursorGain = false;
};

inline bool ShouldRetainCommittedStrictTarget(
    const StrictCommitmentInput& input) {
    return !input.deferredResumeReady &&
        input.committedState &&
        KeepStrictRoute(input.route);
}

inline bool ShouldExecuteReleaseOrDeferredResume(
    bool deferredResumeReady,
    bool endangered) {
    return deferredResumeReady || !endangered;
}

enum class DeferredRouteAction {
    None,
    Detour,
    Resume,
};

inline DeferredRouteAction DecideDeferredRoute(bool hasDestination,
                                               bool directRouteSafe) {
    if (!hasDestination) return DeferredRouteAction::None;
    return directRouteSafe
        ? DeferredRouteAction::Resume
        : DeferredRouteAction::Detour;
}

class DeferredDestination {
public:
    bool HasValue() const { return valid; }
    const Vec2& Position() const { return position; }
    int Tick() const { return tick; }
    MoveIntentSource Source() const { return source; }
    std::uint64_t Generation() const { return generation; }
    bool SafetyBlocked() const { return safetyBlocked; }

    void Record(const Vec2& value,
                int valueTick,
                MoveIntentSource valueSource = MoveIntentSource::Manual,
                std::uint64_t valueGeneration = 0,
                bool valueSafetyBlocked = true) {
        if (!value.IsValid() || value.IsZero()) return;
        position = value;
        tick = valueTick;
        source = valueSource;
        generation = valueGeneration;
        safetyBlocked = valueSafetyBlocked;
        valid = true;
    }

    void Clear() {
        position = {};
        tick = 0;
        source = MoveIntentSource::ObservedPath;
        generation = 0;
        safetyBlocked = false;
        valid = false;
    }

private:
    Vec2 position = {};
    int tick = 0;
    MoveIntentSource source = MoveIntentSource::ObservedPath;
    std::uint64_t generation = 0;
    bool safetyBlocked = false;
    bool valid = false;
};

enum class MoveIntentRecordResult {
    Ignored,
    Accepted,
    Deferred,
    SafeManual,
    Blocked,
};

struct MoveRouteEvaluation {
    bool evaluated = false;
    bool valid = false;
    bool walkable = false;
    bool strictSafe = false;
};

enum class ManualRouteAction {
    PreserveAndAllowNative,
    PreserveAndBlock,
    Defer,
    AdoptSafe,
};

inline ManualRouteAction DecideManualRouteAction(
    bool controllerOwnsMovement,
    const MoveRouteEvaluation& route) {
    if (!route.evaluated || !route.valid || !route.walkable) {
        return controllerOwnsMovement
            ? ManualRouteAction::PreserveAndBlock
            : ManualRouteAction::PreserveAndAllowNative;
    }
    return route.strictSafe
        ? ManualRouteAction::AdoptSafe
        : ManualRouteAction::Defer;
}

inline int SafeManualAdoptionWindowMs(int pingMs) {
    // Keep one native-order observation window, bounded against bad ping data.
    const std::int64_t derived =
        250 + static_cast<std::int64_t>(std::max(0, pingMs)) * 2;
    return static_cast<int>(std::clamp<std::int64_t>(
        derived,
        250,
        1000));
}

inline int SafeManualAdoptionDeadlineTick(int tick, int pingMs) {
    const std::int64_t deadline =
        static_cast<std::int64_t>(tick) +
        SafeManualAdoptionWindowMs(pingMs);
    return static_cast<int>(std::min<std::int64_t>(
        deadline,
        std::numeric_limits<int>::max()));
}

class MoveIntentState {
public:
    MoveIntentRecordResult RecordManual(
        const Vec2& destination,
        int tick,
        std::uint64_t generation,
        const MoveRouteEvaluation& route,
        bool controllerOwnsMovement,
        int pingMs) {
        ExpireAdoption(tick);
        const ManualRouteAction action = DecideManualRouteAction(
            controllerOwnsMovement,
            route);
        if (!destination.IsValid() || destination.IsZero()) {
            return controllerOwnsMovement
                ? MoveIntentRecordResult::Blocked
                : MoveIntentRecordResult::Ignored;
        }
        if (HasManual() &&
            generation < Manual().Generation()) {
            return MoveIntentRecordResult::Ignored;
        }
        if (action == ManualRouteAction::PreserveAndBlock)
            return MoveIntentRecordResult::Blocked;
        if (action == ManualRouteAction::PreserveAndAllowNative)
            return MoveIntentRecordResult::Ignored;

        goal.Record(
            destination,
            tick,
            MoveIntentSource::Manual,
            generation,
            action == ManualRouteAction::Defer);
        if (action == ManualRouteAction::Defer) {
            safeManualAdoption.Clear();
            adoptionDeadlineTick = 0;
            deferred.Record(
                destination,
                tick,
                MoveIntentSource::Manual,
                generation,
                true);
            return MoveIntentRecordResult::Deferred;
        }

        deferred.Clear();
        safeManualAdoption.Record(
            destination,
            tick,
            MoveIntentSource::Manual,
            generation,
            false);
        adoptionDeadlineTick = SafeManualAdoptionDeadlineTick(
            tick,
            pingMs);
        return MoveIntentRecordResult::SafeManual;
    }

    MoveIntentRecordResult Record(const Vec2& destination,
                                  MoveIntentSource source,
                                  int tick,
                                  std::uint64_t generation,
                                  bool safetyBlocked) {
        ExpireAdoption(tick);
        if (!destination.IsValid() ||
            destination.IsZero() ||
            source == MoveIntentSource::Controller ||
            source == MoveIntentSource::Manual) {
            return MoveIntentRecordResult::Ignored;
        }

        if (HasManual()) return MoveIntentRecordResult::Ignored;
        if (source == MoveIntentSource::ObservedPath &&
            deferred.HasValue()) {
            return MoveIntentRecordResult::Ignored;
        }

        goal.Record(
            destination,
            tick,
            source,
            generation,
            safetyBlocked);
        if (safetyBlocked) {
            deferred.Record(
                destination,
                tick,
                source,
                generation,
                true);
            return MoveIntentRecordResult::Deferred;
        }
        deferred.Clear();
        return MoveIntentRecordResult::Accepted;
    }

    bool HasUnsafeManualDeferred() const {
        return deferred.HasValue() &&
            deferred.Source() == MoveIntentSource::Manual;
    }
    bool HasSafeManualAdoption() const {
        return safeManualAdoption.HasValue();
    }
    bool HasManual() const {
        return HasUnsafeManualDeferred() ||
            HasSafeManualAdoption();
    }
    bool HasDeferred() const { return deferred.HasValue(); }
    bool HasGoal() const { return goal.HasValue(); }
    const DeferredDestination& Manual() const {
        return safeManualAdoption.HasValue()
            ? safeManualAdoption
            : deferred;
    }
    const DeferredDestination& Deferred() const { return deferred; }
    DeferredDestination& Deferred() { return deferred; }
    const DeferredDestination& Goal() const { return goal; }
    int AdoptionDeadlineTick() const { return adoptionDeadlineTick; }

    bool BlocksControllerTarget() const {
        return safeManualAdoption.HasValue();
    }

    bool IsManualEcho(const Vec2& destination,
                      int tick,
                      int maximumAgeMs = 150) const {
        if (!HasManual() ||
            (safeManualAdoption.HasValue() &&
             tick >= adoptionDeadlineTick)) {
            return false;
        }
        const DeferredDestination& activeManual = Manual();
        return
            destination.IsValid() &&
            !destination.IsZero() &&
            tick >= activeManual.Tick() &&
            (activeManual.SafetyBlocked() ||
             TickDifference(tick, activeManual.Tick()) <=
                 std::max(0, maximumAgeMs)) &&
            destination.DistanceSqr(
                activeManual.Position()) <= 3600.0f;
    }

    bool AdoptObservedPath(const Vec2& pathEnd, int tick) {
        ExpireAdoption(tick);
        if (!safeManualAdoption.HasValue() ||
            !pathEnd.IsValid() ||
            pathEnd.IsZero() ||
            pathEnd.DistanceSqr(
                safeManualAdoption.Position()) > 6400.0f) {
            return false;
        }
        safeManualAdoption.Clear();
        adoptionDeadlineTick = 0;
        return true;
    }

    bool ExpireAdoption(int tick) {
        if (!safeManualAdoption.HasValue() ||
            tick < adoptionDeadlineTick) {
            return false;
        }
        safeManualAdoption.Clear();
        adoptionDeadlineTick = 0;
        return true;
    }

    void CompleteDeferredResume() {
        deferred.Clear();
    }

    void ClearDeferred() { deferred.Clear(); }

    void Clear() {
        safeManualAdoption.Clear();
        deferred.Clear();
        goal.Clear();
        adoptionDeadlineTick = 0;
    }

private:
    DeferredDestination safeManualAdoption;
    DeferredDestination deferred;
    DeferredDestination goal;
    int adoptionDeadlineTick = 0;
};

} // namespace ZDEvade
