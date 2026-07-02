#pragma once

#include "../Core/Objects.h"
#include "../Enumerations/HitChance.h"
#include "../Enumerations/MinionTypes.h"
#include "../Enumerations/SkillshotType.h"
#include "../Extensions/Enumerable.h"
#include "../GameObjects/ObjectManager.h"
#include "../Math/ConvexHull.h"
#include "../Math/Prediction.h"

#include <algorithm>
#include <cfloat>
#include <cstddef>
#include <vector>

namespace SDK::Core::Utils {

struct FarmLocation {
    Vec2 Position = {};
    int MinionsHit = 0;

    FarmLocation() = default;
    FarmLocation(const Vec2& position, int minionsHit)
        : Position(position), MinionsHit(minionsHit) {}
};

class Minion {
public:
    static FarmLocation GetBestCircularFarmLocation(const std::vector<Vec2>& minionPositions,
                                                    float width,
                                                    float range,
                                                    int useMecMax = 9) {
        Vec2 best{};
        int bestCount = 0;
        const Vec2 startPos = SDK::ObjectManager::Player().Position().To2D();
        const float rangeSqr = range * range;
        const float widthSqr = width * width;

        if (minionPositions.empty()) {
            return FarmLocation(best, bestCount);
        }

        if (useMecMax > 0 && minionPositions.size() <= static_cast<std::size_t>(useMecMax)) {
            const auto subGroups = SDK::Extensions::GetCombinations(minionPositions);
            for (const auto& subGroup : subGroups) {
                if (subGroup.empty()) {
                    continue;
                }

                const auto circle = SDK::ConvexHull::GetMec(subGroup);
                if (circle.Radius <= width && circle.Center.DistanceSqr(startPos) <= rangeSqr) {
                    return FarmLocation(circle.Center, static_cast<int>(subGroup.size()));
                }
            }
        }

        for (const auto& pos : minionPositions) {
            if (pos.DistanceSqr(startPos) > rangeSqr) {
                continue;
            }

            int count = 0;
            for (const auto& other : minionPositions) {
                if (pos.DistanceSqr(other) <= widthSqr) {
                    ++count;
                }
            }

            if (count >= bestCount) {
                best = pos;
                bestCount = count;
            }
        }

        return FarmLocation(best, bestCount);
    }

    static FarmLocation GetBestLineFarmLocation(const std::vector<Vec2>& minions, float width, float range) {
        Vec2 best{};
        int bestCount = 0;
        const Vec2 startPos = SDK::ObjectManager::Player().Position().To2D();
        const float rangeSqr = range * range;
        const float widthSqr = width * width;

        std::vector<Vec2> candidates = minions;
        for (std::size_t i = 0; i < minions.size(); ++i) {
            for (std::size_t j = 0; j < minions.size(); ++j) {
                if (i != j) {
                    candidates.push_back((minions[i] + minions[j]) / 2.0f);
                }
            }
        }

        for (const auto& pos : candidates) {
            if (pos.DistanceSqr(startPos) > rangeSqr) {
                continue;
            }

            const Vec2 endPos = startPos + ((pos - startPos).Normalized() * range);
            int count = 0;
            for (const auto& minion : minions) {
                if (DistancePointSegmentSqr(minion, startPos, endPos) <= widthSqr) {
                    ++count;
                }
            }

            if (count >= bestCount) {
                best = endPos;
                bestCount = count;
            }
        }

        return FarmLocation(best, bestCount);
    }

    static std::vector<Vec2> GetMinionsPredictedPositions(const std::vector<AIBaseClient>& minions,
                                                          float delay,
                                                          float width,
                                                          float speed,
                                                          Vec3 from,
                                                          float range,
                                                          bool collision,
                                                          SkillshotType stype,
                                                          Vec3 rangeCheckFrom = {}) {
        std::vector<Vec2> positions;
        const auto player = SDK::ObjectManager::Player();
        from = from.To2D().IsValid() && !from.To2D().IsZero()
            ? from
            : player.Position();

        for (const auto& minion : minions) {
            if (!minion.IsValid() || minion.IsDead()) {
                continue;
            }

            PredictionInput input;
            input.Unit = minion;
            input.Delay = delay;
            input.Radius = width;
            input.Speed = speed;
            input.From = from;
            input.Range = range;
            input.Collision = collision;
            input.SetType(stype);
            input.RangeCheckFrom = rangeCheckFrom;

            const auto prediction = SDK::Prediction::GetPrediction(input);
            if (prediction.Hitchance >= HitChance::High) {
                positions.push_back(prediction.GetUnitPosition().To2D());
            }
        }
        return positions;
    }

    static MinionTypes GetMinionType(const AIMinionClient& minion) {
        return minion.GetMinionType();
    }

    static bool IsMinion(const AIMinionClient& minion) {
        return minion.IsMinion();
    }

    static bool IsPet(const AIMinionClient& minion) {
        return minion.IsPet();
    }

    static bool IsClone(const AIMinionClient& minion) {
        return minion.IsClone();
    }

private:
    static float DistancePointSegmentSqr(const Vec2& point, const Vec2& start, const Vec2& end) {
        const Vec2 segment = end - start;
        const float lengthSqr = segment.LengthSqr();
        if (lengthSqr <= 1e-6f) {
            return point.DistanceSqr(start);
        }

        float t = (point - start).Dot(segment) / lengthSqr;
        t = std::max(0.0f, std::min(1.0f, t));
        const Vec2 projection = start + (segment * t);
        return point.DistanceSqr(projection);
    }
};

} // namespace SDK::Core::Utils

namespace SDK::Utils {
    using FarmLocation = ::SDK::Core::Utils::FarmLocation;
    using Minion = ::SDK::Core::Utils::Minion;
} // namespace SDK::Utils
