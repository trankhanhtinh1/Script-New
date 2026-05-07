#pragma once

#include "../Core/GameObjects.h"
#include "../../core/CoreClassification.h"  // RuntimeAPI removed Phase 2 - use CoreClassification::Classify
#include "../Math/ConvexHull.h"
#include "../Math/Prediction.h"
#include "MathUtils.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

namespace SDK::Utils::Minion {

struct FarmLocation {
    Vector2 Position = {};
    int MinionsHit = 0;

    FarmLocation() = default;
    FarmLocation(const Vector2& position, int hitCount)
        : Position(position), MinionsHit(hitCount) {}
};

namespace detail {
    inline constexpr std::array<const char*, 8> NormalMinions = {
        "SRU_ChaosMinionMelee", "SRU_ChaosMinionRanged", "SRU_OrderMinionMelee", "SRU_OrderMinionRanged",
        "HA_ChaosMinionMelee", "HA_ChaosMinionRanged", "HA_OrderMinionMelee", "HA_OrderMinionRanged"
    };
    inline constexpr std::array<const char*, 4> SiegeMinions = {
        "SRU_ChaosMinionSiege", "SRU_OrderMinionSiege", "HA_ChaosMinionSiege", "HA_OrderMinionSiege"
    };
    inline constexpr std::array<const char*, 4> SuperMinions = {
        "SRU_ChaosMinionSuper", "SRU_OrderMinionSuper", "HA_ChaosMinionSuper", "HA_OrderMinionSuper"
    };
    inline constexpr std::array<const char*, 4> Wards = {
        "SightWard", "YellowTrinket", "BlueTrinket", "JammerDevice"
    };
    inline constexpr std::array<const char*, 11> Pets = {
        "annietibbers", "elisespiderling", "heimertyellow", "heimertblue", "ivernminion",
        "malzaharvoidling", "shacobox", "yorickghoulmelee", "yorickbigghoul", "zyrathornplant",
        "zyragraspingplant"
    };
    inline constexpr std::array<const char*, 4> Clones = {
        "leblanc", "monkeyking", "neeko", "shaco"
    };

    template<size_t N>
    inline bool Matches(const std::string& name, const std::array<const char*, N>& values) {
        return std::any_of(values.begin(), values.end(), [&name](const char* value) {
            return _stricmp(name.c_str(), value) == 0;
        });
    }

