#pragma once

#include "EvadeGeometry.h"
#include "../../../SDK/SDK.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace ZDEvade {

class EvadePlanner {
public:
    static PlannerResult FindBest(const SDK::AIHeroClient& player,
                                  const std::vector<Threat>& threats,
                                  const EvadeSettings& settings) {
        PlannerResult result;
        if (!player.IsValid() || player.IsDead() || threats.empty()) return result;

        const int now = SDK::Variables::TickCount();
        const Vec2 heroPos = player.ServerPosition().To2D();
        const Vec2 cursorPos = SDK::Game::CursorPos().To2D();
        const float heroRadius = std::max(10.0f, player.BoundingRadius());
        const float moveSpeed = std::max(50.0f, player.MoveSpeed());
        const float planeY = player.ServerPosition().y;
        const EnvironmentSnapshot environment = CaptureEnvironment(heroRadius);
        const int maxCandidates = std::max(32, settings.maxCandidates);
        if (!heroPos.IsValid() || heroPos.IsZero()) return result;

        std::vector<CandidateSeed> seeds;
        seeds.reserve(static_cast<std::size_t>(maxCandidates));
        AddAnalyticalCandidates(seeds, threats, heroPos, heroRadius, settings, now);
        AddIntersectionCandidates(seeds, threats, heroPos, heroRadius, settings, now);
        AddCursorCandidate(seeds, heroPos, cursorPos, settings);

        result.candidates.reserve(static_cast<std::size_t>(maxCandidates));
        CandidateEvaluation bestStrict;
        CandidateEvaluation bestFallback;
        bool hasStrict = false;
        bool hasFallback = false;

        const auto evaluateSeed = [&](const CandidateSeed& seed) {
            CandidateEvaluation evaluation = EvadeGeometry::EvaluateCandidate(
                seed.position,
                seed.source,
                seed.threatId,
                heroPos,
                cursorPos,
                planeY,
                moveSpeed,
                heroRadius,
                now,
                settings,
                threats);
            AddEnvironmentalMetrics(evaluation, environment);
            result.candidates.push_back(evaluation);
            if (evaluation.strictSafe && (!hasStrict || StrictBetter(evaluation, bestStrict, settings))) {
                bestStrict = evaluation;
                hasStrict = true;
            }
            if (evaluation.valid && evaluation.walkable &&
                (!hasFallback || FallbackBetter(evaluation, bestFallback))) {
                bestFallback = evaluation;
                hasFallback = true;
            }
        };

        const std::size_t analyticalCount = seeds.size();
        for (std::size_t index = 0; index < analyticalCount; ++index) {
            evaluateSeed(seeds[index]);
        }

        const float ringStep = std::max(20.0f, settings.ringStep);
        const Vec2 cursorDirection = (cursorPos - heroPos).Normalized();
        const float ringOffset = cursorDirection.IsZero()
            ? 0.0f
            : std::atan2(cursorDirection.y, cursorDirection.x);
        for (float radius = ringStep;
             radius <= settings.maxSearchRadius && static_cast<int>(seeds.size()) < maxCandidates;
             radius += ringStep) {
            const bool robustStrict = hasStrict &&
                bestStrict.minimumClearance >= std::max(0.0f, settings.preferredClearance);
            const float searchLimit = robustStrict
                ? std::min(settings.maxSearchRadius, bestStrict.travelDistance + ringStep)
                : settings.maxSearchRadius;
            if (radius > searchLimit) break;
            const int checks = std::max(12, static_cast<int>(std::ceil(2.0f * kPi * radius / 55.0f)));
            for (int index = 0;
                 index < checks && static_cast<int>(seeds.size()) < maxCandidates;
                 ++index) {
                const float angle = ringOffset +
                    2.0f * kPi * static_cast<float>(index) / static_cast<float>(checks);
                const Vec2 point(
                    heroPos.x + radius * std::cos(angle),
                    heroPos.y + radius * std::sin(angle));
                const std::size_t before = seeds.size();
                AddSeed(seeds, point, PlannerCandidateSource::Ring, -1, maxCandidates);
                if (seeds.size() != before) evaluateSeed(seeds.back());
            }
        }

        if (hasStrict) {
            result.found = true;
            result.strictSafe = true;
            result.selected = bestStrict;
        } else if (hasFallback) {
            result.found = true;
            result.strictSafe = false;
            result.selected = bestFallback;
        }
        return result;
    }

