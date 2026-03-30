#pragma once

#include "Movement.h"
#include "../ConvexHull.h"
#include "../../Extensions/SharpDX/Vector2Extensions.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace SDK::Prediction::Cluster {

namespace detail {

struct PossibleTarget {
    Vec2 Position = {};
    AIHeroClient Unit = {};
};

inline Vector3 ResolveFrom(const PredictionInput& input) {
    return Movement::ResolveFrom(input);
}

inline float DistanceSquaredToSegment(const Vec2& point, const Vec2& segmentStart, const Vec2& segmentEnd) {
    return SDK::Geometry::ProjectOn(point, segmentStart, segmentEnd).segmentPoint.DistanceSqr(point);
}

inline PredictionOutput MakeOutput(const PredictionInput& input,
                                   const PredictionOutput& base,
                                   const std::vector<AIHeroClient>& hits,
                                   const Vector3& castPosition) {
    PredictionOutput output = base;
    output.Input = input;
    output.CastPosition = castPosition;
    output.AoeTargetsHit = hits;
    output.AoeHitCount = static_cast<int>(hits.size());
    return output;
}

inline std::vector<PossibleTarget> GetPossibleTargets(PredictionInput input) {
    std::vector<PossibleTarget> result = {};
    const auto originalUnit = input.Unit;
    const Vector3 source = Movement::ResolveRangeCheckFrom(input, ResolveFrom(input));

    for (const auto& enemy : ObjectManager::EnemyHeroes()) {
        if (!enemy.IsValid() || enemy.IsDead()) {
            continue;
        }
        if (originalUnit.IsValid() && enemy.NetworkId() == originalUnit.NetworkId()) {
            continue;
        }
        if (!enemy.IsValidTarget(input.Range + 200.0f + input.RealRadius(), source)) {
            continue;
        }

        input.Unit = enemy;
        const auto prediction = Movement::GetPrediction(input, false, false);
        if (prediction.Hitchance >= HitChance::High) {
            result.push_back(PossibleTarget{ prediction.UnitPosition.To2D(), enemy });
        }
    }

    return result;
}

inline std::vector<AIHeroClient> CollectCircleHits(const std::vector<PossibleTarget>& targets,
                                                   const Vec2& center,
                                                   float radius) {
    std::vector<AIHeroClient> hits = {};
    for (const auto& target : targets) {
        if (target.Position.DistanceSqr(center) <= (radius * radius)) {
            hits.push_back(target.Unit);
        }
    }
    return hits;
}

inline std::vector<AIHeroClient> CollectLineHits(const std::vector<PossibleTarget>& targets,
                                                 const Vec2& from,
                                                 const Vec2& to,
                                                 float radius) {
    std::vector<AIHeroClient> hits = {};
    for (const auto& target : targets) {
        if (DistanceSquaredToSegment(target.Position, from, to) <= (radius * radius)) {
            hits.push_back(target.Unit);
        }
    }
    return hits;
}

inline std::vector<AIHeroClient> CollectConeHits(const std::vector<PossibleTarget>& targets,
                                                 const Vec2& directionPoint,
                                                 double range,
                                                 float angle) {
    std::vector<AIHeroClient> hits = {};
    const Vec2 edge1 = directionPoint.Rotated(-angle * 0.5f);
    const Vec2 edge2 = edge1.Rotated(angle);

    for (const auto& target : targets) {
        const Vec2 point = target.Position;
        if (point.LengthSqr() >= static_cast<float>(range * range)) {
            continue;
        }
        if (edge1.Cross(point) > 0.0f && point.Cross(edge2) > 0.0f) {
            hits.push_back(target.Unit);
        }
    }
    return hits;
}

} // namespace detail

namespace Circle {

inline PredictionOutput GetCirclePrediction(PredictionInput input) {
    if (input.From.IsZero()) {
        input.From = detail::ResolveFrom(input);
    }
    if (input.RangeCheckFrom.IsZero()) {
        input.RangeCheckFrom = input.From;
    }

    const auto mainTargetPrediction = Movement::GetPrediction(input, false, true);
    if (!mainTargetPrediction.IsValid()) {
        return mainTargetPrediction;
    }

    std::vector<detail::PossibleTarget> possibleTargets = {
        { mainTargetPrediction.UnitPosition.To2D(), AIHeroClient(input.Unit.Address()) }
    };

    if (mainTargetPrediction.Hitchance >= HitChance::Medium) {
        auto extras = detail::GetPossibleTargets(input);
        possibleTargets.insert(possibleTargets.end(), extras.begin(), extras.end());
    }

    while (possibleTargets.size() > 1) {
        std::vector<Vec2> points = {};
        points.reserve(possibleTargets.size());
        for (const auto& target : possibleTargets) {
            points.push_back(target.Position);
        }

        const auto mec = ConvexHull::GetMec(points);
        if (mec.Radius <= input.RealRadius() - 10.0f &&
            mec.Center.DistanceSqr(input.RangeCheckFrom.IsZero() ? detail::ResolveFrom(input).To2D() : input.RangeCheckFrom.To2D()) <
                (input.Range * input.Range)) {
            const auto hits = detail::CollectCircleHits(possibleTargets, mec.Center, input.RealRadius());
            return detail::MakeOutput(input, mainTargetPrediction, hits, Vector3::From2D(mec.Center, mainTargetPrediction.CastPosition.y));
        }

        float maxDist = -1.0f;
        size_t maxIndex = 1;
        for (size_t i = 1; i < possibleTargets.size(); ++i) {
            const float dist = possibleTargets[i].Position.DistanceSqr(possibleTargets[0].Position);
            if (dist > maxDist) {
                maxDist = dist;
                maxIndex = i;
            }
        }
        possibleTargets.erase(possibleTargets.begin() + static_cast<long long>(maxIndex));
    }

    return mainTargetPrediction;
}

} // namespace Circle

