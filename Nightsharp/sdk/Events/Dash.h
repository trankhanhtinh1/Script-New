#pragma once

#include "../../menu/MenuUI.h"
#include "../Core/Game.h"
#include "../Core/Objects.h"

#include <new>
#include <unordered_map>
#include <vector>

namespace SDK::Events::Dash {

struct DashArgs {
    int Duration = 0;
    Vector2 EndPos = {};
    int EndTick = 0;
    bool IsBlink = false;
    std::vector<Vector2> Path = {};
    float Speed = 0.0f;
    Vector2 StartPos = {};
    int StartTick = 0;
    AIBaseClient Unit = {};

    bool IsValid() const {
        return Unit.IsValid() && EndTick > StartTick;
    }
};

using DashHandler = void(*)(const DashArgs&);

namespace detail {
    struct DashState {
        DashArgs Args = {};
        Vector2 LastEnd = {};
    };

    inline std::unordered_map<int, DashState>* g_dashStates = nullptr;
    inline MenuUI::FixedList<DashHandler, 64> g_dashHandlers = {};

    inline bool EnsureDashStorage() {
        if (!g_dashStates) {
            g_dashStates = new(std::nothrow) std::unordered_map<int, DashState>();
        }
        return g_dashStates != nullptr;
    }

    inline Vector2 ResolveDashStart(const AIBaseClient& hero) {
        Vector3 start = hero.PreviousPosition();
        if (start.IsZero()) {
            start = hero.Position();
        }
        return start.To2D();
    }

    inline Vector2 ResolveDashEnd(const AIBaseClient& hero) {
        Vector3 end = hero.PathEnd();
        if (end.IsZero()) {
            end = hero.OrderPosition();
        }
        if (end.IsZero()) {
            end = hero.Position();
        }
        return end.To2D();
    }

    inline float ResolveDashSpeed(const AIBaseClient& hero) {
        const Vector3 velocity = hero.Velocity();
        const float speed = velocity.Length2D();
        if (speed > 50.0f) {
            return speed;
        }

        const float moveSpeed = hero.MoveSpeed();
        return moveSpeed > 0.0f ? moveSpeed * 2.0f : 0.0f;
    }

    inline bool DashPathChanged(const DashState& state, const DashArgs& next) {
        return state.Args.EndTick == 0 ||
               state.Args.EndPos.Distance(next.EndPos) > 20.0f ||
               state.Args.StartPos.Distance(next.StartPos) > 20.0f;
    }
}

inline void Initialize() {
    detail::EnsureDashStorage();
}

inline bool AddOnDash(DashHandler handler) {
    return handler && detail::g_dashHandlers.push_back(handler);
}

inline bool OnDash(DashHandler handler) {
    return AddOnDash(handler);
}

inline DashArgs GetDashInfo(const AIBaseClient& unit) {
    if (!detail::g_dashStates || !unit.IsValid()) {
        return {};
    }

    const auto it = detail::g_dashStates->find(unit.NetworkId());
    return (it != detail::g_dashStates->end()) ? it->second.Args : DashArgs{};
}

inline bool IsDashing(const AIBaseClient& unit) {
    const DashArgs info = GetDashInfo(unit);
    return info.IsValid() && Game::TickCount() <= info.EndTick;
}

inline void Update() {
    if (!detail::EnsureDashStorage()) {
        return;
    }

    const int now = Game::TickCount();
    for (const auto& hero : ObjectManager::Heroes()) {
        if (!hero.IsValid() || hero.IsDead()) {
            if (hero.NetworkId() != 0) {
                detail::g_dashStates->erase(hero.NetworkId());
            }
            continue;
        }

        if (!hero.IsDashing()) {
            detail::g_dashStates->erase(hero.NetworkId());
            continue;
        }

        DashArgs next = {};
        next.Unit = hero;
        next.StartPos = detail::ResolveDashStart(hero);
        next.EndPos = detail::ResolveDashEnd(hero);
        next.Speed = detail::ResolveDashSpeed(hero);
        next.StartTick = now - (Game::Ping() / 2);
        const float distance = next.StartPos.Distance(next.EndPos);
        next.Duration = (next.Speed > 1.0f) ? static_cast<int>((distance / next.Speed) * 1000.0f) : 0;
        if (next.Duration <= 0) {
            next.Duration = 50;
        }
        next.EndTick = next.StartTick + next.Duration;
        next.IsBlink = next.Speed > 5000.0f || next.Duration <= 10;
        next.Path.clear();
        next.Path.push_back(next.StartPos);
        next.Path.push_back(next.EndPos);

        auto& state = (*detail::g_dashStates)[hero.NetworkId()];
        if (detail::DashPathChanged(state, next)) {
            state.Args = next;
            state.LastEnd = next.EndPos;

            for (const auto& handler : detail::g_dashHandlers) {
                if (handler) {
                    handler(state.Args);
                }
            }
        } else {
            state.Args.EndTick = next.EndTick;
            state.Args.Duration = next.Duration;
            state.Args.Speed = next.Speed;
        }
    }
}

inline void Reset() {
    if (detail::g_dashStates) {
        detail::g_dashStates->clear();
    }
    detail::g_dashHandlers.clear();
}

} // namespace SDK::Events::Dash