    static CandidateEvaluation EvaluateDestination(
        const SDK::AIHeroClient& player,
        const Vec2& destination,
        PlannerCandidateSource source,
        const std::vector<Threat>& threats,
        const EvadeSettings& settings,
        float travelSpeed,
        float castDelayMs) {
        CandidateEvaluation result;
        if (!player.IsValid()) return result;
        result = EvaluateTravelDestination(
            player,
            destination,
            source,
            threats,
            settings,
            travelSpeed,
            castDelayMs);
        AddEnvironmentalMetrics(
            result,
            CaptureEnvironment(std::max(10.0f, player.BoundingRadius())));
        return result;
    }

    static CandidateEvaluation FindBestSpellPosition(
        const SDK::AIHeroClient& player,
        const std::vector<Threat>& threats,
        const EvadeSettings& settings,
        float range,
        float speed,
        float castDelayMs,
        bool fixedRange,
        bool blink) {
        CandidateEvaluation best;
        if (!player.IsValid() || range <= 0.0f) return best;
        const Vec2 heroPos = player.ServerPosition().To2D();
        const Vec2 cursorPos = SDK::Game::CursorPos().To2D();
        const EnvironmentSnapshot environment = CaptureEnvironment(
            std::max(10.0f, player.BoundingRadius()));
        const int angleCount = 32;
        const int radiusCount = fixedRange ? 1 : 3;
        for (int radiusIndex = 1; radiusIndex <= radiusCount; ++radiusIndex) {
            const float radius = fixedRange
                ? range
                : range * static_cast<float>(radiusIndex) / static_cast<float>(radiusCount);
            for (int angleIndex = 0; angleIndex < angleCount; ++angleIndex) {
                const float angle = 2.0f * kPi * static_cast<float>(angleIndex) / static_cast<float>(angleCount);
                const Vec2 candidate(
                    heroPos.x + radius * std::cos(angle),
                    heroPos.y + radius * std::sin(angle));
                CandidateEvaluation evaluation = blink
                    ? EvadeGeometry::EvaluateBlinkCandidate(
                        candidate,
                        heroPos,
                        cursorPos,
                        player.ServerPosition().y,
                        std::max(10.0f, player.BoundingRadius()),
                        SDK::Variables::TickCount(),
                        castDelayMs,
                        settings,
                        threats)
                    : EvaluateTravelDestination(
                        player,
                        candidate,
                        PlannerCandidateSource::EvadeSpell,
                        threats,
                        settings,
                        std::max(50.0f, speed),
                        castDelayMs);
                AddEnvironmentalMetrics(evaluation, environment);
                if (!evaluation.strictSafe) continue;
                if (!best.valid || StrictBetter(evaluation, best, settings)) best = evaluation;
            }
        }
        if (!cursorPos.IsZero()) {
            const Vec2 direction = (cursorPos - heroPos).Normalized();
            if (!direction.IsZero()) {
                const Vec2 candidate = heroPos + direction * range;
                CandidateEvaluation evaluation = blink
                    ? EvadeGeometry::EvaluateBlinkCandidate(
                        candidate,
                        heroPos,
                        cursorPos,
                        player.ServerPosition().y,
                        std::max(10.0f, player.BoundingRadius()),
                        SDK::Variables::TickCount(),
                        castDelayMs,
                        settings,
                        threats)
                    : EvaluateTravelDestination(
                        player,
                        candidate,
                        PlannerCandidateSource::EvadeSpell,
                        threats,
                        settings,
                        std::max(50.0f, speed),
                        castDelayMs);
                AddEnvironmentalMetrics(evaluation, environment);
                if (evaluation.strictSafe && (!best.valid || StrictBetter(evaluation, best, settings))) best = evaluation;
            }
        }
        return best;
    }

private:
    struct EnvironmentSnapshot {
        std::vector<Vec2> enemies;
        std::vector<Vec2> turrets;
        float turretRange = 875.0f;
    };

