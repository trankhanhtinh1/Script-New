#pragma once

#include "../../../sdk/GameObjects/GameObjects.h"
#include "../../../sdk/Math/ConvexHull.h"
#include "../../../sdk/Math/Prediction/Movement.h"
#include "../../../sdk/Utils/MathUtils.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <vector>

namespace Plugins::FsPred {

class AoePrediction {
public:
    using PredictionEvaluator = std::function<SDK::PredictionOutput(SDK::PredictionInput, bool, bool)>;

    struct PossibleTarget {
        SDK::Vector2 Position{};
        SDK::AIBaseClient Unit{};
    };

    struct MecCircle {
        SDK::Vector2 Center{};
        float Radius = 0.0f;
    };

    static SDK::PredictionOutput GetPrediction(
        const SDK::PredictionInput& input,
        const PredictionEvaluator& predEvaluator) {
        if (SDK::IsLineSpellType(input.Type)) {
            return Line::GetPrediction(input, predEvaluator);
        } else if (SDK::IsCircleSpellType(input.Type)) {
            return Circle::GetPrediction(input, predEvaluator);
        } else if (SDK::IsConeSpellType(input.Type)) {
            return Cone::GetPrediction(input, predEvaluator);
        }
        return SDK::PredictionOutput{};
    }

    static std::vector<PossibleTarget> GetPossibleTargets(
        const SDK::PredictionInput& input,
        const PredictionEvaluator& predEvaluator) {
        std::vector<PossibleTarget> list;
        const auto originalUnit = input.Unit;
        if (!originalUnit.IsValid()) return list;

        const float checkRange = input.Range + 200.0f + input.RealRadius();
        const auto& enemyHeroes = SDK::GameObjects::EnemyHeroesFrame();
        for (const auto& hero : enemyHeroes) {
            if (!hero.IsValid() || hero.NetworkId() == originalUnit.NetworkId()) continue;
            if (!SDK::Extensions::IsValidTarget(hero, checkRange, true, input.ResolveRangeCheckFrom())) continue;

            SDK::PredictionInput targetInput = input;
            targetInput.Unit = hero;
            const SDK::PredictionOutput pred = predEvaluator(targetInput, false, false);
            if (pred.Hitchance >= SDK::HitChance::High) {
                list.push_back({ pred.GetUnitPosition().To2D(), hero });
            }
        }
        return list;
    }

    // MEC (Minimum Enclosing Circle) algorithm implementation
    static MecCircle GetMec(const std::vector<SDK::Vector2>& points) {
        if (points.empty()) return {};
        if (points.size() == 1) return { points[0], 0.0f };

        std::vector<SDK::Vector2> sdkPoints;
        sdkPoints.reserve(points.size());
        for (const auto& pt : points) {
            sdkPoints.push_back(pt);
        }

        const auto mecResult = SDK::ConvexHull::GetMec(sdkPoints);
        return { mecResult.Center, mecResult.Radius };
    }

    class Circle {
    public:
        static SDK::PredictionOutput GetPrediction(
            const SDK::PredictionInput& input,
            const PredictionEvaluator& predEvaluator) {

            SDK::PredictionOutput prediction = predEvaluator(input, false, true);
            std::vector<PossibleTarget> list = {
                { prediction.GetUnitPosition().To2D(), input.Unit }
            };

            if (prediction.Hitchance >= SDK::HitChance::Medium) {
                const auto possible = AoePrediction::GetPossibleTargets(input, predEvaluator);
                list.insert(list.end(), possible.begin(), possible.end());
            }

            while (list.size() > 1) {
                std::vector<SDK::Vector2> positions;
                positions.reserve(list.size());
                for (const auto& item : list) {
                    positions.push_back(item.Position);
                }

                const MecCircle mec = AoePrediction::GetMec(positions);
                const float rangeCheckSqr = input.Range * input.Range;
                const float centerDistSqr = mec.Center.DistanceSquared(input.ResolveRangeCheckFrom().To2D());

                if (mec.Radius <= input.RealRadius() - 10.0f && centerDistSqr < rangeCheckSqr) {
                    SDK::PredictionOutput output;
                    output.Input = input;
                    output.SetCastPosition(SDK::Vector3::From2D(mec.Center));
                    output.SetUnitPosition(prediction.GetUnitPosition());
                    output.Hitchance = prediction.Hitchance;
                    output.AoeTargetsHitCount = static_cast<int>(list.size());
                    for (const auto& target : list) {
                        if (target.Unit.IsValid() && target.Unit.IsHero()) {
                            output.AoeTargetsHit.push_back(SDK::AIHeroClient(target.Unit.Address()));
                        }
                    }
                    return output;
                }

                float maxDist = -1.0f;
                std::size_t removeIndex = 1;
                for (std::size_t i = 1; i < list.size(); ++i) {
                    const float dist = list[i].Position.DistanceSquared(list[0].Position);
                    if (dist > maxDist) {
                        maxDist = dist;
                        removeIndex = i;
                    }
                }
                list.erase(list.begin() + removeIndex);
            }
            return prediction;
        }
    };

