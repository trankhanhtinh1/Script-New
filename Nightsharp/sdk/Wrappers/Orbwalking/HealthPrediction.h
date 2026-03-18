#pragma once

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

#include "GameObjects/GameObject.h"
#include "GameObjects/ObjectManager.h"
#include "GameObjects/GameObjects.h"
#include "GameObjects/Missile.h"
#include "GameObjects/MissileClassification.h"
#include "Wrappers/Damages/DamageCalc.h"
#include "TurretMissileManager.h"
#include "Game.h"
#include <vector>
#include <map>
#include <cmath>
#include <algorithm>

// ============================================================================
// HealthPrediction — Predict future health (improved for last-hit accuracy)
//
// Strategy (hybrid, 2 layers):
//
//   Layer 1 — Game-native InDamage read (Offset::Health::InDamage)
//     The game engine itself tracks incoming damage in a field at obj+0x1210.
//     Reading this directly gives the most accurate "about to take damage"
//     value. We use this as the primary source for imminent damage prediction.
//
//   Layer 2 — Missile tracking (backup / future arrivals)
//     For missiles that will arrive AFTER the InDamage window, we fall back
//     to tracking projectiles in flight and estimating their arrival time.
//     This is the old approach but now only used as a supplement.
//
// Usage:
//   HealthPrediction::Update();          // call once per frame
//   float hp = HealthPrediction::GetPrediction(minion, 250.0f);   // 250ms ahead
//   bool  lh = HealthPrediction::CanLastHit(minion, myDamage, delay);
// ============================================================================

namespace SDK {

    // HealthPredictionType — matching C# EnsoulSharp.SDK
    enum class HealthPredictionType {
        Default = 0,    // Layer 1 (InDamage) + Layer 2 (missiles)
        Simulated = 1   // Default + continuous DPS estimation (minion aggro)
    };

    // Incoming attack record (Layer 2 — missile tracking)
    struct IncomingAttack {
        int   sourceNetId;
        int   targetNetId;
        float damage;
        float arrivalTime;    // absolute game time when damage lands
        bool  isTurretShot;
        bool  isAutoAttack;
        Vec3  startPos;
        Vec3  endPos;
    };

    class HealthPrediction {
    private:
        static inline std::map<int, std::vector<IncomingAttack>> trackedMissiles;
        static inline float lastUpdateTime = 0.0f;

        // ------------------------------------------------------------------
        // ReadInDamage — read game-native incoming damage from memory.
        // Offset::Health::InDamage (0x1210) is the LeagueObfuscation<float>
        // that the game uses internally for the health-bar "damage preview".
        // It represents all damage that has been committed but not yet applied.
        //
        // NOTE: this reads as a plain float (not obfuscated in our dump),
        //       confirmed via IDA: sub_2E3220 HP+400 = 0x1080+0x190 = 0x1210
        // ------------------------------------------------------------------
        static float ReadInDamage(const GameObject& unit) {
            if (!unit.IsValid()) return 0.0f;
            __try {
                float val = Globals::Read<float>(unit.address + Offset::Health::InDamage);
                // Sanity: InDamage should be 0 <= val <= MaxHP
                if (val < 0.0f) return 0.0f;
                float maxHP = unit.GetMaxHealth();
                if (val > maxHP) return 0.0f; // clearly a bad read
                return val;
            } __except(1) { return 0.0f; }
        }