    struct CandidateSeed {
        Vec2 position = {};
        PlannerCandidateSource source = PlannerCandidateSource::Unknown;
        int threatId = -1;
    };

    static inline constexpr float kPi = 3.14159265358979323846f;

    static bool NearlyEqual(float left, float right, float epsilon = 0.5f) {
        return std::fabs(left - right) <= epsilon;
    }

    static bool RelevantAtHero(const Threat& threat,
                               const Vec2& heroPos,
                               float heroRadius,
                               const EvadeSettings& settings,
                               int now) {
        if (threat.IsExpiredAt(now)) return false;
        const int impact = EvadeGeometry::ImpactTickAt(threat, heroPos);
        if (impact - now > static_cast<int>(settings.maxThreatHorizonMs)) return false;
        return EvadeGeometry::ContainsAt(
                   threat,
                   heroPos,
                   heroRadius,
                   settings.pathBuffer,
                   now) ||
               EvadeGeometry::ContainsAt(
                   threat,
                   heroPos,
                   heroRadius,
                   settings.pathBuffer,
                   impact);
    }

    static bool PotentiallyRelevant(const Threat& threat,
                                    const Vec2& heroPos,
                                    float heroRadius,
                                    const EvadeSettings& settings,
                                    int now) {
        if (threat.IsExpiredAt(now)) return false;
        const int impact = EvadeGeometry::ImpactTickAt(threat, heroPos);
        if (impact - now > static_cast<int>(settings.maxThreatHorizonMs)) return false;
        const float reach = settings.maxSearchRadius + heroRadius +
            threat.Radius() + settings.endpointBuffer;
        switch (threat.Type()) {
        case ZDSpellType::Line: {
            const Vec2 activeStart = threat.HasTravelSpeed()
                ? threat.HeadAtTick(now)
                : threat.startPos;
            return EvadeGeometry::DistanceToSegment(
                heroPos, activeStart, threat.endPos) <= reach;
        }
        case ZDSpellType::Arc:
            return EvadeGeometry::DistanceToSegment(
                heroPos, threat.startPos, threat.endPos) <= reach;
        case ZDSpellType::Circular:
        case ZDSpellType::Ring:
            return heroPos.Distance(threat.endPos) <= reach;
        case ZDSpellType::Cone:
            return heroPos.Distance(threat.startPos) <= threat.Range() + reach;
        default:
            return false;
        }
    }

    static std::vector<const Threat*> OrderedThreats(
        const std::vector<Threat>& threats,
        const Vec2& heroPos,
        float heroRadius,
        const EvadeSettings& settings,
        int now,
        bool directOnly,
        std::size_t limit) {
        std::vector<const Threat*> result;
        result.reserve(threats.size());
        for (const auto& threat : threats) {
            const bool relevant = directOnly
                ? RelevantAtHero(threat, heroPos, heroRadius, settings, now)
                : PotentiallyRelevant(threat, heroPos, heroRadius, settings, now);
            if (relevant) result.push_back(&threat);
        }
        std::sort(result.begin(), result.end(), [&](const Threat* left, const Threat* right) {
            const bool leftDirect = RelevantAtHero(*left, heroPos, heroRadius, settings, now);
            const bool rightDirect = RelevantAtHero(*right, heroPos, heroRadius, settings, now);
            if (leftDirect != rightDirect) return leftDirect;
            const int leftImpact = EvadeGeometry::ImpactTickAt(*left, heroPos);
            const int rightImpact = EvadeGeometry::ImpactTickAt(*right, heroPos);
            if (std::abs(leftImpact - rightImpact) > 40) return leftImpact < rightImpact;
            if (left->Danger() != right->Danger()) return left->Danger() > right->Danger();
            return left->id < right->id;
        });
        if (result.size() > limit) result.resize(limit);
        return result;
    }

