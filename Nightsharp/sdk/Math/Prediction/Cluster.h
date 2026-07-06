#pragma once

// ============================================================================
// Cluster.h — AoE (Area of Effect) Prediction
// Ported from EnsoulSharp.SDK/Core/Math/Prediction/Cluster.cs (453 lines)
//
// Provides GetAoEPrediction for Circle, Cone, and Line skillshots.
// Uses MEC (Minimum Enclosing Circle) for Circle prediction, candidate
// midpoint scanning for Cone, and candidate line scanning for Line.
//
// Dependencies:
//   - Movement.h (GetPrediction, PredictionInput, PredictionOutput)
//   - Vec2Ext helpers (Rotated, AngleBetween, ProjectOn)
//   - GameObjects::EnemyHeroes()
//   - Extensions::IsValidTarget()
// ============================================================================

#include "Movement.h"
#include "../ConvexHull.h"
#include "../../Extensions/Unit.h"
#include "../../GameObjects/GameObjects.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace SDK::Prediction::Cluster {

namespace detail {

// ============================================================================
// PossibleTarget — matches C# Cluster.PossibleTarget internal class
// ============================================================================
struct PossibleTarget {
    Vec2 Position = {};
    AIHeroClient Unit = {};
};

// ============================================================================
// GetPossibleTargets — matches C# Cluster.GetPossibleTargets
// Finds enemy heroes near the target that could be hit by AoE
// ============================================================================
inline std::vector<PossibleTarget> GetPossibleTargets(PredictionInput input) {
    std::vector<PossibleTarget> result;
    const auto originalUnit = input.Unit;
    const Vector3 source = input.ResolveRangeCheckFrom();

    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!enemy.IsValid() || enemy.IsDead()) continue;
        if (originalUnit.IsValid() && enemy.NetworkId() == originalUnit.NetworkId()) continue;

        if (!Extensions::IsValidTarget(enemy, input.Range + 200.0f + input.RealRadius(), true, source))
            continue;

        input.Unit = enemy;
        // C#: Movement.GetPrediction(input, false, false)
        // C++ overload: (input, checkCollision, checkRange)
        const auto prediction = Movement::GetPrediction(input, false, false);
        if (prediction.Hitchance >= HitChance::Medium) {
            result.push_back({ prediction.GetUnitPosition().To2D(), enemy });
        }
    }

    return result;
}

// ============================================================================
// CircleCircleIntersection — needed by Line::GetCandidates
// Returns intersection points of two circles
// ============================================================================
inline std::vector<Vec2> CircleCircleIntersection(const Vec2& center1, const Vec2& center2,
                                                   float radius1, float radius2) {
    std::vector<Vec2> result;
    float d = center1.Distance(center2);
    if (d > radius1 + radius2 + 0.0001f) return result;
    if (d < std::abs(radius1 - radius2) - 0.0001f) return result;
    if (d < 0.0001f) return result;

    float a = (radius1 * radius1 - radius2 * radius2 + d * d) / (2.0f * d);
    float h2 = radius1 * radius1 - a * a;
    if (h2 < 0.0f) h2 = 0.0f;
    float h = std::sqrt(h2);

    Vec2 mid = center1 + (center2 - center1).Normalized() * a;
    Vec2 perp = (center2 - center1).Normalized();
    Vec2 perpRotated(-perp.y, perp.x);

    result.push_back(Vec2(mid.x + h * perpRotated.x, mid.y + h * perpRotated.y));
    result.push_back(Vec2(mid.x - h * perpRotated.x, mid.y - h * perpRotated.y));
    return result;
}

// ============================================================================
// DistanceSquared from point to line segment
// ============================================================================
inline float DistanceSquaredToSegment(const Vec2& point, const Vec2& segStart, const Vec2& segEnd) {
    auto proj = Vec2Ext::ProjectOn(point, segStart, segEnd);
    return proj.SegmentPoint.DistanceSqr(point);
}

// ============================================================================
// MakeOutput — helper to build PredictionOutput with AoE hits
// ============================================================================
inline PredictionOutput MakeOutput(const PredictionInput& input,
                                   const PredictionOutput& base,
                                   const std::vector<AIHeroClient>& hits,
                                   const Vector3& castPosition) {
    PredictionOutput output = base;
    output.Input = input;
    output.SetCastPosition(castPosition);
    output.AoeTargetsHit = hits;
    output.AoeTargetsHitCount = static_cast<int>(hits.size());
    return output;
}

} // namespace detail

