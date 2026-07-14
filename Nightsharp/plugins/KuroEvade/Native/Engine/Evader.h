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
    float Clearance = -FLT_MAX;
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
    bool UsedFallback = false;
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

    static SourceEvadePlan FindBestPosition(const SDK::AIHeroClient& player,
                                            const Vec2& desired,
                                            const SourceSkillshotList& skillshots,
                                            const EvadeSettings& settings,
                                            bool allowFallback = false) {
        SourceEvadePlan plan;
        if (!player.IsValid()) {
            return plan;
        }
        const Vec2 hero = player.ServerPosition().To2D();
        const float radius = std::max(1.0f, player.BoundingRadius());
        const float speed = std::max(50.0f, player.MoveSpeed());
        const float height = player.ServerPosition().y;

        std::vector<Vec2> raw;
        if (settings.ImproveMove) {
            AddFastestPositions(raw, hero, radius, skillshots, settings);
        } else {
            AddPolygonCandidates(raw, hero, radius, speed, skillshots, settings);
            AddConcentricCandidates(raw, hero, settings);
        }
        Deduplicate(raw);

        plan.Candidates.reserve(raw.size());
        for (const Vec2& candidate : raw) {
            SourcePositionInfo info = Evaluate(
                candidate, player, hero, desired, radius, speed,
                height, skillshots, settings);
            if (!info.Navigable || info.DistanceToPlayer < radius) {
                continue;
            }
            if (info.SafePoint && info.SafePath) {
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
                if (lhs.SafePoint != rhs.SafePoint) {
                    return lhs.SafePoint > rhs.SafePoint;
                }
                if (lhs.SafePath != rhs.SafePath) {
                    return lhs.SafePath > rhs.SafePath;
                }
                if (lhs.DangerCount != rhs.DangerCount) {
                    return lhs.DangerCount < rhs.DangerCount;
                }
                return lhs.Score < rhs.Score;
            });
        plan.Best = plan.Candidates.front();
        plan.Found = plan.Best.SafePoint && plan.Best.SafePath;
        plan.UsedFallback = !plan.Found;
        if (allowFallback && !plan.Found) {
            plan.Found = true;
        }
        return plan;
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
                const Vec2 end = skillshot->CollisionEnd.IsZero()
                    ? skillshot->End()
                    : skillshot->CollisionEnd;
                const Vec2 projection = SourceGeometry::ProjectOn(hero, start, end).SegmentPoint;
                Vec2 direction = (hero - projection).Normalized();
                if (direction.IsZero()) {
                    direction = SourceGeometry::Perpendicular(skillshot->Direction());
                }
                out.push_back(projection + direction *
                    (skillshot->RawRadius() + radius + settings.SkillShotsExtraRadius + 10.0f));
            } else if (skillshot->IsCircle() || skillshot->IsRing()) {
                const Vec2 center = skillshot->End();
                Vec2 direction = (hero - center).Normalized();
                if (direction.IsZero()) {
                    direction = Vec2(1.0f, 0.0f);
                }
                out.push_back(center + direction *
                    (skillshot->RawRadius() + radius + settings.SkillShotsExtraRadius + 10.0f));
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

    static float ClearanceAt(const Vec2& point,
                             float radius,
                             const SourceSkillshotList& skillshots,
                             const EvadeSettings& settings) {
        float result = FLT_MAX;
        for (const SourceSkillshotPtr& skillshot : skillshots) {
            if (!ShouldConsider(skillshot, settings)) {
                continue;
            }
            float clearance = 0.0f;
            if (skillshot->IsLine()) {
                clearance = SourceGeometry::PointSegmentDistance(
                    point,
                    skillshot->IsFiniteMissile() ? skillshot->MissilePosition(0) : skillshot->Start(),
                    skillshot->CollisionEnd.IsZero() ? skillshot->End() : skillshot->CollisionEnd) -
                    skillshot->EffectiveRadius(settings, radius);
            } else if (skillshot->IsCircle()) {
                clearance = point.Distance(skillshot->End()) -
                    skillshot->EffectiveRadius(settings, radius);
            } else {
                clearance = SourceGeometry::DistanceToPolygon(
                    point, skillshot->PolygonPoints()) - radius - settings.SkillShotsExtraRadius;
            }
            result = std::min(result, clearance);
        }
        return result == FLT_MAX ? 10000.0f : result;
    }

    static SourcePositionInfo Evaluate(const Vec2& candidate,
                                       const SDK::AIHeroClient& player,
                                       const Vec2& hero,
                                       const Vec2& desired,
                                       float radius,
                                       float speed,
                                       float height,
                                       const SourceSkillshotList& skillshots,
                                       const EvadeSettings& settings) {
        SourcePositionInfo info;
        info.Position = candidate;
        info.DistanceToCursor = candidate.Distance(desired);
        info.DistanceToPlayer = candidate.Distance(hero);
        info.PathLength = info.DistanceToPlayer;
        info.Navigable = SourceGeometry::IsNavigable(candidate, height) &&
            SourceGeometry::SegmentIsNavigable(hero, candidate, height);
        info.SafePoint = IsSafePoint(candidate, radius, skillshots, settings,
                                     &info.DangerLevel, &info.DangerCount);
        info.SafePath = info.Navigable && IsSafePath(
            { hero, candidate }, FirstOffset(settings), speed,
            0, radius,
            skillshots, settings).IsSafe;
        info.Clearance = ClearanceAt(candidate, radius, skillshots, settings);

        info.Score = info.DistanceToCursor;
        return info;
    }
};

} // namespace Plugins::KuroEvade
