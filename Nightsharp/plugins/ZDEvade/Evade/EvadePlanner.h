#pragma once

#include "EvadeGeometry.h"
#ifndef ZDEVADE_PLANNER_SEED_ONLY
#include "../../../SDK/SDK.h"
#endif

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <tuple>
#include <vector>

namespace ZDEvade {

class EvadePlanner {
public:
#ifndef ZDEVADE_PLANNER_SEED_ONLY
    static PlannerResult FindBest(const SDK::AIHeroClient& player,
                                  const std::vector<Threat>& threats,
                                  const EvadeSettings& settings,
                                  const Vec2& goal) {
        PlannerResult result;
        if (!player.IsValid() || player.IsDead() || threats.empty()) return result;

        const int now = SDK::Variables::TickCount();
        const Vec2 heroPos = player.ServerPosition().To2D();
        const Vec2 cursorPos = goal.IsValid() && !goal.IsZero()
            ? goal
            : SDK::Game::CursorPos().To2D();
        const float heroRadius = std::max(10.0f, player.BoundingRadius());
        const float moveSpeed = std::max(50.0f, player.MoveSpeed());
        const float planeY = player.ServerPosition().y;
        const EnvironmentSnapshot environment = CaptureEnvironment(heroRadius);
        const int maxCandidates = std::clamp(settings.maxCandidates, 32, 320);
        if (!heroPos.IsValid() || heroPos.IsZero()) return result;
        EvadeSettings candidateSettings = settings;
        candidateSettings.maxCandidates = maxCandidates;

        const std::vector<CandidateSeed> seeds = GenerateCandidateSeeds(
            threats,
            heroPos,
            cursorPos,
            heroRadius,
            candidateSettings,
            now);

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
            evaluation.stabilityBranchKey =
                seed.stabilityBranchKey;
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

        for (const CandidateSeed& seed : seeds) evaluateSeed(seed);

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
#endif

    static std::vector<CandidateSeed> GenerateCandidateSeeds(
        const std::vector<Threat>& threats,
        const Vec2& heroPos,
        const Vec2& cursorPos,
        float heroRadius,
        const EvadeSettings& settings,
        int now) {
        const int maxCandidates = std::clamp(settings.maxCandidates, 0, 320);
        if (maxCandidates == 0) return {};
        if (HasUnsupportedArc(threats)) return {};
        const CandidateBudget budget = CandidateBudget::ForMax(maxCandidates);
        EvadeSettings generationSettings = settings;
        generationSettings.maxCandidates = maxCandidates;

        std::array<std::vector<CandidateSeed>, 5> phases;
        for (auto& phase : phases)
            phase.reserve(static_cast<std::size_t>(maxCandidates));
        AddAnalyticalCandidates(
            phases[0], threats, heroPos, heroRadius, generationSettings, now);
        AddCursorCandidate(
            phases[1], heroPos, cursorPos, generationSettings);
        AddSingleThreatDetourCandidates(
            phases[2],
            threats,
            heroPos,
            cursorPos,
            heroRadius,
            generationSettings,
            now);
        AddIntersectionCandidates(
            phases[3], threats, heroPos, heroRadius, generationSettings, now);
        AddRingCandidates(
            phases[4], heroPos, cursorPos, generationSettings);

        const std::array<int, 5> quotas = {
            budget.analytical,
            budget.cursor,
            budget.singleThreatDetour,
            budget.exactIntersections,
            budget.radialFallback,
        };
        std::array<std::size_t, 5> next = {};
        std::vector<CandidateSeed> result;
        result.reserve(static_cast<std::size_t>(maxCandidates));

        for (std::size_t phaseIndex = 0; phaseIndex < phases.size(); ++phaseIndex) {
            int accepted = 0;
            while (next[phaseIndex] < phases[phaseIndex].size() &&
                   accepted < quotas[phaseIndex]) {
                if (MergeSeed(
                        result,
                        phases[phaseIndex][next[phaseIndex]],
                        maxCandidates)) {
                    ++accepted;
                }
                ++next[phaseIndex];
            }
        }

        // Borrow only after every class has completed its reserved phase.
        for (std::size_t phaseIndex = 0;
             phaseIndex < phases.size() &&
             static_cast<int>(result.size()) < maxCandidates;
             ++phaseIndex) {
            while (next[phaseIndex] < phases[phaseIndex].size() &&
                   static_cast<int>(result.size()) < maxCandidates) {
                MergeSeed(
                    result,
                    phases[phaseIndex][next[phaseIndex]],
                    maxCandidates);
                ++next[phaseIndex];
            }
        }
        return result;
    }

    static std::vector<CandidateSeed> GenerateExactIntersectionSeeds(
        const std::vector<Threat>& threats,
        const Vec2& heroPos,
        float heroRadius,
        const EvadeSettings& settings,
        int now) {
        EvadeSettings generationSettings = settings;
        generationSettings.maxCandidates =
            std::clamp(settings.maxCandidates, 0, 320);
        if (generationSettings.maxCandidates == 0) return {};
        if (HasUnsupportedArc(threats)) return {};
        std::vector<CandidateSeed> result;
        result.reserve(
            static_cast<std::size_t>(generationSettings.maxCandidates));
        AddIntersectionCandidates(
            result,
            threats,
            heroPos,
            heroRadius,
            generationSettings,
            now);
        return result;
    }

#ifndef ZDEVADE_PLANNER_SEED_ONLY
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
#endif

private:
#ifndef ZDEVADE_PLANNER_SEED_ONLY
    struct EnvironmentSnapshot {
        std::vector<Vec2> enemies;
        std::vector<Vec2> turrets;
        float turretRange = 875.0f;
    };
#endif

    static inline constexpr float kPi = 3.14159265358979323846f;

    // Cursor, radial-ring, and multi-threat intersections do not have one
    // intrinsic threat side. A fixed 16-sector world direction key gives
    // nearby regenerated seeds stable ownership without conflating branches.
    static int CoarseDirectionBranchKey(
        const Vec2& origin,
        const Vec2& target,
        int base) {
        const Vec2 direction = (target - origin).Normalized();
        if (direction.IsZero()) return base;
        float angle = std::atan2(direction.y, direction.x);
        if (angle < 0.0f) angle += 2.0f * kPi;
        const int bucket = std::clamp(
            static_cast<int>(std::floor(
                angle * StabilityBranch::CoarseDirectionBins /
                (2.0f * kPi))),
            0,
            StabilityBranch::CoarseDirectionBins - 1);
        return base + bucket;
    }

    static int ConeBranchKey(
        const Threat& threat,
        const Vec2& candidate) {
        Vec2 axis = threat.direction.IsZero()
            ? (threat.endPos - threat.startPos).Normalized()
            : threat.direction.Normalized();
        if (axis.IsZero()) return StabilityBranch::ConeRadialCap;
        const float side = axis.Cross(
            candidate - threat.startPos);
        if (side > 0.01f) return StabilityBranch::ConeLeft;
        if (side < -0.01f) return StabilityBranch::ConeRight;
        return StabilityBranch::ConeRadialCap;
    }

    static int CircleBranchKey(
        const Vec2& center,
        const Vec2& hero,
        const Vec2& candidate) {
        Vec2 heroRadial = (hero - center).Normalized();
        if (heroRadial.IsZero()) heroRadial = Vec2(1.0f, 0.0f);
        const Vec2 candidateRadial =
            (candidate - center).Normalized();
        const float side = heroRadial.Cross(candidateRadial);
        if (side > 0.01f)
            return StabilityBranch::CircleCounterClockwise;
        if (side < -0.01f)
            return StabilityBranch::CircleClockwise;
        return StabilityBranch::CircleRadial;
    }

    static int RingBranchKey(
        const Vec2& center,
        float innerRadius,
        float outerRadius,
        const Vec2& hero,
        const Vec2& candidate) {
        Vec2 heroRadial = (hero - center).Normalized();
        if (heroRadial.IsZero()) heroRadial = Vec2(1.0f, 0.0f);
        const Vec2 candidateRadial =
            (candidate - center).Normalized();
        const float candidateRadius =
            candidate.Distance(center);
        const bool inner =
            innerRadius > 0.0f &&
            std::fabs(candidateRadius - innerRadius) <=
                std::fabs(candidateRadius - outerRadius);
        const float side = heroRadial.Cross(candidateRadial);
        if (inner) {
            if (side > 0.01f)
                return StabilityBranch::RingInnerCounterClockwise;
            if (side < -0.01f)
                return StabilityBranch::RingInnerClockwise;
            return StabilityBranch::RingInnerRadial;
        }
        if (side > 0.01f)
            return StabilityBranch::RingOuterCounterClockwise;
        if (side < -0.01f)
            return StabilityBranch::RingOuterClockwise;
        return StabilityBranch::RingOuterRadial;
    }

    static int DetourBranchKey(
        const DetourEnvelope& envelope,
        const Vec2& hero,
        const Vec2& candidate) {
        switch (envelope.geometry) {
        case DetourGeometry::Line:
        case DetourGeometry::Arc: {
            const Vec2 axis =
                (envelope.end - envelope.start).Normalized();
            if (axis.IsZero()) {
                return CircleBranchKey(
                    envelope.start,
                    hero,
                    candidate);
            }
            const Vec2 offset = candidate - envelope.start;
            const float side = axis.Cross(offset);
            if (side > 0.01f)
                return StabilityBranch::LineDetourLeft;
            if (side < -0.01f)
                return StabilityBranch::LineDetourRight;
            return offset.Dot(axis) < 0.0f
                ? StabilityBranch::LineStartCap
                : StabilityBranch::LineEndCap;
        }
        case DetourGeometry::Circular:
            return CircleBranchKey(
                envelope.center,
                hero,
                candidate);
        case DetourGeometry::Ring:
            return RingBranchKey(
                envelope.center,
                envelope.innerRadius,
                envelope.outerRadius,
                hero,
                candidate);
        case DetourGeometry::Cone: {
            Vec2 axis = envelope.direction.Normalized();
            if (axis.IsZero())
                return StabilityBranch::ConeRadialCap;
            const float side = axis.Cross(
                candidate - envelope.center);
            if (side > 0.01f)
                return StabilityBranch::ConeLeft;
            if (side < -0.01f)
                return StabilityBranch::ConeRight;
            return StabilityBranch::ConeRadialCap;
        }
        default:
            return StabilityBranch::Unknown;
        }
    }

    static bool HasUnsupportedArc(const std::vector<Threat>& threats) {
        return std::any_of(
            threats.begin(),
            threats.end(),
            [](const Threat& threat) {
                return threat.Type() == ZDSpellType::Arc &&
                    !threat.ArcSupported();
            });
    }

    static bool RelevantAtHero(const Threat& threat,
                               const Vec2& heroPos,
                               float heroRadius,
                               const EvadeSettings& settings,
                               int now) {
        if (threat.IsExpiredAt(now)) return false;
        return EvadeGeometry::ThreatensPointNowOrAtFutureImpact(
            threat,
            heroPos,
            heroRadius,
            settings.pathBuffer,
            now,
            settings.maxThreatHorizonMs);
    }

    static bool EndExplosionRelevant(const Threat& threat,
                                     const EvadeSettings& settings,
                                     int now) {
        if (threat.IsExpiredAt(now) || !threat.HasEndExplosionArea())
            return false;
        const int start = threat.EndExplosionStartTick();
        const int end = SaturatingTickAdd(
            start,
            std::max(100, threat.EndExplosionDuration()));
        if (TickDifference(start, now) <= 0)
            return TickDifference(end, now) >= 0;
        const float horizon = std::clamp(
            std::isfinite(settings.maxThreatHorizonMs)
                ? settings.maxThreatHorizonMs
                : kMaximumAnalysisHorizonMs,
            0.0f,
            kMaximumAnalysisHorizonMs);
        return static_cast<double>(TickDifference(start, now)) <=
            static_cast<double>(horizon);
    }

    static bool PotentiallyRelevant(const Threat& threat,
                                    const Vec2& heroPos,
                                    float heroRadius,
                                    const EvadeSettings& settings,
                                    int now) {
        if (threat.IsExpiredAt(now)) return false;
        const int impact = EvadeGeometry::ImpactTickAt(threat, heroPos);
        if (TickDifference(impact, now) >
            static_cast<int>(settings.maxThreatHorizonMs)) return false;
        if (threat.Type() == ZDSpellType::Cone) {
            if (!threat.HasValidConeAngle()) return true;
            return heroPos.Distance(threat.startPos) <=
                threat.Range() +
                settings.maxSearchRadius +
                std::max(0.0f, heroRadius) +
                std::max(0.0f, settings.endpointBuffer) +
                threat.PositionUncertainty() +
                threat.ConeEdgePadding();
        }
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
            return false;
        case ZDSpellType::Circular:
        case ZDSpellType::Ring:
            return heroPos.Distance(threat.endPos) <= reach;
        case ZDSpellType::Cone:
            return true;
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
            const bool bodyRelevant = directOnly
                ? RelevantAtHero(threat, heroPos, heroRadius, settings, now)
                : PotentiallyRelevant(threat, heroPos, heroRadius, settings, now);
            if (bodyRelevant || EndExplosionRelevant(threat, settings, now))
                result.push_back(&threat);
        }
        const auto orderingKey = [&](const Threat* threat) {
            const bool direct = RelevantAtHero(
                *threat, heroPos, heroRadius, settings, now);
            int impact = EvadeGeometry::ImpactTickAt(*threat, heroPos);
            if (EndExplosionRelevant(*threat, settings, now)) {
                const int explosionStart = threat->EndExplosionStartTick();
                if (TickDifference(explosionStart, impact) < 0)
                    impact = explosionStart;
            }
            constexpr int kImpactBucketMs = 40;
            const int impactBucket = impact / kImpactBucketMs;
            return std::tuple(
                direct ? 0 : 1,
                impactBucket,
                impact,
                -threat->Danger(),
                threat->id);
        };
        std::sort(
            result.begin(),
            result.end(),
            [&](const Threat* left, const Threat* right) {
                return orderingKey(left) < orderingKey(right);
            });
        if (result.size() > limit) result.resize(limit);
        return result;
    }

