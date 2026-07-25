#pragma once

#include "Events.h"
#include "../GameObjects/GameObjects.h"
#include "../GameObjects/ObjectManager.h"
#include "../../Core/CoreControl.h"

#include <algorithm>
#include <cmath>

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
    inline SDK::Events::detail::EventList<TurretArgs> TurretHandlers{ "TurretAttack" };
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

    inline float ResolveMissileSpeed(const ::Core::Events::ProcessSpellEventArgs& args) {
        if (std::isfinite(args.MissileSpeed) && args.MissileSpeed > 0.0f) {
            return args.MissileSpeed;
        }
        return 1200.0f;
    }

    inline void SeedFromGameObjects() {
        // REMOVED: Turret/Inhibitor/Nexus disabled by user request
        /*
        for (const auto& turret : SDK::GameObjects::Turrets()) {
            if (!turret.IsValid()) {
                continue;
            }

            const uint32_t key = static_cast<uint32_t>(turret.NetworkId());
            auto* entry = Find(key, true);
            if (!entry) {
                continue;
            }

            entry->Turret = turret.Address();
            entry->NetworkId = key;
            entry->IsWindingUp = turret.Spellbook().IsWindingUp();
        }
        */
    }
} // namespace detail

// REMOVED: Turret/Inhibitor/Nexus disabled by user request
/*inline bool AddOnTurretAttack(TurretHandler handler) {
    if (!handler) {
        return false;
    }

    SDK::Events::Initialize();
    if (!SDK::Events::detail::EnsureDoCastRawSubscribed()) {
        return false;
    }

    const bool hadHandlers = detail::TurretHandlers.HasHandlers();
    const bool added = detail::TurretHandlers.Add(handler);
    if (added && !hadHandlers) {
        ++SDK::Events::detail::TurretConsumerRefs;
    } else if (!added && !hadHandlers) {
        SDK::Events::detail::ReleaseDoCastRawIfUnused();
    }
    return added;
}*/

// REMOVED: Turret/Inhibitor/Nexus disabled by user request
/*inline bool RemoveOnTurretAttack(TurretHandler handler) {
    const bool removed = detail::TurretHandlers.Remove(handler);
    if (removed && !detail::TurretHandlers.HasHandlers()) {
        if (SDK::Events::detail::TurretConsumerRefs > 0) {
            --SDK::Events::detail::TurretConsumerRefs;
        }
        SDK::Events::detail::ReleaseDoCastRawIfUnused();
    }
    return removed;
}*/

// REMOVED: Turret/Inhibitor/Nexus disabled by user request
/*inline bool OnTurretAttack(TurretHandler handler) {
    return AddOnTurretAttack(handler);
}*/

} // namespace SDK::Events::Turret

namespace SDK::Events {
    // REMOVED: Turret/Inhibitor/Nexus disabled by user request
    /*
    inline bool AddOnTurretAttack(Turret::TurretHandler handler) { return Turret::AddOnTurretAttack(handler); }
    inline bool RemoveOnTurretAttack(Turret::TurretHandler handler) { return Turret::RemoveOnTurretAttack(handler); }
    inline bool OnTurretAttack(Turret::TurretHandler handler) { return Turret::OnTurretAttack(handler); }
    */

namespace detail {
// REMOVED: Turret/Inhibitor/Nexus disabled by user request
/*inline void EventTurret(const ProcessSpellEventArgs& args) {
        if (args.Sender.Type != ::Core::Objects::ObjectType::AITurretClient ||
            !args.IsAutoAttack ||
            args.TargetNetworkId == 0 ||
            args.TargetNetworkId == 0xFFFFFFFFu) {
            return;
        }

        const uint32_t key = Turret::detail::KeyFor(args.Sender);
        auto* turret = Turret::detail::Find(key, true);
        if (!turret) {
            return;
        }

        const AITurretClient turretObject(args.Sender.Ptr);
        const AttackableUnit target =
            ObjectManager::GetUnitByNetworkId<AttackableUnit>(
                static_cast<int>(args.TargetNetworkId));

        turret->Turret = args.Sender.Ptr;
        turret->NetworkId = key;
        turret->TargetNetworkId = args.TargetNetworkId;
        turret->AttackStart = Turret::detail::TickCount();
        turret->AttackDelay = CoreControl::GetAttackWindupMs(args.Sender.Ptr);
        if (target.IsValid()) {
            const float missileSpeed = Turret::detail::ResolveMissileSpeed(args);
            turret->AttackDelay +=
                turretObject.Distance(target) / missileSpeed * 1000.0f;
        }
        turret->AttackEnd = static_cast<int>(
            turret->AttackStart + std::max(0.0f, turret->AttackDelay));
        turret->IsWindingUp = turretObject.Spellbook().IsWindingUp();
        turret->Raw = args;

        Turret::detail::TurretHandlers.Fire(*turret);
    }*/

    inline void EventTurretConstruct() {
        Turret::detail::SeedFromGameObjects();
    }
} // namespace detail
} // namespace SDK::Events
