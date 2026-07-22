#pragma once

#include "EvadeTypes.h"
#include "EvadeRoutingPolicy.h"
#include "EvadeMath.h"
#include "../../../Core/CoreNavGrid.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cfloat>
#include <vector>

namespace ZDEvade {

class EvadeGeometry {
public:
    struct ContinuousCollisionResult {
        bool collides = false;
        float firstContactMs = FLT_MAX;
        float minimumClearance = FLT_MAX;
        float exposureMs = 0.0f;
    };

    static float DistanceToSegment(const Vec2& point,
                                   const Vec2& start,
                                   const Vec2& end,
                                   bool* onSegment = nullptr,
                                   Vec2* projectionOut = nullptr) {
        return EvadeGeometryMath::DistanceToSegment(point, start, end, onSegment, projectionOut);
    }

    static Vec2 Rotate(const Vec2& value, float radians) {
        return EvadeGeometryMath::Rotate(value, radians);
    }

    static float SignedAngle(const Vec2& from, const Vec2& to) {
        return EvadeGeometryMath::SignedAngle(from, to);
    }

    static bool ThreatActiveAt(const Threat& threat, int tick) {
        return ThreatBodyActiveAt(threat, tick) || EndExplosionActiveAt(threat, tick);
    }

    static int ImpactTickAt(const Threat& threat, const Vec2& point) {
        if (threat.Type() == ZDSpellType::Line && threat.HasTravelSpeed()) {
            bool onSegment = false;
            Vec2 projection;
            DistanceToSegment(point, threat.startPos, threat.endPos, &onSegment, &projection);
            return threat.ArrivalTickAt(projection);
        }
        if (threat.Type() == ZDSpellType::Circular && threat.HasTravelSpeed()) {
            return threat.ArrivalTick();
        }
        return SaturatingTickAdd(threat.startTick, threat.Delay());
    }

    static float TimeMarginAt(const Threat& threat,
                              const Vec2& point,
                              int arrivalTick) {
        return static_cast<float>(
            TickDifference(ImpactTickAt(threat, point), arrivalTick));
    }

    static bool ContainsAt(const Threat& threat,
                           const Vec2& point,
                           float heroRadius,
                           float extraBuffer,
                           int tick) {
        if (!threat.HasValidGeometry()) return true;
        if (EndExplosionContainsAt(threat, point, heroRadius, extraBuffer, tick)) return true;
        // No exact arc geometry is implemented. An Arc reaching geometry is
        // therefore unsafe by default; detector admission prevents this path
        // during normal operation.
        if (threat.Type() == ZDSpellType::Arc) return true;
        return ThreatBodyContainsAt(
            threat,
            point,
            heroRadius,
            extraBuffer,
            tick);
    }

    static bool OccupiesAt(const Threat& threat,
                           const Vec2& point,
                           float heroRadius,
                           float extraBuffer,
                           int tick) {
        if (!threat.HasValidGeometry()) return true;
        if (EndExplosionContainsAt(threat, point, heroRadius, extraBuffer, tick)) return true;
        if (threat.Type() == ZDSpellType::Line && threat.HasTravelSpeed()) {
            if (!ThreatBodyActiveAt(threat, tick)) return false;
            const float radius = std::max(
                0.0f,
                threat.Radius() + heroRadius + extraBuffer + threat.PositionUncertainty());
            return point.Distance(threat.HeadAtTick(tick)) <= radius;
        }
        return ContainsAt(threat, point, heroRadius, extraBuffer, tick);
    }

    static Vec2 ClosestLineExit(const Threat& threat,
                                const Vec2& heroPos,
                                float heroRadius,
                                float extraBuffer,
                                bool left,
                                int now) {
        const Vec2 head = threat.HeadAtTick(now);
        Vec2 projection;
        DistanceToSegment(heroPos, head, threat.endPos, nullptr, &projection);
        Vec2 away = heroPos - projection;
        if (away.LengthSqr() < 1.0f) {
            const Vec2 normal(-threat.direction.y, threat.direction.x);
            away = left ? normal : normal * -1.0f;
        } else {
            away = away.Normalized();
            if (!left) away = away * -1.0f;
        }
        const float distance = ExitCenterDistance(
            threat.Radius(),
            heroRadius,
            extraBuffer,
            threat.PositionUncertainty());
        return projection + away * distance;
    }

    static Vec2 ClosestCircleExit(const Threat& threat,
                                  const Vec2& heroPos,
                                  float heroRadius,
                                  float extraBuffer) {
        Vec2 direction = (heroPos - threat.endPos).Normalized();
        if (direction.IsZero()) direction = (heroPos - threat.startPos).Normalized();
        if (direction.IsZero()) direction = Vec2(1.0f, 0.0f);
        const float distance = ExitCenterDistance(
            threat.Radius(),
            heroRadius,
            extraBuffer,
            threat.PositionUncertainty());
        return threat.endPos + direction * distance;
    }

    static Vec2 ClosestRingExit(const Threat& threat,
                                const Vec2& heroPos,
                                float heroRadius,
                                float extraBuffer) {
        const float expansion = ExitCenterDistance(
            0.0f,
            heroRadius,
            extraBuffer,
            threat.PositionUncertainty());
        const float outerRadius = ExitCenterDistance(
            threat.Radius(),
            heroRadius,
            extraBuffer,
            threat.PositionUncertainty());
        const float innerRadius = std::max(0.0f, threat.InnerRadius() - expansion);
        Vec2 direction = (heroPos - threat.endPos).Normalized();
        if (direction.IsZero()) direction = (heroPos - threat.startPos).Normalized();
        if (direction.IsZero()) direction = Vec2(1.0f, 0.0f);
        const float distance = heroPos.Distance(threat.endPos);
        if (innerRadius > 2.0f && distance - innerRadius <= outerRadius - distance)
            return threat.endPos + direction * innerRadius;
        return threat.endPos + direction * outerRadius;
    }

    static void AddConeExits(const Threat& threat,
                             const Vec2& heroPos,
                             float heroRadius,
                             float extraBuffer,
                             std::vector<Vec2>& output) {
        if (!threat.HasValidConeAngle()) return;
        const Vec2 baseDirection = threat.direction.IsZero()
            ? (threat.endPos - threat.startPos).Normalized()
            : threat.direction;
        const float halfAngle = std::max(0.05f, threat.Angle() * 0.5f * kDegToRad);
        const Vec2 relative = heroPos - threat.startPos;
        const float distance = relative.Length();
        const float currentAngle = SignedAngle(baseDirection, relative);
        const float boundaryAngle = currentAngle >= 0.0f ? halfAngle : -halfAngle;
        const float expansion = ExitCenterDistance(
            threat.ConeEdgePadding(),
            heroRadius,
            extraBuffer,
            threat.PositionUncertainty());
        const float exitDistance = std::max({
            50.0f,
            distance,
            expansion,
        });
        const float angularBuffer = std::asin(std::clamp(
            expansion / exitDistance,
            0.0f,
            1.0f));
        const float outsideAngle = boundaryAngle +
            (boundaryAngle >= 0.0f ? angularBuffer : -angularBuffer);
        output.push_back(
            threat.startPos +
            Rotate(baseDirection, outsideAngle) * exitDistance);
        output.push_back(
            threat.startPos +
            Rotate(baseDirection, -outsideAngle) * exitDistance);
        for (int side = -1; side <= 1; side += 2) {
            const Vec2 boundaryDirection = Rotate(
                baseDirection,
                halfAngle * static_cast<float>(side));
            const float projection = std::clamp(
                relative.Dot(boundaryDirection),
                0.0f,
                threat.Range());
            const Vec2 outward = side > 0
                ? Vec2(-boundaryDirection.y, boundaryDirection.x)
                : Vec2(boundaryDirection.y, -boundaryDirection.x);
            output.push_back(
                threat.startPos + boundaryDirection * projection + outward * expansion);
        }
        Vec2 radial = relative.Normalized();
        if (radial.IsZero()) radial = baseDirection;
        output.push_back(threat.startPos + radial * (threat.Range() + expansion));
    }

    static bool WalkablePath(const Vec2& from,
                             const Vec2& to,
                             float planeY,
                             float heroRadius) {
        const std::array<Vec2, 2> path = {from, to};
        return WalkablePathImpl(path, planeY, heroRadius);
    }

    static bool WalkablePath(const std::vector<Vec2>& path,
                             float planeY,
                             float heroRadius) {
        return WalkablePathImpl(path, planeY, heroRadius);
    }

    static bool PointWalkable(const Vec2& point,
                              float planeY,
                              float heroRadius) {
        const CoreNavGrid::GridRef navGrid = CoreNavGrid::Get();
        if (!navGrid.IsValid() || !std::isfinite(planeY)) return false;
        const SweptCircleGridGeometry grid = {
            navGrid.minX,
            navGrid.minZ,
            navGrid.maxX,
            navGrid.maxZ,
            navGrid.cellSize,
            navGrid.width,
            navGrid.height,
        };
        return SweptCirclePointWalkable(
            point,
            heroRadius,
            grid,
            [&](int x, int y) {
                return navGrid.GetRawCellFlags(x, y);
            });
    }

    template <typename Path>
    static bool WalkablePathImpl(const Path& path,
                                 float planeY,
                                 float heroRadius) {
        if (path.size() < 2) return false;
        const CoreNavGrid::GridRef navGrid = CoreNavGrid::Get();
        if (!navGrid.IsValid() || !std::isfinite(planeY)) return false;
        const SweptCircleGridGeometry grid = {
            navGrid.minX,
            navGrid.minZ,
            navGrid.maxX,
            navGrid.maxZ,
            navGrid.cellSize,
            navGrid.width,
            navGrid.height,
        };
        return SweptCirclePathWalkable(
            path,
            heroRadius,
            grid,
            [&](int x, int y) {
                return navGrid.GetRawCellFlags(x, y);
            });
    }

    static bool ThreatensPointNowOrAtFutureImpact(
            const Threat& threat,
            const Vec2& point,
            float heroRadius,
            float buffer,
            int now,
            float horizonMs,
            int* firstThreatTick = nullptr) {
        if (ContainsAt(threat, point, heroRadius, buffer, now)) {
            if (firstThreatTick) *firstThreatTick = now;
            return true;
        }

        bool threatened = false;
        int firstTick = now;
        const auto recordFutureTick = [&](int tick) {
            if (!threatened || TickDifference(tick, firstTick) < 0)
                firstTick = tick;
            threatened = true;
        };
        const int impact = ImpactTickAt(threat, point);
        if (FutureTickWithinHorizon(impact, now, horizonMs) &&
            ContainsAt(threat, point, heroRadius, buffer, impact)) {
            recordFutureTick(impact);
        }
        if (threat.HasEndExplosionArea()) {
            const int explosionStart = threat.EndExplosionStartTick();
            if (FutureTickWithinHorizon(explosionStart, now, horizonMs) &&
                EndExplosionContainsAt(
                    threat, point, heroRadius, buffer, explosionStart)) {
                recordFutureTick(explosionStart);
            }
        }
        if (threatened && firstThreatTick) *firstThreatTick = firstTick;
        return threatened;
    }

    static bool EndExplosionThreatensPointWithinHorizon(
            const Threat& threat,
            const Vec2& point,
            float heroRadius,
            float buffer,
            int now,
            float horizonMs) {
        if (!threat.HasEndExplosionArea()) return false;
        if (EndExplosionContainsAt(threat, point, heroRadius, buffer, now))
            return true;
        const int explosionStart = threat.EndExplosionStartTick();
        return FutureTickWithinHorizon(explosionStart, now, horizonMs) &&
            EndExplosionContainsAt(
                threat, point, heroRadius, buffer, explosionStart);
    }

    static bool HeroThreatenedNow(const std::vector<Threat>& threats,
                                  const Vec2& heroPos,
                                  float heroRadius,
                                  float buffer,
                                  int now,
                                  float horizonMs) {
        const float safeHorizonMs = std::clamp(
            std::isfinite(horizonMs) ? horizonMs : kMaximumAnalysisHorizonMs,
            0.0f,
            kMaximumAnalysisHorizonMs);
        for (const auto& threat : threats) {
            if (threat.IsExpiredAt(now)) continue;
            if (!threat.HasValidGeometry()) return true;
            if (threat.Type() == ZDSpellType::Line && threat.HasTravelSpeed()) {
                if (EndExplosionThreatensPointWithinHorizon(
                        threat,
                        heroPos,
                        heroRadius,
                        buffer,
                        now,
                        safeHorizonMs)) return true;
                if (AnalyzeMovingLine(
                        threat,
                        heroPos,
                        heroPos,
                        50.0f,
                        0.0f,
                        heroRadius,
                        buffer,
                        now,
                        safeHorizonMs).collides) return true;
                continue;
            }
            if (ThreatensPointNowOrAtFutureImpact(
                    threat,
                    heroPos,
                    heroRadius,
                    buffer,
                    now,
                    safeHorizonMs)) return true;
        }
        return false;
    }

