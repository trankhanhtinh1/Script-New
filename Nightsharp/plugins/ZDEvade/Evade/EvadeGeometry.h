#pragma once

#include "EvadeTypes.h"
#include "EvadeMath.h"
#include "../../../Core/CoreNavGrid.h"
#include "../../../SDK/SDK.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cfloat>
#include <vector>

namespace ZDEvade {

class EvadeGeometry {
public:
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
        if (threat.expired || tick > threat.endTick + 100) return false;
        if ((threat.Type() == ZDSpellType::Line || threat.Type() == ZDSpellType::Arc) &&
            threat.HasTravelSpeed()) {
            return tick >= TravelStartTick(threat);
        }
        const int activation = threat.Type() == ZDSpellType::Circular && threat.HasTravelSpeed()
            ? ImpactTickAt(threat, threat.endPos)
            : threat.startTick + threat.Delay();
        const int persistence = std::max(100, threat.ExtraEndTime());
        return tick >= activation - 120 && tick <= activation + persistence;
    }

    static int ImpactTickAt(const Threat& threat, const Vec2& point) {
        if (threat.Type() == ZDSpellType::Line && threat.HasTravelSpeed()) {
            bool onSegment = false;
            Vec2 projection;
            DistanceToSegment(point, threat.startPos, threat.endPos, &onSegment, &projection);
            if (threat.observedTick > 0 && threat.observedHead.IsValid() && !threat.observedHead.IsZero()) {
                const float remaining = std::max(
                    0.0f,
                    (projection - threat.observedHead).Dot(threat.direction));
                return threat.observedTick + static_cast<int>(std::ceil(
                    1000.0f * remaining / std::max(1.0f, threat.Speed())));
            }
            const float distance = threat.startPos.Distance(projection);
            const int baseTick = threat.missileBound && threat.launchTick > 0
                ? threat.launchTick
                : threat.startTick + threat.Delay();
            return baseTick + static_cast<int>(std::ceil(1000.0f * distance / threat.Speed()));
        }
        if ((threat.Type() == ZDSpellType::Circular || threat.Type() == ZDSpellType::Arc) &&
            threat.HasTravelSpeed()) {
            if (threat.observedTick > 0 && threat.observedHead.IsValid() && !threat.observedHead.IsZero())
                return threat.observedTick + static_cast<int>(std::ceil(
                    1000.0f * threat.observedHead.Distance(threat.endPos) /
                    std::max(1.0f, threat.Speed())));
            const int baseTick = threat.missileBound && threat.launchTick > 0
                ? threat.launchTick
                : threat.startTick + threat.Delay();
            return baseTick + static_cast<int>(std::ceil(
                1000.0f * threat.startPos.Distance(threat.endPos) / threat.Speed()));
        }
        return threat.startTick + threat.Delay();
    }

    static float TimeMarginAt(const Threat& threat,
                              const Vec2& point,
                              int arrivalTick) {
        return static_cast<float>(ImpactTickAt(threat, point) - arrivalTick);
    }

