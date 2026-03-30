#pragma once

#include "../Enumerations/MinionOrderTypes.h"
#include "../Enumerations/MinionTeam.h"
#include "../Enumerations/MinionTypes.h"
#include "Objects.h"

#include <algorithm>
#include <cfloat>
#include <vector>

namespace SDK::GameObjects {

    inline std::vector<AIMinionClient> GetMinions(const Vector3& from,
                                                  float range,
                                                  MinionTypes type,
                                                  MinionTeam team,
                                                  bool sortByHealth);

    inline std::vector<AIMinionClient> GetMinions(const Vector3& from,
                                                  float range,
                                                  MinionTypes type,
                                                  MinionTeam team,
                                                  MinionOrderTypes order);

    inline bool MatchMinionTeam(const AIMinionClient& minion, MinionTeam team) {
        switch (team) {
        case MinionTeam::Ally:
            return minion.IsAlly();
        case MinionTeam::Enemy:
            return minion.IsEnemy();
        case MinionTeam::Neutral:
            return minion.IsJungleMonster();
        case MinionTeam::NotAlly:
            return !minion.IsAlly();
        case MinionTeam::All:
        default:
            return true;
        }
    }

    inline bool MatchMinionType(const AIMinionClient& minion, MinionTypes type) {
        if (type == MinionTypes::All) {
            return true;
        }
        if (HasFlag(type, MinionTypes::Jungle) && minion.IsJungleMonster()) {
            return true;
        }
        if (HasFlag(type, MinionTypes::Normal) && minion.IsLaneMinion()) {
            return true;
        }
        if (HasFlag(type, MinionTypes::Melee) && minion.IsMelee()) {
            return true;
        }
        if (HasFlag(type, MinionTypes::Ranged) && minion.IsRanged()) {
            return true;
        }
        return false;
    }

    inline std::vector<AIBaseClient> GetHeroes(const Vector3& from, float range = FLT_MAX, bool enemyOnly = true) {
        std::vector<AIBaseClient> out;
        const auto heroes = enemyOnly ? ObjectManager::EnemyHeroes() : ObjectManager::Heroes();
        out.reserve(heroes.size());
        for (const auto& hero : heroes) {
            if (hero.IsValidTarget(range, from)) {
                out.emplace_back(hero.Address());
            }
        }
        return out;
    }

    inline std::vector<AIMinionClient> GetMinions(const Vector3& from, float range = FLT_MAX, bool enemyOnly = true) {
        std::vector<AIMinionClient> out;
        const auto minions = enemyOnly ? ObjectManager::EnemyMinions() : ObjectManager::AllyMinions();
        out.reserve(minions.size());
        for (const auto& minion : minions) {
            if (minion.IsValidTarget(range, from)) {
                out.push_back(minion);
            }
        }
        return out;
    }

    inline std::vector<AIMinionClient> GetMinions(float range) {
        return GetMinions(ObjectManager::Player().Position(), range, true);
    }

    inline std::vector<AIMinionClient> GetMinions(float range,
                                                  MinionTypes type,
                                                  MinionTeam team = MinionTeam::Enemy,
                                                  bool sortByHealth = false) {
        return GetMinions(ObjectManager::Player().Position(), range, type, team, sortByHealth);
    }

    inline std::vector<AIMinionClient> GetMinions(const Vector3& from,
                                                  float range,
                                                  MinionTypes type,
                                                  MinionTeam team = MinionTeam::Enemy,
                                                  bool sortByHealth = false) {
        std::vector<AIMinionClient> pool;
        pool.reserve(512);
        for (const auto& minion : ObjectManager::AllyMinions()) {
            pool.push_back(minion);
        }
        for (const auto& minion : ObjectManager::EnemyMinions()) {
            pool.push_back(minion);
        }
        for (const auto& minion : ObjectManager::JungleMinions()) {
            pool.push_back(minion);
        }

        std::vector<AIMinionClient> out;
        out.reserve(pool.size());
        for (const auto& minion : pool) {
            if (!minion.IsValidTarget(range, from)) {
                continue;
            }
            if (!MatchMinionTeam(minion, team)) {
                continue;
            }
            if (!MatchMinionType(minion, type)) {
                continue;
            }
            out.push_back(minion);
        }

        if (sortByHealth) {
            std::sort(out.begin(), out.end(), [](const AIMinionClient& lhs, const AIMinionClient& rhs) {
                return lhs.Health() < rhs.Health();
            });
        }
        return out;
    }

    inline std::vector<AIMinionClient> GetMinions(const Vector3& from,
                                                  float range,
                                                  MinionTypes type,
                                                  MinionTeam team,
                                                  MinionOrderTypes order) {
        auto out = GetMinions(from, range, type, team, false);
        switch (order) {
        case MinionOrderTypes::Health:
            std::sort(out.begin(), out.end(), [](const AIMinionClient& lhs, const AIMinionClient& rhs) {
                return lhs.Health() < rhs.Health();
            });
            break;
        case MinionOrderTypes::MaxHealth:
            std::sort(out.begin(), out.end(), [](const AIMinionClient& lhs, const AIMinionClient& rhs) {
                return lhs.MaxHealth() > rhs.MaxHealth();
            });
            break;
        case MinionOrderTypes::Distance:
            std::sort(out.begin(), out.end(), [&from](const AIMinionClient& lhs, const AIMinionClient& rhs) {
                return lhs.Distance(from) < rhs.Distance(from);
            });
            break;
        case MinionOrderTypes::None:
        default:
            break;
        }
        return out;
    }

    inline std::vector<AIMinionClient> GetMinions(float range,
                                                  MinionTypes type,
                                                  MinionTeam team,
                                                  MinionOrderTypes order) {
        return GetMinions(ObjectManager::Player().Position(), range, type, team, order);
    }

    inline std::vector<AIMinionClient> GetJungles(const Vector3& from, float range = FLT_MAX) {
        std::vector<AIMinionClient> out;
        const auto monsters = ObjectManager::JungleMinions();
        out.reserve(monsters.size());
        for (const auto& monster : monsters) {
            if (monster.IsValidTarget(range, from)) {
                out.push_back(monster);
            }
        }
        return out;
    }

} // namespace SDK::GameObjects
