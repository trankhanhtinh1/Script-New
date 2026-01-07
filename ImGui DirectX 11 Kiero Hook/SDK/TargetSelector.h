#pragma once
#include "ObjectManager.h"
#include "BuffManager.h"
#include "Game.h"
#include "DamageCalculator.h"
#include "ChampionDatabase.h"
#include <limits>
#include <set>
#include <string>
#include <vector>
#include <vector>
#include <algorithm>
#include <fstream>
#include <iomanip>
#include "../Menu.h"

namespace SDK
{
    // Target Selector Mode (based on NewTargetSelector.cs)
    // Must match Menu.cpp tsMode order: 0=LowestHealth, 1=MostPriority, 2=NearMouse, 3=LeastAttacks, 4=MostAD, 5=MostAP
    enum class TargetSelectorMode {
        LowestHealth = 0,   // GetRealHealth - Lowest effective health
        MostPriority = 1,   // GetPriority - Highest priority champion (ADC > Assassin > Bruiser > Tank)
        NearMouse = 2,      // Closest to mouse cursor
        LeastAttacks = 3,   // Fewest auto attacks to kill
        MostAD = 4,         // Best for AD damage dealers
        MostAP = 5          // Best for AP damage dealers
    };

    class TargetSelector
    {
    public:
        static uint64_t SelectedTarget;      // Manually selected target (Address)
        static bool ForceSelectedTarget;      // Force attack selected target if in range
        static bool OnlySelectedTarget;       // Only attack selected target

        // Get champion priority using ChampionDatabase (supports menu customization)
        // Returns: 5=Max (ADC), 4=High (Assassin), 3=Medium (Fighter), 2=Default, 1=Low (Tank)
        static int GetPriority(GameObject* hero) {
            if (!hero) return 1;
            std::string name = hero->GetName();
            return ChampionDatabase::GetPriority(name);
        }
        
        // ============================================================================
        // IsValidTarget - WITH BuffManager integration
        // Based on NewTargetSelector.cs IsValidTarget()
        // ============================================================================
        static bool IsValidTarget(GameObject* target, float range, Vector3* checkFrom = nullptr) {
            if (!target || !target->IsValid()) return false;
            if (target->IsDead()) return false;
            if (!target->IsVisible()) return false;
            if (!target->IsTargetable()) return false;
            
            // ============================================================================
            // BUFF CHECKS - From NewTargetSelector.cs
            // ============================================================================
            float gameTime = Game::GetTime();
            BuffManager buffMgr(target->Address);
            
            // Check invulnerability (Zhonya, Kayle R, Kindred R, etc.)
            if (buffMgr.IsInvulnerable(gameTime)) {
                return false;
            }
            
            // Check zombie state (Sion passive, Karthus passive, Kog'Maw passive)
            if (buffMgr.IsZombie(gameTime)) {
                return false;
            }
            
            // Check Tryndamere ult - unkillable at low HP
            // From NewTargetSelector.cs: HasBuff("UndyingRage") && health <= 71
            if (buffMgr.HasBuff("UndyingRage", gameTime) && target->GetHealth() <= 71.0f) {
                return false;
            }
            
            // Check untargetable (Fizz E, Vladimir W, Master Yi Q, etc.)
            if (buffMgr.IsUntargetable(gameTime)) {
                return false;
            }
            
            // ============================================================================
            // Range check
            // ============================================================================
            GameObject* local = ObjectManager::GetLocalPlayer();
            if (!local) return false;
            
            Vector3 fromPos = checkFrom ? *checkFrom : local->GetPosition();
            float dist = fromPos.Distance(target->GetPosition());
            
            delete local;
            
            return dist <= range + target->GetBoundingRadius();
        }
        
