#pragma once

#include "Signal.h"

#include <algorithm>
#include <new>
#include <vector>

namespace SDK::Signals::SignalManager {

namespace detail {
    inline std::vector<Signal>* g_signals = nullptr;

    inline bool EnsureStorage() {
        if (!g_signals) {
            g_signals = new(std::nothrow) std::vector<Signal>();
        }
        return g_signals != nullptr;
    }
}

inline void Initialize() {
    detail::EnsureStorage();
}

inline void AddSignal(const Signal& signal) {
    if (!signal.IsValid() || !detail::EnsureStorage()) {
        return;
    }

    auto& signals = *detail::g_signals;
    const auto it = std::find(signals.begin(), signals.end(), signal);
    if (it == signals.end()) {
        signals.push_back(signal);
    }
}

inline void RemoveSignal(const Signal& signal) {
    if (!detail::g_signals) {
        return;
    }

    auto& signals = *detail::g_signals;
    signals.erase(std::remove(signals.begin(), signals.end(), signal), signals.end());
}

inline int Count() {
    return detail::g_signals ? static_cast<int>(detail::g_signals->size()) : 0;
}

inline std::vector<Signal> GetSignals() {
    return detail::g_signals ? *detail::g_signals : std::vector<Signal>{};
}

inline void Update() {
    if (!detail::EnsureStorage()) {
        return;
    }

    const auto snapshot = *detail::g_signals;
    for (const auto& signal : snapshot) {
        if (!signal.IsValid() || !signal.Enabled()) {
            continue;
        }

        const auto& signalWaver = signal.GetSignalWaver();
        if (signalWaver && signalWaver(signal)) {
            signal.TriggerSignal("SignalManager::Update", "Signal was waved.");
            RemoveSignal(signal);
            continue;
        }

        if (signal.Expired() && !signal.CalledExpired()) {
            signal.TriggerOnExpired("SignalManager::Update");
            signal.SetCalledExpired(true);
        }
    }
}

inline void Reset() {
    if (detail::g_signals) {
        detail::g_signals->clear();
    }
}

} // namespace SDK::Signals::SignalManager
