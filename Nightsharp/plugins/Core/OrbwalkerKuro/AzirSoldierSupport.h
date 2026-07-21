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

// Live 16.14 values.  The command radius is Azir W's tether, while 375 is
// the primary target range of a Sand Soldier stab.  The spear continues for
// another 50 units, but that extension may only hit secondary bodies and must
// never be used to decide whether an attack command itself is legal.
inline constexpr float kCommandRadius = 660.0f;
inline constexpr float kPrimaryAttackRange = 375.0f;
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

inline char LowerAscii(char value) {
    return static_cast<char>(
        std::tolower(static_cast<unsigned char>(value)));
}

inline bool EqualsInsensitive(std::string_view left, std::string_view right) {
    if (left.size() != right.size()) {
        return false;
    }
    for (std::size_t index = 0; index < left.size(); ++index) {
        if (LowerAscii(left[index]) != LowerAscii(right[index])) {
            return false;
        }
    }
    return true;
}

inline bool ContainsInsensitive(std::string_view value,
                                std::string_view needle) {
    if (needle.empty()) {
        return true;
    }
    if (needle.size() > value.size()) {
        return false;
    }
    for (std::size_t start = 0; start + needle.size() <= value.size(); ++start) {
        bool match = true;
        for (std::size_t index = 0; index < needle.size(); ++index) {
            if (LowerAscii(value[start + index]) != LowerAscii(needle[index])) {
                match = false;
                break;
            }
        }
        if (match) {
            return true;
        }
    }
    return false;
}

inline bool IsAzirChampionName(std::string_view name) {
    return EqualsInsensitive(name, "Azir");
}

inline bool IsSandSoldierName(std::string_view name) {
    if (name.empty()) {
        return false;
    }

    // Exclude R wall soldiers explicitly
    if (EqualsInsensitive(name, "AzirRSolider") || EqualsInsensitive(name, "AzirRSoldier")) {
        return false;
    }

    // Exact string match (accepting both AzirSoldier and Riot's internal typo AzirSolider)
    return EqualsInsensitive(name, "AzirSoldier") ||
           EqualsInsensitive(name, "Azir_Base_P_Soldier_Ring") ||
           EqualsInsensitive(name, "Azir_Base_W_Soldier") ||
           EqualsInsensitive(name, "Azir_Base_W_Soldier_Ring");
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
                          float commandRadius = kCommandRadius) {
    const float safeRadius = std::max(0.0f, commandRadius) + 35.0f;
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
                             TargetKind kind) {
    return CanUseSoldierAttack(kind) &&
           IsCommandable(azir, soldier) &&
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

struct EventDrivenSoldierCache {
    int tick = -1;
    int playerNetworkId = 0;
    std::vector<::SDK::GameObject> soldiers;
    bool createSubscribed = false;
    bool deleteSubscribed = false;
};

inline EventDrivenSoldierCache& EventSoldierCache() {
    static EventDrivenSoldierCache cache;
    return cache;
}

inline void EnsureSoldierEventSubscribed() {
    auto& cache = EventSoldierCache();
    if (!cache.createSubscribed) {
        cache.createSubscribed = ::SDK::Events::AddOnCreateObject([](const ::SDK::Events::ObjectEventArgs& args) {
            if (!args.Sender.IsValid()) return;
            if (IsSandSoldierName(args.Sender.CharacterName) ||
                IsSandSoldierName(args.Sender.Name)) {
                auto& c = EventSoldierCache();
                ::Core::Objects::ObjectHandle handle{};
                handle.address   = args.Sender.Ptr;
                handle.index     = args.Sender.Index;
                handle.networkId = args.Sender.NetworkId;
                handle.type      = args.Sender.Type;
                const ::SDK::GameObject obj(handle);
                if (obj.IsValid()) {
                    bool found = false;
                    for (const auto& existing : c.soldiers) {
                        if (existing.NetworkId() == obj.NetworkId()) {
                            found = true;
                            break;
                        }
                    }
                    if (!found) {
                        c.soldiers.push_back(obj);
                    }
                }
            }
        });

        // One-time initial seed scan at event subscription time for any soldier created before script load
        const auto player = ::SDK::ObjectManager::Player();
        if (player.IsValid()) {
            const ::SDK::GameObjectTeam playerTeam = player.Team();
            for (const auto& minion : ::SDK::GameObjects::Get<::SDK::AIMinionClient>()) {
                if (minion.IsValid() && !minion.IsDead() && minion.Team() == playerTeam &&
                    (IsSandSoldierName(minion.CharacterName()) || IsSandSoldierName(minion.Name()))) {
                    bool found = false;
                    for (const auto& existing : cache.soldiers) {
                        if (existing.NetworkId() == minion.NetworkId()) {
                            found = true;
                            break;
                        }
                    }
                    if (!found) {
                        cache.soldiers.push_back(minion);
                    }
                }
            }
        }
    }

    if (!cache.deleteSubscribed) {
        cache.deleteSubscribed = ::SDK::Events::AddOnDeleteObject([](const ::SDK::Events::ObjectEventArgs& args) {
            if (!args.Sender.IsValid()) return;
            const int netId = static_cast<int>(args.Sender.NetworkId);
            auto& c = EventSoldierCache();
            c.soldiers.erase(
                std::remove_if(c.soldiers.begin(), c.soldiers.end(),
                    [netId](const ::SDK::GameObject& obj) {
                        return obj.NetworkId() == netId;
                    }),
                c.soldiers.end());
        });
    }
}

inline const std::vector<::SDK::GameObject>& GetAzirSandSoldiers(
    const ::SDK::AIHeroClient& player
) {
    EnsureSoldierEventSubscribed();
    auto& cache = EventSoldierCache();
    const int now = ::SDK::Game::TickCount();
    const int playerNetworkId = player.IsValid() ? player.NetworkId() : 0;
    if (cache.tick == now && cache.playerNetworkId == playerNetworkId) {
        return cache.soldiers;
    }

    cache.tick = now;
    cache.playerNetworkId = playerNetworkId;

    if (!player.IsValid() || player.IsDead() || !IsAzirChampionName(player.CharacterName())) {
        cache.soldiers.clear();
        return cache.soldiers;
    }

    const ::SDK::GameObjectTeam playerTeam = player.Team();

    // Fast loop: validate ONLY tracked soldier GameObjects (0 to 3 items max!)
    for (auto it = cache.soldiers.begin(); it != cache.soldiers.end(); ) {
        const auto& obj = *it;
        if (!obj.IsValid() || obj.IsDead() || obj.Team() != playerTeam) {
            it = cache.soldiers.erase(it);
            continue;
        }
        ++it;
    }

    return cache.soldiers;
}

} // namespace OrbwalkerKuro::AzirSoldierSupport
