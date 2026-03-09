#pragma once
#include <string>
#include <vector>
#include <functional>
#include <chrono>
#include <algorithm>
#include <memory>
#include <unordered_map>
#include <mutex>

// ============================================================================
// SignalManager — Event scheduling / condition-based trigger system
// Port of: EnsoulSharp.SDK/Core/Signals/Signal.cs + SignalManager.cs
//
// A Signal is a condition-based event: you provide a "waver" function that
// returns true when the signal should fire. SignalManager checks all active
// signals each tick and fires them when their condition is met or they expire.
//
// Usage:
//   // Create a signal that fires when target HP < 30%
//   auto signal = SDK::SignalManager::Create(
//       [](SDK::Signal& s) {           // onRaised callback
//           SDK::Notifications::Add("LowHP", "Target is low!", SDK::NotificationType::Warning);
//       },
//       [](SDK::Signal& s) -> bool {   // waver condition
//           return targetHP < 0.3f;
//       },
//       5.0f   // expires after 5 seconds
//   );
//
//   // In your game loop:
//   SDK::SignalManager::Update();      // Call each tick
//
//   // Manual raise:
//   signal->Raise("Manual trigger");
//
//   // Disable/Enable:
//   signal->Disable();
//   signal->Enable();
//
//   // One-shot signal (fires once then auto-removes):
//   SDK::SignalManager::Once([](SDK::Signal& s) {
//       // do something
//   }, [](SDK::Signal& s) { return someCondition; });
// ============================================================================

namespace SDK {

    // Forward declaration
    class Signal;

    // ========================================================================
    // Signal — A condition-based event with expiration
    // Source: EnsoulSharp.SDK/Core/Signals/Signal.cs
    // ========================================================================
    class Signal : public std::enable_shared_from_this<Signal> {
    public:
        // Callback types
        using OnRaisedFn   = std::function<void(Signal&)>;
        using WaverFn      = std::function<bool(Signal&)>;
        using OnExpiredFn   = std::function<void(Signal&)>;
        using OnEnabledChangedFn = std::function<void(Signal&, bool)>;

        // Properties (key-value store for attaching state)
        std::unordered_map<std::string, std::string> Properties;

        // ====================================================================
        // State accessors
        // ====================================================================
        bool IsEnabled()  const { return m_enabled; }
        bool IsRaised()   const { return m_raised; }
        bool IsExpired()  const {
            if (m_expirationSec <= 0) return false;  // No expiration
            return GetElapsed() >= m_expirationSec;
        }
        float GetExpirationTime() const { return m_expirationSec; }
        float GetElapsed() const {
            auto now = std::chrono::steady_clock::now();
            return std::chrono::duration<float>(now - m_createTime).count();
        }
        float GetLastSignaledTime() const { return m_lastSignaledTime; }

        // ====================================================================
        // Enable / Disable
        // ====================================================================
        void Enable() {
            m_enabled = true;
            if (m_onEnabledChanged) m_onEnabledChanged(*this, true);
        }

        void Disable() {
            m_enabled = false;
            if (m_onEnabledChanged) m_onEnabledChanged(*this, false);
        }

        // ====================================================================
        // Raise manually
        // ====================================================================
        void Raise(const std::string& reason = "Manual") {
            if (!m_enabled || m_raised || IsExpired()) return;
            m_raised = true;
            m_lastSignaledTime = GetElapsed();
            if (m_onRaised) m_onRaised(*this);
        }

        // ====================================================================
        // Reset (re-enable for another cycle)
        // ====================================================================
        void Reset() {
            m_enabled = true;
            m_raised = false;
            m_calledExpired = false;
            m_createTime = std::chrono::steady_clock::now();
        }

        // ====================================================================
        // Set callbacks (builder pattern)
        // ====================================================================
        Signal& OnRaised(OnRaisedFn fn)               { m_onRaised = fn; return *this; }
        Signal& OnExpired(OnExpiredFn fn)              { m_onExpired = fn; return *this; }
        Signal& OnEnabledChanged(OnEnabledChangedFn fn){ m_onEnabledChanged = fn; return *this; }

        // ====================================================================
        // Property helpers
        // ====================================================================
        void SetProperty(const std::string& key, const std::string& value) {
            Properties[key] = value;
        }
        std::string GetProperty(const std::string& key, const std::string& defaultVal = "") const {
            auto it = Properties.find(key);
            return (it != Properties.end()) ? it->second : defaultVal;
        }
        bool HasProperty(const std::string& key) const {
            return Properties.find(key) != Properties.end();
        }

    private:
        friend class SignalManager;

        Signal() = default;

        // Internal — called by SignalManager::Update()
        bool CheckWaver() {
            if (m_waver && m_waver(*this)) return true;
            return false;
        }

        void TriggerSignal() {
            if (!m_enabled || m_raised) return;
            m_raised = true;
            m_lastSignaledTime = GetElapsed();
            if (m_onRaised) m_onRaised(*this);
        }

        void TriggerExpired() {
            if (m_calledExpired) return;
            m_calledExpired = true;
            if (m_onExpired) m_onExpired(*this);
        }

        // Callbacks
        OnRaisedFn         m_onRaised;
        WaverFn            m_waver;
        OnExpiredFn        m_onExpired;
        OnEnabledChangedFn m_onEnabledChanged;

        // State
        bool  m_enabled        = true;
        bool  m_raised         = false;
        bool  m_calledExpired  = false;
        float m_expirationSec  = 0.0f;     // 0 = never expires
        float m_lastSignaledTime = 0.0f;

        std::chrono::steady_clock::time_point m_createTime = std::chrono::steady_clock::now();
    };