    static void AddSeed(std::vector<CandidateSeed>& seeds,
                        const Vec2& position,
                        PlannerCandidateSource source,
                        int threatId,
                        int maxCandidates) {
        if (!position.IsValid() || position.IsZero() ||
            static_cast<int>(seeds.size()) >= maxCandidates) return;
        for (const auto& existing : seeds) {
            if (existing.position.DistanceSqr(position) <= 196.0f) return;
        }
        seeds.push_back({position, source, threatId});
    }

    static void AddAnalyticalCandidates(std::vector<CandidateSeed>& seeds,
                                        const std::vector<Threat>& threats,
                                        const Vec2& heroPos,
                                        float heroRadius,
                                        const EvadeSettings& settings,
                                        int now) {
        const std::vector<const Threat*> relevant = OrderedThreats(
            threats,
            heroPos,
            heroRadius,
            settings,
            now,
            true,
            threats.size());
        for (const Threat* threatPtr : relevant) {
            const Threat& threat = *threatPtr;
            switch (threat.Type()) {
            case ZDSpellType::Line:
                AddSeed(
                    seeds,
                    EvadeGeometry::ClosestLineExit(
                        threat,
                        heroPos,
                        heroRadius,
                        settings.endpointBuffer,
                        true),
                    PlannerCandidateSource::LineLeft,
                    threat.id,
                    settings.maxCandidates);
                AddSeed(
                    seeds,
                    EvadeGeometry::ClosestLineExit(
                        threat,
                        heroPos,
                        heroRadius,
                        settings.endpointBuffer,
                        false),
                    PlannerCandidateSource::LineRight,
                    threat.id,
                    settings.maxCandidates);
                break;
            case ZDSpellType::Circular:
                AddSeed(
                    seeds,
                    EvadeGeometry::ClosestCircleExit(
                        threat,
                        heroPos,
                        heroRadius,
                        settings.endpointBuffer),
                    PlannerCandidateSource::CircleExit,
                    threat.id,
                    settings.maxCandidates);
                break;
            case ZDSpellType::Ring:
                AddSeed(
                    seeds,
                    EvadeGeometry::ClosestRingExit(
                        threat,
                        heroPos,
                        heroRadius,
                        settings.endpointBuffer),
                    PlannerCandidateSource::CircleExit,
                    threat.id,
                    settings.maxCandidates);
                break;
            case ZDSpellType::Cone: {
                std::vector<Vec2> exits;
                EvadeGeometry::AddConeExits(
                    threat,
                    heroPos,
                    heroRadius,
                    settings.endpointBuffer,
                    exits);
                for (const Vec2& exit : exits) {
                    AddSeed(
                        seeds,
                        exit,
                        PlannerCandidateSource::ConeSide,
                        threat.id,
                        settings.maxCandidates);
                }
                break;
            }
            case ZDSpellType::Arc:
                AddSeed(
                    seeds,
                    EvadeGeometry::ClosestArcExit(
                        threat,
                        heroPos,
                        heroRadius,
                        settings.endpointBuffer),
                    PlannerCandidateSource::ArcExit,
                    threat.id,
                    settings.maxCandidates);
                break;
            }
        }
    }

    static Vec2 BoundaryCenter(const Threat& threat) {
        return threat.Type() == ZDSpellType::Circular || threat.Type() == ZDSpellType::Ring
            ? threat.endPos
            : threat.startPos;
    }

    static float BoundaryRadius(const Threat& threat,
                                float heroRadius,
                                const EvadeSettings& settings) {
        if (threat.Type() == ZDSpellType::Circular || threat.Type() == ZDSpellType::Ring)
            return threat.Radius() + heroRadius + settings.endpointBuffer;
        if (threat.Type() == ZDSpellType::Cone || threat.Type() == ZDSpellType::Arc)
            return threat.Range() + heroRadius + settings.endpointBuffer;
        return 0.0f;
    }

