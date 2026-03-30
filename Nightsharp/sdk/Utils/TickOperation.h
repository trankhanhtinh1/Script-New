#pragma once

#include "../Core/Game.h"

#include <algorithm>
#include <functional>
#include <new>
#include <vector>

namespace SDK::Utils {

class TickOperation {
public:
    TickOperation(int tickDelay, std::function<void()> action, bool runOnce = false)
        : Action(std::move(action))
        , IsRunning(true)
        , TickDelay(tickDelay)
        , m_nextTick(runOnce ? Variables::TickCount() : Variables::TickCount() + tickDelay) {
        Initialize();
        Register(this);
    }

    ~TickOperation() {
        Dispose();
    }

    TickOperation* Start(bool runOnce = false) {
        if (!IsRunning) {
            IsRunning = true;
            m_nextTick = runOnce ? Variables::TickCount() : Variables::TickCount() + TickDelay;
            Register(this);
        }
        return this;
    }

    TickOperation* Stop() {
        if (IsRunning) {
            IsRunning = false;
            Unregister(this);
        }
        return this;
    }

    void Dispose() {
        if (!m_disposed) {
            m_disposed = true;
            Stop();
            Action = {};
            TickDelay = 0;
            m_nextTick = 0;
        }
    }

    static void Initialize() {
        if (detail::g_initialized) {
            return;
        }
        detail::g_initialized = true;
        Game::OnUpdate(DispatchUpdate);
    }

    static void Reset() {
        if (auto* ops = detail::Operations()) {
            ops->clear();
        }
    }

    std::function<void()> Action = {};
    bool IsRunning = false;
    int TickDelay = 0;

private:
    struct detail {
        static inline std::vector<TickOperation*>*& Operations() {
            static auto* storage = new(std::nothrow) std::vector<TickOperation*>();
            return storage;
        }
        static inline bool g_initialized = false;
    };

    static void Register(TickOperation* operation) {
        auto* ops = detail::Operations();
        if (!ops || !operation) {
            return;
        }
        if (std::find(ops->begin(), ops->end(), operation) == ops->end()) {
            ops->push_back(operation);
        }
    }

    static void Unregister(TickOperation* operation) {
        auto* ops = detail::Operations();
        if (!ops) {
            return;
        }
        ops->erase(std::remove(ops->begin(), ops->end(), operation), ops->end());
    }

    static void DispatchUpdate() {
        auto* ops = detail::Operations();
        if (!ops) {
            return;
        }

        const int now = Variables::TickCount();
        for (auto* operation : *ops) {
            if (!operation || !operation->IsRunning || !operation->Action) {
                continue;
            }
            if (operation->m_nextTick <= now) {
                operation->Action();
                operation->m_nextTick = now + operation->TickDelay;
            }
        }
    }

    int m_nextTick = 0;
    bool m_disposed = false;
};

} // namespace SDK::Utils

