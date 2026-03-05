#pragma once
#include "GameObject.h"
#include "GameObjects.h"
#include "BuffManager.h"
#include "DamageCalc.h"
#include "Game.h"
#include <vector>
#include <algorithm>
#include <string>

#undef min
#undef max

// ============================================================================
// TargetSelector — Select best target for combat
// Reference: EnsoulSharp.SDK/Core/Wrappers/TargetSelector/*
// ============================================================================

namespace SDK {

    class TargetSelector {
    public:
        // Selection modes
        enum class Mode {
            LowestHP,       // Attack lowest health first
            Closest,        // Attack nearest enemy
            MostAD,         // Prioritize highest AD
            MostAP,         // Prioritize highest AP
            Priority,       // Champion priority list (threat score)
            NearMouse,      // Closest to mouse cursor
            LeastAttacks,   // Fewest attacks to kill
            AutoPriority    // DamageCalc-based scoring (recommended)
        };

        // Current selection mode (settable from menu)
        static inline Mode CurrentMode = Mode::AutoPriority;

        // ====================================================================
        // GetRealHealth — health including shields, minus incoming damage
        // EnsoulSharp: Extensions.GetRealHealth()
        // ====================================================================
        static float GetRealHealth(const GameObject& target, DamageType dmgType = DamageType::Physical) {
            float hp = target.GetHealth();

            // Add shields based on damage type
            switch (dmgType) {
            case DamageType::Physical:
                hp += target.GetAllShield() + target.GetPhysicalShield();
                break;
            case DamageType::Magical:
                hp += target.GetAllShield() + target.GetMagicalShield();
                break;
            case DamageType::True:
                // True damage ignores shields (only raw HP)
                break;
            case DamageType::Mixed:
                hp += target.GetAllShield();
                break;
            }

            return hp;
        }

        // ====================================================================
        // IsValidTarget — Enhanced validity check
        // Checks: valid ptr, alive, visible, targetable, not invulnerable,
        //         not zombie (Sion passive, Karthus passive), in range
        // ====================================================================
        static bool IsValidTarget(const GameObject& target, float range = 25000.0f,
                                  const Vec3& from = Vec3()) {
            if (!target.IsValid()) return false;
            if (!target.IsAlive()) return false;
            if (!target.IsVisible()) return false;
            if (!target.IsTargetable()) return false;

            // Check if target is invulnerable (Kayle R, Tryndamere R, etc.)
            if (HasInvulnerableBuff(target)) return false;

            // Check if target is a zombie (Sion passive, Karthus passive, Kog'Maw passive)
            if (target.IsZombie()) return false;

            // Range check
            if (range > 0.0f) {
                Vec3 fromPos = from.IsZero() ? GameObjects::Player.GetPosition() : from;
                float dist = target.GetPosition().Distance2D(fromPos);
                if (dist > range + target.GetBoundingRadius()) return false;
            }

            return true;
        }

        // ====================================================================
        // Main target selection
        // ====================================================================
        static GameObject GetTarget(float range, Mode mode = CurrentMode) {
            auto targets = GetValidTargets(range);
            if (targets.empty()) return GameObject();

            switch (mode) {
            case Mode::LowestHP:     return GetLowestHP(targets);
            case Mode::Closest:      return GetClosest(targets);
            case Mode::MostAD:       return GetMostAD(targets);
            case Mode::MostAP:       return GetMostAP(targets);
            case Mode::Priority:     return GetByPriority(targets);
            case Mode::NearMouse:    return GetNearMouse(targets);
            case Mode::LeastAttacks: return GetLeastAttacks(targets);
            case Mode::AutoPriority: return GetAutoPriority(targets);
            default:                 return GetAutoPriority(targets);
            }
        }

        // Alias: GetTarget(range) uses CurrentMode via default param above

        // Get multiple targets sorted by mode
        static std::vector<GameObject> GetTargets(float range, Mode mode = Mode::AutoPriority) {
            auto targets = GetValidTargets(range);
            SortByMode(targets, mode);
            return targets;
        }

        // ====================================================================
        // Forced target (for click-to-focus)
        // ====================================================================
        static inline GameObject ForcedTarget;

        // When true, only attack the forced/selected target. If OOR → attack nothing.
        // Set by TargetSelectorPlugin from its "Only Attack Select Target" menu option.
        static inline bool OnlyAttackSelected = false;

        static void SetForcedTarget(const GameObject& target) {
            ForcedTarget = target;
        }

        static void ClearForcedTarget() {
            ForcedTarget = GameObject();
        }

        static GameObject GetForcedTarget() {
            if (ForcedTarget.IsValid() && IsValidTarget(ForcedTarget))
                return ForcedTarget;
            return GameObject();
        }

        // ====================================================================
        // Convenience: Get selected or best target
        // ====================================================================
        static GameObject GetSelectedTarget(float range, Mode mode = Mode::AutoPriority) {
            // Priority: forced target > best target
            auto forced = GetForcedTarget();
            if (forced.IsValid()) {
                float dist = GameObjects::Player.DistanceTo(forced);
                if (dist <= range + forced.GetBoundingRadius())
                    return forced;
            }
            return GetTarget(range, mode);
        }