    public:
        // ==================================================================
        // Update — call once per frame (~every 33ms)
        // Tracks in-flight missiles for Layer 2 prediction
        // ==================================================================
        static void Update() {
            float now = Game::GetTime();
            if (now - lastUpdateTime < 0.033f) return;
            lastUpdateTime = now;

            // Prune expired entries (arrived > 500ms ago)
            for (auto it = trackedMissiles.begin(); it != trackedMissiles.end();) {
                auto& vec = it->second;
                vec.erase(std::remove_if(vec.begin(), vec.end(),
                    [now](const IncomingAttack& a) {
                        return a.arrivalTime < now - 0.5f;
                    }), vec.end());
                if (vec.empty())
                    it = trackedMissiles.erase(it);
                else
                    ++it;
            }

            // Track in-flight missiles using new classification system
            auto missiles = MissileManager::GetMissiles();
            for (auto& m : missiles) {
                if (!m.IsValid()) continue;

                int casterNetId = m.GetCasterNetId();
                Vec3 mPos  = m.GetPosition();
                Vec3 endPos = m.GetEndPos();
                Vec3 startPos = m.GetStartPos();

                // Use new missile classification system
                MissileType mType = m.GetMissileType();
                bool isTurret = (mType == MissileType::TurretShot);
                bool isAA     = MissileClassifier::IsAnyAutoAttack(mType);

                // Primary: use TargetNetId from CastInfo (direct, no guessing)
                int bestNetId = m.GetTargetNetId();

                // Fallback: if no direct target, find closest unit to endpoint
                if (bestNetId <= 0) {
                    float bestDist = 180.0f;
                    auto checkList = [&](std::vector<GameObject>& list) {
                        for (auto& u : list) {
                            if (!u.IsValid() || !u.IsAlive()) continue;
                            float d = u.GetPosition().Distance2D(endPos);
                            if (d < bestDist) {
                                bestDist = d;
                                bestNetId = u.GetNetId();
                            }
                        }
                    };
                    checkList(GameObjects::EnemyMinions);
                    checkList(GameObjects::AllyMinions);
                    checkList(GameObjects::JungleMinions);
                    checkList(GameObjects::AllHeroes);
                }
                if (bestNetId <= 0) continue;

                // Already tracking this exact missile (same caster + launch point)?
                bool alreadyTracked = false;
                auto it = trackedMissiles.find(bestNetId);
                if (it != trackedMissiles.end()) {
                    for (auto& a : it->second) {
                        if (a.sourceNetId == casterNetId &&
                            startPos.Distance2D(a.startPos) < 15.0f) {
                            alreadyTracked = true; break;
                        }
                    }
                }
                if (alreadyTracked) continue;

                // Estimate damage from this missile
                float dmg = EstimateMissileDamage(casterNetId, bestNetId, isTurret, isAA);

                // Use REAL missile speed from SpellDataResource instead of hardcoded values
                float dist = mPos.Distance2D(endPos);
                float speed = m.GetMissileSpeed();
                // Fallback to reasonable defaults if SpellDataResource speed unavailable
                if (speed <= 0.0f) {
                    speed = isTurret ? 1200.0f : (isAA ? 1600.0f : 1800.0f);
                }
                float arrival = now + (dist / speed);

                IncomingAttack atk;
                atk.sourceNetId  = casterNetId;
                atk.targetNetId  = bestNetId;
                atk.damage       = dmg;
                atk.arrivalTime  = arrival;
                atk.isTurretShot = isTurret;
                atk.isAutoAttack = isAA;
                atk.startPos     = startPos;
                atk.endPos       = endPos;
                trackedMissiles[bestNetId].push_back(atk);
            }
        }

        // ==================================================================
        // GetPrediction — predict health of a unit after `timeMs` milliseconds
        // Matching C# signature: Health.GetPrediction(unit, time, farmDelay, type)
        //
        // Uses hybrid approach:
        //   - InDamage (game native) for imminent hits (0~300ms)
        //   - Missile tracking for later arrivals (>300ms)
        //   - Simulated: adds continuous DPS estimation
        // ==================================================================
        static float GetPrediction(const GameObject& unit, float timeMs,
                                   int farmDelay = 0,
                                   HealthPredictionType type = HealthPredictionType::Default) {
            if (!unit.IsValid()) return 0.0f;

            // Add farm delay offset (matching C# — orbwalker passes farmDelay from menu)
            float effectiveTimeMs = timeMs + (float)farmDelay;

            float now         = Game::GetTime();
            float currentHP   = unit.GetHealth();
            float predictTime = now + (effectiveTimeMs / 1000.0f);

            // -----------------------------------------------------------------
            // Layer 1: game-native InDamage — most accurate for imminent damage
            // -----------------------------------------------------------------
            float nativeDmg = ReadInDamage(unit);

            // -----------------------------------------------------------------
            // Layer 2: missile tracking — damage arriving in the prediction window
            // -----------------------------------------------------------------
            float missileDmg = 0.0f;
            int netId = unit.GetNetId();
            auto it = trackedMissiles.find(netId);
            if (it != trackedMissiles.end()) {
                for (auto& atk : it->second) {
                    if (!atk.isTurretShot &&
                        atk.arrivalTime > now + 0.15f &&
                        atk.arrivalTime <= predictTime &&
                        atk.arrivalTime >= now) {
                        missileDmg += atk.damage;
                    }
                }
            }

            // -----------------------------------------------------------------
            // Layer 3: turret shot prediction
            // -----------------------------------------------------------------
            float turretDmg = 0.0f;
            if (effectiveTimeMs > 50.0f) {
                turretDmg = TurretMissileManager::GetIncomingDamage(unit, effectiveTimeMs, nullptr, nullptr, 150.0f);
            }

            // -----------------------------------------------------------------
            // Layer 4: continuous DPS estimation (Simulated mode or long window)
            // Simulated mode: always include DPS. Default: only >400ms.
            // -----------------------------------------------------------------
            float continuousDmg = 0.0f;
            if (type == HealthPredictionType::Simulated || effectiveTimeMs > 400.0f) {
                continuousDmg += GetMinionAggroDPS(unit) * (effectiveTimeMs / 1000.0f);
            }

            float totalIncoming = nativeDmg + missileDmg + turretDmg + continuousDmg;
            return std::max(0.0f, currentHP - totalIncoming);
        }