    class Cone {
    public:
        static SDK::PredictionOutput GetPrediction(
            const SDK::PredictionInput& input,
            const PredictionEvaluator& predEvaluator) {

            SDK::PredictionOutput prediction = predEvaluator(input, false, true);
            std::vector<PossibleTarget> list = {
                { prediction.GetUnitPosition().To2D(), input.Unit }
            };

            if (prediction.Hitchance >= SDK::HitChance::Medium) {
                const auto possible = AoePrediction::GetPossibleTargets(input, predEvaluator);
                list.insert(list.end(), possible.begin(), possible.end());
            }

            if (list.size() > 1) {
                const SDK::Vector2 from2D = input.ResolveFrom().To2D();
                std::vector<PossibleTarget> relativeList = list;
                for (auto& target : relativeList) {
                    target.Position = target.Position - from2D;
                }

                std::vector<SDK::Vector2> candidatePoints;
                for (std::size_t i = 0; i < relativeList.size(); ++i) {
                    for (std::size_t j = 0; j < relativeList.size(); ++j) {
                        if (i != j) {
                            const SDK::Vector2 mid = (relativeList[i].Position + relativeList[j].Position) * 0.5f;
                            const bool found = std::any_of(candidatePoints.begin(), candidatePoints.end(), [&](const SDK::Vector2& pt) {
                                return pt.DistanceSquared(mid) < 1.0f;
                            });
                            if (!found) {
                                candidatePoints.push_back(mid);
                            }
                        }
                    }
                }

                int maxHits = -1;
                SDK::Vector2 bestVector{};
                std::vector<SDK::Vector2> relativePoints;
                relativePoints.reserve(relativeList.size());
                for (const auto& item : relativeList) {
                    relativePoints.push_back(item.Position);
                }

                for (const auto& candidate : candidatePoints) {
                    const int hits = GetHits(candidate, static_cast<double>(input.Range), input.Radius, relativePoints);
                    if (hits > maxHits) {
                        maxHits = hits;
                        bestVector = candidate;
                    }
                }

                const SDK::Vector2 castPos2D = bestVector + from2D;
                if (maxHits > 1 && from2D.DistanceSquared(castPos2D) > 2500.0f) {
                    SDK::PredictionOutput output;
                    output.Input = input;
                    output.Hitchance = prediction.Hitchance;
                    output.AoeTargetsHitCount = maxHits;
                    output.SetUnitPosition(prediction.GetUnitPosition());
                    output.SetCastPosition(SDK::Vector3::From2D(castPos2D));
                    for (const auto& target : list) {
                        if (target.Unit.IsValid() && target.Unit.IsHero()) {
                            output.AoeTargetsHit.push_back(SDK::AIHeroClient(target.Unit.Address()));
                        }
                    }
                    return output;
                }
            }
            return prediction;
        }

        static int GetHits(const SDK::Vector2& end, double range, float angle, const std::vector<SDK::Vector2>& points) {
            const float radAngle = angle * 3.14159265358979323846f / 180.0f;
            const SDK::Vector2 edge1 = SDK::Prediction::Vec2Ext::Rotated(end, -radAngle / 2.0f);
            const SDK::Vector2 edge2 = SDK::Prediction::Vec2Ext::Rotated(edge1, radAngle);

            int count = 0;
            for (const auto& point : points) {
                if (static_cast<double>(point.LengthSqr()) < range * range) {
                    const float cross1 = edge1.x * point.y - edge1.y * point.x;
                    const float cross2 = point.x * edge2.y - point.y * edge2.x;
                    if (cross1 > 0.0f && cross2 > 0.0f) {
                        ++count;
                    }
                }
            }
            return count;
        }
    };