        // AaIndicator - Số đòn đánh cần để hạ mục tiêu (lower = better target)
        // Based on NewTargetSelector.cs Base.AaIndicator()
        static float GetAaIndicator(GameObject* source, GameObject* target, DamageType damageType) {
            if (!source || !target) return 999999.0f;
            
            float effectiveHealth = DamageCalculator::GetEffectiveHealth(source, target, damageType);
            float aaDamage = DamageCalculator::GetAutoAttackDamage(source, target, false);
            
            if (aaDamage <= 0) return 999999.0f;
            
            // Return number of auto attacks needed (float for better sorting)
            return effectiveHealth / aaDamage;
        }
        
        // GetRealHealth - Effective health against damage type (lower = better target)
        static float GetRealHealth(GameObject* source, GameObject* target, DamageType damageType) {
            return DamageCalculator::GetEffectiveHealth(source, target, damageType);
        }

        // Main GetTarget including Modes (based on NewTargetSelector.cs)
        static GameObject* GetTarget(float range, TargetSelectorMode mode = TargetSelectorMode::LowestHealth) {
            __try {
                GameObject* local = ObjectManager::GetLocalPlayer();
                if (!local) return nullptr;

                Vector3 myPos = local->GetPosition();
                int myTeam = local->GetTeam();
                uint64_t myAddr = local->Address;

                if (SelectedTarget != 0) {
                    GameObject manualTarget(SelectedTarget);
                    if (manualTarget.IsValid() && !manualTarget.IsDead() && manualTarget.IsVisible()) {
                        float dist = myPos.Distance(manualTarget.GetPosition());
                        bool inRange = dist <= range + manualTarget.GetBoundingRadius();

                        if (ForceSelectedTarget && inRange) {
                            delete local;
                            return new GameObject(SelectedTarget);
                        }

                        if (OnlySelectedTarget) {
                            delete local;
                            if (inRange) return new GameObject(SelectedTarget);
                            return nullptr;
                        }
                    }
                }

                auto heroes = ObjectManager::GetHeroes();

                uint64_t bestAddr = 0;
                float bestScore = 999999.0f;

                for (auto hero : heroes) {
                    __try {
                        if (!hero || !hero->IsValid()) continue;
                        if (hero->Address == myAddr) continue;
                        if (hero->GetTeam() == myTeam) continue;
                        if (hero->IsDead()) continue;
                        if (!hero->IsVisible()) continue;
                        if (!hero->IsTargetable()) continue;

                        float dist = myPos.Distance(hero->GetPosition());
                        if (dist > range + hero->GetBoundingRadius()) continue;

                        float score = hero->GetHealth();

                        if (mode == TargetSelectorMode::MostPriority) {
                            score = (float)(-GetPriority(hero));
                        }
                        else if (mode == TargetSelectorMode::NearMouse) {
                            Vector3 mousePos = Game::GetMousePos();
                            score = mousePos.Distance(hero->GetPosition());
                        }

                        if (score < bestScore) {
                            bestScore = score;
                            bestAddr = hero->Address;
                        }
                    } __except(EXCEPTION_EXECUTE_HANDLER) {}
                }

                for (auto h : heroes) delete h;
                delete local;

                if (bestAddr == 0) {
                    return nullptr;
                }

                return new GameObject(bestAddr);
            } __except(EXCEPTION_EXECUTE_HANDLER) {
                return nullptr;
            }
        }

