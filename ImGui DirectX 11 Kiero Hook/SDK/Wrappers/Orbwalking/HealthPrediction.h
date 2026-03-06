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
#include "DamageCalc.h"
#include "Game.h"
#include <vector>
#include <map>
#include <cmath>
#include <algorithm>

// ============================================================================
// HealthPrediction — Predict future health with incoming damage
// Reference: EnsoulSharp.SDK HealthPrediction + NewOrbwalker.cs
// ============================================================================

namespace SDK {

    // Tracking data for an incoming attack/missile
    struct IncomingAttack {
        uintptr_t sourceAddr;      // caster address
        int       sourceNetId;     // caster net ID
        int       targetNetId;     // target net ID
        float     damage;          // estimated damage
        float     arrivalTime;     // absolute game time when damage arrives
        bool      isTurretShot;    // is this from a turret?
        bool      isAutoAttack;    // is this an auto attack?
        float     missileSpeed;    // missile travel speed
        Vec3      sourcePos;       // where missile was fired from
    };

    class HealthPrediction {
    private:
        // All tracked incoming attacks, keyed by target NetID
        static inline std::map<int, std::vector<IncomingAttack>> incomingAttacks;
        static inline float lastUpdateTime = 0.0f;

    public:
        // ====================================================================
        // Update — Call once per frame
        // Scans missiles and nearby attackers to build damage predictions
        // ====================================================================
        static void Update() {
            float now = Game::GetTime();
            if (now - lastUpdateTime < 0.03f) return; // throttle ~33fps
            lastUpdateTime = now;

            // Prune expired entries
            for (auto it = incomingAttacks.begin(); it != incomingAttacks.end();) {
                auto& vec = it->second;
                vec.erase(std::remove_if(vec.begin(), vec.end(),
                    [now](const IncomingAttack& atk) {
                        return atk.arrivalTime < now - 0.5f; // expired 500ms ago
                    }), vec.end());
                if (vec.empty())
                    it = incomingAttacks.erase(it);
                else
                    ++it;
            }

            // ---- Track missiles for targeted damage ----
            auto missiles = MissileManager::GetMissiles();
            for (auto& m : missiles) {
                if (!m.IsValid()) continue;

                int casterNetId = m.GetCasterNetId();
                Vec3 missilePos = m.GetPosition();
                Vec3 endPos = m.GetEndPos();
                Vec3 startPos = m.GetStartPos();

                bool isTurret = m.IsTurretShot();
                bool isAA = m.IsAutoAttack();

                // Find the target: closest unit near the endpoint
                int bestTargetNetId = 0;
                float bestDist = 200.0f; // search radius around missile end pos
                float bestHP = FLT_MAX;

                auto findTarget = [&](std::vector<GameObject>& units) {
                    for (auto& unit : units) {
                        if (!unit.IsValid() || !unit.IsAlive()) continue;
                        float d = unit.GetPosition().Distance2D(endPos);
                        if (d < bestDist) {
                            bestDist = d;
                            bestTargetNetId = unit.GetNetId();
                        }
                    }
                };

                findTarget(GameObjects::EnemyMinions);
                findTarget(GameObjects::AllyMinions);
                findTarget(GameObjects::JungleMinions);
                findTarget(GameObjects::AllHeroes);

                if (bestTargetNetId == 0) continue;

                // Already tracking this specific missile for this target?
                bool alreadyTracked = false;
                auto it = incomingAttacks.find(bestTargetNetId);
                if (it != incomingAttacks.end()) {
                    for (auto& atk : it->second) {
                        if (atk.sourceNetId == casterNetId &&
                            std::abs(atk.sourcePos.x - startPos.x) < 10.0f) {
                            alreadyTracked = true;
                            break;
                        }
                    }
                }
                if (alreadyTracked) continue;

                // Calculate damage
                float damage = EstimateMissileDamage(casterNetId, bestTargetNetId, isTurret, isAA);

                // Calculate arrival time
                float dist = missilePos.Distance2D(endPos);
                float speed = 1800.0f; // default
                if (isTurret) speed = 1200.0f;
                else if (isAA) speed = 1600.0f;

                float arrival = now + (dist / speed);

                IncomingAttack atk;
                atk.sourceAddr = 0;
                atk.sourceNetId = casterNetId;
                atk.targetNetId = bestTargetNetId;
                atk.damage = damage;
                atk.arrivalTime = arrival;
                atk.isTurretShot = isTurret;
                atk.isAutoAttack = isAA;
                atk.missileSpeed = speed;
                atk.sourcePos = startPos;

                incomingAttacks[bestTargetNetId].push_back(atk);
            }
        }