    static bool MergeSeed(std::vector<CandidateSeed>& seeds,
                          const CandidateSeed& seed,
                          int maxCandidates) {
        if (!seed.position.IsValid() || seed.position.IsZero()) return false;
        if (seed.source == PlannerCandidateSource::Cursor) {
            seeds.erase(
                std::remove_if(
                    seeds.begin(),
                    seeds.end(),
                    [&](const CandidateSeed& existing) {
                        return existing.position.DistanceSqr(seed.position) <=
                            196.0f;
                    }),
                seeds.end());
            if (maxCandidates <= 0 ||
                static_cast<int>(seeds.size()) >= maxCandidates) {
                return false;
            }
            seeds.push_back(seed);
            return true;
        }
        for (const CandidateSeed& existing : seeds) {
            if (existing.position.DistanceSqr(seed.position) <= 196.0f)
                return false;
        }
        if (static_cast<int>(seeds.size()) >= maxCandidates) return false;
        seeds.push_back(seed);
        return true;
    }

    static void AddSeed(std::vector<CandidateSeed>& seeds,
                        const Vec2& position,
                        PlannerCandidateSource source,
                        int threatId,
                        int maxCandidates,
                        int stabilityBranchKey =
                            StabilityBranch::Unknown) {
        MergeSeed(
            seeds,
            CandidateSeed{
                position,
                source,
                threatId,
                stabilityBranchKey},
            maxCandidates);
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
                        true,
                        now),
                    PlannerCandidateSource::LineLeft,
                    threat.id,
                    settings.maxCandidates,
                    StabilityBranch::LineAnalyticalLeft);
                AddSeed(
                    seeds,
                    EvadeGeometry::ClosestLineExit(
                        threat,
                        heroPos,
                        heroRadius,
                        settings.endpointBuffer,
                        false,
                        now),
                    PlannerCandidateSource::LineRight,
                    threat.id,
                    settings.maxCandidates,
                    StabilityBranch::LineAnalyticalRight);
                break;
            case ZDSpellType::Circular: {
                const Vec2 circleExit =
                    EvadeGeometry::ClosestCircleExit(
                        threat,
                        heroPos,
                        heroRadius,
                        settings.endpointBuffer);
                AddSeed(
                    seeds,
                    circleExit,
                    PlannerCandidateSource::CircleExit,
                    threat.id,
                    settings.maxCandidates,
                    CircleBranchKey(
                        threat.endPos,
                        heroPos,
                        circleExit));
                break;
            }
            case ZDSpellType::Ring: {
                const Vec2 ringExit =
                    EvadeGeometry::ClosestRingExit(
                        threat,
                        heroPos,
                        heroRadius,
                        settings.endpointBuffer);
                const float expansion = ExitCenterDistance(
                    0.0f,
                    heroRadius,
                    settings.endpointBuffer,
                    threat.PositionUncertainty());
                const float innerRadius = std::max(
                    0.0f,
                    threat.InnerRadius() - expansion);
                const float outerRadius = ExitCenterDistance(
                    threat.Radius(),
                    heroRadius,
                    settings.endpointBuffer,
                    threat.PositionUncertainty());
                AddSeed(
                    seeds,
                    ringExit,
                    PlannerCandidateSource::CircleExit,
                    threat.id,
                    settings.maxCandidates,
                    RingBranchKey(
                        threat.endPos,
                        innerRadius,
                        outerRadius,
                        heroPos,
                        ringExit));
                break;
            }
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
                        settings.maxCandidates,
                        ConeBranchKey(threat, exit));
                }
                break;
            }
            }
            if (EndExplosionRelevant(threat, settings, now)) {
                const Vec2 center = threat.EndExplosionCenter();
                Vec2 direction = (heroPos - center).Normalized();
                if (direction.IsZero()) direction = Vec2(1.0f, 0.0f);
                AddSeed(
                    seeds,
                    center + direction * ExitCenterDistance(
                        threat.EndExplosionRadius(),
                        heroRadius,
                        settings.endpointBuffer,
                        threat.PositionUncertainty()),
                    PlannerCandidateSource::CircleExit,
                    threat.id,
                    settings.maxCandidates,
                    StabilityBranch::CircleRadial);
            }
        }
    }

    enum class BoundaryPrimitiveKind {
        Segment,
        Circle
    };

    enum class BoundaryCircleFilter {
        Full,
        StartCap,
        EndCap,
        ConeOuterArc
    };

    struct BoundaryPrimitive {
        BoundaryPrimitiveKind kind = BoundaryPrimitiveKind::Segment;
        Vec2 start = {};
        Vec2 end = {};
        Vec2 center = {};
        float radius = 0.0f;
        BoundaryCircleFilter circleFilter = BoundaryCircleFilter::Full;
        Vec2 axis = {};
        float coneHalfAngle = 0.0f;
        bool expandedSectorFilter = false;
        Vec2 sectorOrigin = {};
        float sectorRange = 0.0f;
        float sectorExpansion = 0.0f;
    };

    static BoundaryPrimitive SegmentBoundary(const Vec2& start, const Vec2& end) {
        BoundaryPrimitive result;
        result.kind = BoundaryPrimitiveKind::Segment;
        result.start = start;
        result.end = end;
        return result;
    }

    static BoundaryPrimitive CircleBoundary(
        const Vec2& center,
        float radius,
        BoundaryCircleFilter filter = BoundaryCircleFilter::Full,
        const Vec2& axis = {},
        float coneHalfAngle = 0.0f) {
        BoundaryPrimitive result;
        result.kind = BoundaryPrimitiveKind::Circle;
        result.center = center;
        result.radius = radius;
        result.circleFilter = filter;
        result.axis = axis;
        result.coneHalfAngle = coneHalfAngle;
        return result;
    }

    static BoundaryPrimitive ExpandedSectorBoundary(
        BoundaryPrimitive primitive,
        const Vec2& origin,
        const Vec2& axis,
        float range,
        float halfAngle,
        float expansion) {
        primitive.expandedSectorFilter = true;
        primitive.sectorOrigin = origin;
        primitive.axis = axis;
        primitive.sectorRange = range;
        primitive.coneHalfAngle = halfAngle;
        primitive.sectorExpansion = expansion;
        return primitive;
    }

    static std::vector<BoundaryPrimitive> BuildBoundaryPrimitives(
        const Threat& threat,
        float heroRadius,
        const EvadeSettings& settings,
        int now) {
        std::vector<BoundaryPrimitive> result;
        const float uncertainty = threat.HasTravelSpeed()
            ? threat.PositionUncertainty()
            : 0.0f;
        if (EndExplosionRelevant(threat, settings, now)) {
            result.push_back(CircleBoundary(
                threat.EndExplosionCenter(),
                ExitCenterDistance(
                    threat.EndExplosionRadius(),
                    heroRadius,
                    settings.endpointBuffer,
                    threat.PositionUncertainty())));
        }
        switch (threat.Type()) {
        case ZDSpellType::Line: {
            const Vec2 activeStart = threat.HasTravelSpeed()
                ? threat.HeadAtTick(now)
                : threat.startPos;
            const Vec2 direction = (threat.endPos - activeStart).Normalized();
            if (direction.IsZero()) return result;
            const Vec2 normal(-direction.y, direction.x);
            const float radius = ExitCenterDistance(
                threat.Radius(),
                heroRadius,
                settings.endpointBuffer,
                uncertainty);
            result.push_back(SegmentBoundary(
                activeStart + normal * radius,
                threat.endPos + normal * radius));
            result.push_back(SegmentBoundary(
                activeStart - normal * radius,
                threat.endPos - normal * radius));
            if (radius > 0.0f) {
                result.push_back(CircleBoundary(
                    activeStart,
                    radius,
                    BoundaryCircleFilter::StartCap,
                    direction));
                result.push_back(CircleBoundary(
                    threat.endPos,
                    radius,
                    BoundaryCircleFilter::EndCap,
                    direction));
            }
            return result;
        }
        case ZDSpellType::Circular:
            result.push_back(CircleBoundary(
                threat.endPos,
                ExitCenterDistance(
                    threat.Radius(),
                    heroRadius,
                    settings.endpointBuffer,
                    uncertainty)));
            return result;
        case ZDSpellType::Ring: {
            const float expansion = ExitCenterDistance(
                0.0f,
                heroRadius,
                settings.endpointBuffer,
                uncertainty);
            const float innerRadius = std::max(
                0.0f,
                threat.InnerRadius() - expansion);
            const float outerRadius = ExitCenterDistance(
                threat.Radius(),
                heroRadius,
                settings.endpointBuffer,
                uncertainty);
            if (innerRadius > 0.0f)
                result.push_back(CircleBoundary(threat.endPos, innerRadius));
            if (outerRadius > 0.0f)
                result.push_back(CircleBoundary(threat.endPos, outerRadius));
            return result;
        }
        case ZDSpellType::Cone: {
            if (!threat.HasValidConeAngle()) return result;
            const Vec2 axis = threat.direction.IsZero()
                ? (threat.endPos - threat.startPos).Normalized()
                : threat.direction.Normalized();
            if (axis.IsZero()) return result;
            const float halfAngle =
                threat.Angle() * 0.5f * kPi / 180.0f;
            const float expansion = ExitCenterDistance(
                threat.ConeEdgePadding(),
                heroRadius,
                settings.endpointBuffer,
                uncertainty);
            const float range = std::max(0.0f, threat.Range());
            const Vec2 leftDirection =
                EvadeGeometry::Rotate(axis, halfAngle);
            const Vec2 rightDirection =
                EvadeGeometry::Rotate(axis, -halfAngle);
            const Vec2 leftOutward(
                -leftDirection.y,
                leftDirection.x);
            const Vec2 rightOutward(
                rightDirection.y,
                -rightDirection.x);
            const Vec2 leftEnd = threat.startPos + leftDirection * range;
            const Vec2 rightEnd = threat.startPos + rightDirection * range;
            const auto addExpanded = [&](BoundaryPrimitive primitive) {
                result.push_back(ExpandedSectorBoundary(
                    primitive,
                    threat.startPos,
                    axis,
                    range,
                    halfAngle,
                    expansion));
            };
            addExpanded(SegmentBoundary(
                threat.startPos + leftOutward * expansion,
                leftEnd + leftOutward * expansion));
            addExpanded(SegmentBoundary(
                threat.startPos + rightOutward * expansion,
                rightEnd + rightOutward * expansion));
            addExpanded(CircleBoundary(threat.startPos, expansion));
            addExpanded(CircleBoundary(leftEnd, expansion));
            addExpanded(CircleBoundary(rightEnd, expansion));
            addExpanded(CircleBoundary(
                threat.startPos,
                range + expansion,
                BoundaryCircleFilter::ConeOuterArc,
                axis,
                halfAngle));
            return result;
        }
        case ZDSpellType::Arc:
            // Arc geometry is unsupported and intentionally contributes no
            // intersection boundary.
            return result;
        }
        return result;
    }

    static bool BoundaryContainsPoint(
        const BoundaryPrimitive& boundary,
        const Vec2& point) {
        if (boundary.kind == BoundaryPrimitiveKind::Circle) {
            const Vec2 radial = point - boundary.center;
            switch (boundary.circleFilter) {
            case BoundaryCircleFilter::Full:
                break;
            case BoundaryCircleFilter::StartCap:
                if (radial.Dot(boundary.axis) > 0.05f) return false;
                break;
            case BoundaryCircleFilter::EndCap:
                if (radial.Dot(boundary.axis) < -0.05f) return false;
                break;
            case BoundaryCircleFilter::ConeOuterArc: {
                const Vec2 direction = radial.Normalized();
                if (direction.IsZero() ||
                    direction.Dot(boundary.axis) <
                        std::cos(boundary.coneHalfAngle) - 0.0001f) {
                    return false;
                }
                break;
            }
            }
        }
        if (boundary.expandedSectorFilter) {
            const float signedDistance =
                EvadeGeometryMath::SignedDistanceToSector(
                    point,
                    boundary.sectorOrigin,
                    boundary.axis,
                    boundary.sectorRange,
                    boundary.coneHalfAngle);
            if (!std::isfinite(signedDistance) ||
                std::fabs(signedDistance - boundary.sectorExpansion) >
                    0.08f) {
                return false;
            }
        }
        return true;
    }

    static void AddIntersectionSeed(std::vector<CandidateSeed>& seeds,
                                    const Vec2& point,
                                    const Vec2& heroPos,
                                    const EvadeSettings& settings) {
        if (!point.IsValid() || point.IsZero() ||
            point.Distance(heroPos) > settings.maxSearchRadius + settings.ringStep) return;
        AddSeed(
            seeds,
            point,
            PlannerCandidateSource::Intersection,
            -1,
            settings.maxCandidates,
            CoarseDirectionBranchKey(
                heroPos,
                point,
                StabilityBranch::IntersectionCoarseBase));
    }

    static void AddPrimitiveIntersections(
        std::vector<CandidateSeed>& seeds,
        const BoundaryPrimitive& first,
        const BoundaryPrimitive& second,
        const Vec2& heroPos,
        const EvadeSettings& settings) {
        Vec2 firstPoint;
        Vec2 secondPoint;
        int count = 0;
        if (first.kind == BoundaryPrimitiveKind::Segment &&
            second.kind == BoundaryPrimitiveKind::Segment) {
            const Vec2 firstDelta = first.end - first.start;
            const Vec2 secondDelta = second.end - second.start;
            float firstParameter = 0.0f;
            float secondParameter = 0.0f;
            if (EvadeGeometryMath::LineIntersection(
                    first.start,
                    firstDelta,
                    second.start,
                    secondDelta,
                    firstPoint,
                    &firstParameter,
                    &secondParameter) &&
                firstParameter >= 0.0f && firstParameter <= 1.0f &&
                secondParameter >= 0.0f && secondParameter <= 1.0f) {
                count = 1;
            }
        } else if (first.kind == BoundaryPrimitiveKind::Segment) {
            count = EvadeGeometryMath::SegmentCircleIntersections(
                first.start,
                first.end,
                second.center,
                second.radius,
                firstPoint,
                secondPoint);
        } else if (second.kind == BoundaryPrimitiveKind::Segment) {
            count = EvadeGeometryMath::SegmentCircleIntersections(
                second.start,
                second.end,
                first.center,
                first.radius,
                firstPoint,
                secondPoint);
        } else {
            count = EvadeGeometryMath::CircleIntersections(
                first.center,
                first.radius,
                second.center,
                second.radius,
                firstPoint,
                secondPoint);
        }

        const auto addIfSupported = [&](const Vec2& point) {
            if (!BoundaryContainsPoint(first, point) ||
                !BoundaryContainsPoint(second, point)) return;
            AddIntersectionSeed(seeds, point, heroPos, settings);
        };
        if (count >= 1) addIfSupported(firstPoint);
        if (count >= 2) addIfSupported(secondPoint);
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
        std::vector<std::vector<BoundaryPrimitive>> boundaries;
        boundaries.reserve(relevant.size());
        for (const Threat* threat : relevant) {
            boundaries.push_back(BuildBoundaryPrimitives(
                *threat, heroRadius, settings, now));
        }
        for (std::size_t firstIndex = 0; firstIndex < relevant.size(); ++firstIndex) {
            if (boundaries[firstIndex].empty()) continue;
            for (std::size_t secondIndex = firstIndex + 1;
                 secondIndex < relevant.size();
                 ++secondIndex) {
                for (const BoundaryPrimitive& firstBoundary :
                     boundaries[firstIndex]) {
                    for (const BoundaryPrimitive& secondBoundary :
                         boundaries[secondIndex]) {
                        AddPrimitiveIntersections(
                            seeds,
                            firstBoundary,
                            secondBoundary,
                            heroPos,
                            settings);
                    }
                }
            }
        }
    }

    static DetourEnvelope BuildDetourEnvelope(
        const Threat& threat,
        float heroRadius,
        const EvadeSettings& settings,
        int now) {
        DetourEnvelope envelope;
        const float uncertainty = threat.PositionUncertainty();
        const float detourBuffer = std::max(
            settings.pathBuffer,
            settings.endpointBuffer);
        switch (threat.Type()) {
        case ZDSpellType::Line:
            envelope.geometry = DetourGeometry::Line;
            envelope.start = threat.HasTravelSpeed()
                ? threat.HeadAtTick(now)
                : threat.startPos;
            envelope.end = threat.endPos;
            envelope.outerRadius = ExitCollisionDistance(
                threat.Radius(),
                heroRadius,
                detourBuffer,
                uncertainty);
            break;
        case ZDSpellType::Circular:
            envelope.geometry = DetourGeometry::Circular;
            envelope.center = threat.endPos;
            envelope.outerRadius = ExitCollisionDistance(
                threat.Radius(),
                heroRadius,
                detourBuffer,
                uncertainty);
            break;
        case ZDSpellType::Ring:
            envelope.geometry = DetourGeometry::Ring;
            envelope.center = threat.endPos;
            envelope.innerRadius = std::max(
                0.0f,
                threat.InnerRadius() - ExitCollisionDistance(
                    0.0f,
                    heroRadius,
                    detourBuffer,
                    uncertainty));
            envelope.outerRadius = ExitCollisionDistance(
                threat.Radius(),
                heroRadius,
                detourBuffer,
                uncertainty);
            break;
        case ZDSpellType::Cone:
            envelope.geometry = DetourGeometry::Cone;
            envelope.center = threat.startPos;
            envelope.direction = threat.direction.IsZero()
                ? (threat.endPos - threat.startPos).Normalized()
                : threat.direction;
            envelope.range = threat.Range();
            envelope.halfAngle = threat.Angle() * 0.5f * kPi / 180.0f;
            envelope.outerRadius = ExitCollisionDistance(
                threat.ConeEdgePadding(),
                heroRadius,
                detourBuffer,
                uncertainty);
            break;
        case ZDSpellType::Arc:
            break;
        }
        return envelope;
    }

    static void AddSingleThreatDetourCandidates(
        std::vector<CandidateSeed>& seeds,
        const std::vector<Threat>& threats,
        const Vec2& heroPos,
        const Vec2& goal,
        float heroRadius,
        const EvadeSettings& settings,
        int now) {
        if (!goal.IsValid() || goal.IsZero() ||
            heroPos.Distance(goal) < 1.0f) return;
        const std::vector<const Threat*> relevant = OrderedThreats(
            threats,
            heroPos,
            heroRadius,
            settings,
            now,
            false,
            threats.size());
        for (const Threat* threat : relevant) {
            if (threat->Type() == ZDSpellType::Arc ||
                (threat->Type() == ZDSpellType::Cone &&
                 !threat->HasValidConeAngle())) continue;
            const DetourEnvelope envelope = BuildDetourEnvelope(
                *threat,
                heroRadius,
                settings,
                now);
            if (!RouteNeedsDetour(envelope, heroPos, goal)) continue;
            const std::vector<Vec2> candidates =
                BuildDetourCandidates(envelope, heroPos, goal);
            PlannerCandidateSource source = PlannerCandidateSource::Intersection;
            switch (threat->Type()) {
            case ZDSpellType::Line:
                source = PlannerCandidateSource::LineLeft;
                break;
            case ZDSpellType::Circular:
            case ZDSpellType::Ring:
                source = PlannerCandidateSource::CircleExit;
                break;
            case ZDSpellType::Cone:
                source = PlannerCandidateSource::ConeSide;
                break;
            }
            for (const Vec2& candidate : candidates) {
                AddSeed(
                    seeds,
                    candidate,
                    source,
                    threat->id,
                    settings.maxCandidates,
                    DetourBranchKey(
                        envelope,
                        heroPos,
                        candidate));
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
            settings.maxCandidates,
            CoarseDirectionBranchKey(
                heroPos,
                cursorPos,
                StabilityBranch::CursorCoarseBase));
    }

    static void AddRingCandidates(std::vector<CandidateSeed>& seeds,
                                  const Vec2& heroPos,
                                  const Vec2& cursorPos,
                                  const EvadeSettings& settings) {
        const float step = std::max(20.0f, settings.ringStep);
        const Vec2 cursorDirection = (cursorPos - heroPos).Normalized();
        const float ringOffset = cursorDirection.IsZero()
            ? 0.0f
            : std::atan2(cursorDirection.y, cursorDirection.x);
        for (float radius = step;
             radius <= settings.maxSearchRadius &&
             static_cast<int>(seeds.size()) < settings.maxCandidates;
             radius += step) {
            const int checks = std::max(12, static_cast<int>(std::ceil(2.0f * kPi * radius / 55.0f)));
            for (int index = 0;
                 index < checks && static_cast<int>(seeds.size()) < settings.maxCandidates;
                 ++index) {
                const float angle = ringOffset +
                    2.0f * kPi * static_cast<float>(index) /
                        static_cast<float>(checks);
                const Vec2 point(
                    heroPos.x + radius * std::cos(angle),
                    heroPos.y + radius * std::sin(angle));
                AddSeed(
                    seeds,
                    point,
                    PlannerCandidateSource::Ring,
                    -1,
                    settings.maxCandidates,
                    CoarseDirectionBranchKey(
                        heroPos,
                        point,
                        StabilityBranch::RingCoarseBase));
            }
        }
    }

#ifndef ZDEVADE_PLANNER_SEED_ONLY
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
#endif

    static bool StrictBetter(const CandidateEvaluation& left,
                             const CandidateEvaluation& right,
                             const EvadeSettings&) {
        return PreferStrictRoute(
            {
                left.turretPenalty,
                left.exitDistance,
                left.travelDistance,
                left.timeMarginMs,
                left.minimumClearance,
                left.enemyDistance,
                left.cursorDistance,
            },
            {
                right.turretPenalty,
                right.exitDistance,
                right.travelDistance,
                right.timeMarginMs,
                right.minimumClearance,
                right.enemyDistance,
                right.cursorDistance,
            });
    }

    static bool FallbackBetter(const CandidateEvaluation& left,
                               const CandidateEvaluation& right) {
        return PreferFallbackRoute(
            {
                left.endpointDanger,
                left.maxDanger,
                left.collisionCount,
                left.pathDanger,
                left.dangerExposureMs,
                left.reenteredDanger,
                left.firstCollisionTimeMs,
                left.timeMarginMs,
                left.exitDistance,
                left.travelDistance,
                left.turretPenalty,
                left.enemyDistance,
                left.cursorDistance,
            },
            {
                right.endpointDanger,
                right.maxDanger,
                right.collisionCount,
                right.pathDanger,
                right.dangerExposureMs,
                right.reenteredDanger,
                right.firstCollisionTimeMs,
                right.timeMarginMs,
                right.exitDistance,
                right.travelDistance,
                right.turretPenalty,
                right.enemyDistance,
                right.cursorDistance,
            });
    }
};

}
