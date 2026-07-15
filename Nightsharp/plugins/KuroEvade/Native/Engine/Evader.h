#pragma once

// Candidate generation and selection ported from Evader.cs and Program.cs
// in the supplied source.  It projects onto evade-polygon sides, adds diagonal
// escape points, then supplements them with the source's concentric scan and
// fastest perpendicular exits.

#include "Skillshot.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <unordered_set>
#include <vector>

namespace Plugins::KuroEvade {

struct SourcePositionInfo {
    Vec2 Position;
    int DangerLevel = 0;
    int DangerCount = 0;
    int PathThreatCount = 0;
    int PathDangerLevel = 0;
    float Clearance = -FLT_MAX;
    float WallClearance = 0.0f;
    float WallPenalty = 0.0f;
    int OuterRingExits = 0;
    int InnerRingShelters = 0;
    float DistanceToCursor = FLT_MAX;
    float DistanceToPlayer = FLT_MAX;
    float DistanceToEnemies = FLT_MAX;
    float PathLength = FLT_MAX;
    float Score = FLT_MAX;
    bool SafePoint = false;
    bool SafePath = false;
    bool Navigable = false;
    bool UnderTower = false;
};

struct SourceEvadePlan {
    bool Found = false;
    bool HasCandidate = false;
    bool UsedFallback = false;
    int GeneratedCandidateCount = 0;
    int GradientSteps = 0;
    SourcePositionInfo Best;
    std::vector<SourcePositionInfo> Candidates;
};

class SourceEvader final {
public:
    static bool IsSafePoint(const Vec2& point,
                            float radius,
                            const SourceSkillshotList& skillshots,
                            const EvadeSettings& settings,
                            int* dangerLevel = nullptr,
                            int* dangerCount = nullptr) {
        int level = 0;
        int count = 0;
        for (const SourceSkillshotPtr& skillshot : skillshots) {
            if (!ShouldConsider(skillshot, settings) ||
                !skillshot->ContainsStatic(point, radius, settings)) {
                continue;
            }
            level = std::max(level, DangerValue(*skillshot));
            count += DangerValue(*skillshot);
        }
        if (dangerLevel) {
            *dangerLevel = level;
        }
        if (dangerCount) {
            *dangerCount = count;
        }
        return count == 0;
    }

    static SourceSafePathResult IsSafePath(const std::vector<Vec2>& path,
                                           int timeOffset,
                                           float speed,
                                           int delay,
                                           float radius,
                                           const SourceSkillshotList& skillshots,
                                           const EvadeSettings& settings) {
        SourceSafePathResult result;
        for (const SourceSkillshotPtr& skillshot : skillshots) {
            if (!ShouldConsider(skillshot, settings)) {
                continue;
            }
            SourceSafePathResult current = skillshot->IsSafePath(
                path, timeOffset, speed, delay, radius, settings);
            if (!current.IsSafe) {
                if (!result.Intersection.Valid ||
                    (current.Intersection.Valid &&
                     current.Intersection.Distance < result.Intersection.Distance)) {
                    result.Intersection = current.Intersection;
                }
                result.IsSafe = false;
            }
        }
        return result;
    }

    // Counts threats per skillshot instead of collapsing the route to a single
    // safe/unsafe bit.  This lets the decision layer compare a route that saves
    // one of two incoming spells with a spell that can save both.  Optional
    // outputs keep the hot planner path allocation-free.
    static int CountPathThreats(
            const std::vector<Vec2>& path,
            int timeOffset,
            float speed,
            int delay,
            float radius,
            const SourceSkillshotList& skillshots,
            const EvadeSettings& settings,
            int* highestDanger = nullptr,
            float* lowestHitTime = nullptr,
            SourceSkillshotList* threats = nullptr) {
        int count = 0;
        int danger = 0;
        float firstHit = FLT_MAX;
        if (threats) {
            threats->clear();
        }

        for (const SourceSkillshotPtr& skillshot : skillshots) {
            if (!ShouldConsider(skillshot, settings)) {
                continue;
            }
            const SourceSafePathResult result = skillshot->IsSafePath(
                path, timeOffset, speed, delay, radius, settings);
            if (result.IsSafe) {
                continue;
            }

            ++count;
            danger = std::max(danger, DangerValue(*skillshot));
            const float hitTime = result.Intersection.Valid
                ? static_cast<float>(result.Intersection.Time)
                : skillshot->HitTime(
                    path.empty() ? Vec2() : path.front(), settings);
            firstHit = std::min(firstHit, hitTime);
            if (threats) {
                threats->push_back(skillshot);
            }
        }

        if (highestDanger) {
            *highestDanger = danger;
        }
        if (lowestHitTime) {
            *lowestHitTime = firstHit;
        }
        return count;
    }