// ============================================================================
// Circle prediction — matches C# Cluster.Circle.GetCirclePrediction
// ============================================================================
namespace Circle {

inline PredictionOutput GetCirclePrediction(PredictionInput input) {
    if (!input.Unit.IsHero()) {
        PredictionOutput output;
        output.Input = input;
        return output;
    }

    // C#: Movement.GetPrediction(input, false, true)
    // C++ overload: (input, checkCollision, checkRange)
    const auto mainTargetPrediction = Movement::GetPrediction(input, false, true);

    std::vector<detail::PossibleTarget> possibleTargets = {
        { mainTargetPrediction.GetUnitPosition().To2D(), AIHeroClient(input.Unit.Handle()) }
    };

    if (mainTargetPrediction.Hitchance >= HitChance::Medium) {
        auto extras = detail::GetPossibleTargets(input);
        possibleTargets.insert(possibleTargets.end(), extras.begin(), extras.end());
    }

    while (possibleTargets.size() > 1) {
        std::vector<Vec2> points;
        points.reserve(possibleTargets.size());
        for (const auto& t : possibleTargets)
            points.push_back(t.Position);

        auto mec = SDK::ConvexHull::GetMec(points);

        if (mec.Radius <= input.RealRadius() - 10.0f
            && mec.Center.DistanceSqr(input.ResolveRangeCheckFrom().To2D())
               < input.Range * input.Range)
        {
            std::vector<AIHeroClient> hits;
            for (const auto& t : possibleTargets)
                hits.push_back(t.Unit);

            return detail::MakeOutput(input, mainTargetPrediction, hits,
                Vec3::From2D(mec.Center, mainTargetPrediction.GetCastPosition().y));
        }

        // Remove farthest target from target[0]
        float maxDist = -1.0f;
        size_t maxIndex = 1;
        for (size_t i = 1; i < possibleTargets.size(); ++i) {
            float dist = possibleTargets[i].Position.DistanceSqr(possibleTargets[0].Position);
            if (dist > maxDist) {
                maxDist = dist;
                maxIndex = i;
            }
        }
        possibleTargets.erase(possibleTargets.begin() + static_cast<ptrdiff_t>(maxIndex));
    }

    return mainTargetPrediction;
}

} // namespace Circle

// ============================================================================
// Cone prediction — matches C# Cluster.Cone.GetConePrediction
// ============================================================================
namespace Cone {

// GetHits — count hits within a cone
inline int GetHits(const Vec2& end, double range, float angle, const std::vector<Vec2>& points) {
    Vec2 edge1 = Vec2Ext::Rotated(end, -angle * 0.5f);
    Vec2 edge2 = Vec2Ext::Rotated(edge1, angle);

    int count = 0;
    for (const auto& point : points) {
        if (point.DistanceSqr(Vec2()) < range * range
            && edge1.Cross(point) > 0.0f
            && point.Cross(edge2) > 0.0f)
        {
            ++count;
        }
    }
    return count;
}

inline PredictionOutput GetConePrediction(PredictionInput input) {
    if (!input.Unit.IsHero()) {
        PredictionOutput output;
        output.Input = input;
        return output;
    }

    // C#: Movement.GetPrediction(input, false, true)
    const auto mainTargetPrediction = Movement::GetPrediction(input, false, true);

    std::vector<detail::PossibleTarget> possibleTargets = {
        { mainTargetPrediction.GetUnitPosition().To2D(), AIHeroClient(input.Unit.Handle()) }
    };

    if (mainTargetPrediction.Hitchance >= HitChance::Medium) {
        auto extras = detail::GetPossibleTargets(input);
        possibleTargets.insert(possibleTargets.end(), extras.begin(), extras.end());
    }

    if (possibleTargets.size() <= 1)
        return mainTargetPrediction;

    // Shift positions relative to input.From
    std::vector<Vec2> candidates;
    Vec2 from2D = input.ResolveFrom().To2D();

    for (auto& t : possibleTargets)
        t.Position = t.Position - from2D;

    for (size_t i = 0; i < possibleTargets.size(); ++i) {
        for (size_t j = 0; j < possibleTargets.size(); ++j) {
            if (i == j) continue;
            Vec2 p = (possibleTargets[i].Position + possibleTargets[j].Position) * 0.5f;
            // Check if candidate already exists
            bool found = false;
            for (const auto& c : candidates) {
                if (c == p) { found = true; break; }
            }
            if (!found)
                candidates.push_back(p);
        }
    }

    int bestHits = -1;
    Vec2 bestCandidate = {};
    std::vector<Vec2> positionsList;
    for (const auto& t : possibleTargets)
        positionsList.push_back(t.Position);

    for (const auto& candidate : candidates) {
        int hits = GetHits(candidate, input.Range, input.RealRadius(), positionsList);
        if (hits > bestHits) {
            bestHits = hits;
            bestCandidate = candidate;
        }
    }

    if (bestHits > 1 && bestCandidate.DistanceSqr(Vec2()) > 50.0f * 50.0f) {
        // Collect actual hit units
        Vec2 edge1 = Vec2Ext::Rotated(bestCandidate, -input.RealRadius() * 0.5f);
        Vec2 edge2 = Vec2Ext::Rotated(edge1, input.RealRadius());
        std::vector<AIHeroClient> hits;
        for (const auto& t : possibleTargets) {
            if (t.Position.DistanceSqr(Vec2()) < input.Range * input.Range
                && edge1.Cross(t.Position) > 0.0f
                && t.Position.Cross(edge2) > 0.0f)
            {
                hits.push_back(t.Unit);
            }
        }

        Vec2 cast2D = bestCandidate + from2D;
        return detail::MakeOutput(input, mainTargetPrediction, hits,
            Vec3::From2D(cast2D, mainTargetPrediction.GetCastPosition().y));
    }

    return mainTargetPrediction;
}

} // namespace Cone

