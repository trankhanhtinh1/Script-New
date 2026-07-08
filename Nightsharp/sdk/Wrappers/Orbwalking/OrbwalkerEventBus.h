#pragma once

#include "OrbwalkerTypes.h"

#include <Windows.h>

#include <string>
#include <unordered_map>

namespace SDK::OrbwalkingDetail {

inline bool FunctionInModule(const void* function, HMODULE module) {
    if (!function || !module) {
        return false;
    }

    MEMORY_BASIC_INFORMATION mbi = {};
    return VirtualQuery(function, &mbi, sizeof(mbi)) &&
           mbi.AllocationBase == module;
}

template <typename T, int MaxHandlers = 32>
class EventList {
public:
    using Handler = void(*)(T&);

    bool Add(Handler handler) {
        if (!handler) {
            return false;
        }
        for (int i = 0; i < count_; ++i) {
            if (handlers_[i] == handler) {
                return true;
            }
        }
        if (count_ >= MaxHandlers) {
            return false;
        }
        handlers_[count_++] = handler;
        return true;
    }

    bool Remove(Handler handler) {
        if (!handler) {
            return false;
        }
        for (int i = 0; i < count_; ++i) {
            if (handlers_[i] != handler) {
                continue;
            }
            for (int j = i; j + 1 < count_; ++j) {
                handlers_[j] = handlers_[j + 1];
            }
            handlers_[--count_] = nullptr;
            return true;
        }
        return false;
    }

    void RemoveAt(int index) {
        if (index < 0 || index >= count_) {
            return;
        }
        for (int j = index; j + 1 < count_; ++j) {
            handlers_[j] = handlers_[j + 1];
        }
        handlers_[--count_] = nullptr;
    }

    int RemoveHandlersInModule(HMODULE module) {
        if (!module) {
            return 0;
        }

        int removed = 0;
        for (int i = 0; i < count_;) {
            Handler handler = handlers_[i];
            if (!handler || FunctionInModule(reinterpret_cast<const void*>(handler), module)) {
                RemoveAt(i);
                ++removed;
                continue;
            }
            ++i;
        }
        return removed;
    }

    void Fire(T& args) const {
        for (int i = 0; i < count_; ++i) {
            Handler handler = handlers_[i];
            if (!handler) {
                continue;
            }
            __try {
                handler(args);
            } __except (1) {}
        }
    }

private:
    Handler handlers_[MaxHandlers] = {};
    int count_ = 0;
};

inline EventList<OrbwalkingActionArgs> BeforeAttackHandlers;
inline EventList<OrbwalkingActionArgs> AttackHandlers;
inline EventList<OrbwalkingActionArgs> AfterAttackHandlers;
inline EventList<OrbwalkingActionArgs> BeforeMoveHandlers;
inline EventList<OrbwalkingActionArgs> NonKillableMinionHandlers;
inline OrbwalkerBase* RuntimeInstance = nullptr;
inline IOrbwalker* Implementation = nullptr;
inline std::unordered_map<std::string, IOrbwalker*> Implementations;
inline std::string SelectedImplementationName = "SDK";

inline void FireBeforeAttack(OrbwalkingActionArgs& args) { BeforeAttackHandlers.Fire(args); }
inline void FireOnAttack(OrbwalkingActionArgs& args) { AttackHandlers.Fire(args); }
inline void FireAfterAttack(OrbwalkingActionArgs& args) { AfterAttackHandlers.Fire(args); }
inline void FireBeforeMove(OrbwalkingActionArgs& args) { BeforeMoveHandlers.Fire(args); }
inline void FireNonKillableMinion(OrbwalkingActionArgs& args) { NonKillableMinionHandlers.Fire(args); }

inline int RemoveHandlersFromModule(HMODULE module) {
    int removed = 0;
    removed += BeforeAttackHandlers.RemoveHandlersInModule(module);
    removed += AttackHandlers.RemoveHandlersInModule(module);
    removed += AfterAttackHandlers.RemoveHandlersInModule(module);
    removed += BeforeMoveHandlers.RemoveHandlersInModule(module);
    removed += NonKillableMinionHandlers.RemoveHandlersInModule(module);
    return removed;
}

} // namespace SDK::OrbwalkingDetail