    static ContinuousCollisionResult AnalyzeBlinkOrigin(
        const Threat& threat,
        const Vec2& origin,
        float heroRadius,
        int now,
        float castDelayMs,
        const EvadeSettings& settings) {
        ContinuousCollisionResult result;
        const int completionTick = SaturatingTickAdd(
            now,
            ClampTickOffset(std::round(std::max(0.0f, castDelayMs))));
        if (!TickAfter(completionTick, now)) return result;

        const int horizonEndTick = SaturatingTickAdd(
            now,
            ClampTickOffset(std::floor(
                std::max(0.0f, settings.maxThreatHorizonMs))));
        const int originEndTick = std::min(
            SaturatingTickAdd(completionTick, -1), horizonEndTick);
        if (originEndTick < now) return result;
        const float originEndMs =
            static_cast<float>(TickDifference(originEndTick, now));
        return AnalyzeThreatPath(
            threat,
            origin,
            origin,
            50.0f,
            0.0f,
            heroRadius,
            settings.pathBuffer,
            now,
            0.0f,
            originEndMs,
            false);
    }

    static CandidateEvaluation EvaluateBlinkCandidate(
        const Vec2& candidate,
        const Vec2& heroPos,
        const Vec2& cursorPos,
        float planeY,
        float heroRadius,
        int now,
        float castDelayMs,
        const EvadeSettings& settings,
        const std::vector<Threat>& threats) {
        CandidateEvaluation result;
        result.position = candidate;
        result.source = PlannerCandidateSource::EvadeSpell;
        result.travelDistance = heroPos.Distance(candidate);
        result.exitDistance =
            std::numeric_limits<float>::infinity();
        result.arrivalTimeMs = std::max(0.0f, castDelayMs);
        result.cursorDistance = candidate.Distance(cursorPos);
        result.valid = candidate.IsValid() && !candidate.IsZero() && result.travelDistance >= 1.0f;
        if (!result.valid) return result;
        // Geometry-only tests use a zero collision radius. Keep the runtime
        // nav query non-degenerate without changing collision calculations.
        const float navValidationRadius = heroRadius == 0.0f
            ? kZeroRadiusNavValidationEpsilon
            : heroRadius;
        result.walkable =
            PointWalkable(candidate, planeY, navValidationRadius);
        if (!result.walkable) {
            result.rejectReason = PlannerRejectReason::Wall;
            return result;
        }

        const int arrivalTick = SaturatingTickAdd(
            now, ClampTickOffset(std::round(result.arrivalTimeMs)));
        float firstCollisionMs = FLT_MAX;
        CollisionIdentitySet collisionIdentities;
        CollisionIdentitySet pathCollisionIdentities;
        CollisionIdentitySet endpointCollisionIdentities;
        std::vector<IdentityExposureAccumulator>
            originExposureAccumulators;
        ExposureDangerSet exposureDangers;
        for (std::size_t threatIndex = 0;
             threatIndex < threats.size();
             ++threatIndex) {
            const Threat& threat = threats[threatIndex];
            if (threat.IsExpiredAt(now)) continue;
            const CollisionIdentity identity =
                MakeCollisionIdentity(threat.id, threatIndex);
            const int originImpactTick =
                ImpactTickAt(threat, heroPos);
            const bool originPredictedEnvelope =
                TickDifference(originImpactTick, now) <=
                    static_cast<int>(
                        settings.maxThreatHorizonMs) &&
                TickDifference(originImpactTick, now) >= -100 &&
                ContainsAt(
                    threat,
                    heroPos,
                    heroRadius,
                    settings.pathBuffer,
                    std::max(now, originImpactTick));
            if (originPredictedEnvelope ||
                OccupiesAt(
                    threat,
                    heroPos,
                    heroRadius,
                    settings.pathBuffer,
                    now)) {
                result.startThreatIdentities.Add(identity);
            }
            const ContinuousCollisionResult originCollision = AnalyzeBlinkOrigin(
                threat,
                heroPos,
                heroRadius,
                now,
                result.arrivalTimeMs,
                settings);
            result.minimumClearance = std::min(
                result.minimumClearance,
                originCollision.minimumClearance);
            const bool castCollision = originCollision.collides;
            if (castCollision) {
                firstCollisionMs = std::min(
                    firstCollisionMs,
                    originCollision.firstContactMs);
            }
            if (originCollision.exposureMs > 0.0f) {
                std::size_t exposureIndex =
                    ExposureAccumulatorIndex(
                        originExposureAccumulators,
                        identity);
                if (exposureIndex ==
                    originExposureAccumulators.size()) {
                    originExposureAccumulators.push_back(
                        {identity, threat.Danger()});
                    exposureIndex =
                        originExposureAccumulators.size() - 1;
                }
                IdentityExposureAccumulator& exposure =
                    originExposureAccumulators[exposureIndex];
                exposure.danger = std::max(
                    exposure.danger,
                    threat.Danger());
                exposure.exactExplosionMs = std::max(
                    exposure.exactExplosionMs,
                    originCollision.exposureMs);
            }

            const int explosionTick = threat.EndExplosionStartTick();
            const int explosionEndTick =
                CanonicalEndExplosionEndTick(threat);
            const float explosionRadius = threat.EndExplosionRadius() +
                std::max(0.0f, heroRadius) +
                std::max(0.0f, settings.endpointBuffer) +
                threat.PositionUncertainty();
            bool endpointCollision = threat.HasEndExplosionArea() &&
                TickDifference(explosionTick, now) <=
                    static_cast<int>(settings.maxThreatHorizonMs) &&
                explosionEndTick >= arrivalTick &&
                candidate.Distance(threat.EndExplosionCenter()) <= explosionRadius;
            float endpointFirstCollisionMs = endpointCollision
                ? std::max(
                    result.arrivalTimeMs,
                    static_cast<float>(TickDifference(explosionTick, now)))
                : FLT_MAX;
            if (threat.Type() == ZDSpellType::Line && threat.HasTravelSpeed()) {
                const ContinuousCollisionResult collision = AnalyzeMovingLine(
                    threat,
                    candidate,
                    candidate,
                    50.0f,
                    0.0f,
                    heroRadius,
                    settings.endpointBuffer,
                    now,
                    settings.maxThreatHorizonMs,
                    result.arrivalTimeMs);
                result.minimumClearance = std::min(
                    result.minimumClearance,
                    collision.minimumClearance);
                endpointCollision = endpointCollision || collision.collides;
                if (collision.collides) {
                    endpointFirstCollisionMs = std::min(
                        endpointFirstCollisionMs,
                        collision.firstContactMs);
                    firstCollisionMs = std::min(firstCollisionMs, collision.firstContactMs);
                }
            } else {
                const int impactTick = ImpactTickAt(threat, candidate);
                if (TickDifference(impactTick, now) <=
                    static_cast<int>(settings.maxThreatHorizonMs)) {
                    if (ContainsAt(
                            threat,
                            candidate,
                            heroRadius,
                            settings.endpointBuffer,
                            arrivalTick)) {
                        endpointCollision = true;
                        endpointFirstCollisionMs = std::min(
                            endpointFirstCollisionMs,
                            result.arrivalTimeMs);
                    }
                    if (impactTick >= arrivalTick && ContainsAt(
                        threat,
                        candidate,
                        heroRadius,
                        settings.endpointBuffer,
                        impactTick)) {
                        endpointCollision = true;
                        endpointFirstCollisionMs = std::min(
                            endpointFirstCollisionMs,
                            static_cast<float>(TickDifference(impactTick, now)));
                    }
                }
            }
            if (castCollision && pathCollisionIdentities.Add(identity))
                result.pathDanger += threat.Danger();
            if (endpointCollision &&
                endpointCollisionIdentities.Add(identity)) {
                result.endpointDanger += threat.Danger();
            }
            if (!castCollision && !endpointCollision) continue;
            firstCollisionMs = std::min(
                firstCollisionMs,
                endpointFirstCollisionMs);
            result.maxDanger = std::max(result.maxDanger, threat.Danger());
            collisionIdentities.Add(identity);
        }
        for (const IdentityExposureAccumulator& exposure :
             originExposureAccumulators) {
            result.dangerExposureMs +=
                static_cast<float>(exposure.danger) *
                exposure.exactExplosionMs;
            exposureDangers.AddOrMax(
                exposure.identity,
                exposure.danger);
        }
        const CandidateEvaluation destinationExposure =
            EvaluateStationaryPathCandidateCore(
                candidate,
                cursorPos,
                heroRadius,
                now,
                settings,
                threats,
                result.arrivalTimeMs,
                nullptr,
                &exposureDangers);
        result.dangerExposureMs +=
            destinationExposure.dangerExposureMs;
        result.endThreatIdentities.AddAll(
            destinationExposure.startThreatIdentities);
        result.endThreatIdentities.AddAll(
            endpointCollisionIdentities);
        result.exitedStartEnvelope =
            result.startThreatIdentities.Size() > 0 &&
            !result.endThreatIdentities.Intersects(
                result.startThreatIdentities);
        result.exitDistance =
            result.startThreatIdentities.Size() == 0
            ? 0.0f
            : result.exitedStartEnvelope
                ? result.travelDistance
                : std::numeric_limits<float>::infinity();
        result.summedExposureDanger =
            exposureDangers.SummedDanger();
        result.collisionCount =
            static_cast<int>(collisionIdentities.Size());
        if (result.minimumClearance == FLT_MAX) result.minimumClearance = settings.maxSearchRadius;
        result.pathSafe = result.pathDanger == 0;
        result.endpointSafe = result.endpointDanger == 0;
        result.firstCollisionTimeMs = firstCollisionMs;
        result.timeMarginMs = firstCollisionMs == FLT_MAX
            ? settings.maxThreatHorizonMs
            : firstCollisionMs;
        result.timingSafe = firstCollisionMs == FLT_MAX ||
            firstCollisionMs >= settings.minimumTimeMarginMs;
        result.strictSafe =
            result.pathSafe &&
            result.endpointSafe &&
            result.timingSafe &&
            (result.startThreatIdentities.Size() == 0 ||
             result.exitedStartEnvelope);
        result.rejectReason = result.strictSafe
            ? PlannerRejectReason::None
            : !result.endpointSafe
                ? PlannerRejectReason::EndpointDanger
                : !result.pathSafe ? PlannerRejectReason::PathDanger : PlannerRejectReason::Late;
        return result;
    }

    static CandidateEvaluation EvaluateCandidate(
        const Vec2& candidate,
        PlannerCandidateSource source,
        int sourceThreatId,
        const Vec2& heroPos,
        const Vec2& cursorPos,
        float planeY,
        float moveSpeed,
        float heroRadius,
        int now,
        const EvadeSettings& settings,
        const std::vector<Threat>& threats,
        CollisionIdentitySet* collisionIdentitiesOut = nullptr,
        bool includeEndpointCoverage = true) {
        return EvaluateCandidateCore(
            candidate,
            source,
            sourceThreatId,
            heroPos,
            cursorPos,
            planeY,
            moveSpeed,
            heroRadius,
            now,
            settings,
            threats,
            collisionIdentitiesOut,
            includeEndpointCoverage,
            NavValidationMode::ValidateSegment,
            nullptr);
    }

    static CandidateEvaluation EvaluateStationaryCandidate(
        const Vec2& position,
        const Vec2& cursorPos,
        float planeY,
        float heroRadius,
        int now,
        const EvadeSettings& settings,
        const std::vector<Threat>& threats,
        CollisionIdentitySet* collisionIdentitiesOut = nullptr) {
        (void)planeY;
        return EvaluateStationaryCandidateCore(
            position,
            cursorPos,
            heroRadius,
            now,
            settings,
            threats,
            0.0f,
            collisionIdentitiesOut,
            nullptr);
    }

private:
    enum class NavValidationMode {
        ValidateSegment,
        AlreadyValidatedPolyline,
    };

    struct IdentityExposureAccumulator {
        CollisionIdentity identity;
        int danger = 0;
        float sampledUnionMs = 0.0f;
        float sampledExplosionMs = 0.0f;
        float exactExplosionMs = 0.0f;
        bool previousUnion = false;
        bool previousExplosion = false;
    };

    struct TimeInterval {
        float beginMs = 0.0f;
        float endMs = 0.0f;
    };

    struct StationaryIdentityAccumulator {
        CollisionIdentity identity;
        int danger = 0;
        std::vector<TimeInterval> intervals;
    };

    static bool CircleInsideParameterInterval(
        const Vec2& start,
        const Vec2& end,
        const Vec2& center,
        float radius,
        float* enterOut,
        float* leaveOut) {
        const Vec2 delta = end - start;
        const Vec2 offset = start - center;
        const double a = static_cast<double>(delta.x) * delta.x +
            static_cast<double>(delta.y) * delta.y;
        const double halfB = static_cast<double>(offset.x) * delta.x +
            static_cast<double>(offset.y) * delta.y;
        const double safeRadius =
            std::max(0.0, static_cast<double>(radius));
        const double c = static_cast<double>(offset.x) * offset.x +
            static_cast<double>(offset.y) * offset.y -
            safeRadius * safeRadius;
        const double tolerance =
            1.0e-9 * std::max(1.0, safeRadius * safeRadius);
        if (a <= 1.0e-12) {
            if (c > tolerance) return false;
            if (enterOut) *enterOut = 0.0f;
            if (leaveOut) *leaveOut = 1.0f;
            return true;
        }
        double discriminant = halfB * halfB - a * c;
        const double discriminantTolerance = 1.0e-12 *
            std::max(1.0, halfB * halfB + std::fabs(a * c));
        if (discriminant < -discriminantTolerance) return false;
        discriminant = std::max(0.0, discriminant);
        const double root = std::sqrt(discriminant);
        const double enter = std::clamp(
            (-halfB - root) / a, 0.0, 1.0);
        const double leave = std::clamp(
            (-halfB + root) / a, 0.0, 1.0);
        if (leave < enter) return false;
        if (enterOut) *enterOut = static_cast<float>(enter);
        if (leaveOut) *leaveOut = static_cast<float>(leave);
        return true;
    }

