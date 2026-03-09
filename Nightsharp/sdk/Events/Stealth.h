#pragma once
#include "GameObjects.h"
#include "Game.h"
#include <functional>
#include <vector>
#include <unordered_map>

// ============================================================================
// Stealth — Detects when a hero goes invisible / comes out of stealth
// Reference: EnsoulSharp.SDK/Core/Events/Stealth.cs
//
// Detection method: Poll IsVisible() each frame,
// detect state transition visible → invisible and back.
// ============================================================================

namespace SDK {

    // ========================================================================
    // Stealth Event Args
    // ========================================================================
    struct StealthEventArgs {
        GameObject  Sender;
        bool        IsStealthed;    // true = entered stealth, false = revealed
        float       Time;           // game time when state changed
        Vec3        LastPosition;   // last known position before stealth
    };

    // ========================================================================
    // Callback type
    // ========================================================================
    using OnStealthFn = std::function<void(const StealthEventArgs&)>;

    // ========================================================================
    // StealthDetector — Main class
    // ========================================================================
    class StealthDetector {
    public:
        // Register a callback for stealth events
        static void OnStealth(OnStealthFn callback) {
            s_callbacks.push_back(callback);
        }

        // Call once per frame after GameObjects::Update()
        static void Update() {
            if (s_callbacks.empty()) return;

            float now = Game::GetTime();
            if (now <= 0.0f) return;

            // Track all heroes (enemy primarily, but also allies for completeness)
            for (auto& hero : GameObjects::AllHeroes) {
                if (!hero.IsValid() || !hero.IsAlive()) continue;

                int netId = hero.GetNetId();
                if (netId == 0) continue;

                bool isVisible = hero.IsVisible();
                auto& state = s_visibleStates[netId];

                // First time seeing this hero — initialize
                if (!state.initialized) {
                    state.initialized = true;
                    state.wasVisible = isVisible;
                    state.lastVisiblePos = hero.GetPosition();
                    continue;
                }

                // State transition: visible → invisible (entered stealth)
                if (state.wasVisible && !isVisible) {
                    state.stealthStartTime = now;

                    StealthEventArgs args;
                    args.Sender = hero;
                    args.IsStealthed = true;
                    args.Time = now;
                    args.LastPosition = state.lastVisiblePos;

                    for (auto& cb : s_callbacks) {
                        cb(args);
                    }
                }
                // State transition: invisible → visible (revealed)
                else if (!state.wasVisible && isVisible) {
                    StealthEventArgs args;
                    args.Sender = hero;
                    args.IsStealthed = false;
                    args.Time = now;
                    args.LastPosition = hero.GetPosition();

                    for (auto& cb : s_callbacks) {
                        cb(args);
                    }
                }

                // Update state
                state.wasVisible = isVisible;
                if (isVisible) {
                    state.lastVisiblePos = hero.GetPosition();
                }
            }
        }

        // ====================================================================
        // Utility: Get last known position of a stealthed hero
        // ====================================================================
        static Vec3 GetLastVisiblePosition(int netId) {
            auto it = s_visibleStates.find(netId);
            if (it != s_visibleStates.end()) return it->second.lastVisiblePos;
            return Vec3();
        }

        // Get time since a hero went invisible (0 if visible)
        static float GetStealthDuration(int netId) {
            auto it = s_visibleStates.find(netId);
            if (it != s_visibleStates.end() && !it->second.wasVisible) {
                return Game::GetTime() - it->second.stealthStartTime;
            }
            return 0.0f;
        }

        // Check if any enemy is currently stealthed
        static bool AnyEnemyStealthed() {
            auto* localPlayer = &GameObjects::Player;
            if (!localPlayer->IsValid()) return false;
            auto myTeam = localPlayer->GetTeam();

            for (auto& hero : GameObjects::EnemyHeroes) {
                if (!hero.IsValid() || !hero.IsAlive()) continue;
                int netId = hero.GetNetId();
                auto it = s_visibleStates.find(netId);
                if (it != s_visibleStates.end() && !it->second.wasVisible)
                    return true;
            }
            return false;
        }

        // List all currently stealthed enemies
        static std::vector<GameObject> GetStealthedEnemies() {
            std::vector<GameObject> result;
            for (auto& hero : GameObjects::EnemyHeroes) {
                if (!hero.IsValid() || !hero.IsAlive()) continue;
                int netId = hero.GetNetId();
                auto it = s_visibleStates.find(netId);
                if (it != s_visibleStates.end() && !it->second.wasVisible)
                    result.push_back(hero);
            }
            return result;
        }

        // Clear all state
        static void Clear() {
            s_visibleStates.clear();
            s_callbacks.clear();
        }

    private:
        static inline std::vector<OnStealthFn> s_callbacks;

        struct VisibleState {
            bool  initialized = false;
            bool  wasVisible = true;
            Vec3  lastVisiblePos;
            float stealthStartTime = 0.0f;
        };

        static inline std::unordered_map<int, VisibleState> s_visibleStates;
    };

} // namespace SDK
