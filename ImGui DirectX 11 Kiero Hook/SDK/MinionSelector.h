#pragma once
#include "ObjectManager.h"
#include "DamageCalculator.h"
#include "HealthPrediction.h"
#include "Game.h"
#include <vector>
#include <algorithm>
#include <limits>
#include <string>

namespace SDK
{
    // ============================================================================
    // MINION TYPES (from LaneMinionType offset 0x4CC9)
    // ============================================================================
    enum class MinionType {
        Unknown = 0,
        Melee = 4,
        Ranged = 5,
        Cannon = 6,
        Super = 7
    };

    // ============================================================================
    // MINION PRIORITY for Lane Clear (higher = attack first when pushing)
    // Cannon/Super are high priority for push, low priority for freeze
    // ============================================================================
    enum class FarmMode {
        LastHit,        // Only kill minions we can one-shot
        LaneClear,      // Push wave - attack any minion
        Freeze,         // Keep wave near turret - only last hit
        SlowPush,       // Build a large wave - minimize attacks
        FastPush        // Push as fast as possible
    };

    // ============================================================================
    // MINION SELECTOR - Dedicated minion targeting for farming
    // ============================================================================
    class MinionSelector
    {
    public:
        // ============================================================================
        // Minion Type Detection
        // ============================================================================
        static MinionType GetMinionType(GameObject* minion) {
            if (!minion || !minion->IsValid()) return MinionType::Unknown;
            
            // Read minion type from offset
            uint8_t type = *(uint8_t*)(minion->Address + Offset::LaneMinionType);
            
            switch (type) {
                case 4: return MinionType::Melee;
                case 5: return MinionType::Ranged;
                case 6: return MinionType::Cannon;
                case 7: return MinionType::Super;
                default: return MinionType::Unknown;
            }
        }

        // String representation for debug
        static std::string GetMinionTypeName(MinionType type) {
            switch (type) {
                case MinionType::Melee: return "Melee";
                case MinionType::Ranged: return "Ranged";
                case MinionType::Cannon: return "Cannon";
                case MinionType::Super: return "Super";
                default: return "Unknown";
            }
        }

        // ============================================================================
        // Base Gold Value (for prioritization)
        // ============================================================================
        static int GetMinionGoldValue(MinionType type) {
            switch (type) {
                case MinionType::Cannon: return 60;  // ~60-90 gold
                case MinionType::Super: return 60;   // ~60 gold
                case MinionType::Melee: return 21;   // ~21 gold
                case MinionType::Ranged: return 14;  // ~14 gold
                default: return 14;
            }
        }

        // ============================================================================
        // Check if minion is valid target for farming
        // ============================================================================
        static bool IsValidFarmTarget(GameObject* minion, GameObject* local) {
            if (!minion || !local) return false;
            if (!minion->IsValid() || minion->IsDead()) return false;
            if (!minion->IsVisible() || !minion->IsTargetable()) return false;
            if (minion->GetTeam() == local->GetTeam()) return false;  // Don't attack allied minions
            return true;
        }

        // ============================================================================
        // Check if we need to wait for a minion to become killable
        // Returns true if there's a minion about to die that we should save our attack for
        // ============================================================================
        static bool ShouldWait(float range, float attackDelayMs) {
            GameObject* local = ObjectManager::GetLocalPlayer();
            if (!local) return false;

            Vector3 myPos = local->GetPosition();
            auto minions = ObjectManager::GetMinions();
            
            bool shouldWait = false;
            float myDamage = DamageCalculator::GetAutoAttackDamage(local, nullptr, false);

            for (auto minion : minions) {
                if (!IsValidFarmTarget(minion, local)) continue;

                float dist = myPos.Distance(minion->GetPosition());
                if (dist > range + minion->GetBoundingRadius()) continue;

                // Calculate real damage to this minion
                float realDamage = DamageCalculator::GetAutoAttackDamage(local, minion, false);
                float hp = minion->GetHealth();
                
                // If minion HP is less than 2x our damage, it will die soon
                // We should wait instead of attacking something else
                if (hp < realDamage * 2.5f && hp > realDamage) {
                    // Check if minion will be killable after our attack delay
                    float predictedHP = HealthPrediction::GetHealthPrediction(minion, attackDelayMs);
                    if (predictedHP > 0 && predictedHP <= realDamage * 1.1f) {
                        shouldWait = true;
                        break;
                    }
                }
            }

            for (auto m : minions) delete m;
            delete local;
            
            return shouldWait;
        }

