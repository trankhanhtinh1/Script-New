#pragma once

// Prevent Windows min/max macros from conflicting with std::min/std::max
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

#include "GameObject.h"
#include "ObjectManager.h"
#include "GameObjects.h"
#include "Missile.h"
#include "Game.h"
#include <vector>
#include <map>
#include <cmath>
#include <algorithm>

// ============================================================================
// HealthPrediction — Predict future health with incoming damage
// Reference: EnsoulSharp.SDK HealthPrediction + Script-New-main
// ============================================================================

namespace SDK {

    struct IncomingDamage {
        uintptr_t missileAddr;
        int targetNetId;
        float damage;
        float arrivalTime;
    };

    class HealthPrediction {
    private:
        static inline std::map<int, std::vector<IncomingDamage>> damageCache;
        static inline float lastCacheTime = 0.0f;

    public:
        // ====================================================================
        // Update — Call once per frame
        // ====================================================================
        static void Update() {
            float now = Game::GetTime();
            if (now - lastCacheTime < 0.05f) return; // throttle 50ms
            lastCacheTime = now;

            damageCache.clear();

            auto missiles = MissileManager::GetMissiles();
            for (auto& m : missiles) {
                if (!m.IsValid()) continue;

                int casterNetId = m.GetCasterNetId();
                Vec3 missilePos = m.GetPosition();
                Vec3 endPos = m.GetEndPos();

                // Estimate damage (simplified)
                float damage = 50.0f; // Default estimate

                // Estimate arrival time based on distance
                float dist = missilePos.Distance2D(endPos);
                float arrivalTime = now + (dist / 1800.0f); // ~1800 speed average

                // For targeted missiles, track all nearby potential targets
                // Simplified: use end position proximity
                for (auto& minion : GameObjects::EnemyMinions) {
                    if (!minion.IsValid() || !minion.IsAlive()) continue;
                    float d = minion.GetPosition().Distance2D(endPos);
                    if (d < 100.0f) {
                        IncomingDamage info;
                        info.missileAddr = m.address;
                        info.targetNetId = minion.GetNetId();
                        info.damage = damage;
                        info.arrivalTime = arrivalTime;
                        damageCache[minion.GetNetId()].push_back(info);
                    }
                }
            }
        }

        // ====================================================================
        // Predict health after delay
        // @param unit: Target unit
        // @param timeMs: Delay in milliseconds
        // @return: Predicted health
        // ====================================================================
        static float GetPrediction(const GameObject& unit, float timeMs) {
            if (!unit.IsValid()) return 0.0f;

            float currentHP = unit.GetHealth();
            float now = Game::GetTime();
            float predictTime = now + (timeMs / 1000.0f);

            int netId = unit.GetNetId();
            float incomingDmg = 0.0f;

            auto it = damageCache.find(netId);
            if (it != damageCache.end()) {
                for (auto& info : it->second) {
                    if (info.arrivalTime <= predictTime)
                        incomingDmg += info.damage;
                }
            }

            // Estimate turret damage if under turret
            if (IsUnderTurret(unit)) {
                float turretAttacks = (timeMs / 1000.0f) / 0.83f;
                incomingDmg += turretAttacks * 150.0f;
            }

            // Estimate minion aggro damage
            int aggressors = CountMinionAggressors(unit);
            if (aggressors > 0) {
                float minionDmg = (timeMs / 1000.0f) * aggressors * 12.0f;
                incomingDmg += minionDmg;
            }

            return std::max(0.0f, currentHP - incomingDmg);
        }

        // ====================================================================
        // Lane clear HP prediction (includes ally damage)
        // ====================================================================
        static float GetLaneClearPrediction(const GameObject& unit, float timeMs, float allyDPS = 0.0f) {
            float hp = GetPrediction(unit, timeMs);
            if (allyDPS > 0) {
                hp -= allyDPS * (timeMs / 1000.0f);
            }
            return std::max(0.0f, hp);
        }

        // ====================================================================
        // Should wait for last hit opportunity?
        // ====================================================================
        static bool ShouldWait(const GameObject& minion, float myDamage, float delayMs) {
            if (!minion.IsValid()) return false;

            float hp = minion.GetHealth();
            float predictedHP = GetPrediction(minion, delayMs);

            // Minion will die before our attack → don't bother
            if (predictedHP <= 0) return false;

            // Minion HP approaching killable range → wait
            if (hp <= myDamage * 2.5f && hp > myDamage) {
                if (predictedHP <= myDamage * 1.1f) {
                    return true;
                }
            }

            return false;
        }

        // ====================================================================
        // Count minions about to die
        // ====================================================================
        static int CountMinionsDying(float range, float timeMs) {
            int count = 0;
            auto& player = GameObjects::Player;
            if (!player.IsValid()) return 0;

            for (auto& minion : GameObjects::EnemyMinions) {
                if (!minion.IsAlive() || !minion.IsVisible()) continue;
                if (player.DistanceTo(minion) > range) continue;
                if (GetPrediction(minion, timeMs) <= 0) {
                    count++;
                }
            }
            return count;
        }

    private:
        // Is unit near an enemy turret?
        static bool IsUnderTurret(const GameObject& unit) {
            for (auto& turret : GameObjects::Turrets) {
                if (!turret.IsValid() || !turret.IsAlive()) continue;
                if (turret.GetTeam() == unit.GetTeam()) continue;
                if (unit.DistanceTo(turret) <= 900.0f)
                    return true;
            }
            return false;
        }

        // Count enemy minions in melee range of unit
        static int CountMinionAggressors(const GameObject& unit) {
            int count = 0;
            for (auto& minion : GameObjects::AllyMinions) {
                // Ally minions attacking this enemy unit
                if (!minion.IsValid() || !minion.IsAlive()) continue;
                if (unit.DistanceTo(minion) <= 500.0f)
                    count++;
            }
            return count;
        }
    };

} // namespace SDK