    static bool PathIsDangerous(const Vec2& from,
                                const Vec2& to,
                                float speed,
                                float radius,
                                const SourceSkillshotList& skillshots,
                                const EvadeSettings& settings,
                                int extraDelay = 0) {
        return !IsSafePath({ from, to }, RouteOffset(settings), speed,
                           extraDelay,
                           radius, skillshots, settings).IsSafe;
    }

    static bool IsSafeForDuration(const Vec2& point,
                                  int durationMs,
                                  float radius,
                                  const SourceSkillshotList& skillshots,
                                  const EvadeSettings& settings) {
        durationMs = std::clamp(durationMs, 0, 1500);
        for (const SourceSkillshotPtr& skillshot : skillshots) {
            if (!ShouldConsider(skillshot, settings)) {
                continue;
            }
            const int step = skillshot->IsFiniteMissile() ? 18 : 25;
            for (int time = 0; time <= durationMs; time += step) {
                if (skillshot->ContainsAt(point, radius, time, settings)) {
                    return false;
                }
            }
            if (durationMs % step != 0 &&
                skillshot->ContainsAt(
                    point, radius, durationMs, settings)) {
                return false;
            }
        }
        return true;
    }

    static SourceEvadePlan FindBestPosition(const SDK::AIHeroClient& player,
                                            const Vec2& desired,
                                            const SourceSkillshotList& skillshots,
                                            const EvadeSettings& settings,
                                            bool allowFallback = false,
                                            int movementDelay = 0) {
        SourceEvadePlan plan;
        if (!player.IsValid()) {
            return plan;
        }
        const Vec2 hero = player.ServerPosition().To2D();
        const float radius = std::max(1.0f, player.BoundingRadius());
        const float speed = std::max(50.0f, player.MoveSpeed());
        const float height = player.ServerPosition().y;
        const float navigationProbeDistance = std::max(140.0f, radius + 100.0f);
        const SourceGeometry::NavigationProbe heroNavigation =
            SourceGeometry::ProbeNavigation(
                hero, height, navigationProbeDistance);

        std::vector<Vec2> raw;
        if (settings.ImproveMove) {
            AddFastestPositions(raw, hero, radius, skillshots, settings);
        } else {
            AddPolygonCandidates(raw, hero, radius, speed, skillshots, settings);
            AddConcentricCandidates(raw, hero, settings);
        }
        AddWallAwareCandidates(
            raw, hero, radius, heroNavigation, settings);

        // ImproveMove replaces the broad polar scan with analytical boundary
        // exits followed by a few gradient-descent steps. The derivative is
        // evaluated against every active threat and the local NavMesh field,
        // so crossed skillshots and wall corners are handled together.
        if (settings.ImproveMove) {
            const std::vector<Vec2> seeds = raw;
            for (const Vec2& seed : seeds) {
                int acceptedSteps = 0;
                const Vec2 refined = RefineCandidate(
                    seed, hero, desired, radius, height,
                    skillshots, settings, &acceptedSteps);
                plan.GradientSteps += acceptedSteps;
                if (!refined.IsZero()) {
                    raw.push_back(refined);
                }
            }

            int acceptedSteps = 0;
            const Vec2 combinedEscape = RefineCandidate(
                hero, hero, desired, radius, height,
                skillshots, settings, &acceptedSteps);
            plan.GradientSteps += acceptedSteps;
            if (!combinedEscape.IsZero()) {
                raw.push_back(combinedEscape);
            }
        }
        Deduplicate(raw);
        plan.GeneratedCandidateCount = static_cast<int>(raw.size());

        plan.Candidates.reserve(raw.size());
        for (const Vec2& candidate : raw) {
            SourcePositionInfo info = Evaluate(
                candidate, player, hero, desired, radius, speed,
                height, heroNavigation, skillshots, settings, movementDelay);
            if (!info.Navigable || info.DistanceToPlayer < radius) {
                continue;
            }
            if (info.SafePath) {
                plan.Candidates.push_back(info);
            } else if (allowFallback) {
                info.Score += 100000.0f +
                    static_cast<float>(info.DangerCount) * 25000.0f;
                plan.Candidates.push_back(info);
            }
        }

        if (plan.Candidates.empty()) {
            return plan;
        }
        std::stable_sort(plan.Candidates.begin(), plan.Candidates.end(),
            [](const SourcePositionInfo& lhs, const SourcePositionInfo& rhs) {
                // Coverage is the primary invariant for fallback routes.  A
                // pretty endpoint must never win while crossing more spells.
                if (lhs.PathThreatCount != rhs.PathThreatCount) {
                    return lhs.PathThreatCount < rhs.PathThreatCount;
                }
                if (lhs.PathDangerLevel != rhs.PathDangerLevel) {
                    return lhs.PathDangerLevel < rhs.PathDangerLevel;
                }
                if (lhs.SafePoint != rhs.SafePoint) {
                    return lhs.SafePoint > rhs.SafePoint;
                }
                if (lhs.SafePath != rhs.SafePath) {
                    return lhs.SafePath > rhs.SafePath;
                }
                // When the hero is caught by a ring, a reachable outer exit
                // wins over its inner pocket. The inner pocket remains a
                // fallback only when terrain/timing makes the outside invalid.
                if (lhs.OuterRingExits != rhs.OuterRingExits) {
                    return lhs.OuterRingExits > rhs.OuterRingExits;
                }
                if (lhs.InnerRingShelters != rhs.InnerRingShelters) {
                    return lhs.InnerRingShelters < rhs.InnerRingShelters;
                }
                if (lhs.DangerCount != rhs.DangerCount) {
                    return lhs.DangerCount < rhs.DangerCount;
                }
                return lhs.Score < rhs.Score;
            });
        plan.Best = plan.Candidates.front();
        plan.HasCandidate = true;
        // SafePath is time-aware and keeps sampling the landing point through
        // the threat horizon. SafePoint is static geometry, so requiring both
        // would waste an evade spell when a missile/zone expires before arrival.
        plan.Found = plan.Best.Navigable && plan.Best.PathThreatCount == 0;
        plan.UsedFallback = !plan.Found;
        return plan;
    }

