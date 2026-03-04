#pragma once
#include "GameObject.h"
#include "GameObjects.h"
#include <vector>
#include <algorithm>

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
            Priority,       // Champion priority list
            NearMouse,      // Closest to mouse cursor
            LeastAttacks    // Fewest attacks to kill
        };

        // ====================================================================
        // Main target selection
        // ====================================================================

        static GameObject GetTarget(float range, Mode mode = Mode::LowestHP) {
            auto targets = GetValidTargets(range);
            if (targets.empty()) return GameObject();

            switch (mode) {
            case Mode::LowestHP:
                return GetLowestHP(targets);
            case Mode::Closest:
                return GetClosest(targets);
            case Mode::MostAD:
                return GetMostAD(targets);
            case Mode::MostAP:
                return GetMostAP(targets);
            case Mode::Priority:
                return GetByPriority(targets);
            case Mode::NearMouse:
                return GetNearMouse(targets);
            case Mode::LeastAttacks:
                return GetLeastAttacks(targets);
            default:
                return GetLowestHP(targets);
            }
        }

        // Get multiple targets sorted by mode
        static std::vector<GameObject> GetTargets(float range, Mode mode = Mode::LowestHP) {
            auto targets = GetValidTargets(range);
            SortByMode(targets, mode);
            return targets;
        }

        // ====================================================================
        // Validity check
        // ====================================================================

        static bool IsValidTarget(const GameObject& target, float range = 25000.0f) {
            if (!target.IsValid()) return false;
            if (!target.IsAlive()) return false;
            if (!target.IsVisible()) return false;
            if (!target.IsTargetable()) return false;
            if (range > 0.0f) {
                float dist = GameObjects::Player.DistanceTo(target);
                if (dist > range) return false;
            }
            return true;
        }

    private:
        // ====================================================================
        // Get all valid targets in range
        // ====================================================================
        static std::vector<GameObject> GetValidTargets(float range) {
            std::vector<GameObject> result;
            for (auto& hero : GameObjects::EnemyHeroes) {
                if (IsValidTarget(hero, range)) {
                    result.push_back(hero);
                }
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
            float myAD = GameObjects::Player.GetTotalAD();
            if (myAD <= 0.0f) myAD = 1.0f;
            return *std::min_element(targets.begin(), targets.end(),
                [myAD](const GameObject& a, const GameObject& b) {
                    return (a.GetHealth() / myAD) < (b.GetHealth() / myAD);
                });
        }

        static GameObject GetByPriority(std::vector<GameObject>& targets) {
            // Champion priority: ADC > Mage > Assassin > Support > Tank
            // Simplified: sort by total AD+AP descending → highest threat first
            return *std::max_element(targets.begin(), targets.end(),
                [](const GameObject& a, const GameObject& b) {
                    float scoreA = a.GetTotalAD() + a.GetAP();
                    float scoreB = b.GetTotalAD() + b.GetAP();
                    return scoreA < scoreB;
                });
        }

        static void SortByMode(std::vector<GameObject>& targets, Mode mode) {
            switch (mode) {
            case Mode::LowestHP:
                std::sort(targets.begin(), targets.end(),
                    [](const GameObject& a, const GameObject& b) { return a.GetHealth() < b.GetHealth(); });
                break;
            case Mode::Closest:
                std::sort(targets.begin(), targets.end(),
                    [](const GameObject& a, const GameObject& b) {
                        return GameObjects::Player.DistanceTo(a) < GameObjects::Player.DistanceTo(b);
                    });
                break;
            default:
                break;
            }
        }
    };

} // namespace SDK