    // ========================================================================
    // SignalManager — Manages all active signals, checks conditions each tick
    // Source: EnsoulSharp.SDK/Core/Signals/SignalManager.cs
    // ========================================================================
    class SignalManager {
    public:
        // ====================================================================
        // Create a new signal and register it
        // ====================================================================
        static std::shared_ptr<Signal> Create(
            Signal::OnRaisedFn onRaised = nullptr,
            Signal::WaverFn   waver    = nullptr,
            float expirationSeconds    = 0.0f,      // 0 = no expiration
            Signal::OnExpiredFn onExpired = nullptr)
        {
            auto signal = std::shared_ptr<Signal>(new Signal());
            signal->m_onRaised = onRaised;
            signal->m_waver = waver;
            signal->m_expirationSec = expirationSeconds;
            signal->m_onExpired = onExpired;
            signal->m_enabled = true;
            signal->m_createTime = std::chrono::steady_clock::now();

            std::lock_guard<std::mutex> lock(s_mutex);
            s_signals.push_back(signal);
            return signal;
        }

        // ====================================================================
        // Create a one-shot signal (auto-removed after firing)
        // ====================================================================
        static std::shared_ptr<Signal> Once(
            Signal::OnRaisedFn onRaised,
            Signal::WaverFn   waver,
            float expirationSeconds = 0.0f)
        {
            return Create(onRaised, waver, expirationSeconds);
            // One-shot behavior: signals are removed after being raised in Update()
        }

        // ====================================================================
        // Add an existing signal
        // ====================================================================
        static void AddSignal(std::shared_ptr<Signal> signal) {
            if (!signal) return;
            std::lock_guard<std::mutex> lock(s_mutex);
            // Don't add duplicates
            for (auto& s : s_signals) {
                if (s == signal) return;
            }
            s_signals.push_back(signal);
        }

        // ====================================================================
        // Remove a signal
        // ====================================================================
        static void RemoveSignal(std::shared_ptr<Signal> signal) {
            if (!signal) return;
            std::lock_guard<std::mutex> lock(s_mutex);
            s_signals.erase(
                std::remove(s_signals.begin(), s_signals.end(), signal),
                s_signals.end());
        }

        // ====================================================================
        // Update — Call each game tick
        // Checks all enabled signals: fires them if waver returns true,
        // triggers expiration callback if expired
        // ====================================================================
        static void Update() {
            std::lock_guard<std::mutex> lock(s_mutex);

            std::vector<std::shared_ptr<Signal>> toRemove;

            for (auto& signal : s_signals) {
                if (!signal || !signal->m_enabled) continue;

                // Check waver condition
                if (!signal->m_raised && signal->m_waver) {
                    if (signal->CheckWaver()) {
                        signal->TriggerSignal();
                        toRemove.push_back(signal);  // One-shot: remove after firing
                        continue;
                    }
                }

                // Check expiration
                if (signal->IsExpired() && !signal->m_calledExpired) {
                    signal->TriggerExpired();
                    // Don't auto-remove expired signals — let user decide via Reset() or RemoveSignal()
                }
            }

            // Remove fired signals (one-shot behavior, matches C# SignalManager)
            for (auto& sig : toRemove) {
                s_signals.erase(
                    std::remove(s_signals.begin(), s_signals.end(), sig),
                    s_signals.end());
            }
        }

        // ====================================================================
        // Clear all signals
        // ====================================================================
        static void Clear() {
            std::lock_guard<std::mutex> lock(s_mutex);
            s_signals.clear();
        }

        // ====================================================================
        // Get count of active signals
        // ====================================================================
        static int Count() {
            std::lock_guard<std::mutex> lock(s_mutex);
            return (int)s_signals.size();
        }

        // ====================================================================
        // Convenience: Create a delayed action signal
        // Fires the callback after `delaySec` seconds
        // ====================================================================
        static std::shared_ptr<Signal> Delay(float delaySec, Signal::OnRaisedFn callback) {
            auto startTime = std::chrono::steady_clock::now();
            return Create(
                callback,
                [startTime, delaySec](Signal& s) -> bool {
                    auto now = std::chrono::steady_clock::now();
                    float elapsed = std::chrono::duration<float>(now - startTime).count();
                    return elapsed >= delaySec;
                },
                delaySec + 1.0f  // Expire slightly after delay as safety net
            );
        }

        // ====================================================================
        // Convenience: Create a repeating signal
        // Fires every `intervalSec` seconds, up to `maxCount` times (0 = infinite)
        // ====================================================================
        static std::shared_ptr<Signal> Repeat(
            float intervalSec,
            Signal::OnRaisedFn callback,
            int maxCount = 0,
            float expirationSec = 0.0f)
        {
            auto counter = std::make_shared<int>(0);
            auto lastFired = std::make_shared<std::chrono::steady_clock::time_point>(
                std::chrono::steady_clock::now());

            auto signal = Create(
                [callback, counter, maxCount](Signal& s) {
                    if (callback) callback(s);
                    (*counter)++;
                    s.m_raised = false;  // Allow re-firing
                },
                [lastFired, intervalSec, counter, maxCount](Signal& s) -> bool {
                    if (maxCount > 0 && *counter >= maxCount) return false;
                    auto now = std::chrono::steady_clock::now();
                    float elapsed = std::chrono::duration<float>(now - *lastFired).count();
                    if (elapsed >= intervalSec) {
                        *lastFired = now;
                        return true;
                    }
                    return false;
                },
                expirationSec
            );

            return signal;
        }

    private:
        static inline std::vector<std::shared_ptr<Signal>> s_signals;
        static inline std::mutex s_mutex;
    };

} // namespace SDK