        // NOTE: 2-param GetPrediction(unit, timeMs) is handled by the 4-param
        // overload above with default farmDelay=0, type=Default. No alias needed.

        // ==================================================================
        // CanLastHit — will unit die from our next auto attack?
        // @param myDamage  : our auto attack damage (post-mitigation)
        // @param delayMs   : windup + travel time in milliseconds
        // ==================================================================
        static bool CanLastHit(const GameObject& unit, float myDamage, float delayMs = 250.0f) {
            if (!unit.IsValid() || !unit.IsAlive()) return false;
            float predicted = GetPrediction(unit, delayMs);
            return predicted > 0.0f && predicted <= myDamage;
        }

        // ==================================================================
        // ShouldWait — should we hold our auto to let allies/tower kill first?
        // Returns true when unit is in kill range but isn't safe to last-hit now
        // ==================================================================
        static bool ShouldWait(const GameObject& minion, float myDamage, float delayMs = 250.0f) {
            if (!minion.IsValid()) return false;

            float currentHP   = minion.GetHealth();
            float predictedHP = GetPrediction(minion, delayMs);

            // Already dead by the time we hit — don't waste the attack
            if (predictedHP <= 0.0f) return false;

            // Unit is in our kill range right now — just hit it
            if (currentHP <= myDamage) return false;

            // Unit will soon be in kill range — wait for it
            if (predictedHP > 0.0f && predictedHP <= myDamage * 1.5f)
                return true;

            return false;
        }

        // ==================================================================
        // LaneClearWait — should we hold autos to secure last-hits?
        // Uses a shorter window than Orbwalker::ShouldWait to reduce over-wait.
        // ==================================================================
        static bool LaneClearWait() {
            auto& player = GameObjects::Player;
            if (!player.IsValid()) return false;

            const float range = player.GetRealAttackRange() + 65.0f;
            const float delayMs = player.GetAttackDelay() * 1000.0f;

            for (auto& minion : GameObjects::EnemyMinions) {
                if (!minion.IsValid() || !minion.IsAlive() || !minion.IsVisible()) continue;
                if (minion.GetTeam() == GameObjectTeam::Neutral) continue;
                if (player.DistanceTo(minion) > range) continue;

                const float pred = GetPrediction(minion, delayMs);
                const float dmg = player.GetAutoAttackDamage(minion);
                if (pred > 0.0f && pred <= dmg) {
                    return true;
                }
            }

            return false;
        }

        // ==================================================================
        // GetLaneClearPrediction — health + optional ally DPS
        // ==================================================================
        static float GetLaneClearPrediction(const GameObject& unit, float timeMs, float allyDPS = 0.0f) {
            float hp = GetPrediction(unit, timeMs);
            if (allyDPS > 0)
                hp -= allyDPS * (timeMs / 1000.0f);
            return std::max(0.0f, hp);
        }

        // ==================================================================
        // CountMinionsDying — how many enemy minions will die within timeMs?
        // ==================================================================
        static int CountMinionsDying(float range, float timeMs) {
            int count = 0;
            auto& player = GameObjects::Player;
            if (!player.IsValid()) return 0;
            for (auto& m : GameObjects::EnemyMinions) {
                if (!m.IsAlive() || !m.IsVisible()) continue;
                if (player.DistanceTo(m) > range) continue;
                if (GetPrediction(m, timeMs) <= 0.0f) count++;
            }
            return count;
        }

        // ==================================================================
        // CountKillableMinions — how many minions can we last-hit?
        // ==================================================================
        static int CountKillableMinions(float range, float myDamage, float delayMs = 250.0f) {
            int count = 0;
            auto& player = GameObjects::Player;
            if (!player.IsValid()) return 0;
            for (auto& m : GameObjects::EnemyMinions) {
                if (!m.IsAlive() || !m.IsVisible()) continue;
                if (player.DistanceTo(m) > range) continue;
                if (CanLastHit(m, myDamage, delayMs)) count++;
            }
            return count;
        }

        // ==================================================================
        // HasTurretAggro — is a turret currently targeting this unit?
        // ==================================================================
        static bool HasTurretAggro(const GameObject& unit) {
            return TurretMissileManager::HasIncomingShot(unit) ||
                   TurretAggro::GetTurretTargetingUnit(unit).IsValid();
        }

        // ==================================================================
        // GetAggroTurret — get the turret that's targeting this unit (C# match)
        // ==================================================================
        static GameObject GetAggroTurret(const GameObject& unit) {
            // First check TurretMissileManager for active shots
            TurretMissileManager::ShotInfo shot;
            if (TurretMissileManager::GetIncomingShot(unit, shot)) {
                return shot.Turret;
            }
            // Fallback: check TurretAggro
            return TurretAggro::GetTurretTargetingUnit(unit);
        }