    class Line {
    public:
        static SDK::PredictionOutput GetPrediction(
            const SDK::PredictionInput& input,
            const PredictionEvaluator& predEvaluator) {

            SDK::PredictionOutput prediction = predEvaluator(input, false, true);
            std::vector<PossibleTarget> list = {
                { prediction.GetUnitPosition().To2D(), input.Unit }
            };

            if (prediction.Hitchance >= SDK::HitChance::Medium) {
                const auto possible = AoePrediction::GetPossibleTargets(input, predEvaluator);
                list.insert(list.end(), possible.begin(), possible.end());
            }

            if (list.size() > 1) {
                const SDK::Vector2 from2D = input.ResolveFrom().To2D();
                std::vector<SDK::Vector2> candidates;

                for (const auto& target : list) {
                    const auto c = GetCandidates(from2D, target.Position, input.Radius, input.Range);
                    candidates.insert(candidates.end(), c.begin(), c.end());
                }

                int maxHits = -1;
                SDK::Vector2 bestCandidate{};
                std::vector<SDK::Vector2> bestHitPoints;
                std::vector<SDK::Vector2> allPoints;
                allPoints.reserve(list.size());
                for (const auto& t : list) {
                    allPoints.push_back(t.Position);
                }

                const float primaryRadius = input.Radius + (input.Unit.IsValid() ? input.Unit.BoundingRadius() / 3.0f : 0.0f) - 10.0f;
                const std::vector<SDK::Vector2> primaryTargetPoint = { list[0].Position };

                for (const auto& candidate : candidates) {
                    const auto primaryHits = GetHitsOnLine(from2D, candidate, primaryRadius, primaryTargetPoint);
                    if (primaryHits.size() == 1) {
                        const auto lineHits = GetHitsOnLine(from2D, candidate, input.Radius, allPoints);
                        const int count = static_cast<int>(lineHits.size());
                        if (count >= maxHits) {
                            maxHits = count;
                            bestCandidate = candidate;
                            bestHitPoints = lineHits;
                        }
                    }
                }

                if (maxHits > 1 && !bestHitPoints.empty()) {
                    // Cast along the exact same ray that was hit-tested.
                    // Capping the endpoint to a shorter distance is fine, but the
                    // direction must never be replaced by an unrelated midpoint.
                    SDK::Vector2 finalCastPos = bestCandidate;
                    if (finalCastPos.IsZero()) finalCastPos = bestCandidate;

                    SDK::PredictionOutput output;
                    output.Input = input;
                    output.Hitchance = prediction.Hitchance;
                    output.AoeTargetsHitCount = maxHits;
                    output.SetUnitPosition(prediction.GetUnitPosition());
                    output.SetCastPosition(SDK::Vector3::From2D(finalCastPos));
                    for (const auto& target : list) {
                        if (target.Unit.IsValid() && target.Unit.IsHero()) {
                            output.AoeTargetsHit.push_back(SDK::AIHeroClient(target.Unit.Address()));
                        }
                    }
                    return output;
                }
            }
            return prediction;
        }

        static std::vector<SDK::Vector2> GetCandidates(const SDK::Vector2& from, const SDK::Vector2& to, float radius, float range) {
            const SDK::Vector2 mid = (from + to) * 0.5f;
            const auto circleIntersect = CircleCircleIntersection(from, mid, radius, from.Distance(mid));
            if (circleIntersect.size() > 1) {
                const SDK::Vector2 c1 = from + (to - circleIntersect[0]).Normalized() * range;
                const SDK::Vector2 c2 = from + (to - circleIntersect[1]).Normalized() * range;
                return { c1, c2 };
            }
            return {};
        }

        static std::vector<SDK::Vector2> CircleCircleIntersection(const SDK::Vector2& center1, const SDK::Vector2& center2, float radius1, float radius2) {
            const float d = center1.Distance(center2);
            if (d > radius1 + radius2 || d <= std::abs(radius1 - radius2) || d < 0.0001f) {
                return {};
            }
            const float a = (radius1 * radius1 - radius2 * radius2 + d * d) / (2.0f * d);
            const float hSqr = radius1 * radius1 - a * a;
            if (hSqr < 0.0f) return {};
            const float h = std::sqrt(hSqr);

            const SDK::Vector2 dir = (center2 - center1).Normalized();
            const SDK::Vector2 p2 = center1 + dir * a;
            const SDK::Vector2 perp = SDK::Prediction::Vec2Ext::Perpendicular(dir);

            const SDK::Vector2 i1 = p2 + perp * h;
            const SDK::Vector2 i2 = p2 - perp * h;
            return { i1, i2 };
        }

        static std::vector<SDK::Vector2> GetHitsOnLine(const SDK::Vector2& start, const SDK::Vector2& end, float radius, const std::vector<SDK::Vector2>& points) {
            std::vector<SDK::Vector2> hits;
            const float radiusSqr = radius * radius;

            for (const auto& p : points) {
                const auto proj = SDK::Prediction::Vec2Ext::ProjectOn(p, start, end);
                const float distSqr = p.DistanceSquared(proj.SegmentPoint);
                if (distSqr <= radiusSqr) {
                    hits.push_back(p);
                }
            }
            return hits;
        }
    };
};

} // namespace Plugins::FsPred
