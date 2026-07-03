#pragma once

namespace SDK {

inline void OrbwalkerBase::OnGameUpdateStatic(const Events::GameUpdateEventArgs& args) {
    (void)args;
    if (OrbwalkingDetail::RuntimeInstance) {
        OrbwalkingDetail::RuntimeInstance->OnGameUpdate();
    }
}

inline void OrbwalkerBase::OnProcessSpellStatic(const Events::ProcessSpellEventArgs& args) {
    if (OrbwalkingDetail::RuntimeInstance) {
        OrbwalkingDetail::RuntimeInstance->OnProcessSpell(args);
    }
}

inline void OrbwalkerBase::OnDoCastStatic(const Events::ProcessSpellEventArgs& args) {
    if (OrbwalkingDetail::RuntimeInstance) {
        OrbwalkingDetail::RuntimeInstance->OnDoCast(args);
    }
}

inline void OrbwalkerBase::OnGameUpdate() {
    if (ActiveMode() == OrbwalkingMode::None) {
        return;
    }

    const AttackableUnit target = GetTarget();
    Orbwalk(target, context_.orbwalkerPosition.IsZero() ? Game::CursorPos() : context_.orbwalkerPosition);
}

inline void OrbwalkerBase::OnProcessSpell(const Events::ProcessSpellEventArgs& args) {
    if (!IsLocalAutoAttack(args)) {
        return;
    }

    const int now = Tick();
    const int attackStartTick = context_.pendingAttack
        ? context_.pendingAttackTick + static_cast<int>(OneWayPingMs())
        : now;
    const AttackableUnit target = ResolveAttackTarget(args);
    if (target.IsValid()) {
        context_.lastTarget = target;
    }

    context_.lastAutoAttackTick = attackStartTick;
    context_.lastAttackConfirmTick = now;
    context_.pendingAttack = false;
    context_.pendingAttackTick = 0;
    context_.pendingAttackTargetNetworkId = 0;
    context_.hasConfirmedAttack = true;
    SnapshotAttackTimings(GameObjects::Player());
}

inline void OrbwalkerBase::OnDoCast(const Events::ProcessSpellEventArgs& args) {
    if (!IsLocalAutoAttack(args)) {
        return;
    }

    const int now = Tick();
    const int attackStartTick = context_.pendingAttack
        ? context_.pendingAttackTick + static_cast<int>(OneWayPingMs())
        : 0;
    const AttackableUnit target = ResolveAttackTarget(args);
    if (target.IsValid()) {
        context_.lastTarget = target;
    }

    context_.pendingAttack = false;
    context_.pendingAttackTick = 0;
    context_.pendingAttackTargetNetworkId = 0;
    context_.lastAttackConfirmTick = now;
    context_.hasConfirmedAttack = true;

    if (attackStartTick > 0) {
        context_.lastAutoAttackTick = attackStartTick;
        SnapshotAttackTimings(GameObjects::Player());
    } else if (context_.lastAutoAttackTick <= 0 || now - context_.lastAutoAttackTick > 300) {
        SnapshotAttackTimings(GameObjects::Player());
        context_.lastAutoAttackTick = std::max(0, now - static_cast<int>(context_.attackWindupMs));
    }
}

inline bool OrbwalkerBase::IsLocalAutoAttack(const Events::ProcessSpellEventArgs& args) const {
    if (!Events::IsLocalPlayer(args.Sender)) {
        return false;
    }
    return args.IsAutoAttack || IsAutoAttack(args.SpellName);
}

} // namespace SDK
