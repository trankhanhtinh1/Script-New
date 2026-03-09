#pragma once
// ============================================================================
// DelayAction.h — Delayed Action Queue (EnsoulSharp SDK Port)
// ============================================================================
// Port of EnsoulSharp.SDK/Core/Utils/DelayAction.cs
// Provides:
//   - DelayAction::Add(delayMs, callback) — execute after delay
//   - DelayAction::Update() — process pending actions (call each frame)
//   - Thread-safe, supports cancellation
// ============================================================================

#include "Game.h"

#include <functional>
#include <vector>
#include <mutex>
#include <algorithm>

namespace SDK {

// ============================================================================
// DelayActionItem — Single delayed action
// ============================================================================
struct DelayActionItem {
    float ExecuteTime;          // Game time (seconds) when to execute
    std::function<void()> Func; // Callback function
    bool Cancelled = false;     // If true, skip execution

    DelayActionItem() : ExecuteTime(0) {}
    DelayActionItem(float execTime, std::function<void()> func)
        : ExecuteTime(execTime), Func(std::move(func)) {}
};

// ============================================================================
// DelayAction — Static delayed action manager
// ============================================================================
class DelayAction {
public:

    // ---- Add a delayed action (delay in milliseconds) ----
    static int Add(int delayMs, std::function<void()> func) {
        return Add((float)delayMs, std::move(func));
    }

    static int Add(float delayMs, std::function<void()> func) {
        float executeTime = Game::GetTime() + (delayMs / 1000.0f);

        std::lock_guard<std::mutex> lock(s_mutex);
        int id = s_nextId++;
        s_actions.push_back({ executeTime, std::move(func) });
        s_actionIds.push_back(id);
        return id;
    }

    // ---- Cancel a pending action by ID ----
    static void Cancel(int id) {
        std::lock_guard<std::mutex> lock(s_mutex);
        for (size_t i = 0; i < s_actionIds.size(); i++) {
            if (s_actionIds[i] == id) {
                s_actions[i].Cancelled = true;
                break;
            }
        }
    }

    // ---- Process pending actions (call every frame) ----
    static void Update() {
        float now = Game::GetTime();

        // Collect actions to execute
        std::vector<std::function<void()>> toExecute;

        {
            std::lock_guard<std::mutex> lock(s_mutex);

            // Find actions whose time has come
            size_t i = 0;
            while (i < s_actions.size()) {
                if (s_actions[i].Cancelled) {
                    // Remove cancelled
                    s_actions.erase(s_actions.begin() + i);
                    s_actionIds.erase(s_actionIds.begin() + i);
                    continue;
                }

                if (now >= s_actions[i].ExecuteTime) {
                    toExecute.push_back(std::move(s_actions[i].Func));
                    s_actions.erase(s_actions.begin() + i);
                    s_actionIds.erase(s_actionIds.begin() + i);
                    continue;
                }
                i++;
            }
        }

        // Execute outside lock
        for (auto& func : toExecute) {
            try {
                if (func) func();
            }
            catch (...) {
                // Silently ignore exceptions in delayed actions
            }
        }
    }

    // ---- Clear all pending actions ----
    static void Clear() {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_actions.clear();
        s_actionIds.clear();
    }

    // ---- Get count of pending actions ----
    static size_t PendingCount() {
        std::lock_guard<std::mutex> lock(s_mutex);
        return s_actions.size();
    }

private:
    static inline std::vector<DelayActionItem> s_actions;
    static inline std::vector<int> s_actionIds;
    static inline std::mutex s_mutex;
    static inline int s_nextId = 1;
};

} // namespace SDK
