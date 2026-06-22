#pragma once

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <exception>
#include <cstdio>
#include <limits>
#include <string>
#include <unordered_map>

namespace SDK::Signals {

class Signal;
inline bool TriggerExpiredOnceFromManager(Signal& signal, const char* sender);

namespace SignalManager {
    void AddSignal(Signal* signal);
    void RemoveSignal(Signal* signal);
} // namespace SignalManager

namespace detail {
    inline constexpr int MaxSignalHandlers = 32;
    inline constexpr int ReasonBufferSize = 256;

    inline uint64_t NowMilliseconds() {
        using Clock = std::chrono::steady_clock;
        return static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now().time_since_epoch()).count());
    }

    inline void Copy(char* dst, int dstCount, const char* src) {
        if (!dst || dstCount <= 0) {
            return;
        }

        dst[0] = 0;
        if (src) {
            std::snprintf(dst, static_cast<size_t>(dstCount), "%s", src);
        }
    }

    template <typename Handler>
    struct HandlerList {
        Handler Handlers[MaxSignalHandlers] = {};
        int Count = 0;

        bool Add(Handler handler) {
            if (!handler) {
                return false;
            }
            for (int i = 0; i < Count; ++i) {
                if (Handlers[i] == handler) {
                    return true;
                }
            }
            if (Count >= MaxSignalHandlers) {
                return false;
            }
            Handlers[Count++] = handler;
            return true;
        }

        bool Remove(Handler handler) {
            if (!handler) {
                return false;
            }
            for (int i = 0; i < Count; ++i) {
                if (Handlers[i] != handler) {
                    continue;
                }
                for (int j = i; j + 1 < Count; ++j) {
                    Handlers[j] = Handlers[j + 1];
                }
                Handlers[--Count] = nullptr;
                return true;
            }
            return false;
        }

        void Clear() {
            for (int i = 0; i < Count; ++i) {
                Handlers[i] = nullptr;
            }
            Count = 0;
        }

        bool Empty() const {
            return Count == 0;
        }
    };
} // namespace detail

class Signal {
public:
    using PropertyMap = std::unordered_map<std::string, std::string>;

    struct EnabledStatusChangedArgs {
        bool Status = false;
    };

    struct GlobalSignalRaisedArgs {
        char Reason[detail::ReasonBufferSize] = {};
        Signal* SignalPtr = nullptr;
    };

    struct RaisedArgs {
        char Reason[detail::ReasonBufferSize] = {};
        Signal* SignalPtr = nullptr;
    };

    using OnEnabledStatusChangedDelegate = void(*)(const char* sender, const EnabledStatusChangedArgs& args);
    using OnExpiredDelegate = void(*)(const char* sender, Signal& signal);
    using OnRaisedDelegate = void(*)(const char* sender, const RaisedArgs& args);
    using OnSignalRaisedDelegate = void(*)(const char* sender, Signal& signal);
    using SignalWaverDelegate = bool(*)(Signal& signal);

    static constexpr uint64_t InfiniteExpiration = std::numeric_limits<uint64_t>::max();

    static uint64_t NowMilliseconds() {
        return detail::NowMilliseconds();
    }

    static uint64_t AfterMilliseconds(uint64_t milliseconds) {
        const uint64_t now = NowMilliseconds();
        if (InfiniteExpiration - now <= milliseconds) {
            return InfiniteExpiration;
        }
        return now + milliseconds;
    }

    static Signal* Create(OnRaisedDelegate onRaised = nullptr,
                          SignalWaverDelegate signalWaver = nullptr,
                          uint64_t expiration = InfiniteExpiration,
                          const PropertyMap* defaultProperties = nullptr);

    static Signal* CreateAfterMilliseconds(OnRaisedDelegate onRaised,
                                           SignalWaverDelegate signalWaver,
                                           uint64_t milliseconds,
                                           const PropertyMap* defaultProperties = nullptr) {
        return Create(onRaised, signalWaver, AfterMilliseconds(milliseconds), defaultProperties);
    }

    static bool AddOnSignalRaised(OnSignalRaisedDelegate handler) {
        return SignalRaisedHandlers.Add(handler);
    }

    static bool RemoveOnSignalRaised(OnSignalRaisedDelegate handler) {
        return SignalRaisedHandlers.Remove(handler);
    }

    static bool OnSignalRaised(OnSignalRaisedDelegate handler) {
        return AddOnSignalRaised(handler);
    }

    bool AddOnEnabledStatusChanged(OnEnabledStatusChangedDelegate handler) {
        return EnabledStatusChangedHandlers.Add(handler);
    }

    bool RemoveOnEnabledStatusChanged(OnEnabledStatusChangedDelegate handler) {
        return EnabledStatusChangedHandlers.Remove(handler);
    }

    bool OnEnabledStatusChanged(OnEnabledStatusChangedDelegate handler) {
        return AddOnEnabledStatusChanged(handler);
    }

    bool AddOnExpired(OnExpiredDelegate handler) {
        return ExpiredHandlers.Add(handler);
    }

    bool RemoveOnExpired(OnExpiredDelegate handler) {
        return ExpiredHandlers.Remove(handler);
    }

    bool OnExpired(OnExpiredDelegate handler) {
        return AddOnExpired(handler);
    }

    bool AddOnRaised(OnRaisedDelegate handler) {
        return RaisedHandlers.Add(handler);
    }

    bool RemoveOnRaised(OnRaisedDelegate handler) {
        return RaisedHandlers.Remove(handler);
    }

