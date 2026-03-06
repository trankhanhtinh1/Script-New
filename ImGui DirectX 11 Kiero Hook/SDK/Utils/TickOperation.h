#pragma once
// ============================================================================
// TickOperation.h — Throttled periodic action execution
// Ported from EnsoulSharp.SDK/Core/Utils/TickOperation.cs
// ============================================================================

#include <functional>
#include <vector>
#include <algorithm>
#include <Windows.h>

namespace SDK {

    // ========================================================================
    // TickOperation — executes an action every N milliseconds
    //   Call TickOperation::UpdateAll() once per frame to process all operations
    // ========================================================================
    class TickOperation {
    public:
        using Action = std::function<void()>;

        // Create a new tick operation
        // tickDelayMs: delay between executions in milliseconds
        // action:      function to call
        // runOnce:     if true, runs immediately on first Update
        TickOperation(int tickDelayMs, Action action, bool runOnce = false)
            : m_tickDelay(tickDelayMs)
            , m_action(std::move(action))
            , m_isRunning(true)
        {
            DWORD now = static_cast<DWORD>(GetTickCount64());
            m_nextTick = runOnce ? now : (now + tickDelayMs);

            // Auto-register
            GetOperations().push_back(this);
        }

        ~TickOperation() {
            Stop();
            // Remove from global list
            auto& ops = GetOperations();
            ops.erase(std::remove(ops.begin(), ops.end(), this), ops.end());
        }

        // Non-copyable
        TickOperation(const TickOperation&) = delete;
        TickOperation& operator=(const TickOperation&) = delete;

        // Move OK
        TickOperation(TickOperation&& other) noexcept
            : m_tickDelay(other.m_tickDelay)
            , m_nextTick(other.m_nextTick)
            , m_action(std::move(other.m_action))
            , m_isRunning(other.m_isRunning)
        {
            // Replace in global list
            auto& ops = GetOperations();
            for (auto& op : ops) {
                if (op == &other) { op = this; break; }
            }
            other.m_isRunning = false;
        }

        // Start the operation (if stopped)
        void Start(bool runOnce = false) {
            if (!m_isRunning) {
                m_isRunning = true;
                DWORD now = static_cast<DWORD>(GetTickCount64());
                m_nextTick = runOnce ? now : (now + m_tickDelay);
            }
        }

        // Stop the operation
        void Stop() {
            m_isRunning = false;
        }

        // Check if running
        bool IsRunning() const { return m_isRunning; }

        // Get/set tick delay
        int  GetTickDelay() const { return m_tickDelay; }
        void SetTickDelay(int ms) { m_tickDelay = ms; }

        // Set a new action
        void SetAction(Action action) { m_action = std::move(action); }

        // ====================================================================
        // Static: Update all registered TickOperations (call once per frame)
        // ====================================================================
        static void UpdateAll() {
            DWORD now = static_cast<DWORD>(GetTickCount64());
            for (auto* op : GetOperations()) {
                if (op && op->m_isRunning && op->m_action) {
                    if (now >= op->m_nextTick) {
                        op->m_action();
                        op->m_nextTick = now + op->m_tickDelay;
                    }
                }
            }
        }

    private:
        int      m_tickDelay;   // ms between executions
        DWORD    m_nextTick;    // next tick to execute
        Action   m_action;      // function to call
        bool     m_isRunning;   // is this operation active

        // Global registry of all active tick operations
        static std::vector<TickOperation*>& GetOperations() {
            static std::vector<TickOperation*> ops;
            return ops;
        }
    };

} // namespace SDK
