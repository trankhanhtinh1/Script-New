#pragma once
// ============================================================================
// Cluster.h — Port of EnsoulSharp.SDK/Core/Math/Prediction/Cluster.cs
// ============================================================================
// Area-of-Effect (AoE) Prediction — finds optimal cast position to hit
// the maximum number of enemy heroes.
//
// Supports Circle, Line, and Cone AoE types.
// Uses MEC (Minimum Enclosing Circle) from Polygon.h for Circle.
//
// Usage:
//   PredictionInput input;
//   input.Range = 900; input.Width = 250; input.Speed = 1200; input.Delay = 0.25f;
//   input.Type = SkillshotType::Circle;
//   auto result = SDK::Cluster::GetAoEPrediction(input, mainTarget);
//   if (result.AoEHitCount >= 2) CastSpell(result.CastPosition);
// ============================================================================

#include "Prediction.h"
#include "Polygon.h"
#include "core/Vector.h"

#include <vector>
#include <algorithm>

namespace SDK {

    // ========================================================================
    // Internal: PossibleTarget — prediction output for an enemy hero
    // ========================================================================
    struct PossibleTarget {
        Vec2 Position;      // Predicted position (2D)
        GameObject Unit;    // The enemy hero
    };

    // ========================================================================
    // Cluster — AoE Prediction Engine
    // ========================================================================
    class Cluster {
    public:
        // ====================================================================
        // Main entry: Get AoE prediction (dispatches by type)
        // ====================================================================
        static PredictionResult GetAoEPrediction(const PredictionInput& input,
                                                  const GameObject& mainTarget) {
            switch (input.Type) {
            case SkillshotType::Circle:
                return GetCirclePrediction(input, mainTarget);
            case SkillshotType::Cone:
                return GetConePrediction(input, mainTarget);
            case SkillshotType::Line:
                return GetLinePrediction(input, mainTarget);
            default:
                break;
            }
            // Fallback: single target prediction
            return Prediction::GetPrediction(mainTarget, input);
        }

        // ====================================================================
        // Circle AoE — Uses MEC (Minimum Enclosing Circle)
        // ====================================================================
        static PredictionResult GetCirclePrediction(const PredictionInput& input,
                                                     const GameObject& mainTarget) {
            auto mainPred = Prediction::GetPrediction(mainTarget, input);

            std::vector<PossibleTarget> targets;
            targets.push_back({ mainPred.UnitPosition.To2D(), mainTarget });

            if ((int)mainPred.Hitchance >= (int)HitChance::Medium) {
                auto extras = GetPossibleTargets(input, mainTarget);
                targets.insert(targets.end(), extras.begin(), extras.end());
            }

            float realRadius = input.Width; // For circle, Width = radius

            while (targets.size() > 1) {
                // Collect positions for MEC
                std::vector<Vec2> positions;
                positions.reserve(targets.size());
                for (auto& t : targets) positions.push_back(t.Position);

                auto mec = GeometryAdv::MinEnclosingCircle(positions);
                Vec3 from = input.From.IsZero()
                    ? GameObjects::Player.GetPosition() : input.From;
                Vec2 from2d = from.To2D();

                if (mec.Radius <= realRadius - 10.0f &&
                    mec.Center.DistanceSqr(from2d) < input.Range * input.Range) {
                    PredictionResult result;
                    result.CastPosition = Vec3::From2D(mec.Center, mainTarget.GetPosition().y);
                    result.UnitPosition = mainPred.UnitPosition;
                    result.Hitchance = mainPred.Hitchance;
                    result.AoEHitCount = (int)targets.size();
                    return result;
                }

                // Remove farthest target from main target
                float maxDist = -1.0f;
                int maxIdx = 1;
                for (int i = 1; i < (int)targets.size(); i++) {
                    float dist = targets[i].Position.DistanceSqr(targets[0].Position);
                    if (dist > maxDist) {
                        maxDist = dist;
                        maxIdx = i;
                    }
                }
                targets.erase(targets.begin() + maxIdx);
            }

            return mainPred;
        }

