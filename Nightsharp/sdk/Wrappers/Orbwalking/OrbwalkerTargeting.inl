#pragma once

#include "../../GameObjects/ObjectManager.h"

namespace SDK::OrbwalkingDetail {

inline bool IsValidAttackTarget(const AttackableUnit& target, float range = FLT_MAX) {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || !target.IsValid()) {
        return false;
    }
    if (!target.IsEnemy() || (!target.IsZombie() && target.IsDead())) {
        return false;
    }
    if (!target.IsVisible() || !target.IsTargetable() || target.IsInvulnerable()) {
        return false;
    }
    return range >= FLT_MAX * 0.5f || player.Distance(target) <= range;
}

} // namespace SDK::OrbwalkingDetail

namespace SDK {

inline AttackableUnit OrbwalkerBase::GetTarget() {
    const auto player = GameObjects::Player();
    if (!player.IsValid() || player.IsDead()) {
        return {};
    }

    if (context_.forceTarget.IsValid() &&
        OrbwalkingDetail::IsValidAttackTarget(context_.forceTarget, GetAutoAttackRange(context_.forceTarget))) {
        return context_.forceTarget;
    }

    AIHeroClient best;
    float bestDistance = FLT_MAX;
    for (const auto& hero : GameObjects::EnemyHeroes()) {
        const AttackableUnit target(hero.Handle());
        const float range = GetAutoAttackRange(target);
        if (!OrbwalkingDetail::IsValidAttackTarget(target, range)) {
            continue;
        }

        const float distance = player.Distance(hero);
        if (distance < bestDistance) {
            best = hero;
            bestDistance = distance;
        }
    }
    return best.IsValid() ? AttackableUnit(best.Handle()) : AttackableUnit();
}

inline float OrbwalkerBase::GetAutoAttackRange(const AttackableUnit& target) const {
    const auto player = GameObjects::Player();
    if (!player.IsValid()) {
        return 0.0f;
    }

    float range = player.AttackRange() + player.BoundingRadius();
    if (target.IsValid()) {
        range += target.BoundingRadius();
    }
    return std::max(0.0f, range);
}

inline AttackableUnit OrbwalkerBase::ResolveAttackTarget(const Events::ProcessSpellEventArgs& args) const {
    if (args.Target.IsValid()) {
        return AttackableUnit(args.Target.Ptr);
    }
    if (args.TargetNetworkId != 0 && args.TargetNetworkId != 0xFFFFFFFFu) {
        return ObjectManager::GetUnitByNetworkId<AttackableUnit>(
            static_cast<int>(args.TargetNetworkId));
    }
    if (context_.pendingAttackTargetNetworkId != 0) {
        return ObjectManager::GetUnitByNetworkId<AttackableUnit>(context_.pendingAttackTargetNetworkId);
    }
    return {};
}

} // namespace SDK
