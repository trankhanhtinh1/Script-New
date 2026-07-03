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
    const float readyAt = static_cast<float>(context_.lastAutoAttackTick) +
                          context_.attackDelayMs +
                          AttackSafetyMs();
    return static_cast<float>(now) + extraWindup >= readyAt;
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
    if (!pending && context_.attackCastComplete) {
        return true;
    }

    const float oneWayPing = OneWayPingMs();
    const float cappedPing = std::min(oneWayPing, 15.0f);
    const bool rangedPreCast = !player.IsMelee() && !context_.attackCastComplete;
    const float pingLead = pending || rangedPreCast
        ? 0.0f
        : (context_.hasConfirmedAttack ? std::min(oneWayPing, kMoveSafetyMs) : 0.0f);
    float moveSafety = pending ? (kMoveSafetyMs + cappedPing) : MoveSafetyMs();
    if (rangedPreCast) {
        moveSafety += kRangedPreCastMoveSafetyMs;
    }
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
    if (!CanAttack()) {
        return false;
    }

    const int now = Tick();
    const int targetNetworkId = target.NetworkId();
    if (context_.lastAttackOrderTick > 0 &&
        context_.lastAttackOrderNetworkId == targetNetworkId &&
        now - context_.lastAttackOrderTick >= 0 &&
        now - context_.lastAttackOrderTick < kAttackOrderDelayMs) {
        return false;
    }

    OrbwalkingActionArgs beforeArgs(OrbwalkingType::BeforeAttack, target, {}, "SDK");
    OrbwalkingDetail::FireBeforeAttack(beforeArgs);
    if (!beforeArgs.Process) {
        return false;
    }

    context_.lastAttackOrderTick = now;
    context_.lastAttackOrderNetworkId = targetNetworkId;
    if (!CoreControl::IssueAttack(target.Address(), target.Position(), true)) {
        return false;
    }

    context_.lastTarget = target;
    context_.lastAutoAttackTick = now;
    context_.lastAttackConfirmTick = 0;
    context_.pendingAttack = true;
    context_.pendingAttackTick = now;
    context_.pendingAttackTargetNetworkId = target.NetworkId();
    context_.hasConfirmedAttack = false;
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
    if (context_.lastMoveOrderTick > 0 &&
        now - context_.lastMoveOrderTick >= 0 &&
        now - context_.lastMoveOrderTick < kMoveDelayMs) {
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

    if (!context_.lastMoveOrderPosition.IsZero() &&
        args.Position.DistanceSqr2D(context_.lastMoveOrderPosition) <=
            kMoveDuplicateDistance * kMoveDuplicateDistance &&
        now - context_.lastMoveOrderTick >= 0 &&
        now - context_.lastMoveOrderTick < kMoveDuplicateDelayMs) {
        return;
    }

    context_.lastMoveOrderTick = now;
    context_.lastMoveOrderPosition = args.Position;
    if (CoreControl::IssueMove(args.Position, true)) {
        context_.lastMovementTick = now;
    }
}

inline void OrbwalkerBase::Orbwalk(const AttackableUnit& target, const Vector3& position) {
    const OrbwalkingMode mode = context_.activeMode != OrbwalkingMode::None
        ? context_.activeMode
        : ActiveMode();
    if (mode == OrbwalkingMode::None) {
        return;
    }

    if (target.IsValid() && CanAttack() && Attack(target)) {
        return;
    }

    if (CanMove(0.0f, false)) {
        Move(position.IsZero() ? context_.orbwalkerPosition : position);
    }
}

inline void OrbwalkerBase::ExpirePendingAttack() {
    if (!context_.pendingAttack) {
        return;
    }

    const int now = Tick();
    const int pendingTick = context_.pendingAttackTick;
    if (now - context_.pendingAttackTick < PendingAttackTimeoutMs()) {
        return;
    }

    context_.pendingAttack = false;
    context_.pendingAttackTick = 0;
    context_.pendingAttackTargetNetworkId = 0;
    context_.attackCastComplete = false;
    if (context_.lastAutoAttackTick <= 0) {
        context_.lastAutoAttackTick = pendingTick > 0 ? pendingTick : now;
        context_.hasConfirmedAttack = false;
    }
    context_.attackPauseTick = std::max(context_.attackPauseTick, now + kAttackRetryDelayMs);
}

inline int OrbwalkerBase::PendingAttackTimeoutMs() const {
    const int oneWayPing = static_cast<int>(OneWayPingMs());
    const int eventGrace = kPendingEventGraceMs + oneWayPing;
    const int windupGate = static_cast<int>(
        context_.attackWindupMs + kMoveSafetyMs + static_cast<float>(oneWayPing));
    return std::max(eventGrace, windupGate + kAttackRetryDelayMs);
}

inline float OrbwalkerBase::OneWayPingMs() const {
    return std::clamp(static_cast<float>(Game::Ping()) * 0.5f, 0.0f, kMaxPingLeadMs);
}

inline float OrbwalkerBase::AttackSafetyMs() const {
    const float highAttackSpeedSafety =
        context_.attackDelayMs <= 450.0f ? kHighAttackSpeedSafetyMs : 0.0f;
    const float confirmationSafety =
        context_.hasConfirmedAttack ? 0.0f : std::min(OneWayPingMs(), 45.0f);
    return kAttackSafetyMs + highAttackSpeedSafety + confirmationSafety;
}

inline float OrbwalkerBase::MoveSafetyMs() const {
    return context_.hasConfirmedAttack ? kMoveSafetyMs : kMoveSafetyMs + std::min(OneWayPingMs(), 15.0f);
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
    const float maxWindup = std::clamp(delay * 0.62f, 120.0f, 360.0f);
    const float minWindup = player.IsMelee()
        ? std::min(maxWindup, std::clamp(delay * 0.30f, 160.0f, 260.0f))
        : std::min(maxWindup, std::clamp(delay * 0.22f, 95.0f, 240.0f));
    if (windup <= 0.0f || windup >= delay - 25.0f) {
        return maxWindup;
    }
    return std::clamp(windup, minWindup, maxWindup);
}

} // namespace SDK