    static void AddStationaryInterval(
        std::vector<TimeInterval>& intervals,
        float beginMs,
        float endMs,
        float analysisStartMs,
        float analysisEndMs) {
        const float begin = std::max(beginMs, analysisStartMs);
        const float end = std::min(endMs, analysisEndMs);
        if (!std::isfinite(begin) ||
            !std::isfinite(end) ||
            end < begin) {
            return;
        }
        intervals.push_back({begin, end});
    }

    static std::vector<TimeInterval> StationaryThreatIntervals(
        const Threat& threat,
        const Vec2& position,
        float heroRadius,
        float buffer,
        int now,
        float analysisStartMs,
        float analysisEndMs) {
        std::vector<TimeInterval> intervals;
        if (threat.IsExpiredAt(now) ||
            analysisEndMs < analysisStartMs) {
            return intervals;
        }

        if (threat.Type() == ZDSpellType::Arc) {
            AddStationaryInterval(
                intervals,
                analysisStartMs,
                analysisEndMs,
                analysisStartMs,
                analysisEndMs);
        } else if (threat.Type() == ZDSpellType::Line &&
                   threat.HasTravelSpeed() &&
                   !threat.projectileTerminated) {
            const float radius = std::max(
                0.0f,
                threat.Radius() +
                    heroRadius +
                    buffer +
                    threat.PositionUncertainty());
            const float travelStartMs = static_cast<float>(
                TickDifference(TravelStartTick(threat), now));
            const float travelEndMs = static_cast<float>(
                TickDifference(TravelEndTick(threat), now));
            const float terminalEndMs =
                (threat.missileBound && !threat.projectileTerminated) ||
                    threat.persistent
                ? analysisEndMs
                : static_cast<float>(TickDifference(
                    threat.MovingLineTerminalActiveEndTick(),
                    now));
            std::array<float, 4> boundaries = {
                analysisStartMs,
                analysisEndMs,
                std::clamp(
                    travelStartMs,
                    analysisStartMs,
                    analysisEndMs),
                std::clamp(
                    travelEndMs,
                    analysisStartMs,
                    analysisEndMs),
            };
            std::sort(boundaries.begin(), boundaries.end());
            for (std::size_t index = 0;
                 index + 1 < boundaries.size();
                 ++index) {
                const float begin = std::max(
                    boundaries[index],
                    travelStartMs);
                const float end = std::min(
                    boundaries[index + 1],
                    terminalEndMs);
                if (end < begin) continue;
                const Vec2 headBegin =
                    MovingLineHeadAtTime(threat, now, begin);
                const Vec2 headEnd =
                    MovingLineHeadAtTime(threat, now, end);
                float enter = 0.0f;
                float leave = 0.0f;
                if (!CircleInsideParameterInterval(
                        headBegin,
                        headEnd,
                        position,
                        radius,
                        &enter,
                        &leave)) {
                    continue;
                }
                const float duration = end - begin;
                AddStationaryInterval(
                    intervals,
                    begin + duration * enter,
                    begin + duration * leave,
                    analysisStartMs,
                    analysisEndMs);
            }
        } else {
            int activeStartTick =
                SaturatingTickAdd(threat.startTick, threat.Delay());
            int activeEndTick = threat.persistent
                ? SaturatingTickAdd(
                    now,
                    ClampTickOffset(std::ceil(analysisEndMs)))
                : SaturatingTickAdd(
                    activeStartTick,
                    std::max(100, threat.ExtraEndTime()));
            if (threat.projectileTerminated) {
                if (threat.Type() != ZDSpellType::Circular ||
                    threat.ExtraEndTime() <= 0) {
                    activeEndTick = SaturatingTickAdd(activeStartTick, -1);
                } else {
                    activeStartTick = threat.projectileTerminationTick;
                    activeEndTick = SaturatingTickAdd(
                        activeStartTick,
                        threat.ExtraEndTime());
                }
            } else if (
                threat.Type() == ZDSpellType::Circular &&
                threat.HasTravelSpeed()) {
                activeStartTick = ImpactTickAt(threat, threat.endPos);
                if (!threat.persistent) {
                    activeEndTick = SaturatingTickAdd(
                        activeStartTick,
                        std::max(100, threat.ExtraEndTime()));
                }
            }
            const float activeStartMs = static_cast<float>(
                TickDifference(activeStartTick, now));
            const float activeEndMs = static_cast<float>(
                TickDifference(activeEndTick, now));
            const float probeMs = std::clamp(
                activeStartMs,
                analysisStartMs,
                analysisEndMs);
            const int probeTick = SaturatingTickAdd(
                now,
                ClampTickOffset(std::round(probeMs)));
            if (activeEndMs >= activeStartMs &&
                ThreatBodyContainsAt(
                    threat,
                    position,
                    heroRadius,
                    buffer,
                    probeTick)) {
                AddStationaryInterval(
                    intervals,
                    activeStartMs,
                    activeEndMs,
                    analysisStartMs,
                    analysisEndMs);
            }
        }

        if (threat.HasEndExplosionArea()) {
            const float explosionStartMs = static_cast<float>(
                TickDifference(
                    threat.EndExplosionStartTick(),
                    now));
            const float explosionEndMs = static_cast<float>(
                TickDifference(
                    CanonicalEndExplosionEndTick(threat),
                    now));
            const float radius = std::max(
                0.0f,
                threat.EndExplosionRadius() +
                    heroRadius +
                    buffer +
                    threat.PositionUncertainty());
            if (position.Distance(
                    threat.EndExplosionCenter()) <= radius) {
                AddStationaryInterval(
                    intervals,
                    explosionStartMs,
                    explosionEndMs,
                    analysisStartMs,
                    analysisEndMs);
            }
        }
        return intervals;
    }

    static void MergeStationaryIntervals(
        std::vector<TimeInterval>& intervals) {
        if (intervals.empty()) return;
        std::sort(
            intervals.begin(),
            intervals.end(),
            [](const TimeInterval& left, const TimeInterval& right) {
                if (left.beginMs != right.beginMs)
                    return left.beginMs < right.beginMs;
                return left.endMs < right.endMs;
            });
        std::size_t output = 0;
        for (std::size_t index = 1;
             index < intervals.size();
             ++index) {
            if (intervals[index].beginMs <=
                intervals[output].endMs + 0.001f) {
                intervals[output].endMs = std::max(
                    intervals[output].endMs,
                    intervals[index].endMs);
                continue;
            }
            ++output;
            intervals[output] = intervals[index];
        }
        intervals.resize(output + 1);
    }

    static std::size_t StationaryAccumulatorIndex(
        const std::vector<StationaryIdentityAccumulator>& accumulators,
        const CollisionIdentity& identity) {
        for (std::size_t index = 0;
             index < accumulators.size();
             ++index) {
            if (accumulators[index].identity == identity) return index;
        }
        return accumulators.size();
    }

    static CandidateEvaluation EvaluateStationaryPathCandidateCore(
        const Vec2& position,
        const Vec2& cursorPos,
        float heroRadius,
        int now,
        const EvadeSettings& settings,
        const std::vector<Threat>& threats,
        float analysisStartMs,
        CollisionIdentitySet* collisionIdentitiesOut,
        ExposureDangerSet* exposureDangersOut) {
        CandidateEvaluation result;
        result.position = position;
        result.source = PlannerCandidateSource::Unknown;
        result.cursorDistance = position.Distance(cursorPos);
        result.travelDistance = 0.0f;
        result.arrivalTimeMs = 0.0f;
        result.valid = position.IsValid() && !position.IsZero();
        result.walkable = result.valid;
        if (!result.valid) return result;

        const float horizon = std::clamp(
            std::isfinite(settings.maxThreatHorizonMs)
                ? settings.maxThreatHorizonMs
                : kMaximumAnalysisHorizonMs,
            0.0f,
            kMaximumAnalysisHorizonMs);
        const float start = std::clamp(
            std::isfinite(analysisStartMs) ? analysisStartMs : 0.0f,
            0.0f,
            horizon);
        std::vector<StationaryIdentityAccumulator> pathAccumulators;
        pathAccumulators.reserve(threats.size());

        for (std::size_t threatIndex = 0;
             threatIndex < threats.size();
             ++threatIndex) {
            const Threat& threat = threats[threatIndex];
            if (threat.IsExpiredAt(now)) continue;
            const CollisionIdentity identity =
                MakeCollisionIdentity(threat.id, threatIndex);
            std::size_t index =
                StationaryAccumulatorIndex(pathAccumulators, identity);
            if (index == pathAccumulators.size()) {
                pathAccumulators.push_back(
                    {identity, threat.Danger(), {}});
                index = pathAccumulators.size() - 1;
            } else {
                pathAccumulators[index].danger = std::max(
                    pathAccumulators[index].danger,
                    threat.Danger());
            }
            std::vector<TimeInterval> intervals =
                StationaryThreatIntervals(
                    threat,
                    position,
                    heroRadius,
                    settings.pathBuffer,
                    now,
                    start,
                    horizon);
            pathAccumulators[index].intervals.insert(
                pathAccumulators[index].intervals.end(),
                intervals.begin(),
                intervals.end());
            const ContinuousCollisionResult clearance =
                AnalyzeThreatPath(
                    threat,
                    position,
                    position,
                    50.0f,
                    0.0f,
                    heroRadius,
                    settings.pathBuffer,
                    now,
                    start,
                    horizon,
                    false);
            result.minimumClearance = std::min(
                result.minimumClearance,
                clearance.minimumClearance);
        }

        struct DangerEvent {
            float timeMs = 0.0f;
            int delta = 0;
        };
        std::vector<DangerEvent> events;
        CollisionIdentitySet collisionIdentities;
        ExposureDangerSet exposureDangers;
        for (StationaryIdentityAccumulator& accumulator :
             pathAccumulators) {
            MergeStationaryIntervals(accumulator.intervals);
            if (accumulator.intervals.empty()) continue;
            collisionIdentities.Add(accumulator.identity);
            result.encounteredCollisionIdentities.Add(
                accumulator.identity);
            result.encounteredEnvelopeIdentities.Add(
                accumulator.identity);
            if (accumulator.intervals.front().beginMs <=
                start + 0.001f) {
                result.startThreatIdentities.Add(
                    accumulator.identity);
            }
            if (accumulator.intervals.back().endMs + 0.001f >=
                horizon) {
                result.endThreatIdentities.Add(
                    accumulator.identity);
            }
            result.maxDanger = std::max(
                result.maxDanger,
                accumulator.danger);
            for (const TimeInterval& interval :
                 accumulator.intervals) {
                result.firstCollisionTimeMs = std::min(
                    result.firstCollisionTimeMs,
                    interval.beginMs);
                const float exposureDurationMs =
                    std::max(0.0f, interval.endMs - interval.beginMs);
                result.dangerExposureMs +=
                    accumulator.danger * exposureDurationMs;
                if (exposureDurationMs > 0.0f) {
                    exposureDangers.AddOrMax(
                        accumulator.identity,
                        accumulator.danger);
                }
                events.push_back(
                    {interval.beginMs, accumulator.danger});
                events.push_back(
                    {interval.endMs, -accumulator.danger});
            }
        }
        std::sort(
            events.begin(),
            events.end(),
            [](const DangerEvent& left, const DangerEvent& right) {
                if (left.timeMs != right.timeMs)
                    return left.timeMs < right.timeMs;
                return left.delta > right.delta;
            });
        int activeDanger = 0;
        for (const DangerEvent& event : events) {
            activeDanger += event.delta;
            result.pathDanger = std::max(
                result.pathDanger,
                activeDanger);
        }

        result.collisionCount =
            static_cast<int>(collisionIdentities.Size());
        if (collisionIdentitiesOut)
            collisionIdentitiesOut->AddAll(collisionIdentities);
        if (exposureDangersOut)
            exposureDangersOut->AddAll(exposureDangers);
        result.summedExposureDanger =
            exposureDangers.SummedDanger();
        result.enteredNewThreat =
            result.encounteredCollisionIdentities.AnyNotIn(
                result.startThreatIdentities) ||
            result.encounteredEnvelopeIdentities.AnyNotIn(
                result.startThreatIdentities);
        if (result.minimumClearance == FLT_MAX)
            result.minimumClearance = settings.maxSearchRadius;
        result.pathSafe = result.pathDanger == 0;
        result.endpointSafe = true;
        result.exitDistance =
            result.startThreatIdentities.Size() == 0
            ? 0.0f
            : std::numeric_limits<float>::infinity();
        result.timeMarginMs =
            result.firstCollisionTimeMs == FLT_MAX
                ? horizon
                : result.firstCollisionTimeMs;
        result.timingSafe =
            result.firstCollisionTimeMs == FLT_MAX ||
            result.firstCollisionTimeMs >=
                settings.minimumTimeMarginMs;
        result.strictSafe =
            result.pathSafe &&
            result.timingSafe &&
            !result.enteredNewThreat &&
            !result.reenteredDanger;
        result.rejectReason = result.strictSafe
            ? PlannerRejectReason::None
            : !result.pathSafe
                ? PlannerRejectReason::PathDanger
                : PlannerRejectReason::Late;
        return result;
    }