        // ====================================================================
        // GetPrediction — Predict unit health after timeMs delay
        // @param unit: target unit
        // @param timeMs: delay in milliseconds
        // @return: predicted health value
        // ====================================================================
        static float GetPrediction(const GameObject& unit, float timeMs) {
            if (!unit.IsValid()) return 0.0f;

            float currentHP = unit.GetHealth();
            float now = Game::GetTime();
            float predictTime = now + (timeMs / 1000.0f);

            int netId = unit.GetNetId();
            float incomingDmg = 0.0f;

            // Sum all tracked incoming damage arriving before predictTime
            auto it = incomingAttacks.find(netId);
            if (it != incomingAttacks.end()) {
                for (auto& atk : it->second) {
                    if (atk.arrivalTime <= predictTime && atk.arrivalTime >= now - 0.1f)
                        incomingDmg += atk.damage;
                }
            }

            // Estimate continuous turret damage
            if (HasTurretAggro(unit)) {
                float turretDPS = GetTurretDPS(unit);
                float duration = timeMs / 1000.0f;
                incomingDmg += turretDPS * duration;
            }

            // Estimate minion aggro DPS
            float minionDPS = GetMinionAggroDPS(unit);
            if (minionDPS > 0) {
                incomingDmg += minionDPS * (timeMs / 1000.0f);
            }

            return std::max(0.0f, currentHP - incomingDmg);
        }

        // ====================================================================
        // GetLaneClearPrediction — Prediction including additional ally DPS
        // ====================================================================
        static float GetLaneClearPrediction(const GameObject& unit, float timeMs, float allyDPS = 0.0f) {
            float hp = GetPrediction(unit, timeMs);
            if (allyDPS > 0) {
                hp -= allyDPS * (timeMs / 1000.0f);
            }
            return std::max(0.0f, hp);
        }

        // ====================================================================
        // ShouldWait — Should we wait to last-hit? (improved logic)
        // Reference: NewOrbwalker.cs ShouldWait()
        // ====================================================================
        static bool ShouldWait(const GameObject& minion, float myDamage, float delayMs) {
            if (!minion.IsValid()) return false;

            float hp = minion.GetHealth();
            float predictedHP = GetPrediction(minion, delayMs);

            // Minion will die before our attack arrives → don't waste
            if (predictedHP <= 0) return false;

            // Minion HP is approaching killable range → wait
            if (hp <= myDamage * 2.5f && hp > myDamage) {
                if (predictedHP <= myDamage * 1.2f)
                    return true;
            }

            return false;
        }

        // ====================================================================
        // HasTurretAggro — Is a turret targeting this unit?
        // ====================================================================
        static bool HasTurretAggro(const GameObject& unit) {
            GameObjectTeam unitTeam = unit.GetTeam();
            Vec3 unitPos = unit.GetPosition();

            // Check if unit is in enemy turret range
            for (auto& turret : GameObjects::AllTurrets) {
                if (!turret.IsAlive()) continue;
                if (turret.GetTeam() == unitTeam) continue; // ally turret won't shoot us

                float dist = turret.GetPosition().Distance2D(unitPos);
                if (dist <= 875.0f) {
                    // Check if turret has a missile targeting this unit
                    // (Simplified: any turret in range counts)
                    return true;
                }
            }
            return false;
        }

        // ====================================================================
        // HasMinionAggro — Are minions targeting this unit?
        // ====================================================================
        static bool HasMinionAggro(const GameObject& unit) {
            return GetMinionAggroCount(unit) > 0;
        }

        // ====================================================================
        // Count minions about to die in range
        // ====================================================================
        static int CountMinionsDying(float range, float timeMs) {
            int count = 0;
            auto& player = GameObjects::Player;
            if (!player.IsValid()) return 0;

            for (auto& minion : GameObjects::EnemyMinions) {
                if (!minion.IsAlive() || !minion.IsVisible()) continue;
                if (player.DistanceTo(minion) > range) continue;
                if (GetPrediction(minion, timeMs) <= 0)
                    count++;
            }
            return count;
        }

