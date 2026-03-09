#pragma once
#include "GameObjects.h"
#include "AiManager.h"
#include "Game.h"
#include <functional>
#include <vector>
#include <unordered_map>

// ============================================================================
// Dash — Detects when any hero starts a dash
// Reference: EnsoulSharp.SDK/Core/Events/Dash.cs
//
// Detection method: Poll AiManager::IsDashing() each frame,
// detect state transition from not-dashing to dashing.
// ============================================================================

namespace SDK {

    // ========================================================================
    // Dash Event Args
    // ========================================================================
    struct DashEventArgs {
        GameObject  Sender;
        Vec3        StartPos;
        Vec3        EndPos;
        float       Speed;
        float       Duration;     // estimated duration (distance / speed)
        float       StartTime;    // game time when dash started
        float       EndTime;      // estimated end time
        bool        IsBlink;      // true = instant blink (e.g. Ezreal E, Kassadin R)
    };

    // ========================================================================
    // Callback type
    // ========================================================================
    using OnDashFn = std::function<void(const DashEventArgs&)>;

    // ========================================================================
    // Dash — Main class
    // ========================================================================
    class DashDetector {
    public:
        // Register a callback for dash events
        static void OnDash(OnDashFn callback) {
            s_callbacks.push_back(callback);
        }

        // Call once per frame after GameObjects::Update()
        static void Update() {
            if (s_callbacks.empty()) return;

            float now = Game::GetTime();
            if (now <= 0.0f) return;

            for (auto& hero : GameObjects::AllHeroes) {
                if (!hero.IsValid() || !hero.IsAlive()) continue;

                int netId = hero.GetNetId();
                if (netId == 0) continue;

                AiManager ai(hero.address);
                bool isDashing = ai.IsValid() && ai.IsDashing();

                auto& state = s_dashStates[netId];

                if (isDashing && !state.wasDashing) {
                    // Dash just started!
                    state.wasDashing = true;
                    state.startPos = hero.GetPosition();
                    state.startTime = now;

                    Vec3 endPos = ai.GetPathEnd();
                    float speed = ai.GetDashSpeed();
                    if (speed < 100.0f) speed = hero.GetMoveSpeed() * 2.0f; // fallback

                    float dist = state.startPos.Distance(endPos);
                    float duration = (speed > 0.0f) ? (dist / speed) : 0.0f;

                    // Detect blink: if distance is large and speed is very high, it's likely a blink
                    bool isBlink = (speed > 5000.0f || duration < 0.01f);

                    DashEventArgs args;
                    args.Sender    = hero;
                    args.StartPos  = state.startPos;
                    args.EndPos    = endPos;
                    args.Speed     = speed;
                    args.Duration  = duration;
                    args.StartTime = now;
                    args.EndTime   = now + duration;
                    args.IsBlink   = isBlink;

                    for (auto& cb : s_callbacks) {
                        cb(args);
                    }
                }
                else if (!isDashing && state.wasDashing) {
                    // Dash ended
                    state.wasDashing = false;
                }
            }
        }

        // ====================================================================
        // Utility: check if a hero is currently dashing
        // ====================================================================
        static bool IsDashing(const GameObject& hero) {
            if (!hero.IsValid()) return false;
            AiManager ai(hero.address);
            return ai.IsValid() && ai.IsDashing();
        }

        // Get dash info for a currently dashing hero
        static DashEventArgs GetDashInfo(const GameObject& hero) {
            DashEventArgs args = {};
            if (!hero.IsValid()) return args;

            int netId = hero.GetNetId();
            auto it = s_dashStates.find(netId);
            if (it == s_dashStates.end() || !it->second.wasDashing) return args;

            AiManager ai(hero.address);
            if (!ai.IsValid() || !ai.IsDashing()) return args;

            args.Sender    = hero;
            args.StartPos  = it->second.startPos;
            args.EndPos    = ai.GetPathEnd();
            args.Speed     = ai.GetDashSpeed();
            args.StartTime = it->second.startTime;

            float dist = args.StartPos.Distance(args.EndPos);
            args.Duration = (args.Speed > 0.0f) ? (dist / args.Speed) : 0.0f;
            args.EndTime = args.StartTime + args.Duration;
            args.IsBlink = (args.Speed > 5000.0f || args.Duration < 0.01f);

            return args;
        }

        // Clear all state (e.g. on game reset)
        static void Clear() {
            s_dashStates.clear();
            s_callbacks.clear();
        }

    private:
        static inline std::vector<OnDashFn> s_callbacks;

        struct DashState {
            bool  wasDashing = false;
            Vec3  startPos;
            float startTime = 0.0f;
        };

        static inline std::unordered_map<int, DashState> s_dashStates;
    };

} // namespace SDK