    static CandidateEvaluation EvaluateStationaryCandidateCore(
        const Vec2& position,
        const Vec2& cursorPos,
        float heroRadius,
        int now,
        const EvadeSettings& settings,
        const std::vector<Threat>& threats,
        float analysisStartMs,
        CollisionIdentitySet* collisionIdentitiesOut,
        ExposureDangerSet* exposureDangersOut) {
        CollisionIdentitySet collisionIdentities;
        CandidateEvaluation result =
            EvaluateStationaryPathCandidateCore(
                position,
                cursorPos,
                heroRadius,
                now,
                settings,
                threats,
                analysisStartMs,
                &collisionIdentities,
                exposureDangersOut);
        if (!result.valid) return result;

        const float horizon = std::clamp(
            std::isfinite(settings.maxThreatHorizonMs)
                ? settings.maxThreatHorizonMs
                : kMaximumAnalysisHorizonMs,
            0.0f,
            kMaximumAnalysisHorizonMs);
        const float start = std::clamp(
            std::isfinite(analysisStartMs) ? analysisStartMs : 0.0f,
            0.0f,
            horizon);
        std::vector<StationaryIdentityAccumulator> endpointAccumulators;
        endpointAccumulators.reserve(threats.size());
        for (std::size_t threatIndex = 0;
             threatIndex < threats.size();
             ++threatIndex) {
            const Threat& threat = threats[threatIndex];
            if (threat.IsExpiredAt(now)) continue;
            const CollisionIdentity identity =
                MakeCollisionIdentity(threat.id, threatIndex);
            std::size_t index =
                StationaryAccumulatorIndex(
                    endpointAccumulators,
                    identity);
            if (index == endpointAccumulators.size()) {
                endpointAccumulators.push_back(
                    {identity, threat.Danger(), {}});
                index = endpointAccumulators.size() - 1;
            } else {
                endpointAccumulators[index].danger = std::max(
                    endpointAccumulators[index].danger,
                    threat.Danger());
            }
            std::vector<TimeInterval> intervals =
                StationaryThreatIntervals(
                    threat,
                    position,
                    heroRadius,
                    settings.endpointBuffer,
                    now,
                    start,
                    horizon);
            endpointAccumulators[index].intervals.insert(
                endpointAccumulators[index].intervals.end(),
                intervals.begin(),
                intervals.end());
            const ContinuousCollisionResult clearance =
                AnalyzeThreatPath(
                    threat,
                    position,
                    position,
                    50.0f,
                    0.0f,
                    heroRadius,
                    settings.endpointBuffer,
                    now,
                    start,
                    horizon,
                    false);
            result.minimumClearance = std::min(
                result.minimumClearance,
                clearance.minimumClearance);
        }

        for (StationaryIdentityAccumulator& accumulator :
             endpointAccumulators) {
            MergeStationaryIntervals(accumulator.intervals);
            if (accumulator.intervals.empty()) continue;
            result.endpointDanger += accumulator.danger;
            result.maxDanger = std::max(
                result.maxDanger,
                accumulator.danger);
            collisionIdentities.Add(accumulator.identity);
            result.encounteredCollisionIdentities.Add(
                accumulator.identity);
            result.encounteredEnvelopeIdentities.Add(
                accumulator.identity);
            if (accumulator.intervals.front().beginMs <=
                start + 0.001f) {
                result.startThreatIdentities.Add(
                    accumulator.identity);
            }
            if (accumulator.intervals.back().endMs + 0.001f >=
                horizon) {
                result.endThreatIdentities.Add(
                    accumulator.identity);
            }
            result.firstCollisionTimeMs = std::min(
                result.firstCollisionTimeMs,
                accumulator.intervals.front().beginMs);
        }
        result.collisionCount =
            static_cast<int>(collisionIdentities.Size());
        if (collisionIdentitiesOut)
            collisionIdentitiesOut->AddAll(collisionIdentities);
        result.endpointSafe = result.endpointDanger == 0;
        result.enteredNewThreat =
            result.encounteredCollisionIdentities.AnyNotIn(
                result.startThreatIdentities) ||
            result.encounteredEnvelopeIdentities.AnyNotIn(
                result.startThreatIdentities);
        result.timeMarginMs =
            result.firstCollisionTimeMs == FLT_MAX
                ? horizon
                : result.firstCollisionTimeMs;
        result.timingSafe =
            result.firstCollisionTimeMs == FLT_MAX ||
            result.firstCollisionTimeMs >=
                settings.minimumTimeMarginMs;
        result.strictSafe =
            result.pathSafe &&
            result.endpointSafe &&
            result.timingSafe &&
            !result.enteredNewThreat &&
            !result.reenteredDanger;
        result.rejectReason = result.strictSafe
            ? PlannerRejectReason::None
            : !result.endpointSafe
                ? PlannerRejectReason::EndpointDanger
                : !result.pathSafe
                    ? PlannerRejectReason::PathDanger
                    : PlannerRejectReason::Late;
        return result;
    }