    static SourcePositionInfo EvaluatePosition(
            const SDK::AIHeroClient& player,
            const Vec2& candidate,
            const Vec2& desired,
            const SourceSkillshotList& skillshots,
            const EvadeSettings& settings,
            int movementDelay = 0) {
        if (!player.IsValid()) {
            return {};
        }
        const Vec2 hero = player.ServerPosition().To2D();
        const float radius = std::max(1.0f, player.BoundingRadius());
        const float height = player.ServerPosition().y;
        const SourceGeometry::NavigationProbe heroNavigation =
            SourceGeometry::ProbeNavigation(
                hero, height, std::max(140.0f, radius + 100.0f));
        return Evaluate(
            candidate, player, hero, desired, radius,
            std::max(50.0f, player.MoveSpeed()), height,
            heroNavigation, skillshots, settings, movementDelay);
    }

    static float LowestHitTime(const Vec2& point,
                               const SourceSkillshotList& skillshots,
                               const EvadeSettings& settings) {
        float result = FLT_MAX;
        for (const SourceSkillshotPtr& skillshot : skillshots) {
            if (ShouldConsider(skillshot, settings)) {
                result = std::min(result, skillshot->HitTime(point, settings));
            }
        }
        return result;
    }

    static int HighestDanger(const Vec2& point,
                             float radius,
                             const SourceSkillshotList& skillshots,
                             const EvadeSettings& settings) {
        int result = 0;
        for (const SourceSkillshotPtr& skillshot : skillshots) {
            if (ShouldConsider(skillshot, settings) &&
                skillshot->ContainsStatic(point, radius, settings)) {
                result = std::max(result, DangerValue(*skillshot));
            }
        }
        return result;
    }

    static bool ShouldConsider(const SourceSkillshotPtr& skillshot,
                               const EvadeSettings& settings) {
        if (!skillshot || !skillshot->Native || !skillshot->IsActive() ||
            skillshot->ForceDisabled) {
            return false;
        }
        if (skillshot->FromFog && settings.DisableFow) {
            return false;
        }
        if (settings.OnlyDangerous && !skillshot->Data.IsDangerous) {
            return false;
        }
        return true;
    }

    static int DangerValue(const SourceSkillshot& skillshot) {
        return std::clamp(skillshot.SpellData().DangerValue, 1, 5);
    }

private:
    struct BoundaryDifferential {
        bool Valid = false;
        float Clearance = FLT_MAX;
        Vec2 Outward;
    };

    struct PotentialDifferential {
        float Potential = 0.0f;
        Vec2 Gradient;
    };

