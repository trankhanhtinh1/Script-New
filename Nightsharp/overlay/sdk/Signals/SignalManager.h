#pragma once

#include "../Events/Events.h"
#include "Signal.h"

#include <algorithm>
#include <vector>

namespace SDK::Signals::SignalManager {

namespace detail {
    inline std::vector<Signal*> Signals;

    inline bool Contains(Signal* signal) {
        return std::find(Signals.begin(), Signals.end(), signal) != Signals.end();
    }
} // namespace detail

inline void Game_OnUpdate(const SDK::Events::GameUpdateEventArgs& args);

inline void Initialize() {
    SDK::Events::AddOnGameUpdate(&Game_OnUpdate);
}

inline void AddSignal(Signal* signal) {
    if (!signal) {
        return;
    }

    Initialize();
    if (!detail::Contains(signal)) {
        detail::Signals.push_back(signal);
    }
}

inline void RemoveSignal(Signal* signal) {
    if (!signal) {
        return;
    }

    detail::Signals.erase(
        std::remove(detail::Signals.begin(), detail::Signals.end(), signal),
        detail::Signals.end());
}

inline void Reset() {
    SDK::Events::RemoveOnGameUpdate(&Game_OnUpdate);
    detail::Signals.clear();
}

inline int Count() {
    return static_cast<int>(detail::Signals.size());
}

inline const std::vector<Signal*>& ActiveSignals() {
    return detail::Signals;
}

inline bool InvokeSignalWaver(Signal* signal) {
    if (!signal || !signal->SignalWaver) {
        return false;
    }

    bool wave = false;
    __try {
        wave = signal->SignalWaver(*signal);
    } __except (1) {
        wave = false;
    }
    return wave;
}

inline void Game_OnUpdate(const SDK::Events::GameUpdateEventArgs& args) {
    (void)args;

    std::vector<Signal*> snapshot;
    snapshot.reserve(detail::Signals.size());
    for (Signal* signal : detail::Signals) {
        if (signal && signal->Enabled) {
            snapshot.push_back(signal);
        }
    }

    for (Signal* signal : snapshot) {
        if (!signal) {
            continue;
        }

        if (InvokeSignalWaver(signal)) {
            TriggerSignalFromManager(*signal, "Game_OnUpdate", "Signal was waved.");
            RemoveSignal(signal);
        }

        if (signal->Expired()) {
            (void)TriggerExpiredOnceFromManager(*signal, "Game_OnUpdate");
        }
    }
}

} // namespace SDK::Signals::SignalManager