namespace Cone {

inline PredictionOutput GetConePrediction(PredictionInput input) {
    if (input.From.IsZero()) {
        input.From = detail::ResolveFrom(input);
    }
    if (input.RangeCheckFrom.IsZero()) {
        input.RangeCheckFrom = input.From;
    }

    const auto mainTargetPrediction = Movement::GetPrediction(input, false, true);
    if (!mainTargetPrediction.IsValid()) {
        return mainTargetPrediction;
    }

    std::vector<detail::PossibleTarget> possibleTargets = {
        { mainTargetPrediction.UnitPosition.To2D(), AIHeroClient(input.Unit.Address()) }
    };

    if (mainTargetPrediction.Hitchance >= HitChance::Medium) {
        auto extras = detail::GetPossibleTargets(input);
        possibleTargets.insert(possibleTargets.end(), extras.begin(), extras.end());
    }

    if (possibleTargets.size() <= 1) {
        return mainTargetPrediction;
    }

    std::vector<Vec2> candidates = {};
    candidates.reserve(possibleTargets.size() * possibleTargets.size());
    for (auto& target : possibleTargets) {
        target.Position = target.Position - input.From.To2D();
    }

    for (size_t i = 0; i < possibleTargets.size(); ++i) {
        for (size_t j = 0; j < possibleTargets.size(); ++j) {
            if (i == j) {
                continue;
            }
            const Vec2 candidate = (possibleTargets[i].Position + possibleTargets[j].Position) * 0.5f;
            candidates.push_back(candidate);
        }
    }

    int bestHits = -1;
    Vec2 bestCandidate = {};
    for (const auto& candidate : candidates) {
        const auto hits = detail::CollectConeHits(possibleTargets, candidate, input.Range, input.Radius);
        if (static_cast<int>(hits.size()) > bestHits) {
            bestHits = static_cast<int>(hits.size());
            bestCandidate = candidate;
        }
    }

    if (bestHits > 1 && input.From.To2D().DistanceSqr(bestCandidate) > (50.0f * 50.0f)) {
        const auto hits = detail::CollectConeHits(possibleTargets, bestCandidate, input.Range, input.Radius);
        return detail::MakeOutput(input, mainTargetPrediction, hits, Vector3::From2D(bestCandidate + input.From.To2D(), mainTargetPrediction.CastPosition.y));
    }

    return mainTargetPrediction;
}

} // namespace Cone