    static bool IsInsideRingEnvelope(
            const Vec2& point,
            float radius,
            const SourceSkillshot& skillshot,
            const EvadeSettings& settings) {
        if (!skillshot.Native || !skillshot.IsRing()) {
            return false;
        }
        const float padding = std::max(0.0f, radius) +
            static_cast<float>(std::max(0, settings.SkillShotsExtraRadius));
        const float ringWidth = static_cast<float>(
            std::max(0, skillshot.Native->SData.RingRadius));
        const float outer = skillshot.RawRadius() + ringWidth + padding;
        return point.Distance(skillshot.End()) <= outer;
    }

    static int FirstOffset(const EvadeSettings& settings) {
        return std::clamp(
            settings.EvadingFirstTimeOffset,
            0, 500);
    }

    static int SecondOffset(const EvadeSettings& settings) {
        return std::clamp(
            settings.EvadingSecondTimeOffset,
            0, 500);
    }

    static int RouteOffset(const EvadeSettings& settings) {
        return std::clamp(
            settings.CrossingTimeOffset,
            0, 500);
    }

    static void AddFastestPositions(std::vector<Vec2>& out,
                                    const Vec2& hero,
                                    float radius,
                                    const SourceSkillshotList& skillshots,
                                    const EvadeSettings& settings) {
        for (const SourceSkillshotPtr& skillshot : skillshots) {
            if (!ShouldConsider(skillshot, settings)) {
                continue;
            }
            if (skillshot->IsLine()) {
                const Vec2 start = skillshot->IsFiniteMissile()
                    ? skillshot->MissilePosition(0)
                    : skillshot->Start();
                const Vec2 end = skillshot->EffectiveEnd(settings);
                const Vec2 projection = SourceGeometry::ProjectOn(hero, start, end).SegmentPoint;
                Vec2 direction = (hero - projection).Normalized();
                if (direction.IsZero()) {
                    direction = SourceGeometry::Perpendicular(skillshot->Direction());
                }
                const float distance = skillshot->RawRadius() + radius +
                    settings.SkillShotsExtraRadius + 10.0f;
                out.push_back(projection + direction * distance);
                // The opposite analytical root is cheap and is essential when
                // the nearest side is sealed by terrain.
                out.push_back(projection - direction * distance);
            } else if (skillshot->IsCircle()) {
                const Vec2 center = skillshot->End();
                Vec2 direction = (hero - center).Normalized();
                if (direction.IsZero()) {
                    direction = Vec2(1.0f, 0.0f);
                }
                out.push_back(center + direction *
                    (skillshot->RawRadius() + radius + settings.SkillShotsExtraRadius + 10.0f));
            } else if (skillshot->IsRing()) {
                const Vec2 center = skillshot->End();
                Vec2 direction = (hero - center).Normalized();
                if (direction.IsZero()) direction = Vec2(1.0f, 0.0f);
                const float padding = radius +
                    static_cast<float>(std::max(0, settings.SkillShotsExtraRadius));
                const float ringWidth = static_cast<float>(
                    std::max(0, skillshot->Native->SData.RingRadius));
                const float outer = skillshot->RawRadius() + ringWidth +
                    padding + 10.0f;
                const float inner = std::max(
                    0.0f, skillshot->RawRadius() - ringWidth - padding - 10.0f);
                out.push_back(center + direction * outer);
                if (inner > 10.0f) {
                    out.push_back(center + direction * inner);
                }
            } else {
                for (const auto& polygon : skillshot->EvadeBoundaries(
                         radius,
                         static_cast<float>(settings.ExtraEvadeDistance),
                         settings)) {
                    Vec2 best;
                    float distance = FLT_MAX;
                    for (std::size_t i = 0; i < polygon.size(); ++i) {
                        const Vec2 point = SourceGeometry::ProjectOn(
                            hero, polygon[i], polygon[(i + 1) % polygon.size()]).SegmentPoint;
                        const float current = hero.DistanceSqr(point);
                        if (current < distance) {
                            distance = current;
                            best = point;
                        }
                    }
                    if (!best.IsZero()) {
                        out.push_back(best);
                    }
                }
            }
        }
    }