    static void AddIntersectionSeed(std::vector<CandidateSeed>& seeds,
                                    const Vec2& point,
                                    const Vec2& heroPos,
                                    const Vec2& outward,
                                    const EvadeSettings& settings) {
        if (!point.IsValid() || point.IsZero() ||
            point.Distance(heroPos) > settings.maxSearchRadius + settings.ringStep) return;
        Vec2 direction = outward.Normalized();
        if (direction.IsZero()) direction = (point - heroPos).Normalized();
        AddSeed(
            seeds,
            point + direction * std::max(4.0f, settings.endpointBuffer * 0.25f),
            PlannerCandidateSource::Intersection,
            -1,
            settings.maxCandidates);
    }

    static void AddIntersectionCandidates(std::vector<CandidateSeed>& seeds,
                                          const std::vector<Threat>& threats,
                                          const Vec2& heroPos,
                                          float heroRadius,
                                          const EvadeSettings& settings,
                                          int now) {
        const std::vector<const Threat*> relevant = OrderedThreats(
            threats,
            heroPos,
            heroRadius,
            settings,
            now,
            false,
            16);
        for (std::size_t firstIndex = 0; firstIndex < relevant.size(); ++firstIndex) {
            const Threat& first = *relevant[firstIndex];
            for (std::size_t secondIndex = firstIndex + 1;
                 secondIndex < relevant.size();
                 ++secondIndex) {
                const Threat& second = *relevant[secondIndex];
                const bool firstLine = first.Type() == ZDSpellType::Line;
                const bool secondLine = second.Type() == ZDSpellType::Line;
                if (!firstLine && !secondLine) {
                    const Vec2 firstCenter = BoundaryCenter(first);
                    const Vec2 secondCenter = BoundaryCenter(second);
                    const float firstRadius = BoundaryRadius(first, heroRadius, settings);
                    const float secondRadius = BoundaryRadius(second, heroRadius, settings);
                    if (firstRadius <= 0.0f || secondRadius <= 0.0f) continue;
                    Vec2 firstPoint;
                    Vec2 secondPoint;
                    const int count = EvadeGeometryMath::CircleIntersections(
                        firstCenter,
                        firstRadius,
                        secondCenter,
                        secondRadius,
                        firstPoint,
                        secondPoint);
                    if (count >= 1) AddIntersectionSeed(
                        seeds,
                        firstPoint,
                        heroPos,
                        (firstPoint - firstCenter) + (firstPoint - secondCenter),
                        settings);
                    if (count >= 2) AddIntersectionSeed(
                        seeds,
                        secondPoint,
                        heroPos,
                        (secondPoint - firstCenter) + (secondPoint - secondCenter),
                        settings);
                    continue;
                }
                if (firstLine && secondLine) {
                    const Vec2 firstDirection = first.direction.IsZero()
                        ? (first.endPos - first.startPos).Normalized()
                        : first.direction;
                    const Vec2 secondDirection = second.direction.IsZero()
                        ? (second.endPos - second.startPos).Normalized()
                        : second.direction;
                    if (firstDirection.IsZero() || secondDirection.IsZero()) continue;
                    const Vec2 firstNormal(-firstDirection.y, firstDirection.x);
                    const Vec2 secondNormal(-secondDirection.y, secondDirection.x);
                    const float firstRadius = first.Radius() + heroRadius + settings.endpointBuffer;
                    const float secondRadius = second.Radius() + heroRadius + settings.endpointBuffer;
                    for (int firstSide = -1; firstSide <= 1; firstSide += 2) {
                        for (int secondSide = -1; secondSide <= 1; secondSide += 2) {
                            Vec2 point;
                            float firstParameter = 0.0f;
                            float secondParameter = 0.0f;
                            if (!EvadeGeometryMath::LineIntersection(
                                    first.startPos + firstNormal * (firstRadius * static_cast<float>(firstSide)),
                                    firstDirection,
                                    second.startPos + secondNormal * (secondRadius * static_cast<float>(secondSide)),
                                    secondDirection,
                                    point,
                                    &firstParameter,
                                    &secondParameter)) continue;
                            if (firstParameter < 0.0f || firstParameter > first.startPos.Distance(first.endPos) ||
                                secondParameter < 0.0f || secondParameter > second.startPos.Distance(second.endPos)) continue;
                            AddIntersectionSeed(
                                seeds,
                                point,
                                heroPos,
                                firstNormal * static_cast<float>(firstSide) +
                                    secondNormal * static_cast<float>(secondSide),
                                settings);
                        }
                    }
                    continue;
                }
                const Threat& line = firstLine ? first : second;
                const Threat& radial = firstLine ? second : first;
                const Vec2 direction = line.direction.IsZero()
                    ? (line.endPos - line.startPos).Normalized()
                    : line.direction;
                if (direction.IsZero()) continue;
                const Vec2 normal(-direction.y, direction.x);
                const float lineRadius = line.Radius() + heroRadius + settings.endpointBuffer;
                const Vec2 center = BoundaryCenter(radial);
                const float radius = BoundaryRadius(radial, heroRadius, settings);
                if (radius <= 0.0f) continue;
                for (int side = -1; side <= 1; side += 2) {
                    const Vec2 start = line.startPos + normal * (lineRadius * static_cast<float>(side));
                    const Vec2 end = line.endPos + normal * (lineRadius * static_cast<float>(side));
                    Vec2 firstPoint;
                    Vec2 secondPoint;
                    const int count = EvadeGeometryMath::SegmentCircleIntersections(
                        start, end, center, radius, firstPoint, secondPoint);
                    if (count >= 1) AddIntersectionSeed(
                        seeds,
                        firstPoint,
                        heroPos,
                        normal * static_cast<float>(side) + (firstPoint - center).Normalized(),
                        settings);
                    if (count >= 2) AddIntersectionSeed(
                        seeds,
                        secondPoint,
                        heroPos,
                        normal * static_cast<float>(side) + (secondPoint - center).Normalized(),
                        settings);
                }
            }
        }
    }