    static CandidateEvaluation EvaluateCandidateCore(
        const Vec2& candidate,
        PlannerCandidateSource source,
        int sourceThreatId,
        const Vec2& heroPos,
        const Vec2& cursorPos,
        float planeY,
        float moveSpeed,
        float heroRadius,
        int now,
        const EvadeSettings& settings,
        const std::vector<Threat>& threats,
        CollisionIdentitySet* collisionIdentitiesOut,
        bool includeEndpointCoverage,
        NavValidationMode navValidationMode,
        ExposureDangerSet* exposureDangersOut) {
        CandidateEvaluation result;
        result.position = candidate;
        result.source = source;
        result.sourceThreatId = sourceThreatId;
        result.cursorDistance = candidate.Distance(cursorPos);
        result.travelDistance = heroPos.Distance(candidate);
        const float speed = std::max(50.0f, moveSpeed);
        result.arrivalTimeMs = std::max(0.0f, settings.inputDelayMs) +
            1000.0f * result.travelDistance / speed;
        result.valid = candidate.IsValid() && !candidate.IsZero() && result.travelDistance >= 1.0f;
        if (!result.valid) {
            result.rejectReason = PlannerRejectReason::Invalid;
            return result;
        }
        // Geometry-only callers use a zero collision radius. Keep navigation
        // validation non-degenerate without inflating collision calculations.
        const float navValidationRadius =
            heroRadius == 0.0f
                ? kZeroRadiusNavValidationEpsilon
                : heroRadius;
        result.walkable =
            navValidationMode ==
                NavValidationMode::AlreadyValidatedPolyline ||
            WalkablePath(
                heroPos,
                candidate,
                planeY,
                navValidationRadius);
        if (!result.walkable) {
            result.rejectReason = PlannerRejectReason::Wall;
            return result;
        }

        const float spatialStepMs = 1000.0f * std::max(4.0f, settings.pathStep) / speed;
        const float temporalStepMs = std::clamp(
            std::min(std::max(6.0f, settings.temporalStepMs), spatialStepMs),
            6.0f,
            24.0f);
        const float pathAnalysisDurationMs = std::min(
            result.arrivalTimeMs,
            std::clamp(
                std::isfinite(settings.maxThreatHorizonMs)
                    ? settings.maxThreatHorizonMs
                    : kMaximumAnalysisHorizonMs,
                0.0f,
                kMaximumAnalysisHorizonMs));
        const int steps = AnalysisSampleCount(
            pathAnalysisDurationMs,
            temporalStepMs);
        bool foundEnvelopeSafe = false;
        bool envelopeReentered = false;
        bool actualCollision = false;
        float firstSafeDistance = FLT_MAX;
        float firstCollisionMs = FLT_MAX;
        float minimumClearance = FLT_MAX;
        int maximumDanger = 0;
        int maximumPathDanger = 0;
        CollisionIdentitySet collisionIdentities;
        CollisionIdentitySet endpointCollisionIdentities;
        CollisionIdentitySet previousEnvelopeIdentities;
        CollisionIdentitySet exitedEnvelopeIdentities;
        std::vector<IdentityExposureAccumulator> exposureAccumulators;
        ExposureDangerSet exposureDangers;
        for (std::size_t threatIndex = 0;
             threatIndex < threats.size();
             ++threatIndex) {
            const Threat& threat = threats[threatIndex];
            if (threat.IsExpiredAt(now)) continue;
            const CollisionIdentity identity =
                MakeCollisionIdentity(threat.id, threatIndex);
            const std::size_t exposureIndex =
                ExposureAccumulatorIndex(
                    exposureAccumulators,
                    identity);
            if (exposureIndex == exposureAccumulators.size()) {
                exposureAccumulators.push_back(
                    {identity, threat.Danger()});
            } else {
                exposureAccumulators[exposureIndex].danger =
                    std::max(
                        exposureAccumulators[exposureIndex].danger,
                        threat.Danger());
            }
        }

        for (int step = 0; step <= steps; ++step) {
            const float fraction = static_cast<float>(step) / static_cast<float>(steps);
            const float timeMs = pathAnalysisDurationMs * fraction;
            const Vec2 point = HeroPositionAt(
                heroPos,
                candidate,
                speed,
                std::max(0.0f, settings.inputDelayMs),
                timeMs,
                false);
            const float travelled = heroPos.Distance(point);
            const int sampleTick = SaturatingTickAdd(
                now, ClampTickOffset(std::round(timeMs)));
            int pointDanger = 0;
            int pointMaxDanger = 0;
            int envelopeDanger = 0;
            CollisionIdentitySet pointCollisionIdentities;
            CollisionIdentitySet envelopeCollisionIdentities;
            std::vector<unsigned char> occupiedByIdentity(
                exposureAccumulators.size(), 0);
            std::vector<unsigned char> explosionByIdentity(
                exposureAccumulators.size(), 0);

            for (std::size_t threatIndex = 0;
                 threatIndex < threats.size();
                 ++threatIndex) {
                const Threat& threat = threats[threatIndex];
                if (threat.IsExpiredAt(now)) continue;
                const CollisionIdentity identity =
                    MakeCollisionIdentity(threat.id, threatIndex);
                const int impactTick = ImpactTickAt(threat, point);
                const bool predictedEnvelope =
                    TickDifference(impactTick, now) <=
                        static_cast<int>(settings.maxThreatHorizonMs) &&
                    TickDifference(impactTick, now) >= -100 && ContainsAt(
                        threat,
                        point,
                        heroRadius,
                        settings.pathBuffer,
                        std::max(now, impactTick));
                const bool occupied = OccupiesAt(
                    threat,
                    point,
                    heroRadius,
                    settings.pathBuffer,
                    sampleTick);
                const std::size_t exposureIndex = ExposureAccumulatorIndex(
                    exposureAccumulators, identity);
                if (exposureIndex < exposureAccumulators.size()) {
                    occupiedByIdentity[exposureIndex] =
                        occupiedByIdentity[exposureIndex] ||
                        occupied;
                    explosionByIdentity[exposureIndex] =
                        explosionByIdentity[exposureIndex] ||
                        EndExplosionContainsAt(
                            threat,
                            point,
                            heroRadius,
                            settings.pathBuffer,
                            sampleTick);
                }
                if ((predictedEnvelope || occupied) &&
                    envelopeCollisionIdentities.Add(identity)) {
                    envelopeDanger += threat.Danger();
                }
                if (predictedEnvelope || occupied) {
                    result.encounteredEnvelopeIdentities.Add(
                        identity);
                }
                if (ThreatActiveAt(threat, sampleTick)) {
                    minimumClearance = std::min(
                        minimumClearance,
                        SignedClearanceAt(
                            threat,
                            point,
                            heroRadius,
                            settings.pathBuffer,
                            sampleTick));
                }
                if (!occupied) continue;
                result.encounteredCollisionIdentities.Add(
                    identity);
                const int danger = threat.Danger();
                if (pointCollisionIdentities.Add(identity))
                    pointDanger += danger;
                pointMaxDanger = std::max(pointMaxDanger, danger);
                firstCollisionMs = std::min(firstCollisionMs, timeMs);
            }

            const float sampleDurationMs =
                pathAnalysisDurationMs / static_cast<float>(steps);
            for (std::size_t index = 0;
                 index < exposureAccumulators.size();
                 ++index) {
                IdentityExposureAccumulator& exposure =
                    exposureAccumulators[index];
                const bool occupied = occupiedByIdentity[index] != 0;
                const bool explosion = explosionByIdentity[index] != 0;
                if (step > 0) {
                    exposure.sampledUnionMs +=
                        0.5f * static_cast<float>(
                            exposure.previousUnion + occupied) *
                        sampleDurationMs;
                    exposure.sampledExplosionMs +=
                        0.5f * static_cast<float>(
                            exposure.previousExplosion + explosion) *
                        sampleDurationMs;
                }
                exposure.previousUnion = occupied;
                exposure.previousExplosion = explosion;
            }
            maximumDanger = std::max(maximumDanger, pointMaxDanger);
            maximumPathDanger = std::max(maximumPathDanger, pointDanger);
            if (step == 0) {
                result.startThreatIdentities.AddAll(
                    envelopeCollisionIdentities);
            } else {
                previousEnvelopeIdentities.ForEach(
                    [&](const CollisionIdentity& identity) {
                        if (!envelopeCollisionIdentities.Contains(
                                identity)) {
                            exitedEnvelopeIdentities.Add(identity);
                        }
                    });
                if (envelopeCollisionIdentities.Intersects(
                        exitedEnvelopeIdentities)) {
                    envelopeReentered = true;
                }
            }
            previousEnvelopeIdentities =
                envelopeCollisionIdentities;
            if (step == steps) {
                result.endThreatIdentities.AddAll(
                    envelopeCollisionIdentities);
            }
            const bool startsOutsideEveryEnvelope =
                result.startThreatIdentities.Size() == 0;
            const bool clearedEveryStartingEnvelope =
                !startsOutsideEveryEnvelope &&
                !envelopeCollisionIdentities.Intersects(
                    result.startThreatIdentities);
            if (!foundEnvelopeSafe &&
                ((step == 0 && startsOutsideEveryEnvelope) ||
                 (step > 0 && clearedEveryStartingEnvelope))) {
                foundEnvelopeSafe = true;
                firstSafeDistance = travelled;
            }
            if (pointDanger > 0) actualCollision = true;
        }

        const int endpointTick = SaturatingTickAdd(
            now, ClampTickOffset(std::round(result.arrivalTimeMs)));
        int endpointDanger = 0;
        int endpointMaxDanger = 0;
        for (std::size_t threatIndex = 0;
             threatIndex < threats.size();
             ++threatIndex) {
            const Threat& threat = threats[threatIndex];
            if (threat.IsExpiredAt(now)) continue;
            const CollisionIdentity identity =
                MakeCollisionIdentity(threat.id, threatIndex);
            const float pathAnalysisEndMs = std::min(
                result.arrivalTimeMs,
                std::max(0.0f, settings.maxThreatHorizonMs));
            if (GeometricEnvelopeIntersects(
                    threat,
                    heroPos,
                    candidate,
                    heroRadius,
                    settings.pathBuffer,
                    now,
                    settings.maxThreatHorizonMs)) {
                result.encounteredEnvelopeIdentities.Add(
                    identity);
            }
            const ContinuousCollisionResult collision = AnalyzeThreatPath(
                threat,
                heroPos,
                candidate,
                speed,
                std::max(0.0f, settings.inputDelayMs),
                heroRadius,
                settings.pathBuffer,
                now,
                0.0f,
                pathAnalysisEndMs,
                false);
            minimumClearance = std::min(minimumClearance, collision.minimumClearance);
            const std::size_t exposureIndex = ExposureAccumulatorIndex(
                exposureAccumulators, identity);
            if (exposureIndex < exposureAccumulators.size()) {
                exposureAccumulators[exposureIndex].exactExplosionMs =
                    std::max(
                        exposureAccumulators[exposureIndex].exactExplosionMs,
                        collision.exposureMs);
            }
            const bool pathCollision = collision.collides;
            if (pathCollision) {
                actualCollision = true;
                result.encounteredCollisionIdentities.Add(
                    identity);
                result.encounteredEnvelopeIdentities.Add(
                    identity);
                firstCollisionMs = std::min(
                    firstCollisionMs,
                    collision.firstContactMs);
                maximumDanger = std::max(maximumDanger, threat.Danger());
                maximumPathDanger = std::max(
                    maximumPathDanger,
                    threat.Danger());
            }

            bool endpointCollision = false;
            float endpointFirstCollisionMs = FLT_MAX;
            if (includeEndpointCoverage &&
                result.arrivalTimeMs <=
                    settings.maxThreatHorizonMs) {
                const int explosionTick = threat.EndExplosionStartTick();
                if (threat.HasEndExplosionArea() &&
                    TickDifference(explosionTick, now) <=
                        static_cast<int>(settings.maxThreatHorizonMs)) {
                    if (EndExplosionContainsAt(
                            threat,
                            candidate,
                            heroRadius,
                            settings.endpointBuffer,
                            endpointTick)) {
                        endpointCollision = true;
                        endpointFirstCollisionMs = result.arrivalTimeMs;
                    }
                    if (explosionTick >= endpointTick &&
                        EndExplosionContainsAt(
                            threat,
                            candidate,
                            heroRadius,
                            settings.endpointBuffer,
                            explosionTick)) {
                        endpointCollision = true;
                        endpointFirstCollisionMs = std::min(
                            endpointFirstCollisionMs,
                            static_cast<float>(TickDifference(explosionTick, now)));
                    }
                }
                if (threat.Type() == ZDSpellType::Line &&
                    threat.HasTravelSpeed()) {
                    const ContinuousCollisionResult endpoint = AnalyzeMovingLine(
                        threat,
                        candidate,
                            candidate,
                            speed,
                            0.0f,
                            heroRadius,
                            settings.endpointBuffer,
                            now,
                            settings.maxThreatHorizonMs,
                            result.arrivalTimeMs);
                    minimumClearance = std::min(
                        minimumClearance,
                        endpoint.minimumClearance);
                    endpointCollision = endpointCollision || endpoint.collides;
                    if (endpoint.collides) {
                        endpointFirstCollisionMs = std::min(
                            endpointFirstCollisionMs,
                            endpoint.firstContactMs);
                        firstCollisionMs = std::min(
                            firstCollisionMs,
                            endpoint.firstContactMs);
                    }
                } else {
                    const int impactTick = ImpactTickAt(threat, candidate);
                    if (TickDifference(impactTick, now) <=
                        static_cast<int>(settings.maxThreatHorizonMs)) {
                        if (ContainsAt(
                                threat,
                                candidate,
                                heroRadius,
                                settings.endpointBuffer,
                                endpointTick)) {
                            endpointCollision = true;
                            endpointFirstCollisionMs = std::min(
                                endpointFirstCollisionMs,
                                result.arrivalTimeMs);
                        }
                        if (impactTick >= endpointTick && ContainsAt(
                            threat,
                            candidate,
                            heroRadius,
                            settings.endpointBuffer,
                            impactTick)) {
                            endpointCollision = true;
                            endpointFirstCollisionMs = std::min(
                                endpointFirstCollisionMs,
                                static_cast<float>(TickDifference(impactTick, now)));
                        }
                    }
                }
            }
            if (endpointCollision) {
                result.encounteredCollisionIdentities.Add(
                    identity);
                result.encounteredEnvelopeIdentities.Add(
                    identity);
                result.endThreatIdentities.Add(identity);
                if (endpointCollisionIdentities.Add(identity))
                    endpointDanger += threat.Danger();
                endpointMaxDanger = std::max(
                    endpointMaxDanger,
                    threat.Danger());
                firstCollisionMs = std::min(
                    firstCollisionMs,
                    endpointFirstCollisionMs);
            }
            if (!pathCollision && !endpointCollision) continue;

            collisionIdentities.Add(identity);
        }

        float exposureMs = 0.0f;
        for (const IdentityExposureAccumulator& exposure :
             exposureAccumulators) {
            const float sampledOutsideExplosionMs = std::max(
                0.0f,
                exposure.sampledUnionMs - exposure.sampledExplosionMs);
            const float exposureDurationMs =
                sampledOutsideExplosionMs + exposure.exactExplosionMs;
            exposureMs += static_cast<float>(exposure.danger) *
                exposureDurationMs;
            if (exposureDurationMs > 0.0f) {
                exposureDangers.AddOrMax(
                    exposure.identity,
                    exposure.danger);
            }
        }
        if (includeEndpointCoverage &&
            result.arrivalTimeMs <=
                settings.maxThreatHorizonMs) {
            CollisionIdentitySet endpointHoldIdentities;
            const CandidateEvaluation endpointHold =
                EvaluateStationaryCandidateCore(
                    candidate,
                    cursorPos,
                    heroRadius,
                    now,
                    settings,
                    threats,
                    result.arrivalTimeMs,
                    &endpointHoldIdentities,
                    &exposureDangers);
            collisionIdentities.AddAll(endpointHoldIdentities);
            result.encounteredCollisionIdentities.AddAll(
                endpointHold.encounteredCollisionIdentities);
            result.encounteredEnvelopeIdentities.AddAll(
                endpointHold.encounteredEnvelopeIdentities);
            result.endThreatIdentities.AddAll(
                endpointHold.startThreatIdentities);
            if (endpointHold.encounteredCollisionIdentities.Intersects(
                    exitedEnvelopeIdentities) ||
                endpointHold.encounteredEnvelopeIdentities.Intersects(
                    exitedEnvelopeIdentities)) {
                envelopeReentered = true;
            }
            endpointDanger = std::max(
                endpointDanger,
                endpointHold.endpointDanger);
            endpointMaxDanger = std::max(
                endpointMaxDanger,
                endpointHold.maxDanger);
            maximumDanger = std::max(
                maximumDanger,
                endpointHold.maxDanger);
            maximumPathDanger = std::max(
                maximumPathDanger,
                endpointHold.pathDanger);
            minimumClearance = std::min(
                minimumClearance,
                endpointHold.minimumClearance);
            firstCollisionMs = std::min(
                firstCollisionMs,
                endpointHold.firstCollisionTimeMs);
            exposureMs += endpointHold.dangerExposureMs;
            if (!endpointHold.pathSafe)
                actualCollision = true;
        }
        if (collisionIdentitiesOut)
            collisionIdentitiesOut->AddAll(collisionIdentities);
        if (exposureDangersOut)
            exposureDangersOut->AddAll(exposureDangers);

        if (minimumClearance == FLT_MAX) minimumClearance = settings.maxSearchRadius;
        result.endpointDanger = endpointDanger;
        result.pathDanger = maximumPathDanger;
        result.maxDanger = std::max(maximumDanger, endpointMaxDanger);
        result.collisionCount =
            static_cast<int>(collisionIdentities.Size());
        result.endpointSafe = endpointDanger == 0;
        result.pathSafe = !actualCollision;
        result.reenteredDanger = envelopeReentered;
        result.enteredNewThreat =
            result.encounteredCollisionIdentities.AnyNotIn(
                result.startThreatIdentities) ||
            result.encounteredEnvelopeIdentities.AnyNotIn(
                result.startThreatIdentities);
        result.exitedStartEnvelope =
            result.startThreatIdentities.Size() > 0 &&
            foundEnvelopeSafe;
        result.exitDistance = foundEnvelopeSafe
            ? firstSafeDistance
            : std::numeric_limits<float>::infinity();
        result.firstCollisionTimeMs = firstCollisionMs;
        result.dangerExposureMs = exposureMs;
        result.summedExposureDanger =
            exposureDangers.SummedDanger();
        result.minimumClearance = minimumClearance;
        result.timeMarginMs = firstCollisionMs == FLT_MAX
            ? settings.maxThreatHorizonMs
            : firstCollisionMs;
        result.timingSafe = firstCollisionMs == FLT_MAX ||
            firstCollisionMs >= settings.minimumTimeMarginMs;
        result.strictSafe =
            result.endpointSafe &&
            result.pathSafe &&
            result.timingSafe &&
            !result.reenteredDanger &&
            !result.enteredNewThreat &&
            (result.startThreatIdentities.Size() == 0 ||
             result.exitedStartEnvelope);
        if (result.strictSafe) {
            result.rejectReason = PlannerRejectReason::None;
        } else if (!result.endpointSafe) {
            result.rejectReason = PlannerRejectReason::EndpointDanger;
        } else if (result.reenteredDanger ||
                   result.enteredNewThreat) {
            result.rejectReason = PlannerRejectReason::Reentry;
        } else if (!result.pathSafe) {
            result.rejectReason = PlannerRejectReason::PathDanger;
        } else if (!result.timingSafe) {
            result.rejectReason = PlannerRejectReason::Late;
        } else {
            result.rejectReason = PlannerRejectReason::NoExit;
        }
        return result;
    }

public:
    static CandidateEvaluation EvaluatePathCandidate(
        const std::vector<Vec2>& path,
        PlannerCandidateSource source,
        int sourceThreatId,
        const Vec2& cursorPos,
        float planeY,
        float moveSpeed,
        float heroRadius,
        int now,
        const EvadeSettings& settings,
        const std::vector<Threat>& threats,
        CollisionIdentitySet* collisionIdentitiesOut = nullptr,
        int stabilityBranchKey =
            StabilityBranch::Unknown) {
        CandidateEvaluation result;
        result.source = source;
        result.sourceThreatId = sourceThreatId;
        result.stabilityBranchKey = stabilityBranchKey;
        result.position = path.empty() ? Vec2() : path.back();
        result.cursorDistance = path.empty() ? FLT_MAX : path.back().Distance(cursorPos);
        result.travelDistance = EvadeGeometryMath::PolylineLength(path);
        result.exitDistance =
            std::numeric_limits<float>::infinity();
        result.valid = path.size() >= 2 && result.position.IsValid() &&
            !result.position.IsZero() && result.travelDistance >= 1.0f;
        if (!result.valid) {
            result.rejectReason = PlannerRejectReason::Invalid;
            return result;
        }
        result.walkable = WalkablePath(
            path,
            planeY,
            heroRadius);
        if (!result.walkable) {
            result.rejectReason = PlannerRejectReason::Wall;
            return result;
        }

        result.pathSafe = true;
        result.endpointSafe = true;
        result.timingSafe = true;
        result.minimumClearance = FLT_MAX;
        result.firstCollisionTimeMs = FLT_MAX;
        const float speed = std::max(50.0f, moveSpeed);
        const float horizon = std::clamp(
            std::isfinite(settings.maxThreatHorizonMs)
                ? settings.maxThreatHorizonMs
                : kMaximumAnalysisHorizonMs,
            0.0f,
            kMaximumAnalysisHorizonMs);
        const float fullArrivalTimeMs =
            std::max(0.0f, settings.inputDelayMs) +
            1000.0f * result.travelDistance / speed;
        float elapsedMs = 0.0f;
        float distanceBefore = 0.0f;
        bool foundExit = false;
        bool firstMotionSegment = true;
        bool firstEvaluatedSegment = true;
        CollisionIdentitySet collisionIdentities;
        CollisionIdentitySet activeThreatIdentities;
        CollisionIdentitySet exitedThreatIdentities;
        ExposureDangerSet exposureDangers;
        for (std::size_t index = 1; index < path.size(); ++index) {
            const Vec2& segmentStart = path[index - 1];
            const Vec2& segmentEnd = path[index];
            const float segmentDistance =
                segmentStart.Distance(segmentEnd);
            if (segmentDistance <= 0.001f) continue;
            const bool instantaneousInitialHorizon =
                elapsedMs == 0.0f && horizon == 0.0f;
            if (elapsedMs >= horizon &&
                !instantaneousInitialHorizon) {
                break;
            }

            const float segmentInputDelayMs = firstMotionSegment
                ? std::max(0.0f, settings.inputDelayMs)
                : 0.0f;
            firstMotionSegment = false;
            const float segmentTravelTimeMs =
                1000.0f * segmentDistance / speed;
            const float segmentDurationMs =
                segmentInputDelayMs + segmentTravelTimeMs;
            const float remainingHorizonMs =
                std::max(0.0f, horizon - elapsedMs);
            const bool clipsAtHorizon =
                segmentDurationMs > remainingHorizonMs;

            EvadeSettings segmentSettings = settings;
            segmentSettings.inputDelayMs = segmentInputDelayMs;
            segmentSettings.maxThreatHorizonMs = remainingHorizonMs;
            const int segmentNow = SaturatingTickAdd(
                now, ClampTickOffset(std::round(elapsedMs)));

            Vec2 evaluatedEnd = segmentEnd;
            if (clipsAtHorizon) {
                const float movementTimeMs = std::max(
                    0.0f,
                    remainingHorizonMs - segmentInputDelayMs);
                const float movementFraction = std::clamp(
                    movementTimeMs / segmentTravelTimeMs,
                    0.0f,
                    1.0f);
                evaluatedEnd = segmentStart +
                    (segmentEnd - segmentStart) * movementFraction;
            }

            CandidateEvaluation segment;
            if (evaluatedEnd.Distance(segmentStart) <= 0.001f) {
                segment = EvaluateStationaryPathCandidateCore(
                    segmentStart,
                    cursorPos,
                    heroRadius,
                    segmentNow,
                    segmentSettings,
                    threats,
                    0.0f,
                    &collisionIdentities,
                    &exposureDangers);
            } else {
                segment = EvaluateCandidateCore(
                    evaluatedEnd,
                    source,
                    sourceThreatId,
                    segmentStart,
                    cursorPos,
                    planeY,
                    moveSpeed,
                    heroRadius,
                    segmentNow,
                    segmentSettings,
                    threats,
                    &collisionIdentities,
                    !clipsAtHorizon && index + 1 == path.size(),
                    NavValidationMode::AlreadyValidatedPolyline,
                    &exposureDangers);
            }
            if (!segment.valid || !segment.walkable) {
                result.walkable = false;
                result.rejectReason = segment.rejectReason;
                return result;
            }
            if (firstEvaluatedSegment) {
                result.startThreatIdentities.AddAll(
                    segment.startThreatIdentities);
                activeThreatIdentities =
                    segment.startThreatIdentities;
                if (result.startThreatIdentities.Size() == 0) {
                    result.exitDistance = 0.0f;
                    foundExit = true;
                }
                firstEvaluatedSegment = false;
            }
            if (segment.encounteredCollisionIdentities.Intersects(
                    exitedThreatIdentities) ||
                segment.encounteredEnvelopeIdentities.Intersects(
                    exitedThreatIdentities)) {
                result.reenteredDanger = true;
            }
            result.encounteredCollisionIdentities.AddAll(
                segment.encounteredCollisionIdentities);
            result.encounteredEnvelopeIdentities.AddAll(
                segment.encounteredEnvelopeIdentities);
            activeThreatIdentities.ForEach(
                [&](const CollisionIdentity& identity) {
                    if (!segment.endThreatIdentities.Contains(
                            identity)) {
                        exitedThreatIdentities.Add(identity);
                    }
                });
            segment.encounteredEnvelopeIdentities.ForEach(
                [&](const CollisionIdentity& identity) {
                    if (!segment.endThreatIdentities.Contains(
                            identity)) {
                        exitedThreatIdentities.Add(identity);
                    }
                });
            activeThreatIdentities =
                segment.endThreatIdentities;
            result.endThreatIdentities =
                segment.endThreatIdentities;
            result.pathSafe = result.pathSafe && segment.pathSafe;
            result.pathDanger = std::max(result.pathDanger, segment.pathDanger);
            result.maxDanger = std::max(result.maxDanger, segment.maxDanger);
            result.dangerExposureMs += segment.dangerExposureMs;
            result.reenteredDanger = result.reenteredDanger || segment.reenteredDanger;
            result.minimumClearance = std::min(
                result.minimumClearance,
                segment.minimumClearance);
            if (!foundExit &&
                segment.exitedStartEnvelope &&
                std::isfinite(segment.exitDistance)) {
                result.exitDistance = distanceBefore + segment.exitDistance;
                result.exitedStartEnvelope = true;
                foundExit = true;
            }
            if (segment.firstCollisionTimeMs != FLT_MAX) {
                result.firstCollisionTimeMs = std::min(
                    result.firstCollisionTimeMs,
                    elapsedMs + segment.firstCollisionTimeMs);
            }
            elapsedMs += clipsAtHorizon
                ? remainingHorizonMs
                : segmentDurationMs;
            distanceBefore += segmentStart.Distance(evaluatedEnd);
            if (!clipsAtHorizon && index + 1 == path.size()) {
                result.endpointSafe = segment.endpointSafe;
                result.endpointDanger = segment.endpointDanger;
            }
            if (clipsAtHorizon) break;
        }
        result.collisionCount =
            static_cast<int>(collisionIdentities.Size());
        if (collisionIdentitiesOut)
            collisionIdentitiesOut->AddAll(collisionIdentities);
        result.summedExposureDanger =
            exposureDangers.SummedDanger();
        result.enteredNewThreat =
            result.encounteredCollisionIdentities.AnyNotIn(
                result.startThreatIdentities) ||
            result.encounteredEnvelopeIdentities.AnyNotIn(
                result.startThreatIdentities);
        result.arrivalTimeMs = fullArrivalTimeMs;
        if (result.minimumClearance == FLT_MAX)
            result.minimumClearance = settings.maxSearchRadius;
        result.timeMarginMs = result.firstCollisionTimeMs == FLT_MAX
            ? settings.maxThreatHorizonMs
            : result.firstCollisionTimeMs;
        result.timingSafe = result.firstCollisionTimeMs == FLT_MAX ||
            result.firstCollisionTimeMs >= settings.minimumTimeMarginMs;
        result.strictSafe =
            result.pathSafe &&
            result.endpointSafe &&
            result.timingSafe &&
            !result.reenteredDanger &&
            !result.enteredNewThreat &&
            (result.startThreatIdentities.Size() == 0 ||
             result.exitedStartEnvelope);
        result.rejectReason = result.strictSafe
            ? PlannerRejectReason::None
            : !result.endpointSafe
                ? PlannerRejectReason::EndpointDanger
                : result.reenteredDanger ||
                        result.enteredNewThreat
                    ? PlannerRejectReason::Reentry
                    : !result.pathSafe
                        ? PlannerRejectReason::PathDanger
                        : !result.timingSafe
                            ? PlannerRejectReason::Late
                            : PlannerRejectReason::NoExit;
        return result;
    }

private:
    static inline constexpr float kPi = 3.14159265358979323846f;
    static inline constexpr float kDegToRad = kPi / 180.0f;

