#pragma once
#include "ObjectManager.h"
#include "GameObject.h"
#include "Game.h"
#include <vector>
#include <algorithm>

// ============================================================================
// GameObjects — Cached & categorized object lists (updated each frame)
// Reference: EnsoulSharp.SDK/Core/GameObjects.cs
// ============================================================================

namespace SDK {
namespace GameObjects {

    // ====================================================================
    // Cached lists — updated each frame via Update()
    // ====================================================================
    inline GameObject Player;

    inline std::vector<GameObject> AllHeroes;
    inline std::vector<GameObject> AllyHeroes;
    inline std::vector<GameObject> EnemyHeroes;

    inline std::vector<GameObject> AllMinions;
    inline std::vector<GameObject> AllyMinions;
    inline std::vector<GameObject> EnemyMinions;
    inline std::vector<GameObject> JungleMinions;

    inline std::vector<GameObject> Turrets;

    // ====================================================================
    // Update — Call once per frame from render hook
    // ====================================================================
    inline void Update() {
        // Update local player
        Player = ObjectManager::GetLocalPlayer();
        if (!Player.IsValid()) return;

        GameObjectTeam myTeam = Player.GetTeam();

        // ---- Heroes ----
        AllHeroes = ObjectManager::GetHeroes();
        AllyHeroes.clear();
        EnemyHeroes.clear();

        for (auto& hero : AllHeroes) {
            if (!hero.IsValid()) continue;
            if (hero.GetTeam() == myTeam)
                AllyHeroes.push_back(hero);
            else
                EnemyHeroes.push_back(hero);
        }

        // ---- Turrets (from TurretManager directly) ----
        Turrets.clear();
        {
            uintptr_t tmgr = Globals::Read<uintptr_t>(Globals::base + Offset::Global::TurretManager);
            if (Globals::IsValidPtr(tmgr)) {
                uintptr_t tlist = Globals::Read<uintptr_t>(tmgr + 0x8);
                int tcount = Globals::Read<int>(tmgr + 0x10);
                if (Globals::IsValidPtr(tlist) && tcount > 0 && tcount <= 30) {
                    uintptr_t addrs[30] = {};
                    int n = Globals::ReadPtrArray(tlist, tcount, addrs, 30);
                    for (int i = 0; i < n; i++) {
                        if (Globals::IsValidPtr(addrs[i])) {
                            GameObject t(addrs[i]);
                            if (t.IsAlive())
                                Turrets.push_back(t);
                        }
                    }
                }
            }
        }

        // ---- Minions + Jungle (from MinionManager) ----
        AllMinions.clear();
        AllyMinions.clear();
        EnemyMinions.clear();
        JungleMinions.clear();
        {
            auto minions = ObjectManager::GetMinions();
            for (auto& obj : minions) {
                if (!obj.IsValid() || !obj.IsAlive()) continue;

                GameObjectTeam team = obj.GetTeam();

                // Jungle monsters (Neutral team)
                if (team == GameObjectTeam::Neutral) {
                    if (obj.GetMaxHealth() > 1.0f)
                        JungleMinions.push_back(obj);
                    continue;
                }

                // Lane minions
                float maxHP = obj.GetMaxHealth();
                if (maxHP > 0.0f && maxHP < 10000.0f) {
                    AllMinions.push_back(obj);
                    if (team == myTeam)
                        AllyMinions.push_back(obj);
                    else
                        EnemyMinions.push_back(obj);
                }
            }
        }
    }

    // ====================================================================
    // Utility — Get enemies in range
    // ====================================================================
    inline std::vector<GameObject> GetEnemyHeroesInRange(float range, const Vec3& from = Vec3()) {
        std::vector<GameObject> result;
        Vec3 origin = from.IsZero() ? Player.GetPosition() : from;
        for (auto& hero : EnemyHeroes) {
            if (!hero.IsAlive() || !hero.IsVisible()) continue;
            if (hero.GetPosition().Distance2D(origin) <= range)
                result.push_back(hero);
        }
        return result;
    }

    inline std::vector<GameObject> GetAllyHeroesInRange(float range, const Vec3& from = Vec3()) {
        std::vector<GameObject> result;
        Vec3 origin = from.IsZero() ? Player.GetPosition() : from;
        for (auto& hero : AllyHeroes) {
            if (!hero.IsAlive()) continue;
            if (hero.GetPosition().Distance2D(origin) <= range)
                result.push_back(hero);
        }
        return result;
    }

    inline std::vector<GameObject> GetEnemyMinionsInRange(float range, const Vec3& from = Vec3()) {
        std::vector<GameObject> result;
        Vec3 origin = from.IsZero() ? Player.GetPosition() : from;
        for (auto& minion : EnemyMinions) {
            if (!minion.IsAlive() || !minion.IsVisible()) continue;
            if (minion.GetPosition().Distance2D(origin) <= range)
                result.push_back(minion);
        }
        return result;
    }

    inline std::vector<GameObject> GetJungleMonstersInRange(float range, const Vec3& from = Vec3()) {
        std::vector<GameObject> result;
        Vec3 origin = from.IsZero() ? Player.GetPosition() : from;
        for (auto& mob : JungleMinions) {
            if (!mob.IsAlive() || !mob.IsVisible()) continue;
            if (mob.GetPosition().Distance2D(origin) <= range)
                result.push_back(mob);
        }
        return result;
    }

    // ====================================================================
    // Count helpers
    // ====================================================================
    inline int CountEnemyHeroesInRange(float range, const Vec3& from = Vec3()) {
        return (int)GetEnemyHeroesInRange(range, from).size();
    }

    inline int CountAllyHeroesInRange(float range, const Vec3& from = Vec3()) {
        return (int)GetAllyHeroesInRange(range, from).size();
    }

} // namespace GameObjects
} // namespace SDK