        // ============================================================================
        // GET LAST HIT MINION - Primary farming function
        // Returns the best minion that can be killed with one auto attack
        // Uses DamageCalculator for accurate damage prediction
        // ============================================================================
        static GameObject* GetLastHitMinion(float range) {
            GameObject* local = ObjectManager::GetLocalPlayer();
            if (!local) return nullptr;

            Vector3 myPos = local->GetPosition();
            auto minions = ObjectManager::GetMinions();

            GameObject* bestMinion = nullptr;
            float bestScore = (std::numeric_limits<float>::lowest)();
            MinionType bestType = MinionType::Unknown;

            for (auto minion : minions) {
                if (!IsValidFarmTarget(minion, local)) continue;

                float dist = myPos.Distance(minion->GetPosition());
                if (dist > range + minion->GetBoundingRadius()) continue;

                // Calculate REAL damage using DamageCalculator (with armor reduction)
                float realDamage = DamageCalculator::GetAutoAttackDamage(local, minion, false);

                // Predict health considering attack animation + projectile travel
                float attackWindup = local->GetAttackWindup();  // in seconds
                float projectileTime = dist / 1800.0f;  // Average projectile speed
                float totalDelayMs = (attackWindup + projectileTime) * 1000.0f;
                
                float predictedHP = HealthPrediction::GetHealthPrediction(minion, totalDelayMs);
                
                // Can we last hit this minion?
                if (predictedHP > 0 && predictedHP <= realDamage) {
                    MinionType type = GetMinionType(minion);
                    float goldValue = (float)GetMinionGoldValue(type);
                    
                    // Score: Higher gold value + lower HP = better target
                    // Priority: Cannon > Super > Melee > Ranged (when all are killable)
                    float score = goldValue * 10.0f - predictedHP;
                    
                    // Bonus for minions about to die (urgent last hits)
                    if (predictedHP < realDamage * 0.5f) {
                        score += 100.0f;  // Urgent!
                    }

                    if (score > bestScore) {
                        bestScore = score;
                        bestMinion = minion;
                        bestType = type;
                    }
                }
            }

            // Prepare return value (create copy)
            GameObject* result = nullptr;
            if (bestMinion) {
                result = new GameObject(bestMinion->Address);
            }

            // Cleanup
            for (auto m : minions) delete m;
            delete local;

            return result;
        }

        // ============================================================================
        // GET LANE CLEAR MINION - For pushing lane
        // Returns the best minion to attack when no last-hittable minion available
        // Priority: Lowest HP first (to clear wave faster)
        // ============================================================================
        static GameObject* GetLaneClearMinion(float range) {
            GameObject* local = ObjectManager::GetLocalPlayer();
            if (!local) return nullptr;

            Vector3 myPos = local->GetPosition();
            auto minions = ObjectManager::GetMinions();

            // First, check if any minion is last-hittable
            GameObject* lastHitTarget = nullptr;
            float lastHitScore = (std::numeric_limits<float>::lowest)();

            GameObject* pushTarget = nullptr;
            float lowestHP = (std::numeric_limits<float>::max)();
            MinionType highestPriorityType = MinionType::Unknown;

            for (auto minion : minions) {
                if (!IsValidFarmTarget(minion, local)) continue;

                float dist = myPos.Distance(minion->GetPosition());
                if (dist > range + minion->GetBoundingRadius()) continue;

                float hp = minion->GetHealth();
                float realDamage = DamageCalculator::GetAutoAttackDamage(local, minion, false);
                MinionType type = GetMinionType(minion);

                // Check for last hit opportunity first
                float attackWindup = local->GetAttackWindup();
                float projectileTime = dist / 1800.0f;
                float totalDelayMs = (attackWindup + projectileTime) * 1000.0f;
                float predictedHP = HealthPrediction::GetHealthPrediction(minion, totalDelayMs);

                if (predictedHP > 0 && predictedHP <= realDamage) {
                    float goldValue = (float)GetMinionGoldValue(type);
                    float score = goldValue * 10.0f - predictedHP;
                    if (score > lastHitScore) {
                        lastHitScore = score;
                        lastHitTarget = minion;
                    }
                }
                
                // Track lowest HP for pushing (when no last hit available)
                // Prefer attacking higher priority minions when HP is similar
                bool shouldReplace = false;
                
                if (!pushTarget) {
                    shouldReplace = true;
                } else {
                    // Compare by type priority first (Cannon > Super > Melee > Ranged)
                    int typePriority = GetTypePriority(type);
                    int currentTypePriority = GetTypePriority(highestPriorityType);
                    
                    if (typePriority > currentTypePriority) {
                        shouldReplace = true;
                    } else if (typePriority == currentTypePriority && hp < lowestHP) {
                        shouldReplace = true;
                    }
                }

                if (shouldReplace) {
                    lowestHP = hp;
                    pushTarget = minion;
                    highestPriorityType = type;
                }
            }

            // Priority: Last hit > Push
            GameObject* result = nullptr;
            if (lastHitTarget) {
                result = new GameObject(lastHitTarget->Address);
            } else if (pushTarget) {
                result = new GameObject(pushTarget->Address);
            }

            // Cleanup
            for (auto m : minions) delete m;
            delete local;

            return result;
        }