        // ====================================================================
        // Cone AoE — Find best direction to maximize hits
        // ====================================================================
        static PredictionResult GetConePrediction(const PredictionInput& input,
                                                   const GameObject& mainTarget) {
            auto mainPred = Prediction::GetPrediction(mainTarget, input);

            std::vector<PossibleTarget> targets;
            targets.push_back({ mainPred.UnitPosition.To2D(), mainTarget });

            if ((int)mainPred.Hitchance >= (int)HitChance::Medium) {
                auto extras = GetPossibleTargets(input, mainTarget);
                targets.insert(targets.end(), extras.begin(), extras.end());
            }

            if (targets.size() > 1) {
                Vec3 from = input.From.IsZero()
                    ? GameObjects::Player.GetPosition() : input.From;
                Vec2 from2d = from.To2D();

                // Generate candidate directions (midpoints of all pairs)
                std::vector<Vec2> candidates;
                for (size_t i = 0; i < targets.size(); i++) {
                    targets[i].Position = targets[i].Position - from2d;
                }

                for (size_t i = 0; i < targets.size(); i++) {
                    for (size_t j = 0; j < targets.size(); j++) {
                        if (i == j) continue;
                        Vec2 p = (targets[i].Position + targets[j].Position) * 0.5f;
                        candidates.push_back(p);
                    }
                }

                int bestHits = -1;
                Vec2 bestCandidate;

                std::vector<Vec2> positions;
                for (auto& t : targets) positions.push_back(t.Position);

                float coneAngle = input.Width; // For cone, Width = angle (radians)

                for (auto& candidate : candidates) {
                    int hits = GetConeHits(candidate, input.Range, coneAngle, positions);
                    if (hits > bestHits) {
                        bestHits = hits;
                        bestCandidate = candidate;
                    }
                }

                if (bestHits > 1 && bestCandidate.LengthSqr() > 50.0f * 50.0f) {
                    PredictionResult result;
                    result.CastPosition = Vec3::From2D(bestCandidate + from2d, mainTarget.GetPosition().y);
                    result.UnitPosition = mainPred.UnitPosition;
                    result.Hitchance = mainPred.Hitchance;
                    result.AoEHitCount = bestHits;
                    return result;
                }
            }

            return mainPred;
        }

        // ====================================================================
        // Line AoE — Find best direction to maximize hits along a line
        // ====================================================================
        static PredictionResult GetLinePrediction(const PredictionInput& input,
                                                   const GameObject& mainTarget) {
            auto mainPred = Prediction::GetPrediction(mainTarget, input);

            std::vector<PossibleTarget> targets;
            targets.push_back({ mainPred.UnitPosition.To2D(), mainTarget });

            if ((int)mainPred.Hitchance >= (int)HitChance::Medium) {
                auto extras = GetPossibleTargets(input, mainTarget);
                targets.insert(targets.end(), extras.begin(), extras.end());
            }

            if (targets.size() > 1) {
                Vec3 from = input.From.IsZero()
                    ? GameObjects::Player.GetPosition() : input.From;
                Vec2 from2d = from.To2D();

                // Generate candidate endpoints
                std::vector<Vec2> candidates;
                for (auto& t : targets) {
                    auto c = GetLineCandidates(from2d, t.Position, input.Width, input.Range);
                    candidates.insert(candidates.end(), c.begin(), c.end());
                }

                int bestHits = -1;
                Vec2 bestCandidate;

                std::vector<Vec2> positions;
                for (auto& t : targets) positions.push_back(t.Position);

                float boundingRadius = mainTarget.GetBoundingRadius();

                for (auto& candidate : candidates) {
                    // Check if main target is still hit
                    auto mainHits = GetLineHits(from2d, candidate,
                                                 input.Width + boundingRadius / 3.0f - 10.0f,
                                                 { targets[0].Position });
                    if (mainHits.size() != 1) continue;

                    auto hits = GetLineHits(from2d, candidate, input.Width, positions);
                    int hitCount = (int)hits.size();
                    if (hitCount > bestHits) {
                        bestHits = hitCount;
                        bestCandidate = candidate;
                    }
                }

                if (bestHits > 1) {
                    // Center the cast position for better coverage
                    auto finalHits = GetLineHits(from2d, bestCandidate, input.Width, positions);
                    if (finalHits.size() >= 2) {
                        Vec2 center;
                        for (auto& h : finalHits) {
                            center = center + h;
                        }
                        center = center / (float)finalHits.size();

                        // Project center onto the line
                        Vec2 dir = (bestCandidate - from2d).Normalized();
                        float proj = (center - from2d).Dot(dir);
                        Vec2 projPoint = from2d + dir * proj;

                        PredictionResult result;
                        result.CastPosition = Vec3::From2D(projPoint, mainTarget.GetPosition().y);
                        result.UnitPosition = mainPred.UnitPosition;
                        result.Hitchance = mainPred.Hitchance;
                        result.AoEHitCount = bestHits;
                        return result;
                    }
                }
            }

            return mainPred;
        }