// ============================================================================
// Line prediction — matches C# Cluster.Line.GetLinePrediction
// ============================================================================
namespace Line {

// GetCandidates — returns candidate end positions for line prediction
inline std::vector<Vec2> GetCandidates(const Vec2& from,
                                       const Vec2& to,
                                       float range,
                                       const Vec2& endPosition) {
    const Vec2 segmentEnd = from.Extend(endPosition, range);
    const auto projection = Vec2Ext::ProjectOn(to, from, segmentEnd);
    return projection.IsOnSegment
        ? std::vector<Vec2>{ projection.SegmentPoint }
        : std::vector<Vec2>{};
}

// GetHits — returns points within radius of line segment [start, end]
inline std::vector<Vec2> GetHits(const Vec2& start, const Vec2& end,
                                 double radius, const std::vector<Vec2>& points) {
    std::vector<Vec2> hits;
    for (const auto& point : points) {
        if (detail::DistanceSquaredToSegment(point, start, end) <= radius * radius)
            hits.push_back(point);
    }
    return hits;
}

inline std::vector<detail::PossibleTarget> GetHitTargets(
    const Vec2& start,
    const Vec2& end,
    double radius,
    const std::vector<detail::PossibleTarget>& targets) {
    std::vector<detail::PossibleTarget> hits;
    for (const auto& target : targets) {
        if (detail::DistanceSquaredToSegment(target.Position, start, end) <= radius * radius) {
            hits.push_back(target);
        }
    }
    return hits;
}

inline PredictionOutput GetLinePrediction(PredictionInput input) {
    if (!input.Unit.IsHero()) {
        PredictionOutput output;
        output.Input = input;
        return output;
    }

    // C#: Movement.GetPrediction(input, false, true)
    const auto mainTargetPrediction = Movement::GetPrediction(input, false, true);

    std::vector<detail::PossibleTarget> possibleTargets = {
        { mainTargetPrediction.GetUnitPosition().To2D(), AIHeroClient(input.Unit.Handle()) }
    };

    if (mainTargetPrediction.Hitchance >= HitChance::Medium) {
        auto extras = detail::GetPossibleTargets(input);
        possibleTargets.insert(possibleTargets.end(), extras.begin(), extras.end());
    }

    if (possibleTargets.size() <= 1)
        return mainTargetPrediction;

    Vec2 from2D = input.ResolveFrom().To2D();

    // Gather candidates from all targets
    std::vector<Vec2> candidates;
    for (const auto& target : possibleTargets) {
        auto targetCandidates = GetCandidates(
            from2D,
            target.Position,
            input.Range,
            possibleTargets[0].Position);
        candidates.insert(candidates.end(), targetCandidates.begin(), targetCandidates.end());
    }

    int bestHits = -1;
    Vec2 bestCandidate = {};
    std::vector<detail::PossibleTarget> bestHitTargets;
    std::vector<Vec2> positionsList;
    for (const auto& t : possibleTargets)
        positionsList.push_back(t.Position);

    for (const auto& candidate : candidates) {
        // Check that main target is hit with slightly larger radius
        auto mainHit = GetHits(from2D, candidate, input.RealRadius(), { possibleTargets[0].Position });

        if (mainHit.size() != 1) continue;

        auto hits = GetHitTargets(from2D, candidate, input.RealRadius(), possibleTargets);
        if (static_cast<int>(hits.size()) >= bestHits) {
            bestHits = static_cast<int>(hits.size());
            bestCandidate = candidate;
            bestHitTargets = hits;
        }
    }

    if (bestHits > 1) {
        // Center the cast position between the two farthest apart hit points
        float maxDistance = -1.0f;
        Vec2 p1 = {}, p2 = {};

        for (size_t i = 0; i < bestHitTargets.size(); ++i) {
            for (size_t j = 0; j < bestHitTargets.size(); ++j) {
                auto proj1 = Vec2Ext::ProjectOn(bestHitTargets[i].Position, from2D, bestCandidate);
                auto proj2 = Vec2Ext::ProjectOn(bestHitTargets[j].Position, from2D, bestCandidate);

                float dist = bestHitTargets[i].Position.DistanceSqr(proj1.LinePoint)
                           + bestHitTargets[j].Position.DistanceSqr(proj2.LinePoint);

                if (dist >= maxDistance
                    && Vec2Ext::AngleBetween(proj1.LinePoint - bestHitTargets[i].Position,
                                             proj2.LinePoint - bestHitTargets[j].Position) > 90.0f)
                {
                    maxDistance = dist;
                    p1 = bestHitTargets[i].Position;
                    p2 = bestHitTargets[j].Position;
                }
            }
        }

        Vec2 cast2D = (maxDistance >= 0.0f) ? (p1 + p2) * 0.5f : bestCandidate;

        // Collect actual hit units
        std::vector<AIHeroClient> hits;
        for (const auto& t : possibleTargets) {
            if (detail::DistanceSquaredToSegment(t.Position, from2D, cast2D) <= input.RealRadius() * input.RealRadius())
                hits.push_back(t.Unit);
        }

        return detail::MakeOutput(input, mainTargetPrediction, hits,
            Vec3::From2D(cast2D, mainTargetPrediction.GetCastPosition().y));
    }

    return mainTargetPrediction;
}

} // namespace Line