    static bool ContainsAt(const Threat& threat,
                           const Vec2& point,
                           float heroRadius,
                           float extraBuffer,
                           int tick) {
        if (!ThreatActiveAt(threat, tick)) return false;
        const float uncertainty = threat.HasTravelSpeed()
            ? threat.PositionUncertainty()
            : 0.0f;
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
                    threat.InnerRadius() - heroRadius - extraBuffer - uncertainty),
                radius) <= 0.0f;
        case ZDSpellType::Cone:
            return ContainsCone(threat, point, heroRadius + extraBuffer + uncertainty);
        case ZDSpellType::Arc:
            return ContainsArc(threat, point, radius);
        default:
            return true;
        }
    }

    static bool OccupiesAt(const Threat& threat,
                           const Vec2& point,
                           float heroRadius,
                           float extraBuffer,
                           int tick) {
        if (threat.Type() == ZDSpellType::Line && threat.HasTravelSpeed()) {
            if (!ThreatActiveAt(threat, tick) || tick > TravelEndTick(threat) +
                std::max(80, threat.ExtraEndTime())) return false;
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
                                bool left) {
        const Vec2 head = threat.HeadAtTick(SDK::Variables::TickCount());
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
        const float distance = threat.Radius() + heroRadius + extraBuffer +
            threat.PositionUncertainty();
        return projection + away * distance;
    }

    static Vec2 ClosestCircleExit(const Threat& threat,
                                  const Vec2& heroPos,
                                  float heroRadius,
                                  float extraBuffer) {
        Vec2 direction = (heroPos - threat.endPos).Normalized();
        if (direction.IsZero()) direction = (heroPos - threat.startPos).Normalized();
        if (direction.IsZero()) direction = Vec2(1.0f, 0.0f);
        return threat.endPos + direction * (
            threat.Radius() + heroRadius + extraBuffer + threat.PositionUncertainty());
    }

    static Vec2 ClosestRingExit(const Threat& threat,
                                const Vec2& heroPos,
                                float heroRadius,
                                float extraBuffer) {
        const float expansion = heroRadius + extraBuffer + threat.PositionUncertainty();
        const float outerRadius = threat.Radius() + expansion;
        const float innerRadius = std::max(0.0f, threat.InnerRadius() - expansion);
        Vec2 direction = (heroPos - threat.endPos).Normalized();
        if (direction.IsZero()) direction = (heroPos - threat.startPos).Normalized();
        if (direction.IsZero()) direction = Vec2(1.0f, 0.0f);
        const float distance = heroPos.Distance(threat.endPos);
        if (innerRadius > 2.0f && distance - innerRadius <= outerRadius - distance)
            return threat.endPos + direction * (innerRadius - 2.0f);
        return threat.endPos + direction * (outerRadius + 2.0f);
    }

    static void AddConeExits(const Threat& threat,
                             const Vec2& heroPos,
                             float heroRadius,
                             float extraBuffer,
                             std::vector<Vec2>& output) {
        const Vec2 baseDirection = threat.direction.IsZero()
            ? (threat.endPos - threat.startPos).Normalized()
            : threat.direction;
        const float halfAngle = std::max(0.05f, threat.Angle() * 0.5f * kDegToRad);
        const Vec2 relative = heroPos - threat.startPos;
        const float distance = relative.Length();
        const float currentAngle = SignedAngle(baseDirection, relative);
        const float boundaryAngle = currentAngle >= 0.0f ? halfAngle : -halfAngle;
        const float expansion = threat.Radius() + heroRadius + extraBuffer +
            threat.PositionUncertainty();
        const float angularBuffer = expansion / std::max(50.0f, distance);
        const float outsideAngle = boundaryAngle + (boundaryAngle >= 0.0f ? angularBuffer : -angularBuffer);
        output.push_back(threat.startPos + Rotate(baseDirection, outsideAngle) * std::max(50.0f, distance));
        output.push_back(threat.startPos + Rotate(baseDirection, -outsideAngle) * std::max(50.0f, distance));
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

    static Vec2 ClosestArcExit(const Threat& threat,
                               const Vec2& heroPos,
                               float heroRadius,
                               float extraBuffer) {
        Vec2 projection;
        DistanceToSegment(heroPos, threat.startPos, threat.endPos, nullptr, &projection);
        Vec2 direction = (heroPos - projection).Normalized();
        if (direction.IsZero()) direction = Vec2(-threat.direction.y, threat.direction.x);
        if (direction.IsZero()) direction = Vec2(1.0f, 0.0f);
        const float chordLength = threat.startPos.Distance(threat.endPos);
        const float expansion = threat.Radius() + heroRadius + extraBuffer +
            threat.PositionUncertainty() + chordLength * 0.18f;
        return projection + direction * expansion;
    }

    static bool WalkablePath(const Vec2& from,
                             const Vec2& to,
                             float planeY) {
        return WalkablePath(std::vector<Vec2>{from, to}, planeY);
    }

    static bool WalkablePath(const std::vector<Vec2>& path,
                             float planeY) {
        if (path.size() < 2) return false;
        for (std::size_t index = 1; index < path.size(); ++index) {
            const Vec2& from = path[index - 1];
            const Vec2& to = path[index];
            if (!from.IsValid() || !to.IsValid() || to.IsZero()) return false;
            const Vec3 worldFrom = Vec3::From2D(from, planeY);
            const Vec3 worldTo = Vec3::From2D(to, planeY);
            if (!CoreNavGrid::IsWalkable(worldTo) ||
                CoreNavGrid::IsWallBetween(worldFrom, worldTo)) return false;
        }
        return true;
    }

    static bool HeroThreatenedNow(const std::vector<Threat>& threats,
                                  const Vec2& heroPos,
                                  float heroRadius,
                                  float buffer,
                                  int now,
                                  float horizonMs) {
        for (const auto& threat : threats) {
            if (threat.IsExpiredAt(now)) continue;
            if (threat.Type() == ZDSpellType::Line && threat.HasTravelSpeed()) {
                if (AnalyzeMovingLine(
                        threat,
                        heroPos,
                        heroPos,
                        50.0f,
                        0.0f,
                        heroRadius,
                        buffer,
                        now,
                        horizonMs).collides) return true;
                continue;
            }
            const int impact = ImpactTickAt(threat, heroPos);
            if (impact - now > static_cast<int>(horizonMs)) continue;
            if (ContainsAt(threat, heroPos, heroRadius, buffer, now) ||
                ContainsAt(threat, heroPos, heroRadius, buffer, impact)) return true;
        }
        return false;
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
        result.exitDistance = result.travelDistance;
        result.arrivalTimeMs = std::max(0.0f, castDelayMs);
        result.cursorDistance = candidate.Distance(cursorPos);
        result.valid = candidate.IsValid() && !candidate.IsZero() && result.travelDistance >= 1.0f;
        if (!result.valid) return result;
        result.walkable = CoreNavGrid::IsWalkable(Vec3::From2D(candidate, planeY));
        if (!result.walkable) {
            result.rejectReason = PlannerRejectReason::Wall;
            return result;
        }

        const int arrivalTick = now + static_cast<int>(std::round(result.arrivalTimeMs));
        float firstCollisionMs = FLT_MAX;
        for (const auto& threat : threats) {
            if (threat.IsExpiredAt(now)) continue;
            bool castCollision = false;
            bool endpointCollision = false;
            if (threat.Type() == ZDSpellType::Line && threat.HasTravelSpeed()) {
                const ContinuousCollisionResult collision = AnalyzeMovingLine(
                    threat,
                    heroPos,
                    candidate,
                    100000.0f,
                    result.arrivalTimeMs,
                    heroRadius,
                    settings.endpointBuffer,
                    now,
                    settings.maxThreatHorizonMs,
                    0.0f,
                    true);
                result.minimumClearance = std::min(
                    result.minimumClearance,
                    collision.minimumClearance);
                castCollision = collision.collides && collision.firstContactMs < result.arrivalTimeMs;
                endpointCollision = collision.collides && collision.firstContactMs >= result.arrivalTimeMs;
                if (collision.collides)
                    firstCollisionMs = std::min(firstCollisionMs, collision.firstContactMs);
            } else {
                const int impactTick = ImpactTickAt(threat, candidate);
                if (impactTick - now > static_cast<int>(settings.maxThreatHorizonMs)) continue;
                const ContinuousCollisionResult collision = AnalyzeStaticThreat(
                    threat,
                    heroPos,
                    candidate,
                    100000.0f,
                    result.arrivalTimeMs,
                    heroRadius,
                    settings.pathBuffer,
                    now,
                    0.0f,
                    result.arrivalTimeMs,
                    true);
                result.minimumClearance = std::min(
                    result.minimumClearance,
                    collision.minimumClearance);
                castCollision = collision.collides &&
                    collision.firstContactMs < result.arrivalTimeMs;
                if (collision.collides)
                    firstCollisionMs = std::min(firstCollisionMs, collision.firstContactMs);
                endpointCollision = ContainsAt(
                    threat,
                    candidate,
                    heroRadius,
                    settings.endpointBuffer,
                    arrivalTick) ||
                    (impactTick >= arrivalTick && ContainsAt(
                        threat,
                        candidate,
                        heroRadius,
                        settings.endpointBuffer,
                        impactTick));
            }
            if (castCollision) result.pathDanger += threat.Danger();
            if (endpointCollision) result.endpointDanger += threat.Danger();
            if (!castCollision && !endpointCollision) continue;
            result.maxDanger = std::max(result.maxDanger, threat.Danger());
            ++result.collisionCount;
        }
        if (result.minimumClearance == FLT_MAX) result.minimumClearance = settings.maxSearchRadius;
        result.pathSafe = result.pathDanger == 0;
        result.endpointSafe = result.endpointDanger == 0;
        result.firstCollisionTimeMs = firstCollisionMs;
        result.timeMarginMs = firstCollisionMs == FLT_MAX
            ? settings.maxThreatHorizonMs
            : firstCollisionMs;
        result.timingSafe = firstCollisionMs == FLT_MAX ||
            firstCollisionMs >= settings.minimumTimeMarginMs;
        result.strictSafe = result.pathSafe && result.endpointSafe && result.timingSafe;
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
        const std::vector<Threat>& threats) {
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
        result.walkable = WalkablePath(heroPos, candidate, planeY);
        if (!result.walkable) {
            result.rejectReason = PlannerRejectReason::Wall;
            return result;
        }

        const float spatialStepMs = 1000.0f * std::max(4.0f, settings.pathStep) / speed;
        const float temporalStepMs = std::clamp(
            std::min(std::max(6.0f, settings.temporalStepMs), spatialStepMs),
            6.0f,
            24.0f);
        const int steps = std::max(
            2,
            static_cast<int>(std::ceil(result.arrivalTimeMs / temporalStepMs)));
        bool foundEnvelopeSafe = false;
        bool envelopeReentered = false;
        bool actualCollision = false;
        float firstSafeDistance = FLT_MAX;
        float firstCollisionMs = FLT_MAX;
        float minimumClearance = FLT_MAX;
        float exposureMs = 0.0f;
        int previousDanger = 0;
        int maximumDanger = 0;
        int maximumPathDanger = 0;
        int collisionCount = 0;

        for (int step = 0; step <= steps; ++step) {
            const float fraction = static_cast<float>(step) / static_cast<float>(steps);
            const float timeMs = result.arrivalTimeMs * fraction;
            const Vec2 point = HeroPositionAt(
                heroPos,
                candidate,
                speed,
                std::max(0.0f, settings.inputDelayMs),
                timeMs,
                false);
            const float travelled = heroPos.Distance(point);
            const int sampleTick = now + static_cast<int>(std::round(timeMs));
            int pointDanger = 0;
            int pointMaxDanger = 0;
            int envelopeDanger = 0;

            for (const auto& threat : threats) {
                if (threat.IsExpiredAt(now)) continue;
                const int impactTick = ImpactTickAt(threat, point);
                const bool predictedEnvelope =
                    impactTick - now <= static_cast<int>(settings.maxThreatHorizonMs) &&
                    impactTick >= now - 100 && ContainsAt(
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
                if (predictedEnvelope || occupied) envelopeDanger += threat.Danger();
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
                const int danger = threat.Danger();
                pointDanger += danger;
                pointMaxDanger = std::max(pointMaxDanger, danger);
                firstCollisionMs = std::min(firstCollisionMs, timeMs);
            }

            if (step > 0) {
                exposureMs += 0.5f * static_cast<float>(previousDanger + pointDanger) *
                    (result.arrivalTimeMs / static_cast<float>(steps));
            }
            previousDanger = pointDanger;
            maximumDanger = std::max(maximumDanger, pointMaxDanger);
            maximumPathDanger = std::max(maximumPathDanger, pointDanger);
            if (envelopeDanger == 0) {
                if (!foundEnvelopeSafe) {
                    foundEnvelopeSafe = true;
                    firstSafeDistance = travelled;
                }
            } else if (foundEnvelopeSafe) {
                envelopeReentered = true;
            }
            if (pointDanger > 0) actualCollision = true;
        }

        for (const auto& threat : threats) {
            if (threat.IsExpiredAt(now)) continue;
            const ContinuousCollisionResult collision =
                threat.Type() == ZDSpellType::Line && threat.HasTravelSpeed()
                    ? AnalyzeMovingLine(
                        threat,
                        heroPos,
                        candidate,
                        speed,
                        std::max(0.0f, settings.inputDelayMs),
                        heroRadius,
                        settings.pathBuffer,
                        now,
                        settings.maxThreatHorizonMs)
                    : AnalyzeStaticThreat(
                        threat,
                        heroPos,
                        candidate,
                        speed,
                        std::max(0.0f, settings.inputDelayMs),
                        heroRadius,
                        settings.pathBuffer,
                        now,
                        0.0f,
                        result.arrivalTimeMs,
                        false);
            minimumClearance = std::min(minimumClearance, collision.minimumClearance);
            if (!collision.collides) continue;
            ++collisionCount;
            actualCollision = true;
            firstCollisionMs = std::min(firstCollisionMs, collision.firstContactMs);
            maximumDanger = std::max(maximumDanger, threat.Danger());
            maximumPathDanger = std::max(maximumPathDanger, threat.Danger());
        }

        const int endpointTick = now + static_cast<int>(std::round(result.arrivalTimeMs));
        int endpointDanger = 0;
        int endpointMaxDanger = 0;
        for (const auto& threat : threats) {
            if (threat.IsExpiredAt(now)) continue;
            bool endpointCollision = false;
            if (threat.Type() == ZDSpellType::Line && threat.HasTravelSpeed()) {
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
                minimumClearance = std::min(minimumClearance, endpoint.minimumClearance);
                endpointCollision = endpoint.collides;
                if (endpoint.collides)
                    firstCollisionMs = std::min(firstCollisionMs, endpoint.firstContactMs);
            } else {
                const int impactTick = ImpactTickAt(threat, candidate);
                if (impactTick - now > static_cast<int>(settings.maxThreatHorizonMs)) continue;
                endpointCollision = ContainsAt(
                    threat,
                    candidate,
                    heroRadius,
                    settings.endpointBuffer,
                    endpointTick) ||
                    (impactTick >= endpointTick && ContainsAt(
                        threat,
                        candidate,
                        heroRadius,
                        settings.endpointBuffer,
                        impactTick));
            }
            if (!endpointCollision) continue;
            endpointDanger += threat.Danger();
            endpointMaxDanger = std::max(endpointMaxDanger, threat.Danger());
        }

        if (minimumClearance == FLT_MAX) minimumClearance = settings.maxSearchRadius;
        result.endpointDanger = endpointDanger;
        result.pathDanger = maximumPathDanger;
        result.maxDanger = std::max(maximumDanger, endpointMaxDanger);
        result.collisionCount = collisionCount;
        result.endpointSafe = endpointDanger == 0;
        result.pathSafe = !actualCollision;
        result.reenteredDanger = envelopeReentered;
        result.exitDistance = foundEnvelopeSafe ? firstSafeDistance : result.travelDistance;
        result.firstCollisionTimeMs = firstCollisionMs;
        result.dangerExposureMs = exposureMs;
        result.minimumClearance = minimumClearance;
        result.timeMarginMs = firstCollisionMs == FLT_MAX
            ? settings.maxThreatHorizonMs
            : firstCollisionMs;
        result.timingSafe = firstCollisionMs == FLT_MAX ||
            firstCollisionMs >= settings.minimumTimeMarginMs;
        result.strictSafe = result.endpointSafe && result.pathSafe && result.timingSafe;
        if (result.strictSafe) {
            result.rejectReason = PlannerRejectReason::None;
        } else if (!result.endpointSafe) {
            result.rejectReason = PlannerRejectReason::EndpointDanger;
        } else if (result.reenteredDanger) {
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
        const std::vector<Threat>& threats) {
        CandidateEvaluation result;
        result.source = source;
        result.sourceThreatId = sourceThreatId;
        result.position = path.empty() ? Vec2() : path.back();
        result.cursorDistance = path.empty() ? FLT_MAX : path.back().Distance(cursorPos);
        result.travelDistance = EvadeGeometryMath::PolylineLength(path);
        result.exitDistance = result.travelDistance;
        result.valid = path.size() >= 2 && result.position.IsValid() &&
            !result.position.IsZero() && result.travelDistance >= 1.0f;
        if (!result.valid) {
            result.rejectReason = PlannerRejectReason::Invalid;
            return result;
        }
        result.walkable = WalkablePath(path, planeY);
        if (!result.walkable) {
            result.rejectReason = PlannerRejectReason::Wall;
            return result;
        }

        result.pathSafe = true;
        result.endpointSafe = true;
        result.timingSafe = true;
        result.minimumClearance = FLT_MAX;
        result.firstCollisionTimeMs = FLT_MAX;
        float elapsedMs = 0.0f;
        float distanceBefore = 0.0f;
        bool foundExit = false;
        for (std::size_t index = 1; index < path.size(); ++index) {
            if (path[index - 1].Distance(path[index]) <= 0.001f) continue;
            EvadeSettings segmentSettings = settings;
            segmentSettings.inputDelayMs = index == 1
                ? std::max(0.0f, settings.inputDelayMs)
                : 0.0f;
            const CandidateEvaluation segment = EvaluateCandidate(
                path[index],
                source,
                sourceThreatId,
                path[index - 1],
                cursorPos,
                planeY,
                moveSpeed,
                heroRadius,
                now + static_cast<int>(std::round(elapsedMs)),
                segmentSettings,
                threats);
            if (!segment.valid || !segment.walkable) {
                result.walkable = false;
                result.rejectReason = segment.rejectReason;
                return result;
            }
            result.pathSafe = result.pathSafe && segment.pathSafe;
            result.pathDanger = std::max(result.pathDanger, segment.pathDanger);
            result.maxDanger = std::max(result.maxDanger, segment.maxDanger);
            result.collisionCount += segment.collisionCount;
            result.dangerExposureMs += segment.dangerExposureMs;
            result.reenteredDanger = result.reenteredDanger || segment.reenteredDanger;
            result.minimumClearance = std::min(
                result.minimumClearance,
                segment.minimumClearance);
            if (!foundExit && segment.exitDistance < segment.travelDistance) {
                result.exitDistance = distanceBefore + segment.exitDistance;
                foundExit = true;
            }
            if (segment.firstCollisionTimeMs != FLT_MAX) {
                result.firstCollisionTimeMs = std::min(
                    result.firstCollisionTimeMs,
                    elapsedMs + segment.firstCollisionTimeMs);
            }
            elapsedMs += segment.arrivalTimeMs;
            distanceBefore += segment.travelDistance;
            if (index + 1 == path.size()) {
                result.endpointSafe = segment.endpointSafe;
                result.endpointDanger = segment.endpointDanger;
            }
        }
        result.arrivalTimeMs = elapsedMs;
        if (result.minimumClearance == FLT_MAX)
            result.minimumClearance = settings.maxSearchRadius;
        result.timeMarginMs = result.firstCollisionTimeMs == FLT_MAX
            ? settings.maxThreatHorizonMs
            : result.firstCollisionTimeMs;
        result.timingSafe = result.firstCollisionTimeMs == FLT_MAX ||
            result.firstCollisionTimeMs >= settings.minimumTimeMarginMs;
        result.strictSafe = result.pathSafe && result.endpointSafe && result.timingSafe;
        result.rejectReason = result.strictSafe
            ? PlannerRejectReason::None
            : !result.endpointSafe
                ? PlannerRejectReason::EndpointDanger
                : result.reenteredDanger
                    ? PlannerRejectReason::Reentry
                    : !result.pathSafe
                        ? PlannerRejectReason::PathDanger
                        : PlannerRejectReason::Late;
        return result;
    }

private:
    struct ContinuousCollisionResult {
        bool collides = false;
        float firstContactMs = FLT_MAX;
        float minimumClearance = FLT_MAX;
    };

    static inline constexpr float kPi = 3.14159265358979323846f;
    static inline constexpr float kDegToRad = kPi / 180.0f;

    static int TravelStartTick(const Threat& threat) {
        return threat.missileBound && threat.launchTick > 0
            ? threat.launchTick
            : threat.startTick + threat.Delay();
    }

    static int TravelEndTick(const Threat& threat) {
        if (!threat.HasTravelSpeed()) return threat.startTick + threat.Delay();
        if (threat.observedTick > 0 && threat.observedHead.IsValid() && !threat.observedHead.IsZero()) {
            const float remaining = std::max(
                0.0f,
                (threat.endPos - threat.observedHead).Dot(threat.direction));
            return threat.observedTick + static_cast<int>(std::ceil(
                1000.0f * remaining / std::max(1.0f, threat.Speed())));
        }
        return TravelStartTick(threat) + static_cast<int>(std::ceil(
            1000.0f * threat.startPos.Distance(threat.endPos) / threat.Speed()));
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
        if (threat.Type() == ZDSpellType::Line && threat.HasTravelSpeed()) return result;
        int activeStartTick = threat.startTick + threat.Delay() - 120;
        int activeEndTick = threat.startTick + threat.Delay() +
            std::max(100, threat.ExtraEndTime());
        if (threat.Type() == ZDSpellType::Circular && threat.HasTravelSpeed()) {
            const int impact = ImpactTickAt(threat, threat.endPos);
            activeStartTick = impact - 120;
            activeEndTick = impact + std::max(100, threat.ExtraEndTime());
        } else if (threat.Type() == ZDSpellType::Arc && threat.HasTravelSpeed()) {
            activeStartTick = TravelStartTick(threat);
            activeEndTick = threat.endTick + 100;
        }
        const float activeStartMs = std::max(
            std::max(0.0f, analysisStartMs),
            static_cast<float>(activeStartTick - now));
        const float activeEndMs = std::min(
            std::max(0.0f, analysisEndMs),
            static_cast<float>(activeEndTick - now));
        if (activeEndMs < activeStartMs) return result;

        const float radius = std::max(
            0.0f,
            threat.Radius() + heroRadius + buffer + threat.PositionUncertainty());
        const Vec2 direction = threat.direction.IsZero()
            ? (threat.endPos - threat.startPos).Normalized()
            : threat.direction;
        const float chordLength = threat.startPos.Distance(threat.endPos);
        const float arcRadius = radius + chordLength * 0.18f;
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
                return EvadeGeometryMath::SignedDistanceToSector(
                    point,
                    threat.startPos,
                    direction,
                    threat.Range(),
                    std::max(0.05f, threat.Angle() * 0.5f * kDegToRad)) - radius;
            case ZDSpellType::Arc:
                return DistanceToSegment(point, threat.startPos, threat.endPos) - arcRadius;
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
                intervalClearance = EvadeGeometryMath::MinimumSignedDistanceAlongSegment(
                    start, end, clearance, 64);
                contact = EvadeGeometryMath::FirstContactMovingPointSector(
                    start,
                    end,
                    threat.startPos,
                    direction,
                    threat.Range(),
                    std::max(0.05f, threat.Angle() * 0.5f * kDegToRad),
                    radius,
                    0.02f);
                break;
            case ZDSpellType::Arc:
                intervalClearance = EvadeGeometryMath::SegmentSegmentDistance(
                    start, end, threat.startPos, threat.endPos) - arcRadius;
                contact = EvadeGeometryMath::FirstContactMovingPointCapsule(
                    start, end, threat.startPos, threat.endPos, arcRadius);
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
        if (threat.Type() != ZDSpellType::Line || !threat.HasTravelSpeed()) return result;
        const float radius = std::max(
            0.0f,
            threat.Radius() + heroRadius + buffer + threat.PositionUncertainty());
        const float travelStartMs = static_cast<float>(TravelStartTick(threat) - now);
        const float travelEndMs = static_cast<float>(TravelEndTick(threat) - now);
        const float activeStartMs = std::max({0.0f, analysisStartMs, travelStartMs});
        const float activeEndMs = std::min(
            std::max(0.0f, horizonMs),
            std::min(
                static_cast<float>(threat.endTick + 100 - now),
                travelEndMs + static_cast<float>(std::max(30, threat.ExtraEndTime()))));
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
            const Vec2 secondPosition = threat.HeadAtTick(
                now + static_cast<int>(std::round(beginMs)));
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
            const Vec2 secondPosition = threat.HeadAtTick(
                now + static_cast<int>(std::round(activeStartMs)));
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
        const float uncertainty = threat.HasTravelSpeed()
            ? threat.PositionUncertainty()
            : 0.0f;
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
        if (threat.Type() == ZDSpellType::Cone) {
            const Vec2 direction = threat.direction.IsZero()
                ? (threat.endPos - threat.startPos).Normalized()
                : threat.direction;
            return EvadeGeometryMath::SignedDistanceToSector(
                point,
                threat.startPos,
                direction,
                threat.Range(),
                std::max(0.05f, threat.Angle() * 0.5f * kDegToRad)) - radius;
        }
        const float chordLength = threat.startPos.Distance(threat.endPos);
        const float conservativeRadius = radius + chordLength * 0.18f;
        return DistanceToSegment(point, threat.startPos, threat.endPos, nullptr, nullptr) -
            conservativeRadius;
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
        const Vec2 direction = threat.direction.IsZero()
            ? (threat.endPos - threat.startPos).Normalized()
            : threat.direction;
        if (direction.IsZero()) return true;
        return EvadeGeometryMath::SignedDistanceToSector(
            point,
            threat.startPos,
            direction,
            threat.Range(),
            std::max(0.05f, threat.Angle() * 0.5f * kDegToRad)) <=
            std::max(0.0f, threat.Radius() + expansion);
    }

    static bool ContainsArc(const Threat& threat,
                            const Vec2& point,
                            float radius) {
        const Vec2 chord = threat.endPos - threat.startPos;
        const float chordLength = chord.Length();
        if (chordLength < 1.0f) return point.Distance(threat.endPos) <= radius;
        const float conservativeRadius = radius + chordLength * 0.18f;
        const float distance = DistanceToSegment(point, threat.startPos, threat.endPos, nullptr, nullptr);
        return distance <= conservativeRadius;
    }
};

}