    static void AddWallAwareCandidates(std::vector<Vec2>& out,
                                       const Vec2& hero,
                                       float radius,
                                       const SourceGeometry::NavigationProbe& probe,
                                       const EvadeSettings& settings) {
        if (probe.BlockedRays <= 0 || probe.EscapeDirection.IsZero() ||
            probe.Clearance > radius + 80.0f) {
            return;
        }

        const float escapeDistance = std::max(
            120.0f,
            radius + static_cast<float>(settings.ExtraEvadeDistance) + 75.0f);
        out.push_back(hero + probe.EscapeDirection * escapeDistance);
        out.push_back(hero + SourceGeometry::Rotate(
            probe.EscapeDirection, SourceGeometry::Pi / 6.0f) * escapeDistance);
        out.push_back(hero + SourceGeometry::Rotate(
            probe.EscapeDirection, -SourceGeometry::Pi / 6.0f) * escapeDistance);
    }

    static void AddPolygonCandidates(std::vector<Vec2>& out,
                                     const Vec2& hero,
                                     float radius,
                                     float speed,
                                     const SourceSkillshotList& skillshots,
                                     const EvadeSettings& settings) {
        for (const SourceSkillshotPtr& skillshot : skillshots) {
            if (!ShouldConsider(skillshot, settings)) {
                continue;
            }
            for (const auto& polygon : skillshot->EvadeBoundaries(
                     radius,
                     static_cast<float>(settings.ExtraEvadeDistance),
                     settings)) {
                if (polygon.size() < 2) {
                    continue;
                }
                const bool addDiagonals = polygon.size() <= 8;
                for (std::size_t i = 0; i < polygon.size(); ++i) {
                    const Vec2& sideStart = polygon[i];
                    const Vec2& sideEnd = polygon[(i + 1) % polygon.size()];
                    const Vec2 original = SourceGeometry::ProjectOn(
                        hero, sideStart, sideEnd).SegmentPoint;
                    if (hero.DistanceSqr(original) >= 600.0f * 600.0f) {
                        continue;
                    }
                    const float sideLengthSqr = sideStart.DistanceSqr(sideEnd);
                    const int diagonalCount = addDiagonals &&
                        hero.DistanceSqr(original) < 200.0f * 200.0f &&
                        sideLengthSqr > 90.0f * 90.0f
                            ? settings.DiagonalEvadePointsCount
                        : 0;
                    const Vec2 direction = (sideEnd - sideStart).Normalized();
                    for (int diagonal = -diagonalCount;
                         diagonal <= diagonalCount; ++diagonal) {
                        const Vec2 candidate = original + direction *
                            (static_cast<float>(settings.DiagonalEvadePointsStep) *
                             static_cast<float>(diagonal));
                        const std::vector<Vec2> path{ hero, candidate };
                        const bool good = IsSafePath(
                            path, FirstOffset(settings), speed,
                            0, radius,
                            skillshots, settings).IsSafe;
                        const bool acceptable = diagonal == 0 && IsSafePath(
                            path, SecondOffset(settings), speed,
                            0, radius,
                            skillshots, settings).IsSafe;
                        if (good || acceptable) {
                            out.push_back(candidate);
                        }
                    }
                }
            }
        }
    }

    static void AddConcentricCandidates(std::vector<Vec2>& out,
                                        const Vec2& hero,
                                        const EvadeSettings& settings) {
        for (const int radius : { settings.PathFindingDistance,
                                  settings.PathFindingDistance2 }) {
            if (radius <= 0) continue;
            for (int i = 0; i < 36; ++i) {
                const float angle = 2.0f * SourceGeometry::Pi *
                    static_cast<float>(i) / 36.0f;
                out.emplace_back(std::floor(hero.x + radius * std::cos(angle)),
                                 std::floor(hero.y + radius * std::sin(angle)));
            }
        }
    }

    static void Deduplicate(std::vector<Vec2>& points) {
        std::vector<Vec2> unique;
        unique.reserve(points.size());
        for (const Vec2& point : points) {
            if (point.IsZero() || !point.IsValid()) {
                continue;
            }
            if (std::none_of(unique.begin(), unique.end(), [&](const Vec2& value) {
                    return value.DistanceSqr(point) < 15.0f * 15.0f;
                })) {
                unique.push_back(point);
            }
        }
        points.swap(unique);
    }

