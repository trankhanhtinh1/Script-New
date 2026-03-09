#pragma once
#include "ObjectManager.h"
#include "GameObject.h"
#include "Game.h"
#include "sdk/Utils/JungleUtils.h"
#include <vector>
#include <algorithm>
#include <cctype>

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
    inline std::vector<GameObject> AllWards;
    inline std::vector<GameObject> AllyWards;
    inline std::vector<GameObject> EnemyWards;
    inline std::vector<GameObject> JunglePlants;
    inline std::vector<GameObject> JungleLarge;       // Baron, Dragon, Rift Herald, Blue/Red buff
    inline std::vector<GameObject> JungleSmall;       // Smaller jungle mobs
    inline std::vector<GameObject> JungleLegendary;   // Baron, Dragon, Rift Herald
    inline std::vector<GameObject> AllyInhibitors;
    inline std::vector<GameObject> EnemyInhibitors;
    inline std::vector<GameObject> AllyNexus;
    inline std::vector<GameObject> EnemyNexus;
    inline std::vector<GameObject> AzirSoldiers;
    inline std::vector<GameObject> Pets;
    inline std::vector<GameObject> ParticleEmitters;  // EffectEmitter objects (Jarvan flag, Thresh lantern, etc.)

    // Backwards compatibility alias
    inline std::vector<GameObject>& Turrets = AllTurrets;
    inline std::vector<GameObject>& Jungle = JungleMinions;

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
        JungleLarge.clear();
        JungleSmall.clear();
        JungleLegendary.clear();
        AllWards.clear();
        AllyWards.clear();
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
                if (name.empty()) {
                    name = obj.GetChampionName();
                }

                // --- Wards (All/Ally/Enemy) ---
                if (obj.IsWard()) {
                    AllWards.push_back(obj);
                    if (team == myTeam)
                        AllyWards.push_back(obj);
                    else
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

                // Jungle monsters (Neutral team) — subcategorize
            if (team == GameObjectTeam::Neutral) {
                    float maxHP = obj.GetMaxHealth();
                    if (maxHP <= 1.0f) continue;

                    std::string lowerName = name;
                    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), [](unsigned char c) {
                        return (char)std::tolower(c);
                    });

                    std::string characterName = obj.GetChampionName();
                    std::transform(characterName.begin(), characterName.end(), characterName.begin(), [](unsigned char c) {
                        return (char)std::tolower(c);
                    });

                    if (JungleUtils::IsJunglePlantName(lowerName) ||
                        JungleUtils::IsJunglePlantName(characterName)) {
                        JunglePlants.push_back(obj);
                        continue;
                    }

                    JungleType jungleType = JungleUtils::GetJungleType(lowerName);
                    if (jungleType == JungleType::Unknown && !characterName.empty()) {
                        jungleType = JungleUtils::GetJungleType(characterName);
                    }
                    const bool jungleByFlag = obj.IsJungleMonster();
                    const bool jungleByKnownName =
                        JungleUtils::IsKnownJungleMonsterName(lowerName) ||
                        JungleUtils::IsKnownJungleMonsterName(characterName);

                    // Ignore neutral non-jungle helper/effect objects.
                    if (jungleType == JungleType::Unknown && !jungleByFlag && !jungleByKnownName) {
                        continue;
                    }

                    JungleMinions.push_back(obj);

                    if (jungleType == JungleType::Legendary || jungleType == JungleType::Epic) {
                        JungleLegendary.push_back(obj);
                        JungleLarge.push_back(obj);
                    } else if (jungleType == JungleType::Large) {
                        JungleLarge.push_back(obj);
                    } else if (jungleType == JungleType::Small) {
                        JungleSmall.push_back(obj);
                    } else {
                        // Unknown-by-name but confirmed jungle via engine flag.
                        if (maxHP > 900.0f)
                            JungleLarge.push_back(obj);
                        else
                            JungleSmall.push_back(obj);
                    }
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

        // ---- Inhibitors, Nexus & ParticleEmitters (from full object iteration) ----
        AllyInhibitors.clear();
        EnemyInhibitors.clear();
        AllyNexus.clear();
        EnemyNexus.clear();
        ParticleEmitters.clear();
        {
            ObjectManager::ForEach([&](GameObject& obj) {
                std::string name = obj.GetName();
                if (name.empty()) return;

                GameObjectTeam team = obj.GetTeam();

                // --- ParticleEmitters / EffectEmitters ---
                // Detect known particle effect objects used by scripts:
                // Jarvan flag (JarvanIVDemacianStandard), Thresh lantern (ThreshLantern),
                // Zilean bomb (ZileanQBomb), Zyra plants, etc.
                if (name.find("_buf_") != std::string::npos ||
                    name.find("_tar_") != std::string::npos ||
                    name.find("_mis_") != std::string::npos ||
                    name.find("Particle") != std::string::npos ||
                    name.find("particle") != std::string::npos ||
                    name.find("global_ss_") != std::string::npos ||
                    name.find("Perks_") != std::string::npos) {
                    ParticleEmitters.push_back(obj);
                    // Don't return — particles can also match other categories
                }

                if (!obj.IsAlive()) return;

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

    // EnsoulSharp-style helper alias: enemy lane minions in range.
    inline std::vector<GameObject> GetMinions(const Vec3& from, float range) {
        return GetEnemyMinionsInRange(range, from);
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

    // EnsoulSharp-style helper alias.
    inline std::vector<GameObject> GetJungles(const Vec3& from, float range) {
        return GetJungleMonstersInRange(range, from);
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
    inline std::vector<GameObject> GetWardsInRange(float range, const Vec3& from = Vec3()) {
        std::vector<GameObject> result;
        Vec3 origin = from.IsZero() ? Player.GetPosition() : from;
        for (auto& ward : AllWards) {
            if (!ward.IsAlive()) continue;
            if (ward.GetPosition().Distance2D(origin) <= range)
                result.push_back(ward);
        }
        return result;
    }

    inline std::vector<GameObject> GetAllyWardsInRange(float range, const Vec3& from = Vec3()) {
        std::vector<GameObject> result;
        Vec3 origin = from.IsZero() ? Player.GetPosition() : from;
        for (auto& ward : AllyWards) {
            if (!ward.IsAlive()) continue;
            if (ward.GetPosition().Distance2D(origin) <= range)
                result.push_back(ward);
        }
        return result;
    }

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

    // ====================================================================
    // Jungle subcategory helpers
    // ====================================================================
    inline std::vector<GameObject> GetJungleLargeInRange(float range, const Vec3& from = Vec3()) {
        std::vector<GameObject> result;
        Vec3 origin = from.IsZero() ? Player.GetPosition() : from;
        for (auto& mob : JungleLarge) {
            if (!mob.IsAlive() || !mob.IsVisible()) continue;
            if (mob.GetPosition().Distance2D(origin) <= range)
                result.push_back(mob);
        }
        return result;
    }

    inline std::vector<GameObject> GetJungleLegendaryInRange(float range, const Vec3& from = Vec3()) {
        std::vector<GameObject> result;
        Vec3 origin = from.IsZero() ? Player.GetPosition() : from;
        for (auto& mob : JungleLegendary) {
            if (!mob.IsAlive() || !mob.IsVisible()) continue;
            if (mob.GetPosition().Distance2D(origin) <= range)
                result.push_back(mob);
        }
        return result;
    }

    // ====================================================================
    // ParticleEmitter helpers
    // ====================================================================

    /// Find a particle emitter by name substring
    inline GameObject FindParticleEmitter(const std::string& nameSubstr) {
        for (auto& emitter : ParticleEmitters) {
            std::string name = emitter.GetName();
            if (name.find(nameSubstr) != std::string::npos)
                return emitter;
        }
        return GameObject();
    }

    /// Get all particle emitters in range
    inline std::vector<GameObject> GetParticleEmittersInRange(float range, const Vec3& from = Vec3()) {
        std::vector<GameObject> result;
        Vec3 origin = from.IsZero() ? Player.GetPosition() : from;
        for (auto& emitter : ParticleEmitters) {
            if (emitter.GetPosition().Distance2D(origin) <= range)
                result.push_back(emitter);
        }
        return result;
    }

    /// Check if a specific particle effect exists (by name substring)
    inline bool HasParticleEmitter(const std::string& nameSubstr) {
        return FindParticleEmitter(nameSubstr).IsValid();
    }

} // namespace GameObjects
} // namespace SDK
