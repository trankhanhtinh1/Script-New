#pragma once

// ============================================================================
// Turret — push-driven turret-attack event surface
// ============================================================================
// Subscribes to `CoreEventHook::Events::OnTurretAttack` (id 52), fired from
// the VMT-driven spell-cast dispatcher when a turret's `ActiveSpellCast`
// pointer transitions to a new cast.
//
// `sender` = AITurretClient*, `context` = SpellCastInfo*, `intParam` = slot
// (always 64 for turret AAs).
//
// The previous tick-loop scan over `AllyTurrets()` + `EnemyTurrets()` and
// per-turret `LastCastAddress` diff is gone — the dispatcher already owns
// that bookkeeping. Argument resolution (target index → AttackableUnit,
// missile bolt search, attack-end tick estimate from cast delay + travel
// time) still runs at fire-time using the same helpers as before.
// ============================================================================

#include "../../core/CoreEventHook.h"
#include "../../core/offset.h"
#include "../../menu/MenuUI.h"
#include "../Core/Game.h"
#include "../GameObjects/AITurretClient.h"
#include "../GameObjects/AttackableUnit.h"
#include "../GameObjects/MissileClient.h"
#include "../GameObjects/ObjectManager.h"

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

    bool IsValid() const { return Turret.IsValid(); }
};

using TurretHandler = void(*)(const TurretArgs&);

namespace detail {
    struct TurretState { TurretArgs LastArgs = {}; };

    inline std::unordered_map<int, TurretState>* g_states = nullptr;
    inline MenuUI::FixedList<TurretHandler, 64> g_handlers = {};
    inline bool g_registered = false;

    inline bool EnsureStorage() {
        if (!g_states) {
            g_states = new(std::nothrow) std::unordered_map<int, TurretState>();
        }
        return g_states != nullptr;
    }

    inline AttackableUnit ResolveTarget(const AITurretClient& turret) {
        const auto cast = turret.Ref().GetActiveSpellCast();
        if (!cast.IsValid()) return {};
        const int idx = cast.GetTargetIndex();
        if (idx <= 0) return {};
        const GameObject t = ObjectManager::GetByIndex(idx);
        return t.IsValid() ? AttackableUnit(t.Address()) : AttackableUnit{};
    }

    inline GameObject ResolveBolt(const AITurretClient& turret, const AttackableUnit& target) {
        if (!turret.IsValid()) return {};
        const int turretNetId = turret.NetworkId();
        const int targetNetId = target.IsValid() ? target.NetworkId() : 0;
        for (const auto& m : ObjectManager::Missiles()) {
            if (!m.IsValid()) continue;
            if (m.CasterNetworkId() != turretNetId) continue;
            if (targetNetId != 0 && m.TargetNetworkId() != targetNetId) continue;
            return m;
        }
        return {};
    }

    inline TurretArgs BuildArgs(const AITurretClient& turret) {
        TurretArgs args = {};
        args.Turret      = turret;
        args.Target      = ResolveTarget(turret);
        args.AttackStart = Game::TickCount();

        const auto cast = turret.Ref().GetActiveSpellCast();
        const float castDelaySec = cast.IsValid()
            ? std::max(cast.GetCastDelay(), turret.AttackCastDelay())
            : turret.AttackCastDelay();
        float travelMs = 0.0f;
        if (args.Target.IsValid()) {
            const float distance = turret.Distance(args.Target);
            travelMs = distance / 1200.0f * 1000.0f;
        }
        args.AttackDelay = castDelaySec * 1000.0f + travelMs;
        args.AttackEnd   = args.AttackStart + static_cast<int>(args.AttackDelay);
        args.TurretBoltObject = ResolveBolt(turret, args.Target);
        return args;
    }

    // CoreEventHook trampoline.
    inline void OnTurretAttackThunk(uintptr_t sender, uintptr_t /*context*/, int /*intParam*/) {
        if (!EnsureStorage()) return;
        AITurretClient turret(sender);
        if (!turret.IsValid() || turret.IsDead()) return;

        TurretArgs args = BuildArgs(turret);
        (*g_states)[turret.NetworkId()].LastArgs = args;

        for (const auto& h : g_handlers) {
            if (h) h(args);
        }
    }
}

inline void Initialize() {
    detail::EnsureStorage();
    if (!detail::g_registered) {
        CoreEventHook::SetCallback(Offset::Events::OnTurretAttack, detail::OnTurretAttackThunk);
        detail::g_registered = true;
    }
}

inline bool AddOnTurretAttack(TurretHandler handler) {
    Initialize();
    return handler && detail::g_handlers.push_back(handler);
}

inline bool OnTurretAttack(TurretHandler handler) {
    return AddOnTurretAttack(handler);
}

inline void Update() {
    // Push-driven — nothing to poll.
}

inline void Reset() {
    if (detail::g_states) detail::g_states->clear();
    detail::g_handlers.clear();
}

} // namespace SDK::Events::Turret