    static BoundaryDifferential BoundaryAt(
            const Vec2& point,
            float unitRadius,
            const SourceSkillshot& skillshot,
            const EvadeSettings& settings,
            bool preferOuterRing = false) {
        BoundaryDifferential result;
        if (!skillshot.Native) {
            return result;
        }

        if (skillshot.IsLine()) {
            const Vec2 start = skillshot.IsFiniteMissile()
                ? skillshot.MissilePosition(0)
                : skillshot.Start();
            const Vec2 end = skillshot.EffectiveEnd(settings);
            const Vec2 nearest = SourceGeometry::ProjectOn(
                point, start, end).SegmentPoint;
            Vec2 outward = (point - nearest).Normalized();
            if (outward.IsZero()) {
                outward = SourceGeometry::Perpendicular(
                    (end - start).Normalized());
            }
            result.Valid = !outward.IsZero();
            result.Clearance = point.Distance(nearest) -
                skillshot.EffectiveRadius(settings, unitRadius);
            result.Outward = outward;
            return result;
        }

        const Vec2 center = skillshot.CollisionEnd.IsZero()
            ? skillshot.End()
            : skillshot.CollisionEnd;
        if (skillshot.IsCircle()) {
            Vec2 outward = (point - center).Normalized();
            if (outward.IsZero()) outward = Vec2(1.0f, 0.0f);
            result.Valid = true;
            result.Clearance = point.Distance(center) -
                skillshot.EffectiveRadius(settings, unitRadius);
            result.Outward = outward;
            return result;
        }

        if (skillshot.IsRing()) {
            const Vec2 ringCenter = skillshot.End();
            Vec2 radial = (point - ringCenter).Normalized();
            if (radial.IsZero()) radial = Vec2(1.0f, 0.0f);
            const float distance = point.Distance(ringCenter);
            const float padding = std::max(0.0f, unitRadius) +
                static_cast<float>(std::max(0, settings.SkillShotsExtraRadius));
            const float ringWidth = static_cast<float>(
                std::max(0, skillshot.Native->SData.RingRadius));
            const float inner = std::max(
                0.0f, skillshot.RawRadius() - ringWidth - padding);
            const float outer = skillshot.RawRadius() + ringWidth + padding;

            result.Valid = true;
            if (preferOuterRing) {
                // Treat the inner pocket as a temporary local minimum. While
                // the hero is caught in this ring, the optimization gradient
                // points through the outer boundary whenever that route can
                // subsequently pass the full path-safety validation.
                result.Clearance = distance - outer;
                result.Outward = radial;
                return result;
            }
            if (distance < inner) {
                result.Clearance = inner - distance;
                result.Outward = radial * -1.0f;
            } else if (distance > outer) {
                result.Clearance = distance - outer;
                result.Outward = radial;
            } else {
                const float toInner = inner > 0.0f
                    ? distance - inner
                    : FLT_MAX;
                const float toOuter = outer - distance;
                if (toInner < toOuter) {
                    result.Clearance = -toInner;
                    result.Outward = radial * -1.0f;
                } else {
                    result.Clearance = -toOuter;
                    result.Outward = radial;
                }
            }
            return result;
        }

        const std::vector<Vec2> polygon = skillshot.PolygonPoints();
        const SourceGeometry::PolygonProjection projection =
            SourceGeometry::ClosestPointOnPolygon(point, polygon);
        if (!projection.Valid) {
            return result;
        }

        Vec2 outward = projection.Inside
            ? (projection.Point - point).Normalized()
            : (point - projection.Point).Normalized();
        if (outward.IsZero()) {
            outward = (point - skillshot.Start()).Normalized();
        }
        if (outward.IsZero()) outward = Vec2(1.0f, 0.0f);
        const float padding = std::max(0.0f, unitRadius) +
            static_cast<float>(std::max(0, settings.SkillShotsExtraRadius));
        result.Valid = true;
        result.Clearance = (projection.Inside
            ? -projection.Distance
            : projection.Distance) - padding;
        result.Outward = outward;
        return result;
    }

