#pragma once
#include "FsPredAoeMath.h"

#include "../../../sdk/GameObjects/GameObjects.h"
#include "../../../sdk/Math/Prediction/Movement.h"
#include "../../../sdk/Wrappers/Spells/Spell.h"
#include "../../../SectionProfiler.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace Plugins::FsPred {

class AoePrediction {
public:
    static constexpr std::size_t kMaxTargets = AoeMath::kMaxTargets;

    struct PossibleTarget {
        SDK::Vector2 Position{};
        SDK::AIBaseClient Unit{};
        float HitRadius = 0.0f;
    };

    struct TargetSet {
        std::array<PossibleTarget, kMaxTargets> Items{};
        std::size_t Count = 0;
    };

    struct MecCircle {
        SDK::Vector2 Center{};
        float Radius = 0.0f;
        bool Valid = false;
    };

    template <typename PredictionEvaluator>
    static SDK::PredictionOutput GetPrediction(
        const SDK::PredictionInput& input,
        PredictionEvaluator&& predEvaluator) {
        NS_PROFILE("FsPred.AoE");
        switch (AoeMath::ResolveShapeDispatch(input.Type)) {
        case AoeMath::ShapeDispatch::Line:
            return Line::GetPrediction(input, predEvaluator);
        case AoeMath::ShapeDispatch::Circle:
            return Circle::GetPrediction(input, predEvaluator);
        case AoeMath::ShapeDispatch::Cone:
            return Cone::GetPrediction(input, predEvaluator);
        case AoeMath::ShapeDispatch::SingleTargetFallback:
        default:
            return predEvaluator(input, false, true);
        }
    }

private:
    static constexpr float kGeometryEpsilon = 1.0e-3f;
    static constexpr float kFallbackConeAngleDegrees = 45.0f;

    static float ResolveConeHalfAngle(
        const SDK::PredictionInput& input) {
        float angleDegrees = kFallbackConeAngleDegrees;
        if (input.Spell) {
            const auto spellInstance = input.Spell->Instance();
            if (spellInstance.IsValid()) {
                const std::string spellName = spellInstance.Name();
                const auto* entry = SDK::SpellDatabase::GetByName(spellName);
                if (entry && entry->Angle > 1 && entry->Angle < 180) {
                    angleDegrees = static_cast<float>(entry->Angle);
                }
            }
        }
        return AoeMath::ConeHalfAngleRadians(angleDegrees);
    }

    static int CountBits(std::uint8_t mask) {
        return AoeMath::HitCount(mask);
    }

    static bool ContainsNetworkId(const TargetSet& targets,
                                  std::uint32_t networkId) {
        for (std::size_t index = 0; index < targets.Count; ++index) {
            if (targets.Items[index].Unit.NetworkId() == networkId) {
                return true;
            }
        }
        return false;
    }

    template <typename PredictionEvaluator>
    static TargetSet BuildTargets(
        const SDK::PredictionInput& input,
        const SDK::PredictionOutput& primaryPrediction,
        PredictionEvaluator& predEvaluator) {
        TargetSet targets{};
        if (!input.Unit.IsValid()) {
            return targets;
        }

        const auto addTarget = [&](const SDK::AIBaseClient& unit,
                                   const SDK::Vector2& position) {
            if (targets.Count >= kMaxTargets || !unit.IsValid() ||
                ContainsNetworkId(targets, unit.NetworkId())) {
                return;
            }
            const float hitRadius = std::max(
                0.0f,
                input.Radius +
                    (input.AddHitBox ? unit.BoundingRadius() : 0.0f));
            targets.Items[targets.Count++] = {
                position,
                unit,
                hitRadius
            };
        };

        addTarget(input.Unit, primaryPrediction.GetUnitPosition().To2D());
        if (primaryPrediction.Hitchance < SDK::HitChance::Medium) {
            return targets;
        }

        const float checkRange = input.Range == FLT_MAX
            ? FLT_MAX
            : std::max(0.0f, input.Range) + 200.0f + input.RealRadius();
        for (const auto& hero : SDK::GameObjects::EnemyHeroesFrame()) {
            if (targets.Count >= kMaxTargets) {
                break;
            }
            if (!hero.IsValid() || ContainsNetworkId(targets, hero.NetworkId())) {
                continue;
            }
            if (!SDK::Extensions::IsValidTarget(
                    hero,
                    checkRange,
                    true,
                    input.ResolveRangeCheckFrom())) {
                continue;
            }

            SDK::PredictionInput targetInput = input;
            targetInput.Unit = hero;
            targetInput.AoE = false;
            const SDK::PredictionOutput predicted =
                predEvaluator(targetInput, false, false);
            if (predicted.Hitchance >= SDK::HitChance::High) {
                addTarget(hero, predicted.GetUnitPosition().To2D());
            }
        }
        return targets;
    }

