#pragma once
// ============================================================================
// MinionUtils.h — Minion Classification Utilities (EnsoulSharp SDK Port)
// ============================================================================
// Full port of EnsoulSharp.SDK/Core/Utils/Minion.cs
// Provides:
//   - GetMinionClass(), IsMinion(), IsPet(), IsClone()
//   - IsSiegeMinion(), IsSuperMinion(), IsWard()
//   - NormalMinionList, SiegeMinionList, SuperMinionList, WardList, PetList
//   - FarmLocation helpers (GetBestCircularFarmLocation, GetBestLineFarmLocation)
// ============================================================================

#include "core/Vector.h"
#include "GameObject.h"

#include <string>
#include <vector>
#include <algorithm>

namespace SDK {

// ============================================================================
// MinionClassification flags (separate from MinionClass in Enums.h which is
// the raw game value at LaneType offset; this is for SDK-level classification)
// ============================================================================
enum class MinionClass : int {
    Unknown = 0,
    Normal  = 1 << 0,
    Melee   = 1 << 1,
    Ranged  = 1 << 2,
    Siege   = 1 << 3,
    Super   = 1 << 4,
    Ward    = 1 << 5,
};

inline MinionClass operator|(MinionClass a, MinionClass b) {
    return (MinionClass)((int)a | (int)b);
}
inline bool HasFlag(MinionClass val, MinionClass flag) {
    return ((int)val & (int)flag) != 0;
}

// ============================================================================
// FarmLocation — Best position for farming spells
// ============================================================================
struct FarmLocation {
    Vec2 Position;
    int MinionsHit = 0;

    FarmLocation() = default;
    FarmLocation(const Vec2& pos, int count) : Position(pos), MinionsHit(count) {}
};

// ============================================================================
// MinionUtils — Static utility class
// ============================================================================
class MinionUtils {
public:

    // ---- Get minion type from character name ----
    static MinionClass GetMinionClass(const std::string& charName) {
        for (auto& n : s_normalMelee) {
            if (charName == n) return MinionClass::Normal | MinionClass::Melee;
        }
        for (auto& n : s_normalRanged) {
            if (charName == n) return MinionClass::Normal | MinionClass::Ranged;
        }
        for (auto& n : s_siegeMinions) {
            if (charName == n) return MinionClass::Siege | MinionClass::Ranged;
        }
        for (auto& n : s_superMinions) {
            if (charName == n) return MinionClass::Super | MinionClass::Melee;
        }
        for (auto& n : s_wards) {
            if (charName == n) return MinionClass::Ward;
        }
        return MinionClass::Unknown;
    }

    static MinionClass GetMinionClass(const GameObject& minion) {
        return GetMinionClass(minion.GetChampionName());
    }

    // ---- Quick checks ----
    static bool IsMinion(const std::string& charName) {
        auto type = GetMinionClass(charName);
        return HasFlag(type, MinionClass::Melee) || HasFlag(type, MinionClass::Ranged);
    }

    static bool IsMinion(const GameObject& minion) {
        return IsMinion(minion.GetChampionName());
    }

    static bool IsSiegeMinion(const std::string& charName) {
        return HasFlag(GetMinionClass(charName), MinionClass::Siege);
    }

    static bool IsSuperMinion(const std::string& charName) {
        return HasFlag(GetMinionClass(charName), MinionClass::Super);
    }

    static bool IsWard(const std::string& charName) {
        return HasFlag(GetMinionClass(charName), MinionClass::Ward);
    }

    // ---- Pet detection ----
    static bool IsPet(const std::string& charName, bool includeClones = true) {
        std::string lower = ToLower(charName);
        for (auto& pet : s_pets) {
            if (lower == pet) return true;
        }
        if (includeClones) {
            return IsClone(charName);
        }
        return false;
    }

    static bool IsPet(const GameObject& minion, bool includeClones = true) {
        return IsPet(minion.GetChampionName(), includeClones);
    }

    // ---- Clone detection ----
    static bool IsClone(const std::string& charName) {
        std::string lower = ToLower(charName);
        for (auto& clone : s_clones) {
            if (lower == clone) return true;
        }
        return false;
    }

