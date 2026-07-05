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

inline void OrbwalkerBase::OnStopCastStatic(const Events::StopCastEventArgs& args) {
    if (OrbwalkingDetail::RuntimeInstance) {
        OrbwalkingDetail::RuntimeInstance->OnStopCast(args);
    }
}

inline void OrbwalkerBase::OnDrawStatic() {
    if (OrbwalkingDetail::RuntimeInstance) {
        OrbwalkingDetail::RuntimeInstance->OnDraw();
    }
}

inline void OrbwalkerBase::OnGameUpdate() {
    if (!menu_.Enabled()) {
        ClearPendingAttackState();
        context_.activeMode = OrbwalkingMode::None;
        return;
    }

    context_.activeMode = ActiveMode();
    if (context_.activeMode == OrbwalkingMode::None) {
        ClearPendingAttackState();
        return;
    }

    const Vector3 position = context_.orbwalkerPosition.IsZero() ? Game::CursorPos() : context_.orbwalkerPosition;
    const AttackableUnit target = GetTarget();
    Orbwalk(target, position);
}

inline void OrbwalkerBase::OnProcessSpell(const Events::ProcessSpellEventArgs& args) {
    const bool isAttack = IsLocalAutoAttack(args);
    const bool isAttackReset = IsLocalAutoAttackReset(args);
    if (isAttackReset) {
        ResetAutoAttackTimer();
        return;
    }

    if (!isAttack) {
        return;
    }

    const int now = Tick();
    const bool hadPendingAttack = context_.pendingAttack;
    const int attackStartTick = hadPendingAttack
        ? std::max(context_.pendingAttackTick + static_cast<int>(OneWayPingMs()), now)
        : now;
    const AttackableUnit target = ResolveAttackTarget(args);
    if (target.IsValid()) {
        context_.lastTarget = target;
    }

    if (!hadPendingAttack &&
        context_.hasConfirmedAttack &&
        context_.lastAutoAttackTick > 0 &&
        now - context_.lastAttackConfirmTick <= kDuplicateAttackEventMs) {
        SnapshotAttackTimings(GameObjects::Player());
        return;
    }

    context_.lastAutoAttackTick = attackStartTick;
    context_.lastAttackConfirmTick = now;
    context_.pendingAttack = false;
    context_.pendingAttackTick = 0;
    context_.pendingAttackTargetNetworkId = 0;
    context_.hasConfirmedAttack = true;
    context_.attackCastComplete = false;
    SnapshotAttackTimings(GameObjects::Player());

    const AttackableUnit eventTarget = target.IsValid() ? target : context_.lastTarget;
    OrbwalkingActionArgs attackArgs(
        OrbwalkingType::OnAttack,
        eventTarget,
        eventTarget.IsValid() ? eventTarget.Position() : Vector3(),
        "SDK");
    OrbwalkingDetail::FireOnAttack(attackArgs);
}

inline void OrbwalkerBase::OnDoCast(const Events::ProcessSpellEventArgs& args) {
    const bool isAttackReset = IsLocalAutoAttackReset(args);
    if (!IsLocalAutoAttack(args)) {
        if (isAttackReset) {
            ResetAutoAttackTimer();
        }
        return;
    }

    const int now = Tick();
    SnapshotAttackTimings(GameObjects::Player());
    const int estimatedAttackStartTick = now - static_cast<int>(context_.attackWindupMs);
    const int attackStartTick = context_.pendingAttack
        ? std::max(
            context_.pendingAttackTick + static_cast<int>(OneWayPingMs()),
            estimatedAttackStartTick)
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
    context_.attackCastComplete = true;
    if (context_.lastAttackRequiresDoCastBeforeMove) {
        context_.lastAttackDoCastComplete = true;
        context_.lastAttackDoCastWaitTick = 0;
    }

    if (attackStartTick > 0) {
        context_.lastAutoAttackTick = attackStartTick;
    } else if (context_.lastAutoAttackTick <= 0 || now - context_.lastAutoAttackTick > 300) {
        context_.lastAutoAttackTick = std::max(0, now - static_cast<int>(context_.attackWindupMs));
    }

    if (context_.lastAutoAttackTick > 0 &&
        context_.lastAfterAttackStartTick != context_.lastAutoAttackTick) {
        const AttackableUnit eventTarget = target.IsValid() ? target : context_.lastTarget;
        OrbwalkingActionArgs afterArgs(
            OrbwalkingType::AfterAttack,
            eventTarget,
            eventTarget.IsValid() ? eventTarget.Position() : Vector3(),
            "SDK");
        OrbwalkingDetail::FireAfterAttack(afterArgs);
        context_.lastAfterAttackStartTick = context_.lastAutoAttackTick;
    }

    if (isAttackReset) {
        ResetAutoAttackTimer();
    }
}

