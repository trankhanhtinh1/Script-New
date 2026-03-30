#pragma once

#include "../../menu/MenuUI.h"
#include "../Core/Game.h"
#include "../Core/Objects.h"

#include <new>
#include <unordered_map>

namespace SDK::Events::Stealth {

struct OnStealthEventArgs {
    bool IsStealthed = false;
    Vector3 LastPosition = {};
    AIHeroClient Sender = {};
    float Time = 0.0f;

    bool IsValid() const {
        return Sender.IsValid();
    }
};

using StealthHandler = void(*)(const OnStealthEventArgs&);

namespace detail {
    struct StealthState {
        bool Initialized = false;
        bool WasVisible = true;
        Vector3 LastVisiblePosition = {};
        float StealthStartTime = 0.0f;
    };

    inline std::unordered_map<int, StealthState>* g_stealthStates = nullptr;
    inline MenuUI::FixedList<StealthHandler, 64> g_stealthHandlers = {};

    inline bool EnsureStealthStorage() {
        if (!g_stealthStates) {
            g_stealthStates = new(std::nothrow) std::unordered_map<int, StealthState>();
        }
        return g_stealthStates != nullptr;
    }
}

inline void Initialize() {
    detail::EnsureStealthStorage();
}

inline bool AddOnStealth(StealthHandler handler) {
    return handler && detail::g_stealthHandlers.push_back(handler);
}

inline bool OnStealth(StealthHandler handler) {
    return AddOnStealth(handler);
}

inline Vector3 GetLastVisiblePosition(const AIHeroClient& hero) {
    if (!detail::g_stealthStates || !hero.IsValid()) {
        return {};
    }

    const auto it = detail::g_stealthStates->find(hero.NetworkId());
    return (it != detail::g_stealthStates->end()) ? it->second.LastVisiblePosition : Vector3{};
}

inline float GetStealthDuration(const AIHeroClient& hero) {
    if (!detail::g_stealthStates || !hero.IsValid()) {
        return 0.0f;
    }

    const auto it = detail::g_stealthStates->find(hero.NetworkId());
    if (it == detail::g_stealthStates->end() || it->second.WasVisible) {
        return 0.0f;
    }

    return Game::Time() - it->second.StealthStartTime;
}

inline void Update() {
    if (!detail::EnsureStealthStorage()) {
        return;
    }

    const float now = Game::Time();
    for (const auto& hero : ObjectManager::Heroes()) {
        const int netId = hero.NetworkId();
        if (!hero.IsValid() || netId == 0 || hero.IsDead()) {
            if (netId != 0) {
                detail::g_stealthStates->erase(netId);
            }
            continue;
        }

        const bool isVisible = hero.IsVisible();
        auto& state = (*detail::g_stealthStates)[netId];
        if (!state.Initialized) {
            state.Initialized = true;
            state.WasVisible = isVisible;
            state.LastVisiblePosition = hero.Position();
            continue;
        }

        if (state.WasVisible && !isVisible) {
            state.StealthStartTime = now;

            OnStealthEventArgs args = {};
            args.Sender = hero;
            args.IsStealthed = true;
            args.Time = now;
            args.LastPosition = state.LastVisiblePosition;

            for (const auto& handler : detail::g_stealthHandlers) {
                if (handler) {
                    handler(args);
                }
            }
        } else if (!state.WasVisible && isVisible) {
            OnStealthEventArgs args = {};
            args.Sender = hero;
            args.IsStealthed = false;
            args.Time = now;
            args.LastPosition = hero.Position();

            for (const auto& handler : detail::g_stealthHandlers) {
                if (handler) {
                    handler(args);
                }
            }
        }

        state.WasVisible = isVisible;
        if (isVisible) {
            state.LastVisiblePosition = hero.Position();
        }
    }
}

inline void Reset() {
    if (detail::g_stealthStates) {
        detail::g_stealthStates->clear();
    }
    detail::g_stealthHandlers.clear();
}

} // namespace SDK::Events::Stealth