    static PotentialDifferential PotentialAt(
            const Vec2& point,
            const Vec2& hero,
            const Vec2& desired,
            float radius,
            float height,
            const SourceSkillshotList& skillshots,
            const EvadeSettings& settings) {
        PotentialDifferential result;
        const float safetyBand = std::clamp(
            static_cast<float>(settings.ExtraEvadeDistance) + 20.0f,
            30.0f, 90.0f);

        for (const SourceSkillshotPtr& skillshot : skillshots) {
            if (!ShouldConsider(skillshot, settings)) {
                continue;
            }
            const bool preferOuterRing = IsInsideRingEnvelope(
                hero, radius, *skillshot, settings);
            const BoundaryDifferential boundary = BoundaryAt(
                point, radius, *skillshot, settings, preferOuterRing);
            if (!boundary.Valid || boundary.Outward.IsZero()) {
                continue;
            }

            const float penetration = safetyBand - boundary.Clearance;
            if (penetration <= 0.0f) {
                continue;
            }
            const float hitTime = skillshot->HitTime(point, settings);
            const float urgency = hitTime == FLT_MAX
                ? 1.0f
                : 1.0f + std::clamp(
                    (1400.0f - hitTime) / 1000.0f, 0.0f, 1.4f);
            const float danger = static_cast<float>(DangerValue(*skillshot));
            const float weight = danger * danger * urgency;
            result.Potential += 0.5f * weight * penetration * penetration;
            // d(0.5*w*(band-clearance)^2)/dp. Boundary.Outward is
            // the analytical derivative of clearance, so -gradient exits.
            result.Gradient = result.Gradient -
                boundary.Outward * (weight * penetration);
        }

        // Weak quadratic terms retain cursor intent and select short exits;
        // threat and wall derivatives remain dominant near danger.
        const float desiredWeight = settings.FocusOnEvade ? 0.006f : 0.018f;
        const float travelWeight = settings.FocusOnEvade ? 0.006f : 0.003f;
        const Vec2 desiredDelta = point - desired;
        const Vec2 travelDelta = point - hero;
        result.Potential += 0.5f * desiredWeight * desiredDelta.LengthSqr();
        result.Potential += 0.5f * travelWeight * travelDelta.LengthSqr();
        result.Gradient = result.Gradient + desiredDelta * desiredWeight +
            travelDelta * travelWeight;

        const float probeDistance = std::max(140.0f, radius + 100.0f);
        const SourceGeometry::NavigationProbe probe =
            SourceGeometry::ProbeNavigation(point, height, probeDistance);
        const float requiredClearance = std::min(
            probeDistance - 10.0f, radius + 45.0f);
        const float wallDeficit = std::max(
            0.0f, requiredClearance - probe.Clearance);
        if (wallDeficit > 0.0f) {
            constexpr float WallWeight = 4.0f;
            result.Potential += 0.5f * WallWeight *
                wallDeficit * wallDeficit;
            if (!probe.EscapeDirection.IsZero()) {
                result.Gradient = result.Gradient -
                    probe.EscapeDirection * (WallWeight * wallDeficit);
            }
        }
        if (!SourceGeometry::IsNavigable(point, height)) {
            result.Potential += 1000000.0f;
        }
        return result;
    }

    static Vec2 RefineCandidate(const Vec2& seed,
                                const Vec2& hero,
                                const Vec2& desired,
                                float radius,
                                float height,
                                const SourceSkillshotList& skillshots,
                                const EvadeSettings& settings,
                                int* acceptedSteps) {
        if (acceptedSteps) *acceptedSteps = 0;
        if (seed.IsZero() || !seed.IsValid() ||
            !SourceGeometry::IsNavigable(seed, height)) {
            return seed;
        }

        Vec2 current = seed;
        float stepLength = std::clamp(radius * 0.65f + 15.0f, 32.0f, 58.0f);
        for (int iteration = 0; iteration < 4; ++iteration) {
            const PotentialDifferential currentField = PotentialAt(
                current, hero, desired, radius, height, skillshots, settings);
            if (currentField.Gradient.LengthSqr() <= 0.0001f) {
                break;
            }

            const Vec2 descent = currentField.Gradient.Normalized() * -1.0f;
            bool accepted = false;
            float trialLength = stepLength;
            for (int backtrack = 0; backtrack < 3; ++backtrack) {
                const Vec2 next = current + descent * trialLength;
                if (SourceGeometry::IsNavigable(next, height) &&
                    SourceGeometry::SegmentIsNavigable(
                        current, next, height, 20.0f)) {
                    const PotentialDifferential nextField = PotentialAt(
                        next, hero, desired, radius, height,
                        skillshots, settings);
                    if (nextField.Potential <= currentField.Potential + 0.5f) {
                        current = next;
                        accepted = true;
                        if (acceptedSteps) ++(*acceptedSteps);
                        break;
                    }
                }
                trialLength *= 0.5f;
            }
            if (!accepted) {
                break;
            }
            stepLength *= 0.72f;
        }
        return current;
    }

    static float ClearanceAt(const Vec2& point,
                             float radius,
                             const SourceSkillshotList& skillshots,
                             const EvadeSettings& settings) {
        float result = FLT_MAX;
        for (const SourceSkillshotPtr& skillshot : skillshots) {
            if (!ShouldConsider(skillshot, settings)) {
                continue;
            }
            const BoundaryDifferential boundary = BoundaryAt(
                point, radius, *skillshot, settings);
            if (boundary.Valid) {
                result = std::min(result, boundary.Clearance);
            }
        }
        return result == FLT_MAX ? 10000.0f : result;
    }

