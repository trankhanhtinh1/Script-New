#pragma once

#include "../Core/Game.h"

#include <algorithm>
#include <any>
#include <cstdint>
#include <exception>
#include <functional>
#include <limits>
#include <memory>
#include <new>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace SDK::Signals {

class Signal;

namespace SignalManager {
    void AddSignal(const Signal& signal);
    void RemoveSignal(const Signal& signal);
}

class Signal {
public:
    class EnabledStatusChangedArgs {
    public:
        explicit EnabledStatusChangedArgs(bool status = false)
            : Status(status) {}

        bool Status = false;
    };

    class GlobalSignalRaisedArgs {
    public:
        GlobalSignalRaisedArgs() = default;
        GlobalSignalRaisedArgs(std::string reason, const Signal* signal)
            : Reason(std::move(reason))
            , SignalRef(signal) {}

        std::string Reason = {};
        const Signal* SignalRef = nullptr;
    };

    class RaisedArgs {
    public:
        RaisedArgs() = default;
        RaisedArgs(std::string reason, const Signal* signal)
            : Reason(std::move(reason))
            , SignalRef(signal) {}

        std::string Reason = {};
        const Signal* SignalRef = nullptr;
    };

    using OnEnabledStatusChangedDelegate = std::function<void(const char* sender, const EnabledStatusChangedArgs& args)>;
    using OnRaisedDelegate = std::function<void(const char* sender, const RaisedArgs& args)>;
    using OnSignalRaisedDelegate = std::function<void(const char* sender, const Signal& signal)>;
    using OnExpiredDelegate = std::function<void(const char* sender)>;
    using SignalWaverDelegate = std::function<bool(const Signal& signal)>;
    using PropertiesMap = std::unordered_map<std::string, std::any>;

    Signal() = default;

    static Signal Create(OnRaisedDelegate onRaised = {},
                         SignalWaverDelegate signalWaver = {},
                         uint64_t expirationTick = 0,
                         PropertiesMap defaultProperties = {}) {
        auto impl = std::make_shared<Impl>();
        impl->Enabled = true;
        impl->ExpirationTick = (expirationTick == 0)
            ? std::numeric_limits<uint64_t>::max()
            : expirationTick;
        impl->Properties = std::move(defaultProperties);
        if (onRaised) {
            impl->OnRaisedHandlers.push_back(std::move(onRaised));
        }
        if (signalWaver) {
            impl->SignalWaver = std::move(signalWaver);
        }

        Signal signal(impl);
        SignalManager::AddSignal(signal);
        return signal;
    }

    static bool AddOnSignalRaised(OnSignalRaisedDelegate handler) {
        if (!handler) {
            return false;
        }
        if (!EnsureGlobalHandlers()) {
            return false;
        }
        g_globalHandlers->push_back(std::move(handler));
        return true;
    }

    static bool OnSignalRaised(OnSignalRaisedDelegate handler) {
        return AddOnSignalRaised(std::move(handler));
    }

    bool IsValid() const {
        return static_cast<bool>(m_impl);
    }

    bool operator==(const Signal& other) const {
        return m_impl == other.m_impl;
    }

    bool operator!=(const Signal& other) const {
        return !(*this == other);
    }

    bool Enabled() const {
        return m_impl && m_impl->Enabled;
    }

    uint64_t Expiration() const {
        return m_impl ? m_impl->ExpirationTick : 0;
    }

    void SetExpiration(uint64_t expirationTick) const {
        if (m_impl) {
            m_impl->ExpirationTick = expirationTick;
        }
    }

    bool Expired() const {
        return m_impl &&
               Game::TickCount() >= static_cast<int>(std::min<uint64_t>(m_impl->ExpirationTick, static_cast<uint64_t>(std::numeric_limits<int>::max())));
    }

    uint64_t LastSignaled() const {
        return m_impl ? m_impl->LastSignaledTick : 0;
    }

    void SetLastSignaled(uint64_t tick) const {
        if (m_impl) {
            m_impl->LastSignaledTick = tick;
        }
    }

    PropertiesMap& Properties() const {
        static PropertiesMap empty = {};
        return m_impl ? m_impl->Properties : empty;
    }

    bool Raised() const {
        return m_impl && m_impl->Raised;
    }

    void SetRaised(bool raised) const {
        if (m_impl) {
            m_impl->Raised = raised;
        }
    }

    const SignalWaverDelegate& GetSignalWaver() const {
        static const SignalWaverDelegate empty = {};
        return m_impl ? m_impl->SignalWaver : empty;
    }