namespace Line {

inline std::vector<Vec2> GetCandidates(const Vec2& from, const Vec2& to, float radius, float range) {
    const Vec2 middlePoint = (from + to) * 0.5f;
    const auto intersections = Extensions::CircleCircleIntersection(from, middlePoint, radius, from.Distance(middlePoint));
    if (intersections.size() > 1) {
        Vec2 c1 = intersections[0];
        Vec2 c2 = intersections[1];
        c1 = from + ((to - c1).Normalized() * range);
        c2 = from + ((to - c2).Normalized() * range);
        return { c1, c2 };
    }
    return {};
}

inline std::vector<Vec2> GetHits(const Vec2& start,
                                 const Vec2& end,
                                 double radius,
                                 const std::vector<Vec2>& points) {
    std::vector<Vec2> hits = {};
    for (const auto& point : points) {
        if (Extensions::DistanceSquared(point, start, end, true) <= static_cast<float>(radius * radius)) {
            hits.push_back(point);
        }
    }
    return hits;
}

inline PredictionOutput GetLinePrediction(PredictionInput input) {
    if (input.From.IsZero()) {
        input.From = detail::ResolveFrom(input);
    }
    if (input.RangeCheckFrom.IsZero()) {
        input.RangeCheckFrom = input.From;
    }

    const auto mainTargetPrediction = Movement::GetPrediction(input, false, true);
    if (!mainTargetPrediction.IsValid()) {
        return mainTargetPrediction;
    }

    std::vector<detail::PossibleTarget> possibleTargets = {
        { mainTargetPrediction.UnitPosition.To2D(), AIHeroClient(input.Unit.Address()) }
    };

    if (mainTargetPrediction.Hitchance >= HitChance::Medium) {
        auto extras = detail::GetPossibleTargets(input);
        possibleTargets.insert(possibleTargets.end(), extras.begin(), extras.end());
    }

    if (possibleTargets.size() <= 1) {
        return mainTargetPrediction;
    }

    std::vector<Vec2> candidates = {};
    for (const auto& target : possibleTargets) {
        auto targetCandidates = GetCandidates(input.From.To2D(), target.Position, input.Radius, input.Range);
        candidates.insert(candidates.end(), targetCandidates.begin(), targetCandidates.end());
    }

    int bestHits = -1;
    Vec2 bestCandidate = {};
    std::vector<Vec2> bestHitPoints = {};
    std::vector<Vec2> positions = {};
    positions.reserve(possibleTargets.size());
    for (const auto& target : possibleTargets) {
        positions.push_back(target.Position);
    }

    for (const auto& candidate : candidates) {
        const auto mainHit = GetHits(input.From.To2D(),
                                     candidate,
                                     input.Radius + (input.Unit.BoundingRadius() / 3.0f) - 10.0f,
                                     { possibleTargets[0].Position });
        if (mainHit.size() != 1) {
            continue;
        }

        const auto hits = GetHits(input.From.To2D(), candidate, input.Radius, positions);
        if (static_cast<int>(hits.size()) >= bestHits) {
            bestHits = static_cast<int>(hits.size());
            bestCandidate = candidate;
            bestHitPoints = hits;
        }
    }

    if (bestHits > 1) {
        float maxDistance = -1.0f;
        Vec2 p1 = {};
        Vec2 p2 = {};

        for (size_t i = 0; i < bestHitPoints.size(); ++i) {
            for (size_t j = 0; j < bestHitPoints.size(); ++j) {
                const auto proj1 = SDK::Geometry::ProjectOn(bestHitPoints[i], input.From.To2D(), bestCandidate);
                const auto proj2 = SDK::Geometry::ProjectOn(bestHitPoints[j], input.From.To2D(), bestCandidate);
                const float dist = bestHitPoints[i].DistanceSqr(proj1.segmentPoint) +
                                   bestHitPoints[j].DistanceSqr(proj2.segmentPoint);
                if (dist >= maxDistance &&
                    std::fabs((proj1.segmentPoint - bestHitPoints[i]).AngleBetween(proj2.segmentPoint - bestHitPoints[j])) >
                        (90.0f * (static_cast<float>(M_PI) / 180.0f))) {
                    maxDistance = dist;
                    p1 = bestHitPoints[i];
                    p2 = bestHitPoints[j];
                }
            }
        }

        const Vec2 cast2D = (maxDistance >= 0.0f) ? ((p1 + p2) * 0.5f) : bestCandidate;
        const auto hits = detail::CollectLineHits(possibleTargets, input.From.To2D(), cast2D, input.Radius);
        return detail::MakeOutput(input, mainTargetPrediction, hits, Vector3::From2D(cast2D, mainTargetPrediction.CastPosition.y));
    }

    return mainTargetPrediction;
}

} // namespace Line

inline std::vector<AIHeroClient> GetPossibleTargets(const PredictionInput& input, const AIBaseClient& originalTarget) {
    auto copy = input;
    copy.Unit = originalTarget;
    std::vector<AIHeroClient> result = {};
    for (const auto& target : detail::GetPossibleTargets(copy)) {
        result.push_back(target.Unit);
    }
    return result;
}

inline PredictionOutput GetAoEPrediction(const PredictionInput& input) {
    if (!input.Unit.IsValid()) {
        return {};
    }

    switch (input.Type) {
    case SpellType::Circle:
        return Circle::GetCirclePrediction(input);
    case SpellType::Cone:
        return Cone::GetConePrediction(input);
    case SpellType::Line:
        return Line::GetLinePrediction(input);
    default:
        return Movement::GetPrediction(input);
    }
}

inline PredictionOutput GetAoEPrediction(const PredictionInput& input, const AIBaseClient& target) {
    auto copy = input;
    copy.Unit = target;
    return GetAoEPrediction(copy);
}

} // namespace SDK::Prediction::Cluster

namespace SDK::Cluster {

inline PredictionOutput GetAoEPrediction(const PredictionInput& input) {
    return Prediction::Cluster::GetAoEPrediction(input);
}

inline PredictionOutput GetAoEPrediction(const PredictionInput& input, const AIBaseClient& target) {
    return Prediction::Cluster::GetAoEPrediction(input, target);
}

inline std::vector<AIHeroClient> GetPossibleTargets(const PredictionInput& input, const AIBaseClient& originalTarget) {
    return Prediction::Cluster::GetPossibleTargets(input, originalTarget);
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
