#pragma once

namespace OrbwalkerKuro {

using namespace ::SDK;

inline bool OrbwalkerBase::EvadeOwnsActions(int now) const {
    return menu_.CoordinateKuroEvade() &&
        Plugins::KuroCombatCoordination::Coordinator::EvadeOwnsActions(now);
}

inline bool OrbwalkerBase::EvadeBlocksMovement(int now) const {
    return menu_.CoordinateKuroEvade() &&
        Plugins::KuroCombatCoordination::Coordinator::BlocksMovement(
            now, menu_.EvadeHandoffGrace());
}

inline bool OrbwalkerBase::EvadeBlocksAttack(int now) const {
    return menu_.CoordinateKuroEvade() &&
        Plugins::KuroCombatCoordination::Coordinator::BlocksNewAttacks(
            now, menu_.EvadeHandoffGrace());
}

// TEMP PROBE (remove after FPS profiling). ACCUMULATOR: sum ms per name, ONE file
// write per second → `[OrbAcc/1s] name=totalMs/count`. No per-call file-I/O
// (avoids observer effect). Game-thread only, no locking. Defined here (in the
// first .inl included) so both Actions.inl and EventHandlers.inl can use it.
struct OrbProbeAcc { const char* n; double ms; unsigned cnt; };
inline OrbProbeAcc g_orbAcc[48] = {};
inline int g_orbAccN = 0;
inline DWORD g_orbAccLast = 0;

struct OrbProbe {
    const char* n;
    LARGE_INTEGER s;
    explicit OrbProbe(const char* name) : n(name) { QueryPerformanceCounter(&s); }
    ~OrbProbe() {
        LARGE_INTEGER e{}, f{};
        QueryPerformanceCounter(&e);
        QueryPerformanceFrequency(&f);
        const double ms = static_cast<double>(e.QuadPart - s.QuadPart) * 1000.0 /
                          static_cast<double>(f.QuadPart);

        OrbProbeAcc* a = nullptr;
        for (int i = 0; i < g_orbAccN; ++i) {
            if (g_orbAcc[i].n == n) { a = &g_orbAcc[i]; break; }
        }
        if (!a && g_orbAccN < 48) { a = &g_orbAcc[g_orbAccN++]; a->n = n; a->ms = 0.0; a->cnt = 0; }
        if (a) { a->ms += ms; a->cnt += 1; }

        const DWORD now = GetTickCount();
        if (now - g_orbAccLast >= 1000) {
            g_orbAccLast = now;
            char b[1536];
            int p = std::snprintf(b, sizeof(b), "[OrbAcc/1s] ");
            for (int i = 0; i < g_orbAccN && p < static_cast<int>(sizeof(b)) - 48; ++i) {
                p += std::snprintf(b + p, sizeof(b) - p, "%s=%.2f/%u ",
                                   g_orbAcc[i].n, g_orbAcc[i].ms, g_orbAcc[i].cnt);
                g_orbAcc[i].ms = 0.0; g_orbAcc[i].cnt = 0;
            }
            p += std::snprintf(b + p, sizeof(b) - p, "\r\n");
            HANDLE h = CreateFileA("C:\\Users\\Public\\nightsharp_fps_drop_debug.txt",
                                   FILE_APPEND_DATA, FILE_SHARE_READ, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (h != INVALID_HANDLE_VALUE) { DWORD w = 0; WriteFile(h, b, static_cast<DWORD>(p), &w, nullptr); CloseHandle(h); }
        }
    }
};

inline bool OrbwalkerBase::CanAttack() { return CanAttack(0.0f); }

inline bool OrbwalkerBase::CanAttack(float extraWindup) {
    ExpirePendingAttack();
    const int now = Tick();
    if (EvadeBlocksAttack(now) || !context_.attackEnabled ||
        now < context_.allPauseTick || now < context_.attackPauseTick) {
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

    ReadAttackTimingsFromMemory(player);
    const float readyAt = static_cast<float>(context_.lastAutoAttackTick) +
                          context_.attackDelayMs +
                          ChampionExtraAttackDelayMs(player) +
                          AttackSafetyMs();
    return static_cast<float>(now) + extraWindup >= readyAt;
}

inline void OrbwalkerBase::CheckAfterAttack() {
    if (context_.lastAutoAttackTick <= 0) {
        return;
    }
    const auto player = GameObjects::Player();
    if (!player.IsValid()) {
        return;
    }
    ReadAttackTimingsFromMemory(player);
    const int now = Tick();
    const int windupEnd = context_.lastAutoAttackTick + static_cast<int>(context_.attackWindupMs);
    if (now >= windupEnd) {
        context_.attackCastComplete = true;
        if (context_.lastAfterAttackStartTick != context_.lastAutoAttackTick) {
            context_.lastAfterAttackStartTick = context_.lastAutoAttackTick;
            const AttackableUnit eventTarget = context_.lastTarget.IsValid() ? context_.lastTarget : AttackableUnit();
            OrbwalkingActionArgs afterArgs(
                OrbwalkingType::AfterAttack,
                eventTarget,
                eventTarget.IsValid() ? eventTarget.Position() : Vector3(),
                "Kuro");
            OrbwalkingDetail::FireAfterAttack(afterArgs);
        }
    }
}

inline bool OrbwalkerBase::CanMove() { return CanMove(0.0f, false); }

inline bool OrbwalkerBase::IsAutoAttacking() {
    return IsWindingUp();
}

inline bool OrbwalkerBase::IsWindingUp() {
    ExpirePendingAttack();
    CheckAfterAttack();
    const auto player = GameObjects::Player();
    if (!player.IsValid() || player.IsDead()) {
        return false;
    }
    if (context_.pendingAttack) {
        return true;
    }
    if (context_.lastAutoAttackTick <= 0 || context_.attackCastComplete) {
        return false;
    }
    return Tick() < AttackCastReadyTick(player);
}

inline bool OrbwalkerBase::IsAttackCastComplete() {
    ExpirePendingAttack();
    CheckAfterAttack();
    return context_.lastAutoAttackTick > 0 && context_.attackCastComplete;
}

inline int OrbwalkerBase::AttackCastDelayRemaining() {
    ExpirePendingAttack();
    CheckAfterAttack();
    const auto player = GameObjects::Player();
    if (!player.IsValid() || player.IsDead()) {
        return 0;
    }
    if (!context_.pendingAttack &&
        (context_.lastAutoAttackTick <= 0 || context_.attackCastComplete)) {
        return 0;
    }
    return std::max(0, AttackCastReadyTick(player) - Tick());
}

inline int OrbwalkerBase::NextAttackReadyTick() {
    ExpirePendingAttack();
    CheckAfterAttack();
    const auto player = GameObjects::Player();
    if (!context_.attackEnabled || !player.IsValid() || player.IsDead()) {
        return 0;
    }
    return AttackReadyTick(player);
}

inline int OrbwalkerBase::AttackCooldownRemaining() {
    const int readyTick = NextAttackReadyTick();
    return readyTick > 0 ? std::max(0, readyTick - Tick()) : 0;
}

inline bool OrbwalkerBase::CanMove(float extraWindup, bool disableMissileCheck) {
    (void)disableMissileCheck;
    ExpirePendingAttack();
    CheckAfterAttack();
    const int now = Tick();
    if (EvadeBlocksMovement(now) || !context_.moveEnabled ||
        now < context_.allPauseTick || now < context_.movePauseTick) {
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

    ReadAttackTimingsFromMemory(player);
    if (menu_.UseTreTrauLogic()) {
        // TreTrau: do not let OnDoCast/attackCastComplete unlock movement by itself.
        const float safetyBuffer = std::clamp(context_.attackWindupMs * 0.08f, 20.0f, 45.0f);
        const float rengarExtra = ChampionRequiresDoCastBeforeMove(player) ? 200.0f : 0.0f;
        const float readyAt = static_cast<float>(attackTick) +
                              context_.attackWindupMs +
                              extraWindup +
                              safetyBuffer +
                              rengarExtra;
        const float serverNow = static_cast<float>(now) + OneWayPingMs();
        return serverNow >= readyAt;
    }

    // Kuro: dev2 movement gate, including do-cast unlock for special cases.
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
    // Akshan Passive 2-Hit Double Shot movement gate
    if (player.IsValid() && _stricmp(player.CharacterName().c_str(), "Akshan") == 0) {
        const int passiveMode = menu_.AkshanPassiveMode(); // 0 = Always 2-Hit, 1 = Always 1-Hit, 2 = Smart
        if (context_.isAkshanSecondShotPending) {
            const int elapsed = now - context_.pendingAkshanSecondShotTick;
            if (elapsed >= 0 && elapsed <= 500) {
                if (passiveMode == 1) {
                    context_.isAkshanSecondShotPending = false;
                } else if (passiveMode == 2 && context_.activeMode == OrbwalkingMode::Flee) {
                    context_.isAkshanSecondShotPending = false;
                } else {
                    const auto target = context_.lastTarget;
                    if (target.IsValid() && target.IsDead()) {
                        context_.isAkshanSecondShotPending = false;
                    } else {
                        return false; // Hold movement unconditionally until 2nd shot fires!
                    }
                }
            } else {
                context_.isAkshanSecondShotPending = false;
            }
        }

        if (context_.isAkshanSecondShotActive) {
            const float secondShotWindup = context_.attackWindupMs * 0.48f + MoveSafetyMs();
            if (static_cast<float>(now - context_.lastAutoAttackTick) < secondShotWindup) {
                return false; // Waiting for second shot windup
            }
            context_.isAkshanSecondShotActive = false;
        }
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
    const auto player = GameObjects::Player();
    if (!context_.attackEnabled ||
        !OrbwalkingDetail::IsValidCurrentAttackTarget(player, target)) {
        return false;
    }
    if (context_.pendingAttack) {
        return false;
    }
    if (!CanAttack()) {
        return false;
    }

    const int now = Tick();
    OrbwalkingActionArgs beforeArgs(OrbwalkingType::BeforeAttack, target, target.Position(), "Kuro");
    OrbwalkingDetail::FireBeforeAttack(beforeArgs);
    if (!beforeArgs.Process) {
        return false;
    }

    const AttackableUnit attackTarget = beforeArgs.Target;
    if (!OrbwalkingDetail::IsValidCurrentAttackTarget(player, attackTarget)) {
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
    ReadAttackTimingsFromMemory(GameObjects::Player());
    TryShowFakeClick(Hud::ClickType::Attack, attackTarget.Position(), now, context_.lastFakeAttackClickTick);

    return true;
}

inline void OrbwalkerBase::Move(const Vector3& position) {
    const int now = Tick();
    if (!context_.moveEnabled || EvadeBlocksMovement(now)) {
        return;
    }

    if (context_.lastMoveOrderTick > 0 &&
        now - context_.lastMoveOrderTick >= 0 &&
        now - context_.lastMoveOrderTick < kMoveDelayMs) {
        return;
    }

    Vector3 movePosition = position.IsZero() ? Game::CursorPos() : position;
    if (movePosition.IsZero()) {
        return;
    }

    OrbwalkingActionArgs args(OrbwalkingType::Movement, {}, movePosition, "Kuro");
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
        TryShowFakeClick(Hud::ClickType::Move, args.Position, now, context_.lastFakeMoveClickTick);
    }
}

inline void OrbwalkerBase::Orbwalk(const AttackableUnit& target, const Vector3& position) {
    const OrbwalkingMode mode = context_.activeMode != OrbwalkingMode::None
        ? context_.activeMode
        : ActiveMode();
    if (mode == OrbwalkingMode::None) {
        return;
    }

    {
        OrbProbe p("Orb-Attack");
        if (target.IsValid() && Attack(target)) {
            return;
        }
    }

    OrbProbe p("Orb-Move");
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
    bool shouldExpire = false;

    if (context_.pendingAttackTargetNetworkId != 0) {
        const auto target = ObjectManager::GetUnitByNetworkId<AttackableUnit>(context_.pendingAttackTargetNetworkId);
        if (!target.IsValid() || target.IsDead() || !target.IsTargetable()) {
            shouldExpire = true;
        }
    }

    if (!shouldExpire && now - context_.pendingAttackTick < PendingAttackTimeoutMs()) {
        return;
    }

    ClearPendingAttackState();
    context_.attackCastComplete = false;
    if (context_.lastAutoAttackTick <= 0) {
        context_.lastAutoAttackTick = pendingTick > 0 ? pendingTick : now;
        context_.hasConfirmedAttack = false;
    }
    if (!shouldExpire) {
        context_.attackPauseTick = std::max(context_.attackPauseTick, now + kAttackRetryDelayMs);
    }
}

inline int OrbwalkerBase::PendingAttackTimeoutMs() {
    ReadAttackTimingsFromMemory(GameObjects::Player());
    const int oneWayPing = static_cast<int>(OneWayPingMs());
    const int eventGrace = kPendingEventGraceMs + oneWayPing;
    const int windupGate = static_cast<int>(
        context_.attackWindupMs + kMoveSafetyMs + static_cast<float>(oneWayPing));
    return std::max(eventGrace, windupGate + kAttackRetryDelayMs);
}

inline int OrbwalkerBase::DoCastMoveGateTimeoutMs() {
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

inline void OrbwalkerBase::ReadAttackTimingsFromMemory(const AIHeroClient& player) {
    if (!player.IsValid() || player.IsDead()) {
        context_.attackDelayMs = kDefaultAttackDelayMs;
        context_.attackWindupMs = kDefaultAttackWindupMs;
        return;
    }

    // OrbwalkerKuro deliberately bypasses CoreControl's timing cache. A failed
    // memory read falls back to a fixed safe default, never to a stale sample.
    const float attackDelay = CoreControl::ReadAttackDelayFor(player.Address());
    const float attackWindup = CoreControl::ReadAttackWindupFor(player.Address());
    context_.attackDelayMs = attackDelay > 0.0f
        ? attackDelay * 1000.0f
        : kDefaultAttackDelayMs;
    context_.attackWindupMs = attackWindup > 0.0f
        ? attackWindup * 1000.0f
        : kDefaultAttackWindupMs;
}

inline int OrbwalkerBase::AttackCastReadyTick(const AIHeroClient& player) {
    ReadAttackTimingsFromMemory(player);
    const int attackTick = context_.pendingAttack
        ? context_.pendingAttackTick
        : context_.lastAutoAttackTick;
    if (attackTick <= 0) {
        return Tick();
    }
    return static_cast<int>(std::ceil(
        static_cast<float>(attackTick) + context_.attackWindupMs + MoveSafetyMs()));
}

inline int OrbwalkerBase::AttackReadyTick(const AIHeroClient& player) {
    const int now = Tick();
    int readyTick = std::max(context_.allPauseTick, context_.attackPauseTick);
    const int attackTick = context_.pendingAttack
        ? context_.pendingAttackTick
        : context_.lastAutoAttackTick;
    if (attackTick > 0) {
        ReadAttackTimingsFromMemory(player);
        readyTick = std::max(readyTick, static_cast<int>(std::ceil(
            static_cast<float>(attackTick) +
            context_.attackDelayMs +
            ChampionExtraAttackDelayMs(player) +
            AttackSafetyMs())));
    }
    return std::max(now, readyTick);
}

} // namespace OrbwalkerKuro
