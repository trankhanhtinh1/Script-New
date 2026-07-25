#pragma once

#include "../../../sdk/Events/Events.h"
#include "../../../sdk/GameObjects/GameObjects.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <string_view>
#include <vector>

namespace OrbwalkerKuro::AzirSoldierSupport {

inline constexpr float kCommandRadius = 660.0f;
inline constexpr float kPrimaryAttackRange = 350.0f;
inline constexpr float kSpearPassThroughRange = 50.0f;
inline constexpr float kAdditionalSoldierDamageMultiplier = 0.25f;
inline constexpr float kOnHitEffectiveness = 0.50f;

struct Point2 {
    float x = 0.0f;
    float y = 0.0f;
};

enum class TargetKind {
    OrdinaryUnit,
    Structure,
    WardOrTrap,
};

inline bool EqualsInsensitive(std::string_view left, std::string_view right) {
    if (left.size() != right.size()) {
        return false;
    }
    return _strnicmp(left.data(), right.data(), left.size()) == 0;
}

inline bool ContainsInsensitive(std::string_view value, std::string_view needle) {
    if (needle.empty()) {
        return true;
    }
    auto it = std::search(
        value.begin(), value.end(),
        needle.begin(), needle.end(),
        [](char c1, char c2) {
            return std::tolower(static_cast<unsigned char>(c1)) ==
                   std::tolower(static_cast<unsigned char>(c2));
        }
    );
    return it != value.end();
}

inline bool IsAzirChampionName(std::string_view name) {
    return EqualsInsensitive(name, "Azir");
}

inline bool IsSandSoldierName(std::string_view name) {
    if (name.empty()) {
        return false;
    }

    return EqualsInsensitive(name, "AzirSoldier") ||
           EqualsInsensitive(name, "AzirSolider");
}

inline bool IsSoldierAttackSpellName(std::string_view name) {
    return EqualsInsensitive(name, "AzirBasicAttackSoldier") ||
           EqualsInsensitive(name, "AzirSoldierBasicAttack") ||
           (ContainsInsensitive(name, "azir") &&
            ContainsInsensitive(name, "soldier") &&
            ContainsInsensitive(name, "attack"));
}

inline bool IsWardOrTrapName(std::string_view name) {
    static constexpr std::array<std::string_view, 15> tokens = {
        "ward", "trinket", "jammerdevice", "teemomushroom",
        "noxioustrap", "shacobox", "jhintrap", "jhinlotustrap",
        "caitlyntrap", "caitlynyordletrap", "nidaleetrap",
        "nidaleebushwhack", "maokaisapling", "fiddlestickseffigy",
        "sightstone",
    };
    return std::any_of(tokens.begin(), tokens.end(), [&](std::string_view token) {
        return ContainsInsensitive(name, token);
    });
}

inline bool CanUseSoldierAttack(TargetKind kind) {
    return kind == TargetKind::OrdinaryUnit;
}

inline float DistanceSquared(Point2 left, Point2 right) {
    const float dx = left.x - right.x;
    const float dy = left.y - right.y;
    return dx * dx + dy * dy;
}

inline bool IsCommandable(Point2 azir,
                          Point2 soldier,
                          float soldierBoundingRadius = 0.0f,
                          float playerBoundingRadius = 0.0f,
                          float commandRadius = kCommandRadius) {
    const float safeRadius = std::max(0.0f, commandRadius) +
                             std::max(0.0f, playerBoundingRadius) +
                             std::max(0.0f, soldierBoundingRadius);
    return DistanceSquared(azir, soldier) <= safeRadius * safeRadius;
}

inline bool CanReachPrimaryTarget(Point2 soldier,
                                  Point2 target,
                                  float targetBoundingRadius = 0.0f,
                                  float attackRange = kPrimaryAttackRange) {
    // AzirSoldierBasicAttack uses castRangeUseBoundingBoxes in current
    // CommunityDragon data, hence the target radius belongs in this test.
    const float range = std::max(0.0f, attackRange) +
                        std::max(0.0f, targetBoundingRadius) + 25.0f;
    return DistanceSquared(soldier, target) <= range * range;
}

inline bool CanCommandAttack(Point2 azir,
                             Point2 soldier,
                             Point2 target,
                             float targetBoundingRadius,
                             TargetKind kind,
                             float soldierBoundingRadius = 0.0f) {
    return CanUseSoldierAttack(kind) &&
           IsCommandable(azir, soldier, soldierBoundingRadius) &&
           CanReachPrimaryTarget(soldier, target, targetBoundingRadius);
}

inline int ClampChampionLevel(int level) {
    return std::clamp(level, 1, 18);
}

inline int ClampWRank(int rank) {
    return std::clamp(rank, 1, 5);
}

inline float SoldierBaseDamage(int championLevel, int wRank) {
    static constexpr std::array<float, 5> base = {
        50.0f, 65.0f, 80.0f, 95.0f, 110.0f,
    };
    const int level = ClampChampionLevel(championLevel);
    const float levelDamage = level >= 10
        ? static_cast<float>(level - 9) * 8.0f
        : 0.0f;
    return base[ClampWRank(wRank) - 1] + levelDamage;
}

inline float SoldierApRatio(int wRank) {
    static constexpr std::array<float, 5> ratios = {
        0.35f, 0.425f, 0.50f, 0.575f, 0.65f,
    };
    return ratios[ClampWRank(wRank) - 1];
}

inline float SoldierRawDamage(int championLevel,
                              int wRank,
                              float abilityPower) {
    return SoldierBaseDamage(championLevel, wRank) +
           SoldierApRatio(wRank) * std::max(0.0f, abilityPower);
}

inline float MultiSoldierDamageMultiplier(int soldierCount) {
    if (soldierCount <= 0) {
        return 0.0f;
    }
    return 1.0f + static_cast<float>(soldierCount - 1) *
                      kAdditionalSoldierDamageMultiplier;
}

inline float MultiSoldierRawDamage(int championLevel,
                                   int wRank,
                                   float abilityPower,
                                   int soldierCount) {
    return SoldierRawDamage(championLevel, wRank, abilityPower) *
           MultiSoldierDamageMultiplier(soldierCount);
}

// A stab can continue 50 units beyond the commanded target.  Current live
// scaling starts at 20% and gains 8 percentage points per level from level 9,
// reaching 100% at level 18.  This is deliberately separate from primary
// target reach/damage so last-hit code cannot confuse a collateral hit with a
// legal attack target.
inline float SecondaryLineDamageMultiplier(int championLevel) {
    const int level = ClampChampionLevel(championLevel);
    return level < 9
        ? 0.20f
        : std::min(1.0f, 0.20f + static_cast<float>(level - 8) * 0.08f);
}

inline std::vector<::SDK::AIMinionClient> GetAzirSandSoldiers(
    const ::SDK::AIHeroClient& player
) {
    std::vector<::SDK::AIMinionClient> result;

    if (!player.IsValid() || player.IsDead() || !IsAzirChampionName(player.CharacterName())) {
        return result;
    }

    const ::SDK::GameObjectTeam playerTeam = player.Team();

    for (const auto& minion : ::SDK::GameObjects::Minions()) {
        if (!minion.IsValid() || minion.IsDead() || minion.Team() != playerTeam) {
            continue;
        }

        const std::string charName = minion.CharacterName();
        const std::string name = minion.Name();
        if (IsSandSoldierName(charName) || IsSandSoldierName(name)) {
            result.push_back(minion);
        }
    }

    return result;
}

} // namespace OrbwalkerKuro::AzirSoldierSupport