    static int CanonicalEndExplosionEndTick(const Threat& threat) {
        return SaturatingTickAdd(
            threat.EndExplosionStartTick(),
            std::max(100, threat.EndExplosionDuration()));
    }

    static std::size_t ExposureAccumulatorIndex(
        const std::vector<IdentityExposureAccumulator>& accumulators,
        const CollisionIdentity& identity) {
        for (std::size_t index = 0; index < accumulators.size(); ++index) {
            if (accumulators[index].identity == identity) return index;
        }
        return accumulators.size();
    }

    static bool ThreatBodyRelevantWithinHorizon(
        const Threat& threat,
        int now,
        float horizonMs) {
        if (threat.expired) return false;
        if (ThreatBodyActiveAt(threat, now)) return true;

        const float safeHorizonMs = std::clamp(
            std::isfinite(horizonMs)
                ? horizonMs
                : kMaximumAnalysisHorizonMs,
            0.0f,
            kMaximumAnalysisHorizonMs);
        const int horizonTick = SaturatingTickAdd(
            now,
            ClampTickOffset(std::floor(safeHorizonMs)));
        int activeStartTick = 0;
        int activeEndTick = 0;

        if (threat.projectileTerminated) {
            if (threat.Type() != ZDSpellType::Circular ||
                threat.projectileTerminationTick <= 0 ||
                threat.ExtraEndTime() <= 0) {
                return false;
            }
            activeStartTick =
                threat.projectileTerminationTick;
            activeEndTick = SaturatingTickAdd(
                activeStartTick,
                threat.ExtraEndTime());
        } else if (
            threat.Type() == ZDSpellType::Line &&
            threat.HasTravelSpeed()) {
            activeStartTick = TravelStartTick(threat);
            activeEndTick = threat.persistent
                ? std::numeric_limits<int>::max()
                : threat.MovingLineTerminalActiveEndTick();
        } else {
            activeStartTick =
                threat.Type() == ZDSpellType::Circular &&
                    threat.HasTravelSpeed()
                ? ImpactTickAt(threat, threat.endPos)
                : SaturatingTickAdd(
                    threat.startTick,
                    threat.Delay());
            activeEndTick = threat.persistent
                ? std::numeric_limits<int>::max()
                : std::min(
                    SaturatingTickAdd(
                        activeStartTick,
                        std::max(
                            100,
                            threat.ExtraEndTime())),
                    SaturatingTickAdd(
                        threat.endTick,
                        100));
        }
        return !TickAfter(activeStartTick, horizonTick) &&
            !TickAfter(now, activeEndTick);
    }

    static bool GeometricEnvelopeIntersects(
        const Threat& threat,
        const Vec2& routeStart,
        const Vec2& routeEnd,
        float heroRadius,
        float buffer,
        int now,
        float horizonMs) {
        const float uncertainty =
            threat.PositionUncertainty();
        const float radius = std::max(
            0.0f,
            threat.Radius() +
                heroRadius +
                buffer +
                uncertainty);
        bool intersects = false;
        if (ThreatBodyRelevantWithinHorizon(
                threat,
                now,
                horizonMs)) {
            switch (threat.Type()) {
            case ZDSpellType::Line: {
                const Vec2 envelopeStart =
                    threat.HasTravelSpeed()
                        ? threat.HeadAtTick(now)
                        : threat.startPos;
                intersects =
                    EvadeGeometryMath::SegmentSegmentDistance(
                        routeStart,
                        routeEnd,
                        envelopeStart,
                        threat.endPos) <= radius;
                break;
            }
            case ZDSpellType::Circular:
                intersects =
                    EvadeGeometryMath::DistanceToSegment(
                        threat.endPos,
                        routeStart,
                        routeEnd) <= radius;
                break;
            case ZDSpellType::Ring: {
                const float inner = std::max(
                    0.0f,
                    threat.InnerRadius() -
                        heroRadius -
                        buffer -
                        uncertainty);
                const auto clearance = [&](const Vec2& point) {
                    return RingSignedClearance(
                        point,
                        threat.endPos,
                        inner,
                        radius);
                };
                intersects =
                    EvadeGeometryMath::MinimumSignedDistanceAlongSegment(
                        routeStart,
                        routeEnd,
                        clearance,
                        64) <= 0.0f;
                break;
            }
            case ZDSpellType::Cone: {
                if (!threat.HasValidConeAngle())
                    return true;
                const Vec2 direction =
                    threat.direction.IsZero()
                        ? (threat.endPos -
                           threat.startPos).Normalized()
                        : threat.direction;
                intersects =
                    EvadeGeometryMath::FirstContactMovingPointSector(
                        routeStart,
                        routeEnd,
                        threat.startPos,
                        direction,
                        threat.Range(),
                        threat.Angle() *
                            0.5f *
                            kDegToRad,
                        ConeExpansion(
                            threat,
                            heroRadius,
                            buffer,
                            uncertainty),
                        0.02f) != FLT_MAX;
                break;
            }
            case ZDSpellType::Arc:
                return true;
            }
        }
        if (intersects) return true;
        if (!threat.HasEndExplosionArea() ||
            !FutureTickWithinHorizon(
                threat.EndExplosionStartTick(),
                now,
                horizonMs)) {
            return false;
        }
        const float explosionRadius =
            threat.EndExplosionRadius() +
            heroRadius +
            buffer +
            uncertainty;
        return EvadeGeometryMath::DistanceToSegment(
            threat.EndExplosionCenter(),
            routeStart,
            routeEnd) <= explosionRadius;
    }

