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

    inline bool HasLoadHandlers() {
        return LoadHandlerCount > 0;
    }
} // namespace detail

inline bool AddOnLoad(LoadHandler handler) {
    if (!handler) {
        return false;
    }

    Initialize();
    if (!detail::EnsureGameUpdateRawSubscribed()) {
        return false;
    }

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
        if (detail::LoadHandlerCount == 0) {
            detail::ReleaseGameUpdateRawIfUnused();
        }
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
            LoadHandler handler = LoadHandlers[i];
            __try {
                handler(args);
            } __except (LogEventHandlerException(
                             "Load",
                             i,
                             reinterpret_cast<const void*>(handler),
                             GetExceptionInformation())) {
                LoadHandlers[i] = nullptr;
            }
        }
    }
} // namespace detail

} // namespace SDK::Events