    static bool CastPositionInRange(const SDK::PredictionInput& input,
                                    const SDK::Vector2& castPosition) {
        if (input.Range == FLT_MAX) {
            return true;
        }
        if (!std::isfinite(input.Range) || input.Range <= 0.0f) {
            return false;
        }
        const float range = input.Range + kGeometryEpsilon;
        return castPosition.DistanceSquared(
                   input.ResolveRangeCheckFrom().To2D()) <=
               range * range;
    }

    static bool CircleCovers(const MecCircle& circle,
                             const TargetSet& targets,
                             std::uint8_t mask) {
        if (!circle.Valid) {
            return false;
        }
        const float radius = circle.Radius + kGeometryEpsilon;
        const float radiusSquared = radius * radius;
        for (std::size_t index = 0; index < targets.Count; ++index) {
            if ((mask & (1u << index)) == 0) {
                continue;
            }
            if (targets.Items[index].Position.DistanceSquared(circle.Center) >
                radiusSquared) {
                return false;
            }
        }
        return true;
    }

    static MecCircle CircleFromPair(const SDK::Vector2& first,
                                    const SDK::Vector2& second) {
        const SDK::Vector2 center = (first + second) * 0.5f;
        return { center, center.Distance(first), true };
    }

    static MecCircle CircleFromTriple(const SDK::Vector2& a,
                                      const SDK::Vector2& b,
                                      const SDK::Vector2& c) {
        const double denominator = 2.0 * (
            static_cast<double>(a.x) * (b.y - c.y) +
            static_cast<double>(b.x) * (c.y - a.y) +
            static_cast<double>(c.x) * (a.y - b.y));
        if (std::abs(denominator) <= 1.0e-8) {
            return {};
        }

        const double aSquared =
            static_cast<double>(a.x) * a.x +
            static_cast<double>(a.y) * a.y;
        const double bSquared =
            static_cast<double>(b.x) * b.x +
            static_cast<double>(b.y) * b.y;
        const double cSquared =
            static_cast<double>(c.x) * c.x +
            static_cast<double>(c.y) * c.y;
        const SDK::Vector2 center{
            static_cast<float>((
                aSquared * (b.y - c.y) +
                bSquared * (c.y - a.y) +
                cSquared * (a.y - b.y)) / denominator),
            static_cast<float>((
                aSquared * (c.x - b.x) +
                bSquared * (a.x - c.x) +
                cSquared * (b.x - a.x)) / denominator)
        };
        if (!center.IsValid()) {
            return {};
        }
        return { center, center.Distance(a), true };
    }

    static MecCircle GetMec(const TargetSet& targets, std::uint8_t mask) {
        MecCircle best{};
        best.Radius = std::numeric_limits<float>::infinity();
        const auto consider = [&](const MecCircle& candidate) {
            if (candidate.Valid && candidate.Radius < best.Radius &&
                CircleCovers(candidate, targets, mask)) {
                best = candidate;
            }
        };

        for (std::size_t first = 0; first < targets.Count; ++first) {
            if ((mask & (1u << first)) == 0) {
                continue;
            }
            consider({ targets.Items[first].Position, 0.0f, true });
            for (std::size_t second = first + 1;
                 second < targets.Count;
                 ++second) {
                if ((mask & (1u << second)) == 0) {
                    continue;
                }
                consider(CircleFromPair(
                    targets.Items[first].Position,
                    targets.Items[second].Position));
                for (std::size_t third = second + 1;
                     third < targets.Count;
                     ++third) {
                    if ((mask & (1u << third)) == 0) {
                        continue;
                    }
                    consider(CircleFromTriple(
                        targets.Items[first].Position,
                        targets.Items[second].Position,
                        targets.Items[third].Position));
                }
            }
        }
        return best;
    }

