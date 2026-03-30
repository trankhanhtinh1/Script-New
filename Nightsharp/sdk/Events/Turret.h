#pragma once

#include "../../menu/MenuUI.h"
#include "../Core/Game.h"
#include "../Core/Objects.h"
#include "../GameObjects/AttackableUnit.h"

#include <algorithm>
#include <new>
#include <unordered_map>

namespace SDK::Events::Turret {

struct TurretArgs {
    float AttackDelay = 0.0f;
    int AttackEnd = 0;
    int AttackStart = 0;
    AttackableUnit Target = {};
    AITurretClient Turret = {};
    GameObject TurretBoltObject = {};

    bool IsWindingUp() const {
        return Turret.IsValid() && Turret.IsWindingUp();
    }

    bool IsValid() const {
        return Turret.IsValid();
    }
};

using TurretHandler = void(*)(const TurretArgs&);

namespace detail {
    struct TurretState {
        uintptr_t LastCastAddress = 0;
        int LastTargetNetId = 0;
        TurretArgs LastArgs = {};
    };

    inline std::unordered_map<int, TurretState>* g_turretStates = nullptr;
    inline MenuUI::FixedList<TurretHandler, 64> g_turretHandlers = {};

    inline bool EnsureStorage() {
        if (!g_turretStates) {
            g_turretStates = new(std::nothrow) std::unordered_map<int, TurretState>();
        }
        return g_turretStates != nullptr;
    }

    inline AttackableUnit ResolveTarget(const AITurretClient& turret) {
        const auto cast = turret.Ref().GetActiveSpellCast();
        if (!cast.IsValid()) {
            return {};
        }

        const int targetIndex = cast.GetTargetIndex();
        if (targetIndex <= 0) {
            return {};
        }

        const GameObject target = ObjectManager::GetByIndex(targetIndex);
        return target.IsValid() ? AttackableUnit(target.Address()) : AttackableUnit{};
    }

    inline GameObject ResolveBolt(const AITurretClient& turret, const AttackableUnit& target) {
        if (!turret.IsValid()) {
            return {};
        }

        const int turretNetId = turret.NetworkId();
        const int targetNetId = target.IsValid() ? target.NetworkId() : 0;
        for (const auto& missile : ObjectManager::Missiles()) {
            if (!missile.IsValid()) {
                continue;
            }
            if (missile.CasterNetworkId() != turretNetId) {
                continue;
            }
            if (targetNetId != 0 && missile.TargetNetworkId() != targetNetId) {
                continue;
            }
            return missile;
        }

        return {};
    }
}

inline void Initialize() {
    detail::EnsureStorage();
}

inline bool AddOnTurretAttack(TurretHandler handler) {
    return handler && detail::g_turretHandlers.push_back(handler);
}

inline bool OnTurretAttack(TurretHandler handler) {
    return AddOnTurretAttack(handler);
}

inline void Update() {
    if (!detail::EnsureStorage()) {
        return;
    }

    auto processTurret = [](const AITurretClient& turret) {
        const int netId = turret.NetworkId();
        if (!turret.IsValid() || netId == 0 || turret.IsDead()) {
            if (netId != 0) {
                detail::g_turretStates->erase(netId);
            }
            return;
        }

        const auto cast = turret.Ref().GetActiveSpellCast();
        if (!cast.IsValid()) {
            return;
        }

        const AttackableUnit target = detail::ResolveTarget(turret);
        const int targetNetId = target.IsValid() ? target.NetworkId() : 0;
        auto& state = (*detail::g_turretStates)[netId];
        if (state.LastCastAddress == cast.address && state.LastTargetNetId == targetNetId) {
            return;
        }

        TurretArgs args = {};
        args.Turret = turret;
        args.Target = target;
        args.AttackStart = Game::TickCount();
        const float castDelaySec = std::max(cast.GetCastDelay(), turret.AttackCastDelay());
        float travelMs = 0.0f;
        if (target.IsValid()) {
            const float distance = turret.Distance(target);
            travelMs = distance / 1200.0f * 1000.0f;
        }
        args.AttackDelay = castDelaySec * 1000.0f + travelMs;
        args.AttackEnd = args.AttackStart + static_cast<int>(args.AttackDelay);
        args.TurretBoltObject = detail::ResolveBolt(turret, target);

        state.LastCastAddress = cast.address;
        state.LastTargetNetId = targetNetId;
        state.LastArgs = args;

        for (const auto& handler : detail::g_turretHandlers) {
            if (handler) {
                handler(args);
            }
        }
    };

    for (const auto& turret : ObjectManager::AllyTurrets()) {
        processTurret(turret);
    }
    for (const auto& turret : ObjectManager::EnemyTurrets()) {
        processTurret(turret);
    }
}

inline void Reset() {
    if (detail::g_turretStates) {
        detail::g_turretStates->clear();
    }
    detail::g_turretHandlers.clear();
}

} // namespace SDK::Events::Turret