        // ====================================================================
        // Count killable minions (our damage can last-hit them)
        // ====================================================================
        static int CountKillableMinions(float range, float myDamage, float timeMs) {
            int count = 0;
            auto& player = GameObjects::Player;
            if (!player.IsValid()) return 0;

            for (auto& minion : GameObjects::EnemyMinions) {
                if (!minion.IsAlive() || !minion.IsVisible()) continue;
                if (player.DistanceTo(minion) > range) continue;
                float predicted = GetPrediction(minion, timeMs);
                if (predicted > 0.0f && predicted <= myDamage)
                    count++;
            }
            return count;
        }

    private:
        // ====================================================================
        // Estimate damage from a missile based on caster stats
        // ====================================================================
        static float EstimateMissileDamage(int casterNetId, int targetNetId, bool isTurret, bool isAA) {
            if (isTurret) {
                // Turret damage scales, but rough estimate
                return 150.0f;
            }

            // Try to find caster and target for proper calculation
            GameObject caster, target;
            for (auto& hero : GameObjects::AllHeroes) {
                if (hero.GetNetId() == casterNetId) { caster = hero; break; }
            }
            if (!caster.IsValid()) {
                // Check minions
                auto findInList = [casterNetId](std::vector<GameObject>& list) -> GameObject {
                    for (auto& obj : list)
                        if (obj.GetNetId() == casterNetId) return obj;
                    return GameObject();
                };
                caster = findInList(GameObjects::AllMinions);
                if (!caster.IsValid()) caster = findInList(GameObjects::JungleMinions);
            }

            // Find target
            auto findTarget = [targetNetId](std::vector<GameObject>& list) -> GameObject {
                for (auto& obj : list)
                    if (obj.GetNetId() == targetNetId) return obj;
                return GameObject();
            };
            target = findTarget(GameObjects::AllMinions);
            if (!target.IsValid()) target = findTarget(GameObjects::EnemyMinions);
            if (!target.IsValid()) target = findTarget(GameObjects::AllyMinions);
            if (!target.IsValid()) {
                for (auto& h : GameObjects::AllHeroes)
                    if (h.GetNetId() == targetNetId) { target = h; break; }
            }

            if (caster.IsValid() && target.IsValid() && isAA) {
                return DamageCalc::GetAutoAttackDamage(caster, target, false);
            }

            // Fallback: use caster's AD or estimate
            if (caster.IsValid()) {
                float ad = caster.GetTotalAD();
                if (ad > 0) return ad * 0.9f; // rough estimate
            }

            return 50.0f; // last resort fallback
        }

        // Count minions attacking a unit
        static int GetMinionAggroCount(const GameObject& unit) {
            int count = 0;
            GameObjectTeam unitTeam = unit.GetTeam();

            // Find minions from opposing team near the unit
            auto& minionList = (unitTeam == GameObjects::Player.GetTeam())
                ? GameObjects::EnemyMinions : GameObjects::AllyMinions;

            for (auto& minion : minionList) {
                if (!minion.IsValid() || !minion.IsAlive()) continue;
                // Minion likely attacking if close and facing (simplified: just distance)
                if (unit.DistanceTo(minion) <= 500.0f)
                    count++;
            }
            return count;
        }

        // Get minion aggro DPS on a unit
        static float GetMinionAggroDPS(const GameObject& unit) {
            int aggressors = GetMinionAggroCount(unit);
            if (aggressors <= 0) return 0.0f;

            // Minion base DPS: ~12 per minion at early game, scales up
            float now = Game::GetTime();
            float gameMinutes = now / 60.0f;
            float baseDPS = 12.0f + gameMinutes * 0.5f; // ~12 early, ~27 at 30min

            return aggressors * baseDPS;
        }

        // Get turret DPS against a unit (turret damage ramps up)
        static float GetTurretDPS(const GameObject& unit) {
            // Turret attacks every ~0.83s, base damage ~150
            // Turret ramps up vs champions
            float baseDmg = 150.0f;
            float attackSpeed = 1.0f / 0.83f; // ~1.2 attacks/sec

            if (unit.IsHero()) {
                // Turrets deal more damage to champions (ramp up)
                baseDmg = 200.0f;
            }

            return baseDmg * attackSpeed;
        }
    };

} // namespace SDK
