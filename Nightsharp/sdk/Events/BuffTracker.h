#pragma once

#include "Events.h"
#include "../Core/Objects.h"

#include <functional>
#include <utility>

namespace SDK::Events::BuffTracker {

using BuffCallback = std::function<void(const AIBaseClient&, const ::SDK::Events::BuffEventArgs&)>;

namespace detail {

inline constexpr int kMaxCallbacks = 32;
inline BuffCallback s_handlers[kMaxCallbacks] = {};
inline int s_count = 0;
inline bool s_registered = false;

inline void Dispatch(const ::SDK::Events::BuffEventArgs& args) {
    if (s_count <= 0 || !args.Sender.IsValid()) {
        return;
    }

    const AIBaseClient sender(args.Sender.Ptr);
    if (!sender.IsValid()) {
        return;
    }

    for (int i = 0; i < s_count; ++i) {
        if (s_handlers[i]) {
            s_handlers[i](sender, args);
        }
    }
}

inline void OnBuffUpdateThunk(const ::SDK::Events::BuffEventArgs& args) {
    __try {
        Dispatch(args);
    } __except (1) {
    }
}

} // namespace detail

inline void Initialize() {
    if (!detail::s_registered) {
        ::SDK::Events::OnBuffUpdate(&detail::OnBuffUpdateThunk);
        detail::s_registered = true;
    }
}

inline void Update() {
}

inline void Reset() {
    ::SDK::Events::RemoveOnBuffUpdate(&detail::OnBuffUpdateThunk);
    for (int i = 0; i < detail::s_count; ++i) {
        detail::s_handlers[i] = nullptr;
    }
    detail::s_count = 0;
    detail::s_registered = false;
}

inline void OnBuffUpdate(BuffCallback cb) {
    Initialize();
    if (cb && detail::s_count < detail::kMaxCallbacks) {
        detail::s_handlers[detail::s_count++] = std::move(cb);
    }
}

inline void OnBuffGain(BuffCallback cb) {
    OnBuffUpdate([cb](const AIBaseClient& sender,
                      const ::SDK::Events::BuffEventArgs& args) {
        if (cb && args.Count > 0) {
            cb(sender, args);
        }
    });
}

inline void OnBuffLose(BuffCallback cb) {
    OnBuffUpdate([cb](const AIBaseClient& sender,
                      const ::SDK::Events::BuffEventArgs& args) {
        if (cb && args.Count == 0) {
            cb(sender, args);
        }
    });
}

inline void Clear() {
    Reset();
}

} // namespace SDK::Events::BuffTracker