    static ContinuousCollisionResult MergeContinuousCollisions(
        const ContinuousCollisionResult& body,
        const ContinuousCollisionResult& explosion) {
        ContinuousCollisionResult result;
        result.collides = body.collides || explosion.collides;
        result.firstContactMs = std::min(
            body.firstContactMs, explosion.firstContactMs);
        result.minimumClearance = std::min(
            body.minimumClearance, explosion.minimumClearance);
        // Both components have the same collision identity. Taking the larger
        // duration prevents overlapping body/explosion exposure from being
        // counted twice; body exposure is currently sampled by the caller.
        result.exposureMs = std::max(body.exposureMs, explosion.exposureMs);
        return result;
    }

    static float CircleInsideDurationMs(const Vec2& start,
                                        const Vec2& end,
                                        const Vec2& center,
                                        float radius,
                                        float durationMs) {
        if (!start.IsValid() || !end.IsValid() || !center.IsValid() ||
            !std::isfinite(radius) || !std::isfinite(durationMs) ||
            durationMs <= 0.0f) return 0.0f;
        const Vec2 delta = end - start;
        const Vec2 offset = start - center;
        const double safeRadius = std::max(0.0, static_cast<double>(radius));
        const double a = static_cast<double>(delta.x) * delta.x +
            static_cast<double>(delta.y) * delta.y;
        const double halfB = static_cast<double>(offset.x) * delta.x +
            static_cast<double>(offset.y) * delta.y;
        const double c = static_cast<double>(offset.x) * offset.x +
            static_cast<double>(offset.y) * offset.y -
            safeRadius * safeRadius;
        if (!std::isfinite(a) || !std::isfinite(halfB) ||
            !std::isfinite(c)) return 0.0f;
        const double contactTolerance =
            1.0e-9 * std::max(1.0, safeRadius * safeRadius);
        if (a <= 1.0e-12)
            return c <= contactTolerance ? durationMs : 0.0f;

        double discriminant = halfB * halfB - a * c;
        const double discriminantTolerance = 1.0e-12 *
            std::max(1.0, halfB * halfB + std::fabs(a * c));
        if (discriminant < -discriminantTolerance) return 0.0f;
        discriminant = std::max(0.0, discriminant);
        const double root = std::sqrt(discriminant);
        const double enter = (-halfB - root) / a;
        const double leave = (-halfB + root) / a;
        const double clippedEnter = std::clamp(enter, 0.0, 1.0);
        const double clippedLeave = std::clamp(leave, 0.0, 1.0);
        if (clippedLeave <= clippedEnter) return 0.0f;
        return durationMs *
            static_cast<float>(clippedLeave - clippedEnter);
    }

    static ContinuousCollisionResult AnalyzeEndExplosion(
        const Threat& threat,
        const Vec2& heroPos,
        const Vec2& candidate,
        float moveSpeed,
        float inputDelayMs,
        float heroRadius,
        float buffer,
        int now,
        float analysisStartMs,
        float analysisEndMs,
        bool teleport) {
        ContinuousCollisionResult result;
        if (!threat.HasEndExplosionArea() ||
            !std::isfinite(analysisStartMs) ||
            !std::isfinite(analysisEndMs)) return result;

        const int explosionStartTick = threat.EndExplosionStartTick();
        const int explosionEndTick =
            CanonicalEndExplosionEndTick(threat);
        const float activeStartMs = std::max({
            0.0f,
            analysisStartMs,
            static_cast<float>(TickDifference(explosionStartTick, now)),
        });
        const float activeEndMs = std::min(
            std::max(0.0f, analysisEndMs),
            static_cast<float>(TickDifference(explosionEndTick, now)));
        if (activeEndMs < activeStartMs) return result;

        const float radius = std::max(
            0.0f,
            threat.EndExplosionRadius() +
                std::max(0.0f, heroRadius) +
                std::max(0.0f, buffer) +
                threat.PositionUncertainty());
        const Vec2 center = threat.EndExplosionCenter();
        const float moveDurationMs = teleport
            ? 0.0f
            : 1000.0f * heroPos.Distance(candidate) /
                std::max(50.0f, moveSpeed);
        const float arrivalMs = inputDelayMs + moveDurationMs;
        std::array<float, 4> boundaries = {
            activeStartMs,
            activeEndMs,
            std::clamp(inputDelayMs, activeStartMs, activeEndMs),
            std::clamp(arrivalMs, activeStartMs, activeEndMs),
        };
        std::sort(boundaries.begin(), boundaries.end());
        for (std::size_t index = 0; index + 1 < boundaries.size(); ++index) {
            const float beginMs = boundaries[index];
            const float endMs = boundaries[index + 1];
            if (endMs <= beginMs) continue;
            const Vec2 start = HeroPositionAt(
                heroPos, candidate, moveSpeed, inputDelayMs, beginMs, teleport);
            const Vec2 end = HeroPositionAt(
                heroPos, candidate, moveSpeed, inputDelayMs, endMs, teleport);
            result.minimumClearance = std::min(
                result.minimumClearance,
                EvadeGeometryMath::DistanceToSegment(center, start, end) -
                    radius);
            const float contact =
                EvadeGeometryMath::FirstContactMovingPointCircle(
                    start, end, center, radius);
            if (contact != FLT_MAX) {
                result.collides = true;
                result.firstContactMs = std::min(
                    result.firstContactMs,
                    beginMs + (endMs - beginMs) * contact);
            }
            result.exposureMs += CircleInsideDurationMs(
                start,
                end,
                center,
                radius,
                endMs - beginMs);
        }

        if (activeEndMs <= activeStartMs) {
            const Vec2 point = HeroPositionAt(
                heroPos,
                candidate,
                moveSpeed,
                inputDelayMs,
                activeStartMs,
                teleport);
            result.minimumClearance = point.Distance(center) - radius;
            if (result.minimumClearance <= 0.0f) {
                result.collides = true;
                result.firstContactMs = activeStartMs;
            }
        }
        return result;
    }

    static ContinuousCollisionResult AnalyzeThreatPath(
        const Threat& threat,
        const Vec2& heroPos,
        const Vec2& candidate,
        float moveSpeed,
        float inputDelayMs,
        float heroRadius,
        float buffer,
        int now,
        float analysisStartMs,
        float analysisEndMs,
        bool teleport) {
        const ContinuousCollisionResult body =
            threat.Type() == ZDSpellType::Line && threat.HasTravelSpeed()
                ? AnalyzeMovingLine(
                    threat,
                    heroPos,
                    candidate,
                    moveSpeed,
                    inputDelayMs,
                    heroRadius,
                    buffer,
                    now,
                    analysisEndMs,
                    analysisStartMs,
                    teleport)
                : AnalyzeStaticThreat(
                    threat,
                    heroPos,
                    candidate,
                    moveSpeed,
                    inputDelayMs,
                    heroRadius,
                    buffer,
                    now,
                    analysisStartMs,
                    analysisEndMs,
                    teleport);
        const ContinuousCollisionResult explosion = AnalyzeEndExplosion(
            threat,
            heroPos,
            candidate,
            moveSpeed,
            inputDelayMs,
            heroRadius,
            buffer,
            now,
            analysisStartMs,
            analysisEndMs,
            teleport);
        return MergeContinuousCollisions(body, explosion);
    }

    static bool EndExplosionActiveAt(const Threat& threat, int tick) {
        if (threat.expired || !threat.HasEndExplosionArea()) return false;
        const int start = threat.EndExplosionStartTick();
        return tick >= start &&
            tick <= CanonicalEndExplosionEndTick(threat);
    }

    static bool EndExplosionContainsAt(const Threat& threat,
                                       const Vec2& point,
                                       float heroRadius,
                                       float extraBuffer,
                                       int tick) {
        if (!EndExplosionActiveAt(threat, tick)) return false;
        const float radius = threat.EndExplosionRadius() +
            std::max(0.0f, heroRadius) +
            std::max(0.0f, extraBuffer) +
            threat.PositionUncertainty();
        return point.Distance(threat.EndExplosionCenter()) <= radius;
    }

    static bool ThreatBodyContainsAt(const Threat& threat,
                                     const Vec2& point,
                                     float heroRadius,
                                     float extraBuffer,
                                     int tick) {
        if (!ThreatBodyActiveAt(threat, tick)) return false;
        if (!threat.HasValidGeometry()) return true;
        const float uncertainty = threat.HasTravelSpeed()
            ? threat.PositionUncertainty()
            : 0.0f;
        if (threat.Type() == ZDSpellType::Cone) {
            return ContainsCone(
                threat,
                point,
                ConeExpansion(
                    threat,
                    heroRadius,
                    extraBuffer,
                    uncertainty));
        }
        const float radius = std::max(
            0.0f,
            threat.Radius() + heroRadius + extraBuffer + uncertainty);
        switch (threat.Type()) {
        case ZDSpellType::Line:
            return ContainsLineAt(threat, point, radius, tick);
        case ZDSpellType::Circular:
            return point.Distance(threat.endPos) <= radius;
        case ZDSpellType::Ring:
            return RingSignedClearance(
                point,
                threat.endPos,
                std::max(
                    0.0f,
                    threat.InnerRadius() -
                        heroRadius -
                        extraBuffer -
                        uncertainty),
                radius) <= 0.0f;
        case ZDSpellType::Arc:
            return true;
        default:
            return true;
        }
    }

    static bool ThreatBodyActiveAt(const Threat& threat, int tick) {
        return threat.IsBodyActiveAt(tick);
    }

    static int TravelStartTick(const Threat& threat) {
        return threat.missileBound && threat.launchTick > 0
            ? threat.launchTick
            : SaturatingTickAdd(threat.startTick, threat.Delay());
    }

    static int TravelEndTick(const Threat& threat) {
        return threat.ArrivalTick();
    }

    static bool FutureTickWithinHorizon(int tick,
                                        int now,
                                        float horizonMs) {
        const float safeHorizonMs = std::clamp(
            std::isfinite(horizonMs) ? horizonMs : kMaximumAnalysisHorizonMs,
            0.0f,
            kMaximumAnalysisHorizonMs);
        const std::int64_t offset = TickDifference(tick, now);
        return offset >= 0 &&
            offset <= ClampTickOffset(safeHorizonMs);
    }

    static Vec2 MovingLineHeadAtTime(const Threat& threat,
                                     int now,
                                     float timeMs) {
        if (!std::isfinite(timeMs)) return threat.HeadAtTick(now);
        const float floorMs = std::floor(timeMs);
        const float ceilMs = std::ceil(timeMs);
        const int floorTick = SaturatingTickAdd(
            now, ClampTickOffset(floorMs));
        const int ceilTick = SaturatingTickAdd(
            now, ClampTickOffset(ceilMs));
        const Vec2 floorHead = threat.HeadAtTick(floorTick);
        if (floorTick == ceilTick) return floorHead;
        const Vec2 ceilHead = threat.HeadAtTick(ceilTick);
        return floorHead + (ceilHead - floorHead) * (timeMs - floorMs);
    }

    static Vec2 HeroPositionAt(const Vec2& heroPos,
                               const Vec2& candidate,
                               float moveSpeed,
                               float inputDelayMs,
                               float timeMs,
                               bool teleport) {
        if (teleport) return timeMs < inputDelayMs ? heroPos : candidate;
        const Vec2 delta = candidate - heroPos;
        const float distance = delta.Length();
        if (distance <= 0.0001f || timeMs <= inputDelayMs) return heroPos;
        const float travelled = std::min(
            distance,
            std::max(0.0f, timeMs - inputDelayMs) * std::max(50.0f, moveSpeed) / 1000.0f);
        return heroPos + delta * (travelled / distance);
    }

