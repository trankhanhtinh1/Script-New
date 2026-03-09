#pragma once
#include "GameObjects.h"
#include "SpellBook.h"
#include "Game.h"
#include <functional>
#include <vector>
#include <unordered_map>

// ============================================================================
// TurretAggro — Tracks turret target changes
// Reference: EnsoulSharp.SDK/Core/Events/Turret.cs
//
// Detection method: Poll each turret's ActiveSpellCast target each frame.
// Detect when turret acquires a new target or loses target.
// ============================================================================

namespace SDK {

    // ========================================================================
    // Turret Target Event Args
    // ========================================================================
    struct TurretAggroEventArgs {
        GameObject  Turret;         // the turret
        GameObject  NewTarget;      // new target (invalid if turret lost target)
        GameObject  OldTarget;      // previous target (invalid if turret had no target)
        bool        IsAllyTurret;   // true if turret belongs to our team
        float       GameTime;       // when the event was detected
    };

    // ========================================================================
    // Callback type
    // ========================================================================
    using OnTurretAggroFn = std::function<void(const TurretAggroEventArgs&)>;

    // ========================================================================
    // TurretAggro — Main class
    // ========================================================================
    class TurretAggro {
    public:
        // Register a callback for turret aggro change events
        static void OnTurretTargetChange(OnTurretAggroFn callback) {
            s_callbacks.push_back(callback);
        }

        // Call once per frame after GameObjects::Update()
        static void Update() {
            if (s_callbacks.empty()) return;

            float now = Game::GetTime();
            if (now <= 0.0f) return;

            auto* localPlayer = &GameObjects::Player;
            if (!localPlayer->IsValid()) return;
            auto myTeam = localPlayer->GetTeam();

            for (auto& turret : GameObjects::AllTurrets) {
                if (!turret.IsValid() || !turret.IsAlive()) continue;

                int turretNetId = turret.GetNetId();
                if (turretNetId == 0) continue;

                bool isAllyTurret = (turret.GetTeam() == myTeam);

                // Try to get turret's current target via ActiveSpellCast
                int currentTargetNetId = GetTurretTargetNetId(turret);

                auto& state = s_turretStates[turretNetId];

                if (currentTargetNetId != state.lastTargetNetId) {
                    // Target changed!
                    TurretAggroEventArgs args;
                    args.Turret = turret;
                    args.IsAllyTurret = isAllyTurret;
                    args.GameTime = now;

                    // Resolve new target
                    if (currentTargetNetId != 0) {
                        args.NewTarget = FindObjectByNetId(currentTargetNetId);
                    }

                    // Resolve old target
                    if (state.lastTargetNetId != 0) {
                        args.OldTarget = FindObjectByNetId(state.lastTargetNetId);
                    }

                    for (auto& cb : s_callbacks) {
                        cb(args);
                    }

                    state.lastTargetNetId = currentTargetNetId;
                }
            }
        }

        // ====================================================================
        // Utility: Check if turret is targeting a specific hero
        // ====================================================================
        static bool IsTurretTargeting(const GameObject& turret, const GameObject& target) {
            if (!turret.IsValid() || !target.IsValid()) return false;
            int targetNetId = GetTurretTargetNetId(turret);
            return targetNetId != 0 && targetNetId == target.GetNetId();
        }

        // Check if any enemy turret is targeting local player
        static bool IsLocalPlayerUnderTurretAggro() {
            auto* localPlayer = &GameObjects::Player;
            if (!localPlayer->IsValid()) return false;

            for (auto& turret : GameObjects::EnemyTurrets) {
                if (!turret.IsValid() || !turret.IsAlive()) continue;
                if (IsTurretTargeting(turret, *localPlayer)) return true;
            }
            return false;
        }

        // Get the turret currently targeting the given unit
        static GameObject GetTurretTargetingUnit(const GameObject& unit) {
            if (!unit.IsValid()) return GameObject();

            for (auto& turret : GameObjects::AllTurrets) {
                if (!turret.IsValid() || !turret.IsAlive()) continue;
                if (IsTurretTargeting(turret, unit)) return turret;
            }
            return GameObject();
        }

        // Check if any enemy turret is targeting a specific hero
        static bool IsUnderEnemyTurretAggro(const GameObject& unit) {
            if (!unit.IsValid()) return false;
            auto* localPlayer = &GameObjects::Player;
            if (!localPlayer->IsValid()) return false;
            auto myTeam = localPlayer->GetTeam();

            for (auto& turret : GameObjects::AllTurrets) {
                if (!turret.IsValid() || !turret.IsAlive()) continue;
                if (turret.GetTeam() == unit.GetTeam()) continue; // skip ally turrets
                if (IsTurretTargeting(turret, unit)) return true;
            }
            return false;
        }

        // Clear all state
        static void Clear() {
            s_turretStates.clear();
            s_callbacks.clear();
        }

    private:
        static inline std::vector<OnTurretAggroFn> s_callbacks;

        struct TurretState {
            int lastTargetNetId = 0;
        };

        static inline std::unordered_map<int, TurretState> s_turretStates;

        // Get turret's current target via ActiveSpellCast (auto-attack target)
        static int GetTurretTargetNetId(const GameObject& turret) {
            __try {
                SpellBook sb(turret.address);
                if (!sb.IsValid()) return 0;

                uintptr_t activeCast = sb.GetActiveSpellCast();
                if (!Globals::IsValidPtr(activeCast)) return 0;

                // Read target net ID from active cast
                int targetNetId = Globals::Read<int>(activeCast + Offset::SpellCastInfo::TargetIndex);
                return targetNetId;
            } __except (1) {
                return 0;
            }
        }

        // Find a game object by NetId (search all lists)
        static GameObject FindObjectByNetId(int netId) {
            if (netId == 0) return GameObject();

            // Search heroes first (most relevant for turret aggro)
            for (auto& hero : GameObjects::AllHeroes) {
                if (hero.IsValid() && hero.GetNetId() == netId)
                    return hero;
            }
            // Search minions
            for (auto& minion : GameObjects::AllMinions) {
                if (minion.IsValid() && minion.GetNetId() == netId)
                    return minion;
            }
            for (auto& minion : GameObjects::JungleMinions) {
                if (minion.IsValid() && minion.GetNetId() == netId)
                    return minion;
            }
            return GameObject();
        }
    };

} // namespace SDK
