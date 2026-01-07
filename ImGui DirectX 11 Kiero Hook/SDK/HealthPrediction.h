#pragma once
#include "GameObject.h"
#include "ObjectManager.h"
#include "Game.h"
#include "Offsets.h"
#include <vector>
#include <map>
#include <string>
#include <cmath>

namespace SDK
{
    // ============================================================================
    // MISSILE INFO - For tracking incoming attacks
    // ============================================================================
    struct MissileInfo
    {
        uint64_t Address;
        int TargetNetId;
        float Damage;
        float ArrivalTime;
    };

    // ============================================================================
    // HEALTH PREDICTION - Predict future health considering incoming damage
    // ============================================================================
    class HealthPrediction
    {
    private:
        static std::map<int, std::vector<MissileInfo>>& GetIncomingDamageCache() {
            static std::map<int, std::vector<MissileInfo>> cache;
            return cache;
        }
        static float& GetLastCacheUpdate() {
            static float lastUpdate = 0.0f;
            return lastUpdate;
        }

    public:
        // ============================================================================
        // Update Cache - Call once per frame to refresh missile tracking
        // ============================================================================
        static void UpdateCache() {
            float gameTime = Game::GetTime();
            auto& incomingDamageCache = GetIncomingDamageCache();
            auto& lastCacheUpdate = GetLastCacheUpdate();

            if (gameTime - lastCacheUpdate < 0.05f) return;
            lastCacheUpdate = gameTime;

            incomingDamageCache.clear();
            
            auto missiles = ObjectManager::GetMissiles();
            
            for (auto missile : missiles) {
                if (!missile || !missile->IsValid()) continue;
                
                // Read missile target NetID
                int targetNetId = *(int*)(missile->Address + Offset::oMissileSrcIdx);  // TODO: This is SrcIdx, need TargetIdx
                if (targetNetId == 0) continue;
                
                // Read missile position for arrival time calculation
                Vector3 missilePos = *(Vector3*)(missile->Address + Offset::oMissilePosition);
                
                // Estimate damage (simplified - should read from SpellData)
                float damage = 50.0f;  // Default estimate
                
                // Create missile info
                MissileInfo info;
                info.Address = missile->Address;
                info.TargetNetId = targetNetId;
                info.Damage = damage;
                info.ArrivalTime = gameTime + 0.5f;  // Estimate 0.5s arrival
                
                incomingDamageCache[targetNetId].push_back(info);
            }
            
            for (auto m : missiles) delete m;
        }
        
        // ============================================================================
        // Get Health Prediction - Predict HP after delay considering incoming damage
        // @param unit: Target unit
        // @param timeMs: Delay in milliseconds
        // @return: Predicted health
        // ============================================================================
        static float GetHealthPrediction(GameObject* unit, float timeMs)
        {
            if (!unit || !unit->IsValid()) return 0.0f;
            
            float currentHealth = unit->GetHealth();
            float gameTime = Game::GetTime();
            float predictTime = gameTime + (timeMs / 1000.0f);
            
            int unitNetId = unit->GetNetworkId();
            float incomingDamage = 0.0f;
            auto& incomingDamageCache = GetIncomingDamageCache();

            auto it = incomingDamageCache.find(unitNetId);
            if (it != incomingDamageCache.end()) {
                for (const auto& missile : it->second) {
                    // Only count missiles that will arrive before our prediction time
                    if (missile.ArrivalTime <= predictTime) {
                        incomingDamage += missile.Damage;
                    }
                }
            }
            
            // Simple estimation: Subtract average DPS when under attack by turret/minions
            // This is a fallback for melee attacks which don't have missiles
            if (IsUnderTurretAttack(unit)) {
                // Turret attacks every ~0.83s, deals ~150 damage at lvl 1
                float turretAttacks = (timeMs / 1000.0f) / 0.83f;
                incomingDamage += turretAttacks * 150.0f;
            }
            
            // Estimate minion aggro damage (simplified)
            int minionAggroCount = CountMinionAggressors(unit);
            if (minionAggroCount > 0) {
                // Each minion attacks every ~1s, deals ~10-15 damage
                float minionAttacks = (timeMs / 1000.0f) * minionAggroCount;
                incomingDamage += minionAttacks * 12.0f;
            }
            
            return std::max(0.0f, currentHealth - incomingDamage);
        }
        