    static SDK::PredictionOutput MakeAoeOutput(
        const SDK::PredictionInput& input,
        const SDK::PredictionOutput& primaryPrediction,
        const TargetSet& targets,
        const SDK::Vector2& castPosition,
        std::uint8_t hitMask) {
        SDK::PredictionOutput output;
        output.Input = input;
        output.Hitchance = primaryPrediction.Hitchance;
        output.SetUnitPosition(primaryPrediction.GetUnitPosition());
        output.SetCastPosition(SDK::Vector3::From2D(castPosition));
        output.AoeTargetsHit.reserve(CountBits(hitMask));
        for (std::size_t index = 0; index < targets.Count; ++index) {
            if ((hitMask & (1u << index)) == 0) {
                continue;
            }
            const auto& unit = targets.Items[index].Unit;
            if (unit.IsValid() && unit.IsHero()) {
                output.AoeTargetsHit.emplace_back(unit.Address());
            }
        }
        output.AoeTargetsHitCount =
            static_cast<int>(output.AoeTargetsHit.size());
        return output;
    }

    static float MinimumHitRadius(const TargetSet& targets,
                                  std::uint8_t mask) {
        float radius = std::numeric_limits<float>::infinity();
        for (std::size_t index = 0; index < targets.Count; ++index) {
            if ((mask & (1u << index)) != 0) {
                radius = std::min(radius, targets.Items[index].HitRadius);
            }
        }
        return radius;
    }

    static std::uint8_t CircleHitMask(const TargetSet& targets,
                                      const SDK::Vector2& center) {
        std::uint8_t mask = 0;
        for (std::size_t index = 0; index < targets.Count; ++index) {
            const float radius = targets.Items[index].HitRadius;
            if (targets.Items[index].Position.DistanceSquared(center) <=
                radius * radius + kGeometryEpsilon) {
                mask |= static_cast<std::uint8_t>(1u << index);
            }
        }
        return mask;
    }

    static bool PointHitsLine(const SDK::Vector2& point,
                              const SDK::Vector2& start,
                              const SDK::Vector2& end,
                              float radius) {
        return AoeMath::PointHitsLine(point, start, end, radius);
    }

    static std::uint8_t LineHitMask(const TargetSet& targets,
                                    const SDK::Vector2& start,
                                    const SDK::Vector2& end) {
        std::uint8_t mask = 0;
        for (std::size_t index = 0; index < targets.Count; ++index) {
            if (PointHitsLine(
                    targets.Items[index].Position,
                    start,
                    end,
                    targets.Items[index].HitRadius)) {
                mask |= static_cast<std::uint8_t>(1u << index);
            }
        }
        return mask;
    }

    static std::size_t CircleCircleIntersection(
        const SDK::Vector2& center1,
        const SDK::Vector2& center2,
        float radius1,
        float radius2,
        std::array<SDK::Vector2, 2>& intersections) {
        const float distance = center1.Distance(center2);
        if (distance > radius1 + radius2 ||
            distance <= std::abs(radius1 - radius2) ||
            distance <= kGeometryEpsilon) {
            return 0;
        }
        const float along =
            (radius1 * radius1 - radius2 * radius2 +
             distance * distance) /
            (2.0f * distance);
        const float heightSquared = radius1 * radius1 - along * along;
        if (heightSquared < 0.0f) {
            return 0;
        }
        const float height = std::sqrt(heightSquared);
        const SDK::Vector2 direction =
            (center2 - center1).Normalized();
        const SDK::Vector2 midpoint = center1 + direction * along;
        const SDK::Vector2 perpendicular{ -direction.y, direction.x };
        intersections[0] = midpoint + perpendicular * height;
        intersections[1] = midpoint - perpendicular * height;
        return 2;
    }

    static bool AddUniqueDirection(
        std::array<SDK::Vector2, 32>& directions,
        std::size_t& count,
        const SDK::Vector2& direction) {
        if (count >= directions.size() || direction.LengthSqr() <= 1.0e-6f) {
            return false;
        }
        const SDK::Vector2 normalized = direction.Normalized();
        for (std::size_t index = 0; index < count; ++index) {
            if (directions[index].Dot(normalized) >= 0.99999f) {
                return false;
            }
        }
        directions[count++] = normalized;
        return true;
    }

