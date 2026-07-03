#pragma once

namespace SDK {

inline bool OrbwalkerBase::CanAttack() { return CanAttack(0.0f); }

inline bool OrbwalkerBase::CanAttack(float extraWindup) {
    ExpirePendingAttack();
    const int now = Tick();
    if (!context_.attackEnabled || now < context_.allPauseTick || now < context_.attackPauseTick) {
        return false;
    }
    if (context_.pendingAttack) {
        return false;
    }
    if (context_.lastAutoAttackTick <= 0) {
        return true;
    }

    const auto player = GameObjects::Player();
    if (!player.IsValid() || player.IsDead()) {
        return false;
    }

    SnapshotAttackTimings(player);
    const float pingLead = context_.hasConfirmedAttack ? OneWayPingMs() : 0.0f;
    const float readyAt = static_cast<float>(context_.lastAutoAttackTick) +
                          context_.attackDelayMs +
                          AttackSafetyMs();
    return static_cast<float>(now) + pingLead + extraWindup >= readyAt;
}

inline bool OrbwalkerBase::CanMove() { return CanMove(0.0f, false); }

inline bool OrbwalkerBase::CanMove(float extraWindup, bool disableMissileCheck) {
    (void)disableMissileCheck;
    ExpirePendingAttack();
    const int now = Tick();
    if (!context_.moveEnabled || now < context_.allPauseTick || now < context_.movePauseTick) {
        return false;
    }

    const bool pending = context_.pendingAttack;
    const int attackTick = pending ? context_.pendingAttackTick : context_.lastAutoAttackTick;
    if (attackTick <= 0) {
        return true;
    }

    const auto player = GameObjects::Player();
    if (!player.IsValid() || player.IsDead()) {
        return false;
    }

    SnapshotAttackTimings(player);
    const float oneWayPing = OneWayPingMs();
    const float pingLead = pending ? 0.0f : (context_.hasConfirmedAttack ? oneWayPing : 0.0f);
    const float moveSafety = pending ? (kMoveSafetyMs + oneWayPing) : MoveSafetyMs();
    const float readyAt = static_cast<float>(attackTick) +
                          context_.attackWindupMs +
                          moveSafety;
    return static_cast<float>(now) + pingLead + extraWindup >= readyAt;
}

inline bool OrbwalkerBase::Attack(const AttackableUnit& target) {
    ExpirePendingAttack();
    if (!context_.attackEnabled || !OrbwalkingDetail::IsValidAttackTarget(target, GetAutoAttackRange(target))) {
        return false;
    }
    if (context_.pendingAttack) {
        return false;
    }

    OrbwalkingActionArgs beforeArgs(OrbwalkingType::BeforeAttack, target, {}, "SDK");
    OrbwalkingDetail::FireBeforeAttack(beforeArgs);
    if (!beforeArgs.Process) {
        return false;
    }

    const int now = Tick();
    if (!CoreControl::IssueAttack(target.Address(), target.Position(), true)) {
        return false;
    }

    context_.lastTarget = target;
    context_.pendingAttack = true;
    context_.pendingAttackTick = now;
    context_.pendingAttackTargetNetworkId = target.NetworkId();
    context_.attackCastComplete = false;
    SnapshotAttackTimings(GameObjects::Player());

    OrbwalkingActionArgs attackArgs(OrbwalkingType::OnAttack, target, {}, "SDK");
    OrbwalkingDetail::FireOnAttack(attackArgs);
    OrbwalkingActionArgs afterArgs(OrbwalkingType::AfterAttack, target, {}, "SDK");
    OrbwalkingDetail::FireAfterAttack(afterArgs);
    return true;
}

inline void OrbwalkerBase::Move(const Vector3& position) {
    if (!context_.moveEnabled) {
        return;
    }

    const int now = Tick();
    if (now - context_.lastMovementTick < kMoveDelayMs) {
        return;
    }

    Vector3 movePosition = position.IsZero() ? Game::CursorPos() : position;
    if (movePosition.IsZero()) {
        return;
    }

    OrbwalkingActionArgs args(OrbwalkingType::Movement, {}, movePosition, "SDK");
    OrbwalkingDetail::FireBeforeMove(args);
    if (!args.Process || args.Position.IsZero()) {
        return;
    }

    if (CoreControl::IssueMove(args.Position, true)) {
        context_.lastMovementTick = now;
    }
}

inline void OrbwalkerBase::Orbwalk(const AttackableUnit& target, const Vector3& position) {
    if (ActiveMode() == OrbwalkingMode::None) {
        return;
    }

    if (target.IsValid() && CanAttack() && Attack(target)) {
        return;
    }

    if (CanMove()) {
        Move(position.IsZero() ? context_.orbwalkerPosition : position);
    }
}

inline void OrbwalkerBase::ExpirePendingAttack() {
    if (!context_.pendingAttack) {
        return;
    }

    const int now = Tick();
    if (now - context_.pendingAttackTick < PendingAttackTimeoutMs()) {
        return;
    }

    context_.pendingAttack = false;
    context_.pendingAttackTick = 0;
    context_.pendingAttackTargetNetworkId = 0;
    context_.attackCastComplete = false;
    context_.attackPauseTick = std::max(context_.attackPauseTick, now + kAttackRetryDelayMs);
}

inline int OrbwalkerBase::PendingAttackTimeoutMs() const {
    return kPendingTimeoutBaseMs + static_cast<int>(OneWayPingMs());
}

inline float OrbwalkerBase::OneWayPingMs() const {
    return std::clamp(static_cast<float>(Game::Ping()) * 0.5f, 0.0f, kMaxPingLeadMs);
}

inline float OrbwalkerBase::AttackSafetyMs() const {
    return context_.hasConfirmedAttack ? kAttackSafetyMs : kAttackSafetyMs + OneWayPingMs();
}

inline float OrbwalkerBase::MoveSafetyMs() const {
    return context_.hasConfirmedAttack ? kMoveSafetyMs : kMoveSafetyMs + OneWayPingMs();
}

inline void OrbwalkerBase::SnapshotAttackTimings(const AIHeroClient& player) {
    if (!player.IsValid() || player.IsDead()) {
        context_.attackDelayMs = kDefaultAttackDelayMs;
        context_.attackWindupMs = kDefaultAttackWindupMs;
        return;
    }

    context_.attackDelayMs = GetAttackDelayMs(player);
    context_.attackWindupMs = GetAttackWindupMs(player);
}

inline float OrbwalkerBase::GetAttackDelayMs(const AIHeroClient& player) const {
    return std::max(250.0f, CoreControl::GetAttackDelay(player.Address()) * 1000.0f);
}

inline float OrbwalkerBase::GetAttackWindupMs(const AIHeroClient& player) const {
    const float delay = GetAttackDelayMs(player);
    const float windup = CoreControl::GetAttackWindup(player.Address()) * 1000.0f;
    return std::clamp(windup, 80.0f, delay);
}

} // namespace SDK