    static void AddCursorCandidate(std::vector<CandidateSeed>& seeds,
                                   const Vec2& heroPos,
                                   const Vec2& cursorPos,
                                   const EvadeSettings& settings) {
        Vec2 direction = (cursorPos - heroPos).Normalized();
        if (direction.IsZero()) return;
        AddSeed(
            seeds,
            heroPos + direction * std::min(300.0f, settings.maxSearchRadius),
            PlannerCandidateSource::Cursor,
            -1,
            settings.maxCandidates);
    }

    static void AddRingCandidates(std::vector<CandidateSeed>& seeds,
                                  const Vec2& heroPos,
                                  const EvadeSettings& settings) {
        const float step = std::max(20.0f, settings.ringStep);
        for (float radius = step;
             radius <= settings.maxSearchRadius &&
             static_cast<int>(seeds.size()) < settings.maxCandidates;
             radius += step) {
            const int checks = std::max(12, static_cast<int>(std::ceil(2.0f * kPi * radius / 55.0f)));
            for (int index = 0;
                 index < checks && static_cast<int>(seeds.size()) < settings.maxCandidates;
                 ++index) {
                const float angle = 2.0f * kPi * static_cast<float>(index) / static_cast<float>(checks);
                const Vec2 point(
                    heroPos.x + radius * std::cos(angle),
                    heroPos.y + radius * std::sin(angle));
                AddSeed(
                    seeds,
                    point,
                    PlannerCandidateSource::Ring,
                    -1,
                    settings.maxCandidates);
            }
        }
    }

    static EnvironmentSnapshot CaptureEnvironment(float heroRadius) {
        EnvironmentSnapshot result;
        result.turretRange = 875.0f + std::max(0.0f, heroRadius);
        for (const auto& enemy : SDK::ObjectManager::EnemyHeroes()) {
            if (!enemy.IsValid() || enemy.IsDead() || !enemy.IsVisible()) continue;
            result.enemies.push_back(enemy.ServerPosition().To2D());
        }
        for (const auto& turret : SDK::ObjectManager::EnemyTurrets()) {
            if (!turret.IsValid() || turret.IsDead()) continue;
            result.turrets.push_back(turret.Position().To2D());
        }
        return result;
    }

