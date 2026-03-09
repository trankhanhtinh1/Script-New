#pragma once
#include "sdk/SDK.h"
#include "sdk/EzEvade/Utils/EvadeUtils.h"
#include <functional>
#include <vector>

namespace EzEvade {
namespace DelayAction {

using Callback = std::function<void()>;

struct Action {
    Callback CallbackObject = nullptr;
    int Time = 0;

    Action() = default;
    Action(int time, Callback callback)
        : CallbackObject(std::move(callback)),
          Time(time + (int)EvadeUtils::TickCount()) {}
};

inline std::vector<Action> ActionList = {};

inline void Update() {
    for (int i = (int)ActionList.size() - 1; i >= 0; --i) {
        if (ActionList[(size_t)i].Time <= (int)EvadeUtils::TickCount()) {
            try {
                if (ActionList[(size_t)i].CallbackObject) {
                    ActionList[(size_t)i].CallbackObject();
                }
            } catch (...) {
            }
            ActionList.erase(ActionList.begin() + i);
        }
    }
}

inline void Add(int time, Callback func) {
    ActionList.emplace_back(time, std::move(func));
}

} // namespace DelayAction
} // namespace EzEvade

