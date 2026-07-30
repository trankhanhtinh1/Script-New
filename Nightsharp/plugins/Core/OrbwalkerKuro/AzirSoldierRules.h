#pragma once

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <string_view>

namespace OrbwalkerKuro::AzirSoldierRules {

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
    if (left.size() != right.size()) return false;
    for (std::size_t i = 0; i < left.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(left[i])) !=
            std::tolower(static_cast<unsigned char>(right[i]))) {
            return false;
        }
    }
    return true;
}

inline bool ContainsInsensitive(std::string_view value,
                                std::string_view needle) {
    if (needle.empty()) return true;
    const auto it = std::search(
        value.begin(), value.end(), needle.begin(), needle.end(),
        [](char left, char right) {
            return std::tolower(static_cast<unsigned char>(left)) ==
                   std::tolower(static_cast<unsigned char>(right));
        });
    return it != value.end();
}

inline bool IsAzirChampionName(std::string_view name) {
    return EqualsInsensitive(name, "Azir");
}

inline bool IsSandSoldierName(std::string_view name) {
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
    return std::any_of(tokens.begin(), tokens.end(),
        [&](std::string_view token) {
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
    const float range = std::max(0.0f, attackRange) +
                        std::max(0.0f, targetBoundingRadius);
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
    if (soldierCount <= 0) return 0.0f;
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

inline float SecondaryLineDamageMultiplier(int championLevel) {
    const int level = ClampChampionLevel(championLevel);
    return level < 9
        ? 0.20f
        : std::min(1.0f, 0.20f + static_cast<float>(level - 8) * 0.08f);
}

} // namespace OrbwalkerKuro::AzirSoldierRules