    static ContinuousCollisionResult AnalyzeStaticThreat(
        const Threat& threat,
        const Vec2& heroPos,
        const Vec2& candidate,
        float moveSpeed,
        float inputDelayMs,
        float heroRadius,
        float buffer,
        int now,
        float analysisStartMs,
        float analysisEndMs,
        bool teleport) {
        ContinuousCollisionResult result;
        if (threat.Type() == ZDSpellType::Arc) {
            result.collides = true;
            result.firstContactMs = 0.0f;
            result.minimumClearance = -FLT_MAX;
            return result;
        }
        if (threat.projectileTerminated ||
            (threat.Type() == ZDSpellType::Line && threat.HasTravelSpeed())) return result;
        int activeStartTick = SaturatingTickAdd(threat.startTick, threat.Delay());
        int activeEndTick = threat.persistent
            ? SaturatingTickAdd(
                now,
                ClampTickOffset(std::ceil(std::max(0.0f, analysisEndMs))))
            : SaturatingTickAdd(
                activeStartTick, std::max(100, threat.ExtraEndTime()));
        if (threat.Type() == ZDSpellType::Circular && threat.HasTravelSpeed()) {
            const int impact = ImpactTickAt(threat, threat.endPos);
            activeStartTick = impact;
            if (!threat.persistent)
                activeEndTick = SaturatingTickAdd(
                    impact, std::max(100, threat.ExtraEndTime()));
        }
        const float activeStartMs = std::max(
            std::max(0.0f, analysisStartMs),
            static_cast<float>(TickDifference(activeStartTick, now)));
        const float activeEndMs = std::min(
            std::max(0.0f, analysisEndMs),
            static_cast<float>(TickDifference(activeEndTick, now)));
        if (activeEndMs < activeStartMs) return result;

        const float radius = threat.Type() == ZDSpellType::Cone
            ? 0.0f
            : std::max(
                0.0f,
                threat.Radius() + heroRadius + buffer + threat.PositionUncertainty());
        const float coneExpansion = ConeExpansion(
            threat,
            heroRadius,
            buffer,
            threat.PositionUncertainty());
        const Vec2 direction = threat.direction.IsZero()
            ? (threat.endPos - threat.startPos).Normalized()
            : threat.direction;
        const auto clearance = [&](const Vec2& point) {
            switch (threat.Type()) {
            case ZDSpellType::Line:
                return DistanceToSegment(point, threat.startPos, threat.endPos) - radius;
            case ZDSpellType::Circular:
                return point.Distance(threat.endPos) - radius;
            case ZDSpellType::Ring:
                return RingSignedClearance(
                    point,
                    threat.endPos,
                    std::max(
                        0.0f,
                        threat.InnerRadius() - heroRadius - buffer - threat.PositionUncertainty()),
                    radius);
            case ZDSpellType::Cone:
                if (!threat.HasValidConeAngle()) return -1.0f;
                return EvadeGeometryMath::SignedDistanceToSector(
                    point,
                    threat.startPos,
                    direction,
                    threat.Range(),
                    threat.Angle() * 0.5f * kDegToRad) - coneExpansion;
            case ZDSpellType::Arc:
                return -FLT_MAX;
            default:
                return -1.0f;
            }
        };

        const float moveDurationMs = teleport
            ? 0.0f
            : 1000.0f * heroPos.Distance(candidate) / std::max(50.0f, moveSpeed);
        const float arrivalMs = inputDelayMs + moveDurationMs;
        std::array<float, 4> boundaries = {
            activeStartMs,
            activeEndMs,
            std::clamp(inputDelayMs, activeStartMs, activeEndMs),
            std::clamp(arrivalMs, activeStartMs, activeEndMs),
        };
        std::sort(boundaries.begin(), boundaries.end());
        for (std::size_t index = 0; index + 1 < boundaries.size(); ++index) {
            const float beginMs = boundaries[index];
            const float endMs = boundaries[index + 1];
            if (endMs < beginMs) continue;
            const Vec2 start = HeroPositionAt(
                heroPos, candidate, moveSpeed, inputDelayMs, beginMs, teleport);
            const Vec2 end = HeroPositionAt(
                heroPos, candidate, moveSpeed, inputDelayMs, endMs, teleport);
            float intervalClearance = FLT_MAX;
            float contact = FLT_MAX;
            switch (threat.Type()) {
            case ZDSpellType::Line:
                intervalClearance = EvadeGeometryMath::SegmentSegmentDistance(
                    start, end, threat.startPos, threat.endPos) - radius;
                contact = EvadeGeometryMath::FirstContactMovingPointCapsule(
                    start, end, threat.startPos, threat.endPos, radius);
                break;
            case ZDSpellType::Circular:
                intervalClearance = EvadeGeometryMath::DistanceToSegment(
                    threat.endPos, start, end) - radius;
                contact = EvadeGeometryMath::FirstContactMovingPointCircle(
                    start, end, threat.endPos, radius);
                break;
            case ZDSpellType::Ring:
                intervalClearance = EvadeGeometryMath::MinimumSignedDistanceAlongSegment(
                    start, end, clearance, 64);
                contact = EvadeGeometryMath::FirstContactMovingPointBySignedDistance(
                    start, end, clearance);
                break;
            case ZDSpellType::Cone:
                if (!threat.HasValidConeAngle()) {
                    intervalClearance = -1.0f;
                    contact = 0.0f;
                    break;
                }
                intervalClearance = EvadeGeometryMath::MinimumSignedDistanceAlongSegment(
                    start, end, clearance, 64);
                contact = EvadeGeometryMath::FirstContactMovingPointSector(
                    start,
                    end,
                    threat.startPos,
                    direction,
                    threat.Range(),
                    threat.Angle() * 0.5f * kDegToRad,
                    coneExpansion,
                    0.02f);
                break;
            case ZDSpellType::Arc:
                intervalClearance = -FLT_MAX;
                contact = 0.0f;
                break;
            default:
                intervalClearance = -1.0f;
                contact = 0.0f;
                break;
            }
            result.minimumClearance = std::min(result.minimumClearance, intervalClearance);
            if (contact == FLT_MAX) continue;
            result.collides = true;
            result.firstContactMs = std::min(
                result.firstContactMs,
                beginMs + (endMs - beginMs) * contact);
        }
        if (activeEndMs - activeStartMs <= 0.001f) {
            const Vec2 point = HeroPositionAt(
                heroPos, candidate, moveSpeed, inputDelayMs, activeStartMs, teleport);
            result.minimumClearance = clearance(point);
            if (result.minimumClearance <= 0.0f) {
                result.collides = true;
                result.firstContactMs = activeStartMs;
            }
        }
        return result;
    }

    static ContinuousCollisionResult AnalyzeMovingLine(
        const Threat& threat,
        const Vec2& heroPos,
        const Vec2& candidate,
        float moveSpeed,
        float inputDelayMs,
        float heroRadius,
        float buffer,
        int now,
        float horizonMs,
        float analysisStartMs = 0.0f,
        bool teleport = false) {
        ContinuousCollisionResult result;
        if (threat.projectileTerminated || threat.Type() != ZDSpellType::Line ||
            !threat.HasTravelSpeed()) return result;
        const float radius = std::max(
            0.0f,
            threat.Radius() + heroRadius + buffer + threat.PositionUncertainty());
        const float travelStartMs =
            static_cast<float>(TickDifference(TravelStartTick(threat), now));
        const float travelEndMs =
            static_cast<float>(TickDifference(TravelEndTick(threat), now));
        const float activeStartMs = std::max({0.0f, analysisStartMs, travelStartMs});
        const float requestedHorizonMs = std::max(0.0f, horizonMs);
        const float activeEndMs =
            (threat.missileBound && !threat.projectileTerminated) ||
                threat.persistent
            ? requestedHorizonMs
            : std::min(
                requestedHorizonMs,
                static_cast<float>(TickDifference(
                    threat.MovingLineTerminalActiveEndTick(), now)));
        if (activeEndMs < activeStartMs) return result;

        const float moveDurationMs = teleport
            ? 0.0f
            : 1000.0f * heroPos.Distance(candidate) / std::max(50.0f, moveSpeed);
        const float arrivalMs = inputDelayMs + moveDurationMs;
        std::array<float, 6> boundaries = {
            activeStartMs,
            activeEndMs,
            std::clamp(inputDelayMs, activeStartMs, activeEndMs),
            std::clamp(arrivalMs, activeStartMs, activeEndMs),
            std::clamp(travelEndMs, activeStartMs, activeEndMs),
            std::clamp(travelStartMs, activeStartMs, activeEndMs),
        };
        std::sort(boundaries.begin(), boundaries.end());
        const Vec2 heroDirection = (candidate - heroPos).Normalized();
        const Vec2 missileVelocity = threat.direction * threat.Speed();

        for (std::size_t index = 0; index + 1 < boundaries.size(); ++index) {
            const float beginMs = boundaries[index];
            const float endMs = boundaries[index + 1];
            if (endMs - beginMs <= 0.001f) continue;
            const float midpointMs = (beginMs + endMs) * 0.5f;
            const Vec2 firstPosition = HeroPositionAt(
                heroPos, candidate, moveSpeed, inputDelayMs, beginMs, teleport);
            const bool heroMoving = !teleport && midpointMs > inputDelayMs && midpointMs < arrivalMs;
            const Vec2 firstVelocity = heroMoving
                ? heroDirection * std::max(50.0f, moveSpeed)
                : Vec2();
            const Vec2 secondPosition = MovingLineHeadAtTime(
                threat, now, beginMs);
            const bool missileMoving = midpointMs >= travelStartMs && midpointMs < travelEndMs;
            const Vec2 secondVelocity = missileMoving ? missileVelocity : Vec2();
            const float durationSeconds = (endMs - beginMs) / 1000.0f;
            const float distance = EvadeGeometryMath::ClosestApproachDistance(
                firstPosition,
                firstVelocity,
                secondPosition,
                secondVelocity,
                durationSeconds);
            result.minimumClearance = std::min(result.minimumClearance, distance - radius);
            const float contact = EvadeGeometryMath::FirstContactTime(
                firstPosition,
                firstVelocity,
                secondPosition,
                secondVelocity,
                radius,
                durationSeconds);
            if (contact == FLT_MAX) continue;
            result.collides = true;
            result.firstContactMs = std::min(
                result.firstContactMs,
                beginMs + contact * 1000.0f);
        }

        if (activeEndMs - activeStartMs <= 0.001f) {
            const Vec2 firstPosition = HeroPositionAt(
                heroPos, candidate, moveSpeed, inputDelayMs, activeStartMs, teleport);
            const Vec2 secondPosition = MovingLineHeadAtTime(
                threat, now, activeStartMs);
            result.minimumClearance = firstPosition.Distance(secondPosition) - radius;
            if (result.minimumClearance <= 0.0f) {
                result.collides = true;
                result.firstContactMs = activeStartMs;
            }
        }
        return result;
    }

    static float SignedClearanceAt(const Threat& threat,
                                   const Vec2& point,
                                   float heroRadius,
                                   float extraBuffer,
                                   int tick) {
        if (threat.Type() == ZDSpellType::Arc) return -FLT_MAX;
        const float uncertainty = threat.HasTravelSpeed()
            ? threat.PositionUncertainty()
            : 0.0f;
        if (threat.Type() == ZDSpellType::Cone) {
            if (!threat.HasValidConeAngle()) return -1.0f;
            const Vec2 direction = threat.direction.IsZero()
                ? (threat.endPos - threat.startPos).Normalized()
                : threat.direction;
            return EvadeGeometryMath::SignedDistanceToSector(
                point,
                threat.startPos,
                direction,
                threat.Range(),
                threat.Angle() * 0.5f * kDegToRad) -
                ConeExpansion(
                    threat,
                    heroRadius,
                    extraBuffer,
                    threat.PositionUncertainty());
        }
        const float radius = std::max(
            0.0f,
            threat.Radius() + heroRadius + extraBuffer + uncertainty);
        if (threat.Type() == ZDSpellType::Line) {
            if (threat.HasTravelSpeed()) return point.Distance(threat.HeadAtTick(tick)) - radius;
            return DistanceToSegment(point, threat.startPos, threat.endPos, nullptr, nullptr) - radius;
        }
        if (threat.Type() == ZDSpellType::Circular) return point.Distance(threat.endPos) - radius;
        if (threat.Type() == ZDSpellType::Ring) {
            return RingSignedClearance(
                point,
                threat.endPos,
                std::max(
                    0.0f,
                    threat.InnerRadius() - heroRadius - extraBuffer - uncertainty),
                radius);
        }
        return -FLT_MAX;
    }

    static float RingSignedClearance(const Vec2& point,
                                     const Vec2& center,
                                     float innerRadius,
                                     float outerRadius) {
        const float inner = std::clamp(innerRadius, 0.0f, std::max(0.0f, outerRadius));
        const float outer = std::max(inner, outerRadius);
        const float distance = point.Distance(center);
        if (distance < inner) return inner - distance;
        if (distance > outer) return distance - outer;
        return -std::min(distance - inner, outer - distance);
    }

    static bool ContainsLineAt(const Threat& threat,
                               const Vec2& point,
                               float radius,
                               int tick) {
        const Vec2 head = threat.HeadAtTick(tick);
        bool onSegment = false;
        const float distance = DistanceToSegment(point, head, threat.endPos, &onSegment, nullptr);
        if (onSegment && distance <= radius) return true;
        return point.Distance(head) <= radius || point.Distance(threat.endPos) <= radius;
    }

    static bool ContainsCone(const Threat& threat,
                             const Vec2& point,
                             float expansion) {
        if (!threat.HasValidConeAngle()) return true;
        const Vec2 direction = threat.direction.IsZero()
            ? (threat.endPos - threat.startPos).Normalized()
            : threat.direction;
        if (direction.IsZero()) return true;
        return EvadeGeometryMath::SignedDistanceToSector(
            point,
            threat.startPos,
            direction,
            threat.Range(),
            threat.Angle() * 0.5f * kDegToRad) <= std::max(0.0f, expansion);
    }

    static float ConeExpansion(const Threat& threat,
                               float heroRadius,
                               float extraBuffer,
                               float uncertainty) {
        return std::max(0.0f, heroRadius) +
            std::max(0.0f, extraBuffer) +
            std::max(0.0f, uncertainty) +
            threat.ConeEdgePadding();
    }

};

}