    void SetSignalWaver(SignalWaverDelegate signalWaver) const {
        if (m_impl) {
            m_impl->SignalWaver = std::move(signalWaver);
        }
    }

    bool CalledExpired() const {
        return m_impl && m_impl->CalledExpired;
    }

    void SetCalledExpired(bool calledExpired) const {
        if (m_impl) {
            m_impl->CalledExpired = calledExpired;
        }
    }

    void AddOnEnabledStatusChanged(OnEnabledStatusChangedDelegate handler) const {
        if (m_impl && handler) {
            m_impl->OnEnabledStatusChangedHandlers.push_back(std::move(handler));
        }
    }

    void AddOnRaised(OnRaisedDelegate handler) const {
        if (m_impl && handler) {
            m_impl->OnRaisedHandlers.push_back(std::move(handler));
        }
    }

    void AddOnExpired(OnExpiredDelegate handler) const {
        if (m_impl && handler) {
            m_impl->OnExpiredHandlers.push_back(std::move(handler));
        }
    }

    void Disable() const {
        if (!m_impl) {
            return;
        }
        m_impl->Enabled = false;
        TriggerEnabledStatusChanged("Signal::Disable", false);
        SignalManager::RemoveSignal(*this);
    }

    void Enable() const {
        if (!m_impl) {
            return;
        }
        m_impl->Enabled = true;
        TriggerEnabledStatusChanged("Signal::Enable", true);
        SignalManager::AddSignal(*this);
    }

    void Raise(const std::string& reason) const {
        TriggerSignal("Signal::Raise", reason);
    }

    void Raise(const char* reason) const {
        TriggerSignal("Signal::Raise", reason ? std::string(reason) : std::string());
    }

    void Raise(const std::exception& exception) const {
        TriggerSignal("Signal::Raise", exception.what() ? std::string(exception.what()) : std::string());
    }

    void Reset() const {
        if (!m_impl) {
            return;
        }
        m_impl->Enabled = true;
        m_impl->Raised = false;
        m_impl->CalledExpired = false;
        SignalManager::AddSignal(*this);
    }

    void TriggerEnabledStatusChanged(const char* sender, bool enabled) const {
        if (!m_impl) {
            return;
        }
        const EnabledStatusChangedArgs args(enabled);
        for (const auto& handler : m_impl->OnEnabledStatusChangedHandlers) {
            if (handler) {
                handler(sender, args);
            }
        }
    }

    void TriggerOnExpired(const char* sender) const {
        if (!m_impl) {
            return;
        }
        for (const auto& handler : m_impl->OnExpiredHandlers) {
            if (handler) {
                handler(sender);
            }
        }
    }

    void TriggerSignal(const char* sender, const std::string& reason) const {
        if (!m_impl || m_impl->OnRaisedHandlers.empty() || Expired() || m_impl->Raised) {
            return;
        }

        m_impl->Raised = true;
        m_impl->LastSignaledTick = static_cast<uint64_t>(Game::TickCount());

        const RaisedArgs raisedArgs(reason, this);
        for (const auto& handler : m_impl->OnRaisedHandlers) {
            if (handler) {
                handler(sender, raisedArgs);
            }
        }

        if (EnsureGlobalHandlers()) {
            for (const auto& handler : *g_globalHandlers) {
                if (handler) {
                    handler(sender, *this);
                }
            }
        }
    }

private:
    struct Impl {
        bool Enabled = false;
        uint64_t ExpirationTick = std::numeric_limits<uint64_t>::max();
        uint64_t LastSignaledTick = 0;
        bool Raised = false;
        bool CalledExpired = false;
        PropertiesMap Properties = {};
        SignalWaverDelegate SignalWaver = {};
        std::vector<OnEnabledStatusChangedDelegate> OnEnabledStatusChangedHandlers = {};
        std::vector<OnRaisedDelegate> OnRaisedHandlers = {};
        std::vector<OnExpiredDelegate> OnExpiredHandlers = {};
    };

    explicit Signal(std::shared_ptr<Impl> impl)
        : m_impl(std::move(impl)) {}

    static bool EnsureGlobalHandlers() {
        if (!g_globalHandlers) {
            g_globalHandlers = new(std::nothrow) std::vector<OnSignalRaisedDelegate>();
        }
        return g_globalHandlers != nullptr;
    }

    inline static std::vector<OnSignalRaisedDelegate>* g_globalHandlers = nullptr;
    std::shared_ptr<Impl> m_impl = {};
};

} // namespace SDK::Signals