    static SDK::Vector2 Rotate(const SDK::Vector2& vector, float radians) {
        const float cosine = std::cos(radians);
        const float sine = std::sin(radians);
        return {
            vector.x * cosine - vector.y * sine,
            vector.x * sine + vector.y * cosine
        };
    }

    static std::uint8_t ConeHitMask(const TargetSet& targets,
                                    const SDK::Vector2& from,
                                    const SDK::Vector2& direction,
                                    float range,
                                    float halfAngleRadians) {
        std::uint8_t mask = 0;
        for (std::size_t index = 0; index < targets.Count; ++index) {
            if (AoeMath::PointHitsCone(
                    targets.Items[index].Position - from,
                    direction,
                    range,
                    halfAngleRadians,
                    targets.Items[index].HitRadius)) {
                mask |= static_cast<std::uint8_t>(1u << index);
            }
        }
        return mask;
    }

    class Circle {
    public:
        template <typename PredictionEvaluator>
        static SDK::PredictionOutput GetPrediction(
            const SDK::PredictionInput& input,
            PredictionEvaluator& predEvaluator) {
            const SDK::PredictionOutput primary =
                predEvaluator(input, false, true);
            const TargetSet targets =
                BuildTargets(input, primary, predEvaluator);
            if (targets.Count < 2) {
                return primary;
            }

            std::uint8_t bestMask = 0;
            SDK::Vector2 bestCenter{};
            int bestCount = 1;
            float bestDistanceSquared = std::numeric_limits<float>::infinity();
            const SDK::Vector2 rangeCheckFrom =
                input.ResolveRangeCheckFrom().To2D();
            const std::uint8_t maskLimit =
                static_cast<std::uint8_t>(1u << targets.Count);
            for (std::uint8_t subset = 1; subset < maskLimit; ++subset) {
                if ((subset & 1u) == 0 || CountBits(subset) < 2) {
                    continue;
                }
                const MecCircle circle = GetMec(targets, subset);
                if (!circle.Valid ||
                    circle.Radius >
                        MinimumHitRadius(targets, subset) + kGeometryEpsilon ||
                    !CastPositionInRange(input, circle.Center)) {
                    continue;
                }
                const std::uint8_t verified =
                    CircleHitMask(targets, circle.Center);
                if ((verified & 1u) == 0) {
                    continue;
                }
                const int count = CountBits(verified);
                const float distanceSquared =
                    circle.Center.DistanceSquared(rangeCheckFrom);
                if (AoeMath::IsBetterPrimaryCandidate(
                        verified,
                        distanceSquared,
                        bestCount,
                        bestDistanceSquared)) {
                    bestCount = count;
                    bestDistanceSquared = distanceSquared;
                    bestMask = verified;
                    bestCenter = circle.Center;
                }
            }

            if (bestCount > 1) {
                return MakeAoeOutput(
                    input,
                    primary,
                    targets,
                    bestCenter,
                    bestMask);
            }
            return primary;
        }
    };

    class Line {
    public:
        template <typename PredictionEvaluator>
        static SDK::PredictionOutput GetPrediction(
            const SDK::PredictionInput& input,
            PredictionEvaluator& predEvaluator) {
            const SDK::PredictionOutput primary =
                predEvaluator(input, false, true);
            const TargetSet targets =
                BuildTargets(input, primary, predEvaluator);
            if (targets.Count < 2 || !std::isfinite(input.Range) ||
                input.Range <= 0.0f || input.Range == FLT_MAX) {
                return primary;
            }

            const SDK::Vector2 from = input.ResolveFrom().To2D();
            std::array<SDK::Vector2, 32> directions{};
            std::size_t directionCount = 0;
            for (std::size_t index = 0; index < targets.Count; ++index) {
                const SDK::Vector2 toTarget =
                    targets.Items[index].Position - from;
                AddUniqueDirection(directions, directionCount, toTarget);

                const SDK::Vector2 midpoint =
                    (from + targets.Items[index].Position) * 0.5f;
                std::array<SDK::Vector2, 2> intersections{};
                const std::size_t intersectionCount = CircleCircleIntersection(
                    from,
                    midpoint,
                    std::max(0.0f, input.Radius),
                    from.Distance(midpoint),
                    intersections);
                for (std::size_t candidate = 0;
                     candidate < intersectionCount;
                     ++candidate) {
                    AddUniqueDirection(
                        directions,
                        directionCount,
                        targets.Items[index].Position - intersections[candidate]);
                }
            }

            int bestCount = 1;
            std::uint8_t bestMask = 0;
            SDK::Vector2 bestCast{};
            float bestDistanceSquared = std::numeric_limits<float>::infinity();
            for (std::size_t index = 0; index < directionCount; ++index) {
                const SDK::Vector2 cast =
                    from + directions[index] * input.Range;
                if (!CastPositionInRange(input, cast)) {
                    continue;
                }
                const std::uint8_t hits =
                    LineHitMask(targets, from, cast);
                if ((hits & 1u) == 0) {
                    continue;
                }
                const int count = CountBits(hits);
                const float distanceSquared = cast.DistanceSquared(
                    input.ResolveRangeCheckFrom().To2D());
                if (AoeMath::IsBetterPrimaryCandidate(
                        hits,
                        distanceSquared,
                        bestCount,
                        bestDistanceSquared)) {
                    bestCount = count;
                    bestDistanceSquared = distanceSquared;
                    bestMask = hits;
                    bestCast = cast;
                }
            }

            if (bestCount > 1) {
                return MakeAoeOutput(
                    input,
                    primary,
                    targets,
                    bestCast,
                    bestMask);
            }
            return primary;
        }
    };

