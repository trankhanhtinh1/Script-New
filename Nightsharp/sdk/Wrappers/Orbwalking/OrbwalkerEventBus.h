#pragma once

#include "OrbwalkerTypes.h"

#include <Windows.h>

#include <string>
#include <unordered_map>

namespace SDK::OrbwalkingDetail {

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
inline OrbwalkerBase* RuntimeInstance = nullptr;
inline IOrbwalker* Implementation = nullptr;
inline std::unordered_map<std::string, IOrbwalker*> Implementations;
inline std::string SelectedImplementationName = "SDK";

inline void FireBeforeAttack(OrbwalkingActionArgs& args) { BeforeAttackHandlers.Fire(args); }
inline void FireOnAttack(OrbwalkingActionArgs& args) { AttackHandlers.Fire(args); }
inline void FireAfterAttack(OrbwalkingActionArgs& args) { AfterAttackHandlers.Fire(args); }
inline void FireBeforeMove(OrbwalkingActionArgs& args) { BeforeMoveHandlers.Fire(args); }

} // namespace SDK::OrbwalkingDetail
