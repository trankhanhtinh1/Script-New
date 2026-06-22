#pragma once

#include "../Events/Events.h"
#include "../Core/Variables.h"

#include <algorithm>
#include <functional>
#include <utility>
#include <vector>

namespace SDK::Core::Utils {

class TickOperation {
public:
    TickOperation(int tickDelay, std::function<void()> action, bool runOnce = false)
        : Action(std::move(action)),
          IsRunning(true),
          TickDelay(tickDelay),
          nextTick_(runOnce ? SDK::Variables::TickCount() : SDK::Variables::TickCount() + tickDelay) {
        Register(this);
    }

    ~TickOperation() {
        Dispose();
    }

    void Dispose() {
        if (IsRunning) {
            Unregister(this);
        }
        Action = {};
        TickDelay = 0;
        nextTick_ = 0;
        IsRunning = false;
    }

    TickOperation& Start(bool runOnce = false) {
        if (!IsRunning) {
            nextTick_ = runOnce ? SDK::Variables::TickCount() : SDK::Variables::TickCount() + TickDelay;
            IsRunning = true;
            Register(this);
        }
        return *this;
    }

    TickOperation& Stop() {
        if (IsRunning) {
            Unregister(this);
            IsRunning = false;
        }
        return *this;
    }

    std::function<void()> Action = {};
    bool IsRunning = false;
    int TickDelay = 0;

private:
    int nextTick_ = 0;

    static std::vector<TickOperation*>& Operations() {
        static std::vector<TickOperation*> operations;
        return operations;
    }

    static void Register(TickOperation* operation) {
        EnsureInstalled();
        auto& operations = Operations();
        if (std::find(operations.begin(), operations.end(), operation) == operations.end()) {
            operations.push_back(operation);
        }
    }

    static void Unregister(TickOperation* operation) {
        auto& operations = Operations();
        operations.erase(std::remove(operations.begin(), operations.end(), operation), operations.end());
    }

    static void OnUpdate(const SDK::Events::GameUpdateEventArgs&) {
        const int now = SDK::Variables::TickCount();
        auto snapshot = Operations();
        for (TickOperation* operation : snapshot) {
            if (!operation || !operation->IsRunning || operation->nextTick_ > now) {
                continue;
            }
            if (operation->Action) {
                try {
                    operation->Action();
                } catch (...) {}
            }
            operation->nextTick_ = SDK::Variables::TickCount() + operation->TickDelay;
        }
    }

    static void EnsureInstalled() {
        static bool installed = false;
        if (!installed) {
            installed = true;
            SDK::Events::AddOnGameUpdate(&OnUpdate);
        }
    }
};

} // namespace SDK::Core::Utils

namespace SDK::Utils {
    using TickOperation = ::SDK::Core::Utils::TickOperation;
} // namespace SDK::Utils