    static CandidateEvaluation EvaluateTravelDestination(
        const SDK::AIHeroClient& player,
        const Vec2& destination,
        PlannerCandidateSource source,
        const std::vector<Threat>& threats,
        const EvadeSettings& settings,
        float travelSpeed,
        float castDelayMs) {
        EvadeSettings adjusted = settings;
        adjusted.inputDelayMs = std::max(0.0f, castDelayMs);
        return EvadeGeometry::EvaluateCandidate(
            destination,
            source,
            -1,
            player.ServerPosition().To2D(),
            SDK::Game::CursorPos().To2D(),
            player.ServerPosition().y,
            std::max(50.0f, travelSpeed),
            std::max(10.0f, player.BoundingRadius()),
            SDK::Variables::TickCount(),
            adjusted,
            threats);
    }

    static void AddEnvironmentalMetrics(CandidateEvaluation& evaluation,
                                        const EnvironmentSnapshot& environment) {
        if (!evaluation.valid) return;
        evaluation.enemyDistance = FLT_MAX;
        for (const Vec2& enemy : environment.enemies) {
            evaluation.enemyDistance = std::min(
                evaluation.enemyDistance,
                evaluation.position.Distance(enemy));
        }
        for (const Vec2& turret : environment.turrets) {
            const float distance = evaluation.position.Distance(turret);
            if (distance < environment.turretRange)
                evaluation.turretPenalty += environment.turretRange - distance;
        }
    }

    static bool StrictBetter(const CandidateEvaluation& left,
                             const CandidateEvaluation& right,
                             const EvadeSettings& settings) {
        const float preferred = std::max(0.0f, settings.preferredClearance);
        const bool leftRobust = left.minimumClearance >= preferred;
        const bool rightRobust = right.minimumClearance >= preferred;
        if (leftRobust != rightRobust) return leftRobust;
        if (!NearlyEqual(left.turretPenalty, right.turretPenalty))
            return left.turretPenalty < right.turretPenalty;
        if (!leftRobust && !NearlyEqual(left.minimumClearance, right.minimumClearance))
            return left.minimumClearance > right.minimumClearance;
        if (!NearlyEqual(left.timeMarginMs, right.timeMarginMs))
            return left.timeMarginMs > right.timeMarginMs;
        if (!NearlyEqual(left.exitDistance, right.exitDistance))
            return left.exitDistance < right.exitDistance;
        if (!NearlyEqual(left.arrivalTimeMs, right.arrivalTimeMs))
            return left.arrivalTimeMs < right.arrivalTimeMs;
        if (!NearlyEqual(left.travelDistance, right.travelDistance))
            return left.travelDistance < right.travelDistance;
        if (!NearlyEqual(left.enemyDistance, right.enemyDistance))
            return left.enemyDistance > right.enemyDistance;
        return left.cursorDistance < right.cursorDistance;
    }

    static bool FallbackBetter(const CandidateEvaluation& left,
                               const CandidateEvaluation& right) {
        if (left.endpointDanger != right.endpointDanger)
            return left.endpointDanger < right.endpointDanger;
        if (left.maxDanger != right.maxDanger)
            return left.maxDanger < right.maxDanger;
        if (left.collisionCount != right.collisionCount)
            return left.collisionCount < right.collisionCount;
        if (!NearlyEqual(left.dangerExposureMs, right.dangerExposureMs))
            return left.dangerExposureMs < right.dangerExposureMs;
        if (left.pathDanger != right.pathDanger)
            return left.pathDanger < right.pathDanger;
        if (left.reenteredDanger != right.reenteredDanger)
            return !left.reenteredDanger;
        if (!NearlyEqual(left.exitDistance, right.exitDistance))
            return left.exitDistance < right.exitDistance;
        if (!NearlyEqual(left.travelDistance, right.travelDistance))
            return left.travelDistance < right.travelDistance;
        if (!NearlyEqual(left.turretPenalty, right.turretPenalty))
            return left.turretPenalty < right.turretPenalty;
        if (!NearlyEqual(left.enemyDistance, right.enemyDistance))
            return left.enemyDistance > right.enemyDistance;
        return left.cursorDistance < right.cursorDistance;
    }
};

}