    private:
        // ====================================================================
        // Get all enemy heroes with High+ prediction as possible targets
        // ====================================================================
        static std::vector<PossibleTarget> GetPossibleTargets(
            const PredictionInput& input,
            const GameObject& excludeUnit)
        {
            std::vector<PossibleTarget> result;

            Vec3 from = input.From.IsZero()
                ? GameObjects::Player.GetPosition() : input.From;

            for (auto& hero : GameObjects::EnemyHeroes) {
                if (!hero.IsValid() || !hero.IsAlive() || !hero.IsVisible()) continue;
                if (hero.GetNetId() == excludeUnit.GetNetId()) continue;

                float dist = from.Distance2D(hero.GetPosition());
                if (dist > input.Range + 200.0f + input.Width) continue;

                // Create a copy of input for this hero
                auto pred = Prediction::GetPrediction(hero, input);
                if ((int)pred.Hitchance >= (int)HitChance::High) {
                    result.push_back({ pred.UnitPosition.To2D(), hero });
                }
            }
            return result;
        }

        // ====================================================================
        // Cone: Count hits within cone
        // ====================================================================
        static int GetConeHits(const Vec2& direction, float range, float angle,
                                const std::vector<Vec2>& points) {
            int count = 0;
            Vec2 edge1 = direction.Rotated(-angle / 2.0f);
            Vec2 edge2 = edge1.Rotated(angle);

            for (auto& point : points) {
                if (point.LengthSqr() > range * range) continue;
                // Point must be between edge1 and edge2
                if (edge1.Cross(point) > 0 && point.Cross(edge2) > 0) {
                    count++;
                }
            }
            return count;
        }

        // ====================================================================
        // Line: Get candidate directions using circle-circle intersection
        // ====================================================================
        static std::vector<Vec2> GetLineCandidates(const Vec2& from, const Vec2& to,
                                                    float radius, float range) {
            std::vector<Vec2> result;

            Vec2 midPoint = (from + to) * 0.5f;
            float midDist = from.Distance(midPoint);

            // Circle-circle intersection: circle at `from` with `midDist`
            // and circle at `midPoint` with `radius`
            auto intersections = CircleCircleIntersection(from, midPoint, radius, midDist);
            if (intersections.size() >= 2) {
                Vec2 c1 = from + (to - intersections[0]).Normalized() * range;
                Vec2 c2 = from + (to - intersections[1]).Normalized() * range;
                result.push_back(c1);
                result.push_back(c2);
            }

            return result;
        }

        // ====================================================================
        // Line: Get points that are within radius of the line
        // ====================================================================
        static std::vector<Vec2> GetLineHits(const Vec2& start, const Vec2& end,
                                              float radius,
                                              const std::vector<Vec2>& points) {
            std::vector<Vec2> result;
            float radiusSqr = radius * radius;
            for (auto& p : points) {
                float distSqr = PointToSegmentDistanceSqr(p, start, end);
                if (distSqr <= radiusSqr) {
                    result.push_back(p);
                }
            }
            return result;
        }

        // ====================================================================
        // Helper: Circle-Circle Intersection
        // ====================================================================
        static std::vector<Vec2> CircleCircleIntersection(
            const Vec2& center1, const Vec2& center2,
            float radius1, float radius2)
        {
            std::vector<Vec2> result;

            float d = center1.Distance(center2);
            if (d > radius1 + radius2 || d < fabsf(radius1 - radius2) || d < 1e-6f)
                return result;

            float a = (radius1 * radius1 - radius2 * radius2 + d * d) / (2.0f * d);
            float h2 = radius1 * radius1 - a * a;
            if (h2 < 0) return result;
            float h = sqrtf(h2);

            Vec2 dir = (center2 - center1).Normalized();
            Vec2 mid = center1 + dir * a;
            Vec2 perp = dir.Perpendicular();

            result.push_back(mid + perp * h);
            result.push_back(mid - perp * h);
            return result;
        }

        // ====================================================================
        // Helper: Squared distance from point to segment
        // ====================================================================
        static float PointToSegmentDistanceSqr(const Vec2& point,
                                                const Vec2& segA, const Vec2& segB) {
            Vec2 ab = segB - segA;
            Vec2 ap = point - segA;
            float lenSqr = ab.LengthSqr();
            if (lenSqr < 1e-10f) return ap.LengthSqr();
            float t = ap.Dot(ab) / lenSqr;
            if (t < 0.0f) t = 0.0f;
            if (t > 1.0f) t = 1.0f;
            Vec2 closest = segA + ab * t;
            return (point - closest).LengthSqr();
        }
    };

} // namespace SDK