    // ---- Farm location helpers ----
    static FarmLocation GetBestCircularFarmLocation(
        const std::vector<Vec2>& minionPositions,
        float width,
        float range,
        const Vec2& from)
    {
        FarmLocation best;
        float rangeSqr = range * range;

        for (auto& pos : minionPositions) {
            if (pos.DistanceSqr(from) > rangeSqr)
                continue;

            int count = 0;
            float widthSqr = width * width;
            for (auto& pos2 : minionPositions) {
                if (pos.DistanceSqr(pos2) <= widthSqr)
                    count++;
            }

            if (count > best.MinionsHit) {
                best.Position = pos;
                best.MinionsHit = count;
            }
        }
        return best;
    }

    static FarmLocation GetBestLineFarmLocation(
        const std::vector<Vec2>& minionPositions,
        float width,
        float range,
        const Vec2& from)
    {
        FarmLocation best;
        float rangeSqr = range * range;

        // Generate candidate positions
        std::vector<Vec2> candidates = minionPositions;
        for (size_t i = 0; i < minionPositions.size(); i++) {
            for (size_t j = i + 1; j < minionPositions.size(); j++) {
                candidates.push_back((minionPositions[i] + minionPositions[j]) * 0.5f);
            }
        }

        float widthSqr = width * width;
        for (auto& pos : candidates) {
            if (pos.DistanceSqr(from) > rangeSqr)
                continue;

            Vec2 endPos = from + (pos - from).Normalized() * range;

            int count = 0;
            for (auto& mPos : minionPositions) {
                float dist = Geometry::PointToSegmentDistance(mPos, from, endPos);
                if (dist * dist <= widthSqr)
                    count++;
            }

            if (count > best.MinionsHit) {
                best.Position = endPos;
                best.MinionsHit = count;
            }
        }
        return best;
    }

private:
    static std::string ToLower(const std::string& s) {
        std::string result = s;
        std::transform(result.begin(), result.end(), result.begin(), ::tolower);
        return result;
    }

    // ---- Name databases ----
    static inline const std::vector<std::string> s_normalMelee = {
        "SRU_ChaosMinionMelee", "SRU_OrderMinionMelee",
        "HA_ChaosMinionMelee",  "HA_OrderMinionMelee"
    };

    static inline const std::vector<std::string> s_normalRanged = {
        "SRU_ChaosMinionRanged", "SRU_OrderMinionRanged",
        "HA_ChaosMinionRanged",  "HA_OrderMinionRanged"
    };

    static inline const std::vector<std::string> s_siegeMinions = {
        "SRU_ChaosMinionSiege", "SRU_OrderMinionSiege",
        "HA_ChaosMinionSiege",  "HA_OrderMinionSiege"
    };

    static inline const std::vector<std::string> s_superMinions = {
        "SRU_ChaosMinionSuper", "SRU_OrderMinionSuper",
        "HA_ChaosMinionSuper",  "HA_OrderMinionSuper"
    };

    static inline const std::vector<std::string> s_wards = {
        "SightWard", "YellowTrinket", "BlueTrinket", "JammerDevice",
        "YellowTrinketUpgrade", "VisionWard", "ControlWard"
    };

    static inline const std::vector<std::string> s_pets = {
        "annietibbers",
        "elisespiderling",
        "heimertyellow", "heimertblue",
        "ivernminion",
        "malzaharvoidling",
        "shacobox",
        "yorickghoulmelee", "yorickbigghoul",
        "zyrathornplant", "zyragraspingplant",
        "illaoiminion",
        // Season 2026 additions
        "voidspawn",
        "jihnmine",         // Jhin trap
        "teemomushroom",    // Teemo shroom
        "shaco",            // Shaco clone
        "yorickmistwalker",
        "azirsoldierghost",
    };

    static inline const std::vector<std::string> s_clones = {
        "leblanc", "monkeyking", "neeko", "shaco",
        // Season 2026
        "fiddlesticks"  // Fiddlesticks effigies
    };
};

} // namespace SDK
