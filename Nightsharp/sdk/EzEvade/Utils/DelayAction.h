#pragma once
#include <vector>
#include <functional>
#include <cstdio>
#include "EvadeUtils.h"

// ============================================================================
// DelayAction
//   C# original: ezEvade.DelayAction (DelayAction.cs, 69 lines)
//   Line-by-line port preserving original logic
//
//   Simple delayed-callback system. Register callbacks with a delay (ms),
//   and call OnUpdate() every tick to dispatch ready callbacks.
// ============================================================================

namespace EzEvade {

    class DelayAction {
    public:
        // ====================================================================
        // Callback typedef
        //   C# line 17: public delegate void Callback();
        // ====================================================================
        using Callback = std::function<void()>;                         // C# line 17

        // ====================================================================
        // Action struct
        //   C# lines 56-66
        // ====================================================================
        struct Action {
            Callback callbackObject;                                    // C# line 58
            float    time = 0;                                          // C# line 59

            // C# lines 61-65
            Action() = default;
            Action(float timeMs, Callback callback) {
                this->time = timeMs + EvadeUtils::TickCount();          // C# line 63
                this->callbackObject = std::move(callback);             // C# line 64
            }
        };

        // ====================================================================
        // ActionList
        //   C# line 19: public static List<Action> ActionList
        // ====================================================================
        static inline std::vector<Action> actionList;                   // C# line 19

        // ====================================================================
        // Constructor / Initialization
        //   C# lines 21-24: hooks Game.OnUpdate
        //   In C++ we call OnUpdate() explicitly from the game loop
        // ====================================================================

        // ====================================================================
        // OnUpdate — dispatch ready callbacks
        //   C# lines 26-48: private static void GameOnOnGameUpdate(EventArgs args)
        // ====================================================================
        static void OnUpdate() {
            // C# line 28: for (var i = ActionList.Count - 1; i >= 0; i--)
            for (int i = (int)actionList.size() - 1; i >= 0; i--)
            {
                // C# line 30: if (ActionList[i].Time <= EvadeUtils.TickCount)
                if (actionList[i].time <= EvadeUtils::TickCount())
                {
                    try                                                 // C# line 32
                    {
                        // C# line 34: if (ActionList[i].CallbackObject != null)
                        if (actionList[i].callbackObject)
                        {
                            actionList[i].callbackObject();             // C# line 36
                        }
                    }
                    catch (...)                                         // C# line 40: catch (Exception e)
                    {
                        printf("[DelayAction] Exception in callback\n"); // C# line 42
                    }

                    // C# line 45: ActionList.RemoveAt(i);
                    actionList.erase(actionList.begin() + i);
                }
            }
        }

        // ====================================================================
        // Add
        //   C# lines 50-54: public static void Add(int time, Callback func)
        // ====================================================================
        static void Add(int time, Callback func) {
            Action action(static_cast<float>(time), std::move(func));   // C# line 52
            actionList.push_back(std::move(action));                    // C# line 53
        }
    };

} // namespace EzEvade