        // ============================================================================
        // GET JUNGLE MINION - For jungle clear
        // Targets jungle monsters (name contains "SRU" for Summoner's Rift, "Minion" for camps)
        // ============================================================================
        static GameObject* GetJungleMinion(float range) {
            GameObject* local = ObjectManager::GetLocalPlayer();
            if (!local) return nullptr;

            Vector3 myPos = local->GetPosition();
            auto minions = ObjectManager::GetAllMinions();  // Use ALL minions (includes jungle)

            GameObject* bestTarget = nullptr;
            float highestPriority = -1.0f;

            for (auto minion : minions) {
                if (!minion->IsValid() || minion->IsDead()) continue;
                if (!minion->IsVisible() || !minion->IsTargetable()) continue;
                
                // Jungle monsters are neutral (Team ID != 100 and != 200)
                int team = minion->GetTeam();
                if (team == 100 || team == 200) continue;  // Skip lane minions

                float dist = myPos.Distance(minion->GetPosition());
                if (dist > range + minion->GetBoundingRadius()) continue;

                // Priority based on MaxHealth (bigger monsters = higher priority)
                float priority = minion->GetMaxHealth();
                
                // Check if this is a large monster (Baron, Dragon, etc.)
                std::string name = minion->GetName();
                if (name.find("Baron") != std::string::npos) priority += 100000;
                if (name.find("Dragon") != std::string::npos) priority += 50000;
                if (name.find("Rift") != std::string::npos) priority += 30000;  // Rift Herald

                if (priority > highestPriority) {
                    highestPriority = priority;
                    bestTarget = minion;
                }
            }

            GameObject* result = nullptr;
            if (bestTarget) {
                result = new GameObject(bestTarget->Address);
            }

            for (auto m : minions) delete m;
            delete local;

            return result;
        }

        // ============================================================================
        // GET UNDER TURRET MINION - For farming under turret
        // Accounts for turret damage to calculate which minion to hit
        // ============================================================================
        static GameObject* GetUnderTurretMinion(float range, float turretDamage) {
            GameObject* local = ObjectManager::GetLocalPlayer();
            if (!local) return nullptr;

            Vector3 myPos = local->GetPosition();
            auto minions = ObjectManager::GetMinions();

            GameObject* bestMinion = nullptr;
            float bestScore = (std::numeric_limits<float>::lowest)();

            for (auto minion : minions) {
                if (!IsValidFarmTarget(minion, local)) continue;

                float dist = myPos.Distance(minion->GetPosition());
                if (dist > range + minion->GetBoundingRadius()) continue;

                float hp = minion->GetHealth();
                float myDamage = DamageCalculator::GetAutoAttackDamage(local, minion, false);
                MinionType type = GetMinionType(minion);

                // Turret farming logic:
                // Melee: 2 turret shots + 1 AA
                // Ranged: 1 turret shot requires AA before, then AA after
                // Cannon: Many turret shots + AAs

                float hpAfterTurret = hp - turretDamage;
                
                // Can we last hit after turret shot?
                if (hpAfterTurret > 0 && hpAfterTurret <= myDamage) {
                    float goldValue = (float)GetMinionGoldValue(type);
                    float score = goldValue * 10.0f - hpAfterTurret;
                    
                    if (score > bestScore) {
                        bestScore = score;
                        bestMinion = minion;
                    }
                }
            }

            GameObject* result = nullptr;
            if (bestMinion) {
                result = new GameObject(bestMinion->Address);
            }

            for (auto m : minions) delete m;
            delete local;

            return result;
        }

        // ============================================================================
        // GET UNKILLABLE MINION - For aggressive play
        // Returns minions that will die to allied damage (we can't last hit)
        // Used to prepare wave or decide when to trade
        // ============================================================================
        static int CountUnkillableMinions(float range, float timeMs) {
            GameObject* local = ObjectManager::GetLocalPlayer();
            if (!local) return 0;

            int count = 0;
            Vector3 myPos = local->GetPosition();
            auto minions = ObjectManager::GetMinions();

            for (auto minion : minions) {
                if (!IsValidFarmTarget(minion, local)) continue;

                float dist = myPos.Distance(minion->GetPosition());
                if (dist > range + minion->GetBoundingRadius()) continue;

                float predictedHP = HealthPrediction::GetHealthPrediction(minion, timeMs);
                
                // Minion will die before we can reach it
                if (predictedHP <= 0) {
                    count++;
                }
            }

            for (auto m : minions) delete m;
            delete local;

            return count;
        }

    private:
        // Helper: Get type priority for lane clearing
        static int GetTypePriority(MinionType type) {
            switch (type) {
                case MinionType::Cannon: return 4;  // Highest
                case MinionType::Super: return 3;
                case MinionType::Melee: return 2;
                case MinionType::Ranged: return 1;  // Lowest
                default: return 0;
            }
        }
    };
}