inline void OrbwalkerBase::OnStopCast(const Events::StopCastEventArgs& args) {
    if (!Events::IsLocalPlayer(args.Sender)) {
        return;
    }
    if (args.Slot >= 0 && args.Slot != 64) {
        return;
    }

    const int now = Tick();
    const bool stoppedPending =
        context_.pendingAttack &&
        now - context_.pendingAttackTick <= PendingAttackTimeoutMs();

    SnapshotAttackTimings(GameObjects::Player());
    const int windupWindow = static_cast<int>(
        context_.attackWindupMs + MoveSafetyMs() + OneWayPingMs() + kDuplicateAttackEventMs);
    const bool stoppedWindup =
        !context_.attackCastComplete &&
        context_.lastAutoAttackTick > 0 &&
        now - context_.lastAutoAttackTick >= 0 &&
        now - context_.lastAutoAttackTick <= windupWindow;

    if (!stoppedPending && !stoppedWindup) {
        return;
    }

    const int stoppedAttackTick = stoppedPending
        ? context_.pendingAttackTick
        : context_.lastAutoAttackTick;

    ClearPendingAttackState();

    if (args.HasBeenCast || args.DestroyMissile) {
        if (stoppedPending && context_.lastAutoAttackTick <= 0) {
            context_.lastAutoAttackTick = now;
            context_.hasConfirmedAttack = true;
        }
        context_.attackCastComplete = true;
        return;
    }

    context_.lastAutoAttackTick = stoppedAttackTick > 0 ? stoppedAttackTick : now;
    context_.lastAttackConfirmTick = 0;
    context_.hasConfirmedAttack = false;
    context_.attackCastComplete = false;
    context_.attackPauseTick = std::max(context_.attackPauseTick, now + kAttackRetryDelayMs);
}

inline bool OrbwalkerBase::IsLocalAutoAttack(const Events::ProcessSpellEventArgs& args) const {
    if (!Events::IsLocalPlayer(args.Sender)) {
        return false;
    }
    return args.IsAutoAttack;
}

inline bool OrbwalkerBase::IsLocalAutoAttackReset(const Events::ProcessSpellEventArgs& args) const {
    if (!Events::IsLocalPlayer(args.Sender)) {
        return false;
    }
    return IsAutoAttackReset(args.SpellName);
}

inline void OrbwalkerBase::OnDraw() {
    if (!menu_.Enabled()) {
        return;
    }

    const auto player = GameObjects::Player();
    if (!player.IsValid() || player.IsDead()) {
        return;
    }

    if (menu_.DrawAARange()) {
        Drawing::DrawCircle(
            player.Position(),
            Utils::AutoAttack::GetRealAutoAttackRange(player),
            0xFF00BFFFu,
            1.5f,
            64);
    }

    if (menu_.DrawExtraHoldPosition()) {
        Drawing::DrawCircle(
            player.Position(),
            player.BoundingRadius() + static_cast<float>(menu_.MovementExtraHold()),
            0xFF800080u,
            1.5f,
            48);
    }

    if (menu_.DrawAARangeEnemy()) {
        for (const auto& enemy : GameObjects::EnemyHeroes()) {
            if (!enemy.IsValid() || enemy.IsDead() || !enemy.IsVisible()) {
                continue;
            }
            Drawing::DrawCircle(
                enemy.Position(),
                Utils::AutoAttack::GetRealAutoAttackRange(enemy, player),
                0xFF00BFFFu,
                1.5f,
                64);
        }
    }

    if (!menu_.DrawKillableMinion()) {
        return;
    }

    const float range = Utils::AutoAttack::GetRealAutoAttackRange(player) * 2.0f;
    const float rangeSqr = range * range;
    for (const auto& minion : GameObjects::EnemyMinions()) {
        if (!OrbwalkingDetail::IsValidMinionTarget(minion) ||
            player.Position().DistanceSqr2D(minion.Position()) > rangeSqr) {
            continue;
        }

        const float damage = Damage::GetAutoAttackDamage(player, minion);
        if (damage <= 0.0f) {
            continue;
        }

        if (menu_.DrawKillableMinionFade()) {
            if (minion.Health() >= damage * 2.0f) {
                continue;
            }
            const int blue = static_cast<int>(std::clamp(255.0f - minion.Health() * 2.0f, 0.0f, 255.0f));
            Drawing::DrawCircle(
                minion.Position(),
                minion.BoundingRadius() * 2.0f,
                0xFF00FF00u | static_cast<std::uint32_t>(blue),
                1.5f,
                32);
        } else if (OrbwalkingDetail::CanLastHitMinion(player, minion, menu_.DelayFarm())) {
            Drawing::DrawCircle(
                minion.Position(),
                minion.BoundingRadius() * 2.0f,
                0xFF00FF00u,
                1.5f,
                32);
        }
    }
}

} // namespace SDK
