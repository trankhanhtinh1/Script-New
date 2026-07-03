#pragma once

#include "Events.h"

namespace SDK::Events::Turret {

struct TurretArgs {
    uintptr_t Turret = 0;
    uint32_t NetworkId = 0;
    uint32_t TargetNetworkId = 0;
    uintptr_t TurretBoltObject = 0;
    float AttackDelay = 0.0f;
    int AttackStart = 0;
    int AttackEnd = 0;
    bool IsWindingUp = false;
    ::Core::Events::ProcessSpellEventArgs Raw = {};
};

using TurretHandler = void(*)(const TurretArgs&);

namespace detail {
    inline constexpr int MaxTurrets = 64;
    inline SDK::Events::detail::EventList<TurretArgs> TurretHandlers;
    inline TurretArgs Turrets[MaxTurrets] = {};
    inline int TurretCount = 0;

    inline int TickCount() {
        return static_cast<int>(GetTickCount64() & 0x7FFFFFFF);
    }

    inline uint32_t KeyFor(const ::Core::Events::ObjectInfo& object) {
        return object.NetworkId ? object.NetworkId : static_cast<uint32_t>(object.Ptr & 0xFFFFFFFFu);
    }

    inline TurretArgs* Find(uint32_t networkId, bool create) {
        if (!networkId) {
            return nullptr;
        }

        for (int i = 0; i < TurretCount; ++i) {
            if (Turrets[i].NetworkId == networkId) {
                return &Turrets[i];
            }
        }

        if (!create || TurretCount >= MaxTurrets) {
            return nullptr;
        }

        TurretArgs& entry = Turrets[TurretCount++];
        entry = {};
        entry.NetworkId = networkId;
        return &entry;
    }
} // namespace detail

inline bool AddOnTurretAttack(TurretHandler handler) {
    SDK::Events::Initialize();
    return detail::TurretHandlers.Add(handler);
}

inline bool RemoveOnTurretAttack(TurretHandler handler) {
    return detail::TurretHandlers.Remove(handler);
}

inline bool OnTurretAttack(TurretHandler handler) {
    return AddOnTurretAttack(handler);
}

} // namespace SDK::Events::Turret

namespace SDK::Events {
    inline bool AddOnTurretAttack(Turret::TurretHandler handler) { return Turret::AddOnTurretAttack(handler); }
    inline bool RemoveOnTurretAttack(Turret::TurretHandler handler) { return Turret::RemoveOnTurretAttack(handler); }
    inline bool OnTurretAttack(Turret::TurretHandler handler) { return Turret::OnTurretAttack(handler); }

namespace detail {
    inline void EventTurret(const ProcessSpellEventArgs& args) {
        const uint32_t key = Turret::detail::KeyFor(args.Sender);
        auto* turret = Turret::detail::Find(key, true);
        if (!turret) {
            return;
        }

        // TODO(EnsoulSharp parity): only fire for real turret basic attacks and
        // fill attack delay/windup from spell data. Waiting on unit type checks,
        // spell basic-attack classification, attack cast delay, and attack delay.
        turret->Turret = args.Sender.Ptr;
        turret->NetworkId = key;
        turret->TargetNetworkId = args.TargetNetworkId;
        turret->AttackStart = Turret::detail::TickCount();
        turret->AttackDelay = 0.0f;
        turret->AttackEnd = 0;
        turret->IsWindingUp = true;
        turret->Raw = args;

        if (args.TargetNetworkId != 0 && args.TargetNetworkId != 0xFFFFFFFFu) {
            Turret::detail::TurretHandlers.Fire(*turret);
        }
    }

    inline void EventTurretConstruct() {
        // TODO(EnsoulSharp parity): EnsoulSharp initializes this from OnLoad by
        // enumerating GameObjects.Turrets. Waiting on ObjectManager::Turrets and
        // AITurretClient wrappers; entries are lazily created from raw OnDoCast.
    }
} // namespace detail
} // namespace SDK::Events