    class Cone {
    public:
        template <typename PredictionEvaluator>
        static SDK::PredictionOutput GetPrediction(
            const SDK::PredictionInput& input,
            PredictionEvaluator& predEvaluator) {
            const SDK::PredictionOutput primary =
                predEvaluator(input, false, true);
            const TargetSet targets =
                BuildTargets(input, primary, predEvaluator);
            if (targets.Count < 2 || !std::isfinite(input.Range) ||
                input.Range <= 0.0f || input.Range == FLT_MAX) {
                return primary;
            }

            const SDK::Vector2 from = input.ResolveFrom().To2D();
            const float halfAngle = ResolveConeHalfAngle(input);
            std::array<SDK::Vector2, 32> directions{};
            std::size_t directionCount = 0;
            for (std::size_t index = 0; index < targets.Count; ++index) {
                const SDK::Vector2 direction =
                    targets.Items[index].Position - from;
                if (direction.LengthSqr() <= kGeometryEpsilon) {
                    continue;
                }
                const SDK::Vector2 normalized = direction.Normalized();
                AddUniqueDirection(directions, directionCount, normalized);
                AddUniqueDirection(
                    directions,
                    directionCount,
                    Rotate(normalized, halfAngle));
                AddUniqueDirection(
                    directions,
                    directionCount,
                    Rotate(normalized, -halfAngle));
            }
            for (std::size_t first = 0; first < targets.Count; ++first) {
                const SDK::Vector2 firstDirection =
                    (targets.Items[first].Position - from).Normalized();
                for (std::size_t second = first + 1;
                     second < targets.Count;
                     ++second) {
                    const SDK::Vector2 secondDirection =
                        (targets.Items[second].Position - from).Normalized();
                    AddUniqueDirection(
                        directions,
                        directionCount,
                        firstDirection + secondDirection);
                }
            }

            int bestCount = 1;
            std::uint8_t bestMask = 0;
            SDK::Vector2 bestCast{};
            float bestDistanceSquared = std::numeric_limits<float>::infinity();
            for (std::size_t index = 0; index < directionCount; ++index) {
                const SDK::Vector2 cast =
                    from + directions[index] * input.Range;
                if (!CastPositionInRange(input, cast)) {
                    continue;
                }
                const std::uint8_t hits = ConeHitMask(
                    targets,
                    from,
                    directions[index],
                    input.Range,
                    halfAngle);
                if ((hits & 1u) == 0) {
                    continue;
                }
                const int count = CountBits(hits);
                const float distanceSquared = cast.DistanceSquared(
                    input.ResolveRangeCheckFrom().To2D());
                if (AoeMath::IsBetterPrimaryCandidate(
                        hits,
                        distanceSquared,
                        bestCount,
                        bestDistanceSquared)) {
                    bestCount = count;
                    bestDistanceSquared = distanceSquared;
                    bestMask = hits;
                    bestCast = cast;
                }
            }

            if (bestCount > 1) {
                return MakeAoeOutput(
                    input,
                    primary,
                    targets,
                    bestCast,
                    bestMask);
            }
            return primary;
        }
    };
};

} // namespace Plugins::FsPred