    bool OnRaised(OnRaisedDelegate handler) {
        return AddOnRaised(handler);
    }

    bool Expired() const {
        return NowMilliseconds() >= Expiration;
    }

    void Disable(const char* caller = "Signal.Disable");
    void Enable(const char* caller = "Signal.Enable");
    void Raise(const char* reason, const char* caller = "Signal.Raise");
    void Raise(const std::exception& exception, const char* caller = "Signal.Raise");
    void Reset();

    bool Enabled = false;
    uint64_t Expiration = InfiniteExpiration;
    uint64_t LastSignaled = 0;
    PropertyMap Properties = {};
    bool Raised = false;
    SignalWaverDelegate SignalWaver = nullptr;

private:
    friend void SignalManager::AddSignal(Signal* signal);
    friend void SignalManager::RemoveSignal(Signal* signal);
    friend void TriggerSignalFromManager(Signal& signal, const char* sender, const char* reason);
    friend bool TriggerExpiredOnceFromManager(Signal& signal, const char* sender);

    Signal(OnRaisedDelegate signalRaised,
           SignalWaverDelegate signalWaver,
           uint64_t expiration,
           const PropertyMap* properties)
        : Expiration(expiration == 0 ? InfiniteExpiration : expiration),
          Properties(properties ? *properties : PropertyMap{}),
          SignalWaver(signalWaver) {
        if (signalRaised) {
            RaisedHandlers.Add(signalRaised);
        }
    }

    void TriggerEnabledStatusChanged(const char* sender, bool enabled);
    void TriggerOnExpired(const char* sender);
    void TriggerSignal(const char* sender, const char* reason);

    bool CalledExpired = false;
    detail::HandlerList<OnEnabledStatusChangedDelegate> EnabledStatusChangedHandlers = {};
    detail::HandlerList<OnExpiredDelegate> ExpiredHandlers = {};
    detail::HandlerList<OnRaisedDelegate> RaisedHandlers = {};

    inline static detail::HandlerList<OnSignalRaisedDelegate> SignalRaisedHandlers = {};
};

inline void TriggerSignalFromManager(Signal& signal, const char* sender, const char* reason) {
    signal.TriggerSignal(sender, reason);
}

inline bool TriggerExpiredOnceFromManager(Signal& signal, const char* sender) {
    if (signal.CalledExpired) {
        return false;
    }

    signal.TriggerOnExpired(sender);
    signal.CalledExpired = true;
    return true;
}

} // namespace SDK::Signals

#include "SignalManager.h"

namespace SDK::Signals {

inline Signal* Signal::Create(OnRaisedDelegate onRaised,
                              SignalWaverDelegate signalWaver,
                              uint64_t expiration,
                              const PropertyMap* defaultProperties) {
    Signal* signal = new Signal(onRaised, signalWaver, expiration, defaultProperties);
    SignalManager::AddSignal(signal);
    signal->Enabled = true;
    return signal;
}

inline void Signal::Disable(const char* caller) {
    Enabled = false;
    TriggerEnabledStatusChanged(caller, false);
    SignalManager::RemoveSignal(this);
}

inline void Signal::Enable(const char* caller) {
    Enabled = true;
    TriggerEnabledStatusChanged(caller, true);
    SignalManager::AddSignal(this);
}

inline void Signal::Raise(const char* reason, const char* caller) {
    TriggerSignal(caller, reason);
}

inline void Signal::Raise(const std::exception& exception, const char* caller) {
    TriggerSignal(caller, exception.what());
}

inline void Signal::Reset() {
    Enabled = true;
    Raised = false;
    CalledExpired = false;
    SignalManager::AddSignal(this);
}

inline void Signal::TriggerEnabledStatusChanged(const char* sender, bool enabled) {
    EnabledStatusChangedArgs args{};
    args.Status = enabled;
    for (int i = 0; i < EnabledStatusChangedHandlers.Count; ++i) {
        if (auto handler = EnabledStatusChangedHandlers.Handlers[i]) {
            __try {
                handler(sender, args);
            } __except (1) {}
        }
    }
}

inline void Signal::TriggerOnExpired(const char* sender) {
    for (int i = 0; i < ExpiredHandlers.Count; ++i) {
        if (auto handler = ExpiredHandlers.Handlers[i]) {
            __try {
                handler(sender, *this);
            } __except (1) {}
        }
    }
}

inline void Signal::TriggerSignal(const char* sender, const char* reason) {
    if (RaisedHandlers.Empty() || Expired() || Raised) {
        return;
    }

    Raised = true;
    LastSignaled = NowMilliseconds();

    RaisedArgs args{};
    args.SignalPtr = this;
    detail::Copy(args.Reason, static_cast<int>(sizeof(args.Reason)), reason);
    for (int i = 0; i < RaisedHandlers.Count; ++i) {
        if (auto handler = RaisedHandlers.Handlers[i]) {
            __try {
                handler(sender, args);
            } __except (1) {}
        }
    }

    for (int i = 0; i < SignalRaisedHandlers.Count; ++i) {
        if (auto handler = SignalRaisedHandlers.Handlers[i]) {
            __try {
                handler(sender, *this);
            } __except (1) {}
        }
    }
}

} // namespace SDK::Signals

namespace SDK::Core::Signals {
    using Signal = ::SDK::Signals::Signal;
    namespace SignalManager = ::SDK::Signals::SignalManager;
} // namespace SDK::Core::Signals