        // ==================================================================
        // TurretAggroStartTick — when did the turret start targeting this unit?
        // Returns system tick (ms) matching C#
        // ==================================================================
        static int TurretAggroStartTick(const GameObject& unit) {
            TurretMissileManager::ShotInfo shot;
            if (TurretMissileManager::GetIncomingShot(unit, shot)) {
                // Estimate start time: current time minus how long ago the shot was fired
                // ShotInfo does not have StartTime; use game time - remaining time as approximation
                float estimatedStartTime = Game::GetTime() - (shot.RemainingTimeMs / 1000.0f);
                return (int)(estimatedStartTime * 1000.0f);
            }
            return 0;
        }

        static bool HasMinionAggro(const GameObject& unit) {
            return GetMinionAggroCount(unit) > 0;
        }

        // ==================================================================
        // GetIncomingAttackCount — how many attacks are heading towards unit?
        // @param impactTimeMs  : window to check (ms from now)
        // ==================================================================
        static int GetIncomingAttackCount(const GameObject& unit, float impactTimeMs) {
            if (!unit.IsValid()) return 0;
            int netId = unit.GetNetId();
            auto it = trackedMissiles.find(netId);
            if (it == trackedMissiles.end()) return 0;

            float now = Game::GetTime();
            float deadline = now + (impactTimeMs / 1000.0f);
            int count = 0;
            for (auto& atk : it->second) {
                if (atk.arrivalTime >= now && atk.arrivalTime <= deadline) {
                    count++;
                }
            }
            return count;
        }

    private:
        // ------------------------------------------------------------------
        // Estimate damage from a missile based on caster stats
        // ------------------------------------------------------------------
        static float EstimateMissileDamage(int casterNetId, int targetNetId,
                                           bool isTurret, bool isAA) {
            if (isTurret) return 160.0f; // typical T1 turret AA damage

            // Find caster
            GameObject caster;
            auto findInList = [casterNetId](std::vector<GameObject>& list) -> GameObject {
                for (auto& o : list) if (o.GetNetId() == casterNetId) return o;
                return GameObject();
            };
            for (auto& h : GameObjects::AllHeroes) if (h.GetNetId() == casterNetId) { caster = h; break; }
            if (!caster.IsValid()) caster = findInList(GameObjects::AllMinions);
            if (!caster.IsValid()) caster = findInList(GameObjects::JungleMinions);

            // Find target
            GameObject target;
            auto findTarget = [targetNetId](std::vector<GameObject>& list) -> GameObject {
                for (auto& o : list) if (o.GetNetId() == targetNetId) return o;
                return GameObject();
            };
            target = findTarget(GameObjects::AllMinions);
            if (!target.IsValid()) target = findTarget(GameObjects::EnemyMinions);
            if (!target.IsValid()) target = findTarget(GameObjects::AllyMinions);
            if (!target.IsValid()) for (auto& h : GameObjects::AllHeroes) if (h.GetNetId() == targetNetId) { target = h; break; }

            if (caster.IsValid() && target.IsValid() && isAA)
                return DamageCalc::GetAutoAttackDamage(caster, target, false, false);

            if (caster.IsValid()) {
                float ad = caster.GetTotalAD();
                if (ad > 0) return ad * 0.9f;
            }
            return 50.0f;
        }

        static int GetMinionAggroCount(const GameObject& unit) {
            int count = 0;
            GameObjectTeam team = unit.GetTeam();
            auto& list = (team == GameObjects::Player.GetTeam())
                ? GameObjects::EnemyMinions : GameObjects::AllyMinions;
            for (auto& m : list) {
                if (!m.IsValid() || !m.IsAlive()) continue;
                if (unit.DistanceTo(m) <= 500.0f) count++;
            }
            return count;
        }

        static float GetMinionAggroDPS(const GameObject& unit) {
            int n = GetMinionAggroCount(unit);
            if (n <= 0) return 0.0f;
            float mins = Game::GetTime() / 60.0f;
            float baseDPS = 12.0f + mins * 0.5f;
            return (float)n * baseDPS;
        }

        static float GetTurretDPS(const GameObject& unit) {
            TurretMissileManager::ShotInfo shot;
            if (!TurretMissileManager::GetIncomingShot(unit, shot)) {
                return unit.IsHero() ? 240.0f : 190.0f;
            }

            float interval = shot.Turret.GetAttackDelay();
            if (interval < 0.4f || interval > 2.5f) {
                interval = 0.85f;
            }
            return shot.Damage / interval;
        }
    };

} // namespace SDK