// ============================================================================
// GetAoEPrediction — main entry point, matches C# Cluster.GetAoEPrediction
// ============================================================================
inline PredictionOutput GetAoEPrediction(const PredictionInput& input) {
    if (IsCircleSpellType(input.Type)) {
        return Circle::GetCirclePrediction(input);
    }
    if (IsConeSpellType(input.Type)) {
        return Cone::GetConePrediction(input);
    }
    if (IsLineSpellType(input.Type)) {
        return Line::GetLinePrediction(input);
    }
    return PredictionOutput();
}

inline PredictionOutput GetAoEPrediction(const PredictionInput& input, const AIBaseClient& target) {
    auto copy = input;
    copy.Unit = target;
    return GetAoEPrediction(copy);
}

} // namespace SDK::Prediction::Cluster

// ============================================================================
// SDK::Cluster — convenience aliases at SDK level
// ============================================================================
namespace SDK::Cluster {

inline PredictionOutput GetAoEPrediction(const PredictionInput& input) {
    return Prediction::Cluster::GetAoEPrediction(input);
}

inline PredictionOutput GetAoEPrediction(const PredictionInput& input, const AIBaseClient& target) {
    return Prediction::Cluster::GetAoEPrediction(input, target);
}

namespace Circle {
    inline PredictionOutput GetCirclePrediction(PredictionInput input) {
        return Prediction::Cluster::Circle::GetCirclePrediction(input);
    }
}

namespace Cone {
    inline PredictionOutput GetConePrediction(PredictionInput input) {
        return Prediction::Cluster::Cone::GetConePrediction(input);
    }
}

namespace Line {
    inline PredictionOutput GetLinePrediction(PredictionInput input) {
        return Prediction::Cluster::Line::GetLinePrediction(input);
    }
}

} // namespace SDK::Cluster

// ============================================================================
// SDK::AoEPrediction — DLL-style public facade.
// Keeps Prediction::Cluster as the implementation namespace.
// ============================================================================
namespace SDK::AoEPrediction {

inline PredictionOutput GetPrediction(const PredictionInput& input) {
    return Prediction::Cluster::GetAoEPrediction(input);
}

inline PredictionOutput GetPrediction(const PredictionInput& input, const AIBaseClient& target) {
    return Prediction::Cluster::GetAoEPrediction(input, target);
}

inline PredictionOutput GetCirclePrediction(PredictionInput input) {
    return Prediction::Cluster::Circle::GetCirclePrediction(input);
}

inline PredictionOutput GetConePrediction(PredictionInput input) {
    return Prediction::Cluster::Cone::GetConePrediction(input);
}

inline PredictionOutput GetLinePrediction(PredictionInput input) {
    return Prediction::Cluster::Line::GetLinePrediction(input);
}

} // namespace SDK::AoEPrediction