        // ============================================================================
        // GetTargets - Get ordered list of valid targets (for multi-target skills)
        // Based on NewTargetSelector.cs GetTargets()
        // ============================================================================
        static std::vector<GameObject*> GetTargets(float range, TargetSelectorMode mode = TargetSelectorMode::LowestHealth) {
            std::vector<GameObject*> result;

            GameObject* local = ObjectManager::GetLocalPlayer();
            if (!local) return result;
            
            Vector3 myPos = local->GetPosition();

            // Scan Enemy Heroes
            auto heroes = ObjectManager::GetHeroes();
            std::vector<GameObject*> validHeroes;
            
            for (auto hero : heroes) {
                if (!hero->IsValid()) continue;
                if (hero->Address == local->Address) continue;
                if (hero->GetTeam() == local->GetTeam()) continue;
                if (!IsValidTarget(hero, range, &myPos)) continue;
                validHeroes.push_back(new GameObject(hero->Address));
            }

            // Sort by mode
            switch (mode) {
                case TargetSelectorMode::MostAD:
                    std::sort(validHeroes.begin(), validHeroes.end(), [local](GameObject* a, GameObject* b) {
                        return GetAaIndicator(local, a, DamageType::Physical) < GetAaIndicator(local, b, DamageType::Physical);
                    });
                    break;
                case TargetSelectorMode::MostPriority:
                    std::sort(validHeroes.begin(), validHeroes.end(), [](GameObject* a, GameObject* b) {
                        return GetPriority(a) > GetPriority(b);
                    });
                    break;
                case TargetSelectorMode::LowestHealth:
                    std::sort(validHeroes.begin(), validHeroes.end(), [local](GameObject* a, GameObject* b) {
                        return a->GetHealth() < b->GetHealth();
                    });
                    break;
                default:
                    break;
            }

            // If ForceSelectedTarget is on and selected is in list, move to front
            if (ForceSelectedTarget && SelectedTarget != 0) {
                for (size_t i = 0; i < validHeroes.size(); i++) {
                    if (validHeroes[i]->Address == SelectedTarget) {
                        GameObject* selected = validHeroes[i];
                        validHeroes.erase(validHeroes.begin() + i);
                        validHeroes.insert(validHeroes.begin(), selected);
                        break;
                    }
                }
            }

            // Cleanup source heroes
            for (auto h : heroes) delete h;
            delete local;

            return validHeroes;
        }

        // ============================================================================
        // Click-to-Select Handler (LMB - Left Mouse Button)
        // Call this from hkPresent when left mouse button is pressed
        // Uses oUnderMouseObj offset to get object under cursor
        // ============================================================================
        static void OnLeftClick() {
            uint64_t moduleBase = (uint64_t)GetModuleHandle(NULL);
            
            // Read UnderMouseObj pointer
            uint64_t underMousePtr = *(uint64_t*)(moduleBase + Offset::oUnderMouseObj);
            if (!underMousePtr || underMousePtr < 0x10000) {
                SelectedTarget = 0;  // Clear selection if clicking empty space
                return;
            }
            
            // Read actual object from offset
            uint64_t objAddress = *(uint64_t*)(underMousePtr + Offset::oUnderMouseObjOffset);
            if (!objAddress || objAddress < 0x10000) {
                SelectedTarget = 0;
                return;
            }
            
            // Validate object
            GameObject obj(objAddress);
            if (!obj.IsValid() || obj.IsDead()) {
                return; // Don't clear - might be clicking on terrain
            }
            
            GameObject* local = ObjectManager::GetLocalPlayer();
            if (!local) return;
            
            // Check if enemy
            if (obj.GetTeam() == local->GetTeam()) {
                delete local;
                return; // Clicked on ally, don't change selection
            }
            
            // Set as selected target (any enemy object - hero, minion, etc.)
            SelectedTarget = objAddress;
            delete local;
        }

        // Get currently selected target as GameObject
        static GameObject* GetSelectedTarget() {
            if (SelectedTarget == 0) return nullptr;
            GameObject* target = new GameObject(SelectedTarget);
            if (target->IsValid() && !target->IsDead()) {
                return target;
            }
            delete target;
            SelectedTarget = 0;  // Clear invalid selection
            return nullptr;
        }

        // Clear manual target selection
        static void ClearSelectedTarget() {
            SelectedTarget = 0;
        }
    };
    
    // Define statics (inline to avoid ODR violations in header-only)
    inline uint64_t TargetSelector::SelectedTarget = 0;
    inline bool TargetSelector::ForceSelectedTarget = true;
    inline bool TargetSelector::OnlySelectedTarget = false;
}

// ============================================================================
// NOTE: Minion targeting functions have been moved to MinionSelector.h
// Use SDK::MinionSelector for:
//   - GetLastHitMinion()
//   - GetLaneClearMinion()
//   - GetJungleMinion()
//   - ShouldWait()
// ============================================================================