    static void ApplyRingPreference(
            SourcePositionInfo& info,
            const Vec2& hero,
            float radius,
            const SourceSkillshotList& skillshots,
            const EvadeSettings& settings) {
        for (const SourceSkillshotPtr& skillshot : skillshots) {
            if (!ShouldConsider(skillshot, settings) ||
                !skillshot->IsRing() ||
                !IsInsideRingEnvelope(
                    hero, radius, *skillshot, settings)) {
                continue;
            }

            const float padding = radius +
                static_cast<float>(std::max(0, settings.SkillShotsExtraRadius));
            const float ringWidth = static_cast<float>(
                std::max(0, skillshot->Native->SData.RingRadius));
            const float inner = std::max(
                0.0f, skillshot->RawRadius() - ringWidth - padding);
            const float outer = skillshot->RawRadius() + ringWidth + padding;
            const float candidateDistance = info.Position.Distance(skillshot->End());
            if (candidateDistance >= outer) {
                ++info.OuterRingExits;
            } else if (inner > 0.0f && candidateDistance <= inner) {
                ++info.InnerRingShelters;
            }
        }
    }

    static SourcePositionInfo Evaluate(const Vec2& candidate,
                                       const SDK::AIHeroClient& player,
                                       const Vec2& hero,
                                       const Vec2& desired,
                                       float radius,
                                       float speed,
                                       float height,
                                       const SourceGeometry::NavigationProbe& heroNavigation,
                                       const SourceSkillshotList& skillshots,
                                       const EvadeSettings& settings,
                                       int movementDelay) {
        SourcePositionInfo info;
        info.Position = candidate;
        info.DistanceToCursor = candidate.Distance(desired);
        info.DistanceToPlayer = candidate.Distance(hero);
        info.PathLength = info.DistanceToPlayer;
        info.Navigable = SourceGeometry::IsNavigable(candidate, height) &&
            SourceGeometry::SegmentIsNavigable(hero, candidate, height, 25.0f);
        info.SafePoint = IsSafePoint(candidate, radius, skillshots, settings,
                                     &info.DangerLevel, &info.DangerCount);
        info.PathThreatCount = CountPathThreats(
            { hero, candidate }, FirstOffset(settings), speed,
            std::max(0, movementDelay), radius,
            skillshots, settings, &info.PathDangerLevel);
        info.SafePath = info.Navigable && info.PathThreatCount == 0;
        info.Clearance = ClearanceAt(candidate, radius, skillshots, settings);
        ApplyRingPreference(
            info, hero, radius, skillshots, settings);
        const float probeDistance = std::max(140.0f, radius + 100.0f);
        const SourceGeometry::NavigationProbe wall =
            SourceGeometry::ProbeNavigation(candidate, height, probeDistance);
        info.WallClearance = wall.Clearance;
        info.WallPenalty = std::max(
            0.0f, radius + 45.0f - info.WallClearance);
        float wallDirectionPenalty = 0.0f;
        if (!heroNavigation.EscapeDirection.IsZero() &&
            heroNavigation.Clearance < radius + 80.0f &&
            info.DistanceToPlayer > SourceGeometry::Epsilon) {
            const Vec2 travelDirection = (candidate - hero).Normalized();
            const float movingTowardWall = std::max(
                0.0f, -travelDirection.Dot(heroNavigation.EscapeDirection));
            wallDirectionPenalty = movingTowardWall *
                (radius + 80.0f - heroNavigation.Clearance) * 3.0f;
        }

        // Cursor intent remains the primary tie-breaker, with explicit costs
        // for wall pinning and unnecessarily long exits. More spell clearance
        // is rewarded only within a bounded range so the hero does not flee.
        const float goalWeight = settings.FocusOnEvade ? 0.35f : 1.0f;
        info.Score = info.DistanceToCursor * goalWeight +
            info.DistanceToPlayer * 0.08f +
            info.WallPenalty * info.WallPenalty * 0.10f -
            std::clamp(info.Clearance, -100.0f, 300.0f) * 0.22f +
            wallDirectionPenalty +
            static_cast<float>(info.InnerRingShelters) * 2000.0f -
            static_cast<float>(info.OuterRingExits) * 350.0f;
        return info;
    }
};

} // namespace Plugins::KuroEvade
