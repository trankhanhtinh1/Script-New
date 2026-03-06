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

    inline std::vector<GameObject> AllTurrets;
    inline std::vector<GameObject> AllyTurrets;
    inline std::vector<GameObject> EnemyTurrets;

    // Extra categorized lists
    inline std::vector<GameObject> EnemyWards;
    inline std::vector<GameObject> JunglePlants;
    inline std::vector<GameObject> AllyInhibitors;
    inline std::vector<GameObject> EnemyInhibitors;
    inline std::vector<GameObject> AllyNexus;
    inline std::vector<GameObject> EnemyNexus;
    inline std::vector<GameObject> AzirSoldiers;
    inline std::vector<GameObject> Pets;

    // Backwards compatibility alias
    inline std::vector<GameObject>& Turrets = AllTurrets;

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
        AllTurrets.clear();
        AllyTurrets.clear();
        EnemyTurrets.clear();
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
                            if (t.IsAlive()) {
                                AllTurrets.push_back(t);
                                if (t.GetTeam() == myTeam)
                                    AllyTurrets.push_back(t);
                                else
                                    EnemyTurrets.push_back(t);
                            }
                        }
                    }
                }
            }
        }

        // ---- Minions + Jungle + Wards + Plants + Pets (from MinionManager) ----
        AllMinions.clear();
        AllyMinions.clear();
        EnemyMinions.clear();
        JungleMinions.clear();
        EnemyWards.clear();
        JunglePlants.clear();
        Pets.clear();
        AzirSoldiers.clear();
        {
            auto minions = ObjectManager::GetMinions();
            bool isAzir = false;
            {
                std::string champName = Player.GetChampionName();
                isAzir = (_stricmp(champName.c_str(), "Azir") == 0);
            }

            for (auto& obj : minions) {
                if (!obj.IsValid() || !obj.IsAlive()) continue;

            GameObjectTeam team = obj.GetTeam();
                std::string name = obj.GetName();

                // --- Wards (enemy) ---
                if (obj.IsWard()) {
                    if (team != myTeam)
                        EnemyWards.push_back(obj);
                    continue;
                }

                // --- Jungle Plants ---
                if (obj.IsPlant()) {
                    JunglePlants.push_back(obj);
                    continue;
            }

                // --- Azir Soldiers ---
                if (isAzir && team == myTeam &&
                    name.find("AzirSoldier") != std::string::npos) {
                    AzirSoldiers.push_back(obj);
                    continue;
                }

                // --- Pets ---
                if (obj.IsPet()) {
                    Pets.push_back(obj);
                    continue;
                }

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

        // ---- Inhibitors & Nexus (from full object iteration) ----
        AllyInhibitors.clear();
        EnemyInhibitors.clear();
        AllyNexus.clear();
        EnemyNexus.clear();
        {
            ObjectManager::ForEach([&](GameObject& obj) {
                if (!obj.IsAlive()) return;
                std::string name = obj.GetName();
                if (name.empty()) return;

                GameObjectTeam team = obj.GetTeam();

                // Inhibitors: "Barracks_T1_*" or "Barracks_T2_*"
                if (name.find("Barracks_T") != std::string::npos) {
                    if (team == myTeam)
                        AllyInhibitors.push_back(obj);
                    else
                        EnemyInhibitors.push_back(obj);
                    return;
                }

                // Nexus: "HQ_T1" or "HQ_T2"
                if (name.find("HQ_T") != std::string::npos) {
                    if (team == myTeam)
                        AllyNexus.push_back(obj);
                    else
                        EnemyNexus.push_back(obj);
                    return;
                }
            });
        }
    }

    // ====================================================================
    // Utility — Get objects in range
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

    inline std::vector<GameObject> GetAllyMinionsInRange(float range, const Vec3& from = Vec3()) {
        std::vector<GameObject> result;
        Vec3 origin = from.IsZero() ? Player.GetPosition() : from;
        for (auto& minion : AllyMinions) {
            if (!minion.IsAlive()) continue;
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

    // Turret in range helpers
    inline GameObject GetClosestAllyTurret(const Vec3& from = Vec3()) {
        Vec3 origin = from.IsZero() ? Player.GetPosition() : from;
        GameObject closest;
        float minDist = FLT_MAX;
        for (auto& turret : AllyTurrets) {
            float dist = turret.GetPosition().Distance2D(origin);
            if (dist < minDist) {
                minDist = dist;
                closest = turret;
            }
        }
        return closest;
    }

    inline GameObject GetClosestEnemyTurret(const Vec3& from = Vec3()) {
        Vec3 origin = from.IsZero() ? Player.GetPosition() : from;
        GameObject closest;
        float minDist = FLT_MAX;
        for (auto& turret : EnemyTurrets) {
            float dist = turret.GetPosition().Distance2D(origin);
            if (dist < minDist) {
                minDist = dist;
                closest = turret;
            }
        }
        return closest;
    }

    inline bool IsUnderAllyTurret(const Vec3& pos) {
        for (auto& turret : AllyTurrets) {
            if (turret.GetPosition().Distance2D(pos) <= 875.0f) // turret range
                return true;
        }
        return false;
    }

    inline bool IsUnderEnemyTurret(const Vec3& pos) {
        for (auto& turret : EnemyTurrets) {
            if (turret.GetPosition().Distance2D(pos) <= 875.0f)
                return true;
        }
        return false;
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

    inline int CountEnemyMinionsInRange(float range, const Vec3& from = Vec3()) {
        return (int)GetEnemyMinionsInRange(range, from).size();
    }

    inline int CountAllyMinionsInRange(float range, const Vec3& from = Vec3()) {
        return (int)GetAllyMinionsInRange(range, from).size();
    }

    // ====================================================================
    // Ward helpers
    // ====================================================================
    inline std::vector<GameObject> GetEnemyWardsInRange(float range, const Vec3& from = Vec3()) {
        std::vector<GameObject> result;
        Vec3 origin = from.IsZero() ? Player.GetPosition() : from;
        for (auto& ward : EnemyWards) {
            if (!ward.IsAlive()) continue;
            if (ward.GetPosition().Distance2D(origin) <= range)
                result.push_back(ward);
        }
        return result;
    }

    // ====================================================================
    // Plant helpers
    // ====================================================================
    inline std::vector<GameObject> GetJunglePlantsInRange(float range, const Vec3& from = Vec3()) {
        std::vector<GameObject> result;
        Vec3 origin = from.IsZero() ? Player.GetPosition() : from;
        for (auto& plant : JunglePlants) {
            if (!plant.IsAlive()) continue;
            if (plant.GetPosition().Distance2D(origin) <= range)
                result.push_back(plant);
        }
        return result;
    }

    // ====================================================================
    // Structure helpers
    // ====================================================================
    inline GameObject GetClosestEnemyInhibitor(const Vec3& from = Vec3()) {
        Vec3 origin = from.IsZero() ? Player.GetPosition() : from;
        GameObject closest;
        float minDist = FLT_MAX;
        for (auto& inhib : EnemyInhibitors) {
            float dist = inhib.GetPosition().Distance2D(origin);
            if (dist < minDist) { minDist = dist; closest = inhib; }
        }
        return closest;
    }

    inline GameObject GetClosestEnemyNexus(const Vec3& from = Vec3()) {
        Vec3 origin = from.IsZero() ? Player.GetPosition() : from;
        if (!EnemyNexus.empty()) return EnemyNexus[0];
        return GameObject();
    }

    // ====================================================================
    // Azir Soldier helpers
    // ====================================================================
    inline bool HasAzirSoldierNear(const GameObject& target, float radius = 350.0f) {
        for (auto& soldier : AzirSoldiers) {
            if (soldier.GetPosition().Distance2D(target.GetPosition()) <= radius)
                return true;
        }
        return false;
    }

    inline int CountAzirSoldiersInRange(float range, const Vec3& from = Vec3()) {
        int count = 0;
        Vec3 origin = from.IsZero() ? Player.GetPosition() : from;
        for (auto& soldier : AzirSoldiers) {
            if (soldier.GetPosition().Distance2D(origin) <= range) count++;
        }
        return count;
    }

} // namespace GameObjects
} // namespace SDK
