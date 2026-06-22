#pragma once

#include "Events.h"

namespace SDK::Events {

struct LoadEventArgs {};

using LoadHandler = void(*)(const LoadEventArgs&);

namespace detail {
    inline constexpr int MaxLoadHandlers = 32;
    inline LoadHandler LoadHandlers[MaxLoadHandlers] = {};
    inline bool LoadInvoked[MaxLoadHandlers] = {};
    inline int LoadHandlerCount = 0;
} // namespace detail

inline bool AddOnLoad(LoadHandler handler) {
    if (!handler) {
        return false;
    }

    Initialize();

    for (int i = 0; i < detail::LoadHandlerCount; ++i) {
        if (detail::LoadHandlers[i] == handler) {
            return true;
        }
    }

    if (detail::LoadHandlerCount >= detail::MaxLoadHandlers) {
        return false;
    }

    const int index = detail::LoadHandlerCount++;
    detail::LoadHandlers[index] = handler;
    detail::LoadInvoked[index] = false;
    return true;
}

inline bool RemoveOnLoad(LoadHandler handler) {
    if (!handler) {
        return false;
    }

    for (int i = 0; i < detail::LoadHandlerCount; ++i) {
        if (detail::LoadHandlers[i] != handler) {
            continue;
        }

        for (int j = i; j + 1 < detail::LoadHandlerCount; ++j) {
            detail::LoadHandlers[j] = detail::LoadHandlers[j + 1];
            detail::LoadInvoked[j] = detail::LoadInvoked[j + 1];
        }
        --detail::LoadHandlerCount;
        detail::LoadHandlers[detail::LoadHandlerCount] = nullptr;
        detail::LoadInvoked[detail::LoadHandlerCount] = false;
        return true;
    }

    return false;
}

inline bool OnLoad(LoadHandler handler) {
    return AddOnLoad(handler);
}

namespace detail {
    inline void EventLoad() {
        if (!IsGameLoaded()) {
            return;
        }

        const LoadEventArgs args{};
        for (int i = 0; i < LoadHandlerCount; ++i) {
            if (!LoadHandlers[i] || LoadInvoked[i]) {
                continue;
            }

            LoadInvoked[i] = true;
            __try {
                LoadHandlers[i](args);
            } __except (1) {}
        }
    }
} // namespace detail

} // namespace SDK::Events