    private:
        // ====================================================================
        // Invulnerable buff check
        // ====================================================================
        static bool HasInvulnerableBuff(const GameObject& target) {
            BuffManager buffs(target.address);
            // Common invulnerable buffs
            static const char* invulnBuffs[] = {
                "KayleR",
                "TryndamereR",
                "kindaborroweytime",  // Kindred R
                "ChronoShift",       // Zilean R
                "FioraW",            // Fiora W (riposte)
                "UndyingRage",       // Tryndamere R
                "JudicatorIntervention", // Old Kayle R
            };
            for (auto& name : invulnBuffs) {
                if (buffs.HasBuff(name)) return true;
            }
            return false;
        }

        // ====================================================================
        // Get all valid targets in range
        // ====================================================================
        static std::vector<GameObject> GetValidTargets(float range) {
            std::vector<GameObject> result;
            for (auto& hero : GameObjects::EnemyHeroes) {
                if (IsValidTarget(hero, range))
                    result.push_back(hero);
            }
            return result;
        }

        // ====================================================================
        // Mode implementations
        // ====================================================================

        static GameObject GetLowestHP(std::vector<GameObject>& targets) {
            return *std::min_element(targets.begin(), targets.end(),
                [](const GameObject& a, const GameObject& b) {
                    return a.GetHealth() < b.GetHealth();
                });
        }

        static GameObject GetClosest(std::vector<GameObject>& targets) {
            return *std::min_element(targets.begin(), targets.end(),
                [](const GameObject& a, const GameObject& b) {
                    return GameObjects::Player.DistanceTo(a) < GameObjects::Player.DistanceTo(b);
                });
        }

        static GameObject GetMostAD(std::vector<GameObject>& targets) {
            return *std::max_element(targets.begin(), targets.end(),
                [](const GameObject& a, const GameObject& b) {
                    return a.GetTotalAD() < b.GetTotalAD();
                });
        }

        static GameObject GetMostAP(std::vector<GameObject>& targets) {
            return *std::max_element(targets.begin(), targets.end(),
                [](const GameObject& a, const GameObject& b) {
                    return a.GetAP() < b.GetAP();
                });
        }

        static GameObject GetNearMouse(std::vector<GameObject>& targets) {
            Vec3 mousePos = Game::GetMouseWorldPos();
            return *std::min_element(targets.begin(), targets.end(),
                [&mousePos](const GameObject& a, const GameObject& b) {
                    return a.GetPosition().Distance2D(mousePos) < b.GetPosition().Distance2D(mousePos);
                });
        }

        static GameObject GetLeastAttacks(std::vector<GameObject>& targets) {
            return *std::min_element(targets.begin(), targets.end(),
                [](const GameObject& a, const GameObject& b) {
                    int aaA = DamageCalc::GetAutoAttacksToKill(GameObjects::Player, a);
                    int aaB = DamageCalc::GetAutoAttacksToKill(GameObjects::Player, b);
                    return aaA < aaB;
                });
        }

        static GameObject GetByPriority(std::vector<GameObject>& targets) {
            // Threat-based priority: ADC > Mage > Assassin > Support > Tank
            // Approximation: highest (AD + AP) / effectiveHP
            return *std::max_element(targets.begin(), targets.end(),
                [](const GameObject& a, const GameObject& b) {
                    float threatA = (a.GetTotalAD() + a.GetAP()) / std::max(1.0f, a.GetHealth());
                    float threatB = (b.GetTotalAD() + b.GetAP()) / std::max(1.0f, b.GetHealth());
                    return threatA < threatB;
                });
        }

        // AutoPriority — Uses DamageCalc::GetTargetScore (lower = better)
        static GameObject GetAutoPriority(std::vector<GameObject>& targets) {
            return *std::min_element(targets.begin(), targets.end(),
                [](const GameObject& a, const GameObject& b) {
                    float scoreA = DamageCalc::GetTargetScore(GameObjects::Player, a, DamageType::Physical);
                    float scoreB = DamageCalc::GetTargetScore(GameObjects::Player, b, DamageType::Physical);
                    return scoreA < scoreB;
                });
        }

        // ====================================================================
        // Sorting
        // ====================================================================
        static void SortByMode(std::vector<GameObject>& targets, Mode mode) {
            switch (mode) {
            case Mode::LowestHP:
                std::sort(targets.begin(), targets.end(),
                    [](const GameObject& a, const GameObject& b) {
                        return a.GetHealth() < b.GetHealth();
                    });
                break;
            case Mode::Closest:
                std::sort(targets.begin(), targets.end(),
                    [](const GameObject& a, const GameObject& b) {
                        return GameObjects::Player.DistanceTo(a) < GameObjects::Player.DistanceTo(b);
                    });
                break;
            case Mode::LeastAttacks:
                std::sort(targets.begin(), targets.end(),
                    [](const GameObject& a, const GameObject& b) {
                        int aaA = DamageCalc::GetAutoAttacksToKill(GameObjects::Player, a);
                        int aaB = DamageCalc::GetAutoAttacksToKill(GameObjects::Player, b);
                        return aaA < aaB;
                    });
                break;
            case Mode::AutoPriority:
                std::sort(targets.begin(), targets.end(),
                    [](const GameObject& a, const GameObject& b) {
                        float scoreA = DamageCalc::GetTargetScore(GameObjects::Player, a, DamageType::Physical);
                        float scoreB = DamageCalc::GetTargetScore(GameObjects::Player, b, DamageType::Physical);
                        return scoreA < scoreB;
                    });
                break;
            default:
                break;
            }
        }
    };

} // namespace SDK