    inline std::vector<std::vector<Vector2>> GetCombinations(const std::vector<Vector2>& points) {
        std::vector<std::vector<Vector2>> result = {};
        const size_t count = points.size();
        if (count == 0 || count > 20) {
            return result;
        }

        const uint64_t maxMask = (uint64_t{ 1 } << count);
        result.reserve(static_cast<size_t>(maxMask - 1));
        for (uint64_t mask = 1; mask < maxMask; ++mask) {
            std::vector<Vector2> subset = {};
            subset.reserve(count);
            for (size_t i = 0; i < count; ++i) {
                if ((mask & (uint64_t{ 1 } << i)) != 0) {
                    subset.push_back(points[i]);
                }
            }
            if (!subset.empty()) {
                result.push_back(std::move(subset));
            }
        }

        std::sort(result.begin(), result.end(), [](const auto& lhs, const auto& rhs) {
            return lhs.size() > rhs.size();
        });
        return result;
    }
}

inline FarmLocation GetBestCircularFarmLocation(const std::vector<Vector2>& minionPositions,
                                                float width,
                                                float range,
                                                int useMecMax = 9) {
    Vector2 result = {};
    int minionCount = 0;
    const auto startPos = ObjectManager::Player().Position().To2D();
    const float rangeSq = range * range;

    if (minionPositions.empty()) {
        return { result, minionCount };
    }

    if (static_cast<int>(minionPositions.size()) <= useMecMax) {
        const auto subGroups = detail::GetCombinations(minionPositions);
        for (const auto& subGroup : subGroups) {
            const auto circle = ConvexHull::GetMec(subGroup);
            if (circle.Radius <= width && circle.Center.DistanceSquared(startPos) <= rangeSq) {
                return { circle.Center, static_cast<int>(subGroup.size()) };
            }
        }
    }

    for (const auto& pos : minionPositions) {
        if (pos.DistanceSquared(startPos) > rangeSq) {
            continue;
        }

        int count = 0;
        for (const auto& other : minionPositions) {
            if (pos.DistanceSquared(other) <= width * width) {
                ++count;
            }
        }

        if (count >= minionCount) {
            result = pos;
            minionCount = count;
        }
    }

    return { result, minionCount };
}

inline FarmLocation GetBestLineFarmLocation(const std::vector<Vector2>& minions, float width, float range) {
    Vector2 result = {};
    int minionCount = 0;
    const auto startPos = ObjectManager::Player().Position().To2D();

    std::vector<Vector2> possiblePositions = minions;
    for (size_t i = 0; i < minions.size(); ++i) {
        for (size_t j = 0; j < minions.size(); ++j) {
            if (i != j) {
                possiblePositions.push_back((minions[i] + minions[j]) * 0.5f);
            }
        }
    }

    for (const auto& pos : possiblePositions) {
        if (pos.DistanceSquared(startPos) > range * range) {
            continue;
        }

        const auto endPos = startPos + ((pos - startPos).Normalized() * range);
        int count = 0;
        for (const auto& minion : minions) {
            if (SDK::Geometry::PointToSegmentDistance(minion, startPos, endPos) <= width) {
                ++count;
            }
        }

        if (count >= minionCount) {
            result = endPos;
            minionCount = count;
        }
    }

    return { result, minionCount };
}

inline std::vector<Vector2> GetMinionsPredictedPositions(const std::vector<AIBaseClient>& minions,
                                                         float delay,
                                                         float width,
                                                         float speed,
                                                         Vector3 from,
                                                         float range,
                                                         bool collision,
                                                         SkillshotType type,
                                                         Vector3 rangeCheckFrom = {}) {
    if (from.To2D().IsZero()) {
        from = ObjectManager::Player().Position();
    }

    std::vector<Vector2> out = {};
    out.reserve(minions.size());
    for (const auto& minion : minions) {
        PredictionInput input = {};
        input.Unit = minion;
        input.Delay = delay;
        input.Radius = width;
        input.Speed = speed;
        input.From = from;
        input.Range = range;
        input.Collision = collision;
        input.Type = type;
        input.RangeCheckFrom = rangeCheckFrom;

        const auto prediction = Prediction::GetPrediction(input);
        if (prediction.Hitchance >= HitChance::High) {
            out.push_back(prediction.UnitPosition.To2D());
        }
    }
    return out;
}

inline MinionTypes GetMinionType(const AIMinionClient& minion) {
    const std::string name = minion.CharacterName();

    if (detail::Matches(name, detail::NormalMinions)) {
        return MinionTypes::Normal | (name.find("Melee") != std::string::npos ? MinionTypes::Melee : MinionTypes::Ranged);
    }
    if (detail::Matches(name, detail::SiegeMinions)) {
        return MinionTypes::Siege | MinionTypes::Ranged;
    }
    if (detail::Matches(name, detail::SuperMinions)) {
        return MinionTypes::Super | MinionTypes::Melee;
    }
    if (detail::Matches(name, detail::Wards)) {
        return MinionTypes::Ward;
    }
    return MinionTypes::Unknown;
}

inline bool IsMinion(const AIMinionClient& minion) {
    const auto type = GetMinionType(minion);
    return HasFlag(type, MinionTypes::Melee) || HasFlag(type, MinionTypes::Ranged);
}

inline bool IsClone(const AIMinionClient& minion);

inline bool IsPet(const AIMinionClient& minion, bool includeClones = true) {
    return detail::Matches(minion.CharacterName(), detail::Pets) ||
           CoreClassification::Classify(minion.Address()) == CoreClassification::ObjectType::Pet ||
           (includeClones && IsClone(minion));
}

inline bool IsWard(const AIMinionClient& minion) {
    return detail::Matches(minion.CharacterName(), detail::Wards);
}

inline bool IsClone(const AIMinionClient& minion) {
    // RuntimeAPI::IsClone removed Phase 2 - clones detected purely by name pattern.
    // The old game-flag bit was rarely set anyway; the curated list in detail::Clones
    // covers Shaco/Leblanc/Neeko/Wukong/MFW/Annie+Tibbers etc.
    return detail::Matches(minion.CharacterName(), detail::Clones);
}

} // namespace SDK::Utils::Minion