        // ============================================================================
        // Get Lane Clear Health Prediction - Includes ally damage estimation
        // For deciding when to attack during lane pushing (not just last hitting)
        // ============================================================================
        static float GetLaneClearHealthPrediction(GameObject* unit, float timeMs, float allyDPS = 0.0f) {
            if (!unit || !unit->IsValid()) return 0.0f;
            
            float predictedHP = GetHealthPrediction(unit, timeMs);
            
            // Subtract estimated ally damage
            if (allyDPS > 0) {
                float allyDamage = allyDPS * (timeMs / 1000.0f);
                predictedHP -= allyDamage;
            }
            
            return std::max(0.0f, predictedHP);
        }
        
        // ============================================================================
        // Should Wait - Check if we should hold our attack for an upcoming last hit
        // @param minion: Minion to check
        // @param myDamage: Our auto attack damage
        // @param delayMs: Time until our attack lands
        // @return: True if we should wait instead of attacking something else
        // ============================================================================
        static bool ShouldWait(GameObject* minion, float myDamage, float delayMs) {
            if (!minion || !minion->IsValid()) return false;
            
            float hp = minion->GetHealth();
            float predictedHP = GetHealthPrediction(minion, delayMs);
            
            // If minion will die before our attack lands, don't bother
            if (predictedHP <= 0) return false;
            
            // If minion HP is close to our damage, we should wait
            // (it will be killable soon, don't waste our attack cooldown)
            if (hp <= myDamage * 2.5f && hp > myDamage) {
                // Check if it will become killable within our attack delay
                if (predictedHP <= myDamage * 1.1f) {
                    return true;
                }
            }
            
            return false;
        }
        
        // ============================================================================
        // Count Minions About To Die - For wave management decisions
        // ============================================================================
        static int CountMinionsDying(float range, float timeMs) {
            GameObject* local = ObjectManager::GetLocalPlayer();
            if (!local) return 0;
            
            int count = 0;
            Vector3 myPos = local->GetPosition();
            auto minions = ObjectManager::GetMinions();
            
            for (auto minion : minions) {
                if (!minion->IsValid() || minion->IsDead()) continue;
                if (minion->GetTeam() == local->GetTeam()) continue;
                
                float dist = myPos.Distance(minion->GetPosition());
                if (dist > range) continue;
                
                float predictedHP = GetHealthPrediction(minion, timeMs);
                if (predictedHP <= 0) {
                    count++;
                }
            }
            
            for (auto m : minions) delete m;
            delete local;
            
            return count;
        }
        
    private:
        // ============================================================================
        // Helper: Check if unit is being attacked by turret
        // ============================================================================
        static bool IsUnderTurretAttack(GameObject* unit) {
            if (!unit) return false;
            
            auto turrets = ObjectManager::GetTurrets();
            Vector3 unitPos = unit->GetPosition();
            int unitTeam = unit->GetTeam();
            
            bool underAttack = false;
            
            for (auto turret : turrets) {
                if (!turret->IsValid() || turret->IsDead()) continue;
                if (turret->GetTeam() == unitTeam) continue;  // Ally turret
                
                float dist = unitPos.Distance(turret->GetPosition());
                if (dist <= 900.0f) {  // Turret range ~775, add buffer
                    // TODO: Check turret's actual target
                    // For now, assume if we're in range, turret might target us
                    underAttack = true;
                    break;
                }
            }
            
            for (auto t : turrets) delete t;
            return underAttack;
        }
        
        // ============================================================================
        // Helper: Count enemy minions attacking unit
        // ============================================================================
        static int CountMinionAggressors(GameObject* unit) {
            if (!unit) return 0;
            
            auto minions = ObjectManager::GetMinions();
            Vector3 unitPos = unit->GetPosition();
            int unitTeam = unit->GetTeam();
            
            int count = 0;
            
            for (auto minion : minions) {
                if (!minion->IsValid() || minion->IsDead()) continue;
                if (minion->GetTeam() == unitTeam) continue;  // Ally minion
                
                float dist = unitPos.Distance(minion->GetPosition());
                if (dist <= 150.0f) {  // Minion attack range ~125
                    count++;
                }
            }
            
            for (auto m : minions) delete m;
            return count;
        }
    };
}
