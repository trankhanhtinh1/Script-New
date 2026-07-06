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

    const auto player = GameObjects::Player();
    if (!player.IsValid() || player.IsDead()) {
        return false;
    }
    if (!ChampionCanAttack(player)) {
        return false;
    }
    if (context_.lastAutoAttackTick <= 0) {
        return true;
    }

    SnapshotAttackTimings(player);
    const float readyAt = static_cast<float>(context_.lastAutoAttackTick) +
                          context_.attackDelayMs +
                          ChampionExtraAttackDelayMs(player) +
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
    if (context_.lastAttackRequiresDoCastBeforeMove) {
        if (context_.lastAttackDoCastComplete) {
            return true;
        }

        const int gateTick = context_.lastAttackDoCastWaitTick > 0
            ? context_.lastAttackDoCastWaitTick
            : attackTick;
        if (now - gateTick < DoCastMoveGateTimeoutMs()) {
            return false;
        }

        ClearDoCastMoveGate();
    }
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
    if (!context_.attackEnabled ||
        !OrbwalkingDetail::IsValidAttackTarget(target, Utils::AutoAttack::GetRealAutoAttackRange(target))) {
        return false;
    }
    if (context_.pendingAttack) {
        return false;
    }
    if (!CanAttack()) {
        return false;
    }

    const int now = Tick();
    OrbwalkingActionArgs beforeArgs(OrbwalkingType::BeforeAttack, target, target.Position(), "SDK");
    OrbwalkingDetail::FireBeforeAttack(beforeArgs);
    if (!beforeArgs.Process) {
        return false;
    }

    const AttackableUnit attackTarget = beforeArgs.Target;
    if (!OrbwalkingDetail::IsValidAttackTarget(
            attackTarget,
            Utils::AutoAttack::GetRealAutoAttackRange(attackTarget))) {
        return false;
    }

    const int targetNetworkId = attackTarget.NetworkId();
    if (context_.lastAttackOrderTick > 0 &&
        context_.lastAttackOrderNetworkId == targetNetworkId &&
        now - context_.lastAttackOrderTick >= 0 &&
        now - context_.lastAttackOrderTick < kAttackOrderDelayMs) {
        return false;
    }

    if (!CoreControl::IssueAttack(attackTarget.Address(), attackTarget.Position(), true)) {
        context_.lastAttackOrderTick = 0;
        context_.lastAttackOrderNetworkId = 0;
        ClearPendingAttackState();
        return false;
    }

    context_.lastAttackOrderTick = now;
    context_.lastAttackOrderNetworkId = targetNetworkId;
    context_.lastTarget = attackTarget;
    context_.lastAutoAttackTick = now;
    context_.lastAttackConfirmTick = 0;
    context_.pendingAttack = true;
    context_.pendingAttackTick = now;
    context_.pendingAttackTargetNetworkId = targetNetworkId;
    context_.hasConfirmedAttack = false;
    context_.attackCastComplete = false;
    context_.lastAttackRequiresDoCastBeforeMove =
        ChampionRequiresDoCastBeforeMove(GameObjects::Player());
    context_.lastAttackDoCastComplete = false;
    context_.lastAttackDoCastWaitTick = context_.lastAttackRequiresDoCastBeforeMove ? now : 0;
    SnapshotAttackTimings(GameObjects::Player());

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

    if (target.IsValid() && Attack(target)) {
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

    ClearPendingAttackState();
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

inline int OrbwalkerBase::DoCastMoveGateTimeoutMs() const {
    return std::clamp(PendingAttackTimeoutMs(), 180, 550);
}

inline float OrbwalkerBase::OneWayPingMs() const {
    return std::clamp(static_cast<float>(Game::Ping()) * 0.5f, 0.0f, kMaxPingLeadMs);
}

inline float OrbwalkerBase::ChampionExtraAttackDelayMs(const AIHeroClient& player) const {
    if (_stricmp(player.CharacterName().c_str(), "Graves") == 0) {
        return (context_.attackDelayMs * 1.0740296828f) - 716.2381256175f - context_.attackDelayMs;
    }
    return 0.0f;
}

inline bool OrbwalkerBase::ChampionRequiresDoCastBeforeMove(const AIHeroClient& player) const {
    return _stricmp(player.CharacterName().c_str(), "Rengar") == 0 &&
           (player.HasBuff("RengarQ") ||
            player.HasBuff("RengarQEmp") ||
            player.HasBuff("rengarqbase") ||
            player.HasBuff("rengarqemp"));
}

inline bool OrbwalkerBase::ChampionCanAttack(const AIHeroClient& player) const {
    if (_stricmp(player.CharacterName().c_str(), "Graves") == 0 &&
        !player.HasBuff("gravesbasicattackammo1")) {
        return false;
    }
    if (_stricmp(player.CharacterName().c_str(), "Jhin") == 0 &&
        player.HasBuff("JhinPassiveReload")) {
        return false;
    }
    return true;
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
    const int now = Tick();
    if (context_.timingSnapshotTick == now) {
        return;
    }
    context_.timingSnapshotTick = now;

    if (!player.IsValid() || player.IsDead()) {
        context_.attackDelayMs = kDefaultAttackDelayMs;
        context_.attackWindupMs = kDefaultAttackWindupMs;
        return;
    }

    // ponytail: CoreControl already owns timing fallback/caching; SDK just consumes ms values.
    context_.attackDelayMs = CoreControl::GetAttackDelayMs(player.Address());
    context_.attackWindupMs = CoreControl::GetAttackWindupMs(player.Address());
}

} // namespace SDK
