#pragma once

using namespace ::SDK;

namespace OrbwalkerKuro {

inline OrbwalkerBase::OrbwalkerBase(Menu* parentMenu)
    : menu_(parentMenu) {
    OrbwalkingDetail::RuntimeInstance = this;
    Events::AddOnGameUpdate(&OrbwalkerBase::OnGameUpdateStatic);
    Events::AddOnProcessSpell(&OrbwalkerBase::OnProcessSpellStatic);
    Events::AddOnDoCast(&OrbwalkerBase::OnDoCastStatic);
    // Events::AddOnStopCast(&OrbwalkerBase::OnStopCastStatic);
    Events::AddOnMissileCreate(&OrbwalkerBase::OnMissileCreateStatic);
    Events::AddOnCreateObject(&OrbwalkerBase::OnCreateObjectStatic);
    Events::AddOnDeleteObject(&OrbwalkerBase::OnDeleteObjectStatic);
    Events::AddOnPlayAnimation(&OrbwalkerBase::OnPlayAnimationStatic);
    Events::AddOnDash(&OrbwalkerBase::OnDashStatic);
    Drawing::AddOnDraw(&OrbwalkerBase::OnDrawStatic);
}

inline OrbwalkerBase::~OrbwalkerBase() {
    Dispose();
}

inline AttackableUnit OrbwalkerBase::ForceTarget() const { return context_.forceTarget; }
inline void OrbwalkerBase::ForceTarget(const AttackableUnit& target) {
    context_.forceTarget = target;
    context_.cachedTargetTick = -1;
}
inline AttackableUnit OrbwalkerBase::LastTarget() const { return context_.lastTarget; }
inline OrbwalkingMode OrbwalkerBase::ActiveMode() const { return menu_.ActiveMode(); }
inline int OrbwalkerBase::LastAutoAttackTick() const { return context_.lastAutoAttackTick; }

inline void OrbwalkerBase::ClearDoCastMoveGate() {
    context_.lastAttackRequiresDoCastBeforeMove = false;
    context_.lastAttackDoCastComplete = false;
    context_.lastAttackDoCastWaitTick = 0;
}

inline void OrbwalkerBase::ClearPendingAttackState() {
    context_.pendingAttack = false;
    context_.pendingAttackTick = 0;
    context_.pendingAttackTargetNetworkId = 0;
    ClearDoCastMoveGate();
}

inline void OrbwalkerBase::LastAutoAttackTick(int value) {
    context_.lastAutoAttackTick = value;
    ClearPendingAttackState();
    context_.attackCastComplete = value > 0;
}

inline int OrbwalkerBase::LastMovementTick() const { return context_.lastMovementTick; }
inline void OrbwalkerBase::LastMovementTick(int value) {
    context_.lastMovementTick = value;
    context_.lastMoveOrderTick = value;
}
inline bool OrbwalkerBase::AttackEnabled() const { return context_.attackEnabled; }
inline void OrbwalkerBase::AttackEnabled(bool value) { context_.attackEnabled = value; }
inline bool OrbwalkerBase::MoveEnabled() const { return context_.moveEnabled; }
inline void OrbwalkerBase::MoveEnabled(bool value) { context_.moveEnabled = value; }
inline void OrbwalkerBase::SetOrbwalkerPosition(const Vector3& position) { context_.orbwalkerPosition = position; }
inline void OrbwalkerBase::SetPauseTime(int time) { context_.allPauseTick = Tick() + std::max(0, time); }
inline void OrbwalkerBase::SetServerPauseTime(int time) { SetPauseTime(time - Game::Ping() / 2); }
inline void OrbwalkerBase::SetAttackPauseTime(int time) { context_.attackPauseTick = Tick() + std::max(0, time); }
inline void OrbwalkerBase::SetAttackServerPauseTime(int time) { SetAttackPauseTime(time - Game::Ping() / 2); }
inline void OrbwalkerBase::SetMovePauseTime(int time) { context_.movePauseTick = Tick() + std::max(0, time); }
inline void OrbwalkerBase::SetMoveServerPauseTime(int time) { SetMovePauseTime(time - Game::Ping() / 2); }

inline void OrbwalkerBase::ResetAutoAttackTimer() {
    ResetAutoAttackTimerWithReason(
        "external/manual API call",
        "IOrbwalker::ResetAutoAttackTimer",
        "external/manual");
}

inline void OrbwalkerBase::ResetAutoAttackTimerWithReason(
    const char* reason,
    const char* source,
    const char* matchType,
    const char* championName,
    const char* spellName,
    int spellSlot,
    const char* senderName,
    const char* missileName,
    std::uint32_t senderNetworkId,
    std::uint32_t sourceNetworkId
) {
    const int now = Tick();

    NightSharpDebug::Logf(
        "[OrbwalkerKuro][AAReset] reason=%s source=%s match=%s tick=%d "
        "champion=%s spell=%s slot=%d sender=%s missile=%s senderNet=%u sourceNet=%u "
        "pre={lastAA=%d attackOrder=%d attackOrderTarget=%d attackConfirm=%d "
        "pending=%d pendingTick=%d pendingTarget=%d allPause=%d attackPause=%d "
        "movePause=%d confirmed=%d castComplete=%d doCastRequired=%d "
        "doCastComplete=%d doCastWait=%d}",
        reason && reason[0] ? reason : "unknown",
        source && source[0] ? source : "unknown",
        matchType && matchType[0] ? matchType : "none",
        now,
        championName && championName[0] ? championName : "none",
        spellName && spellName[0] ? spellName : "none",
        spellSlot,
        senderName && senderName[0] ? senderName : "none",
        missileName && missileName[0] ? missileName : "none",
        static_cast<unsigned>(senderNetworkId),
        static_cast<unsigned>(sourceNetworkId),
        context_.lastAutoAttackTick,
        context_.lastAttackOrderTick,
        context_.lastAttackOrderNetworkId,
        context_.lastAttackConfirmTick,
        context_.pendingAttack ? 1 : 0,
        context_.pendingAttackTick,
        context_.pendingAttackTargetNetworkId,
        context_.allPauseTick,
        context_.attackPauseTick,
        context_.movePauseTick,
        context_.hasConfirmedAttack ? 1 : 0,
        context_.attackCastComplete ? 1 : 0,
        context_.lastAttackRequiresDoCastBeforeMove ? 1 : 0,
        context_.lastAttackDoCastComplete ? 1 : 0,
        context_.lastAttackDoCastWaitTick);

    context_.lastAutoAttackTick = 0;
    context_.lastAttackOrderTick = 0;
    context_.lastAttackOrderNetworkId = 0;
    context_.lastAttackConfirmTick = 0;
    context_.lastAfterAttackStartTick = 0;
    context_.lastAutoAttackResetTick = now;
    context_.attackPauseTick = 0;
    ClearPendingAttackState();
    context_.hasConfirmedAttack = false;
    context_.attackCastComplete = false;
}

inline void OrbwalkerBase::Dispose() {
    if (context_.disposed) {
        return;
    }
    Events::RemoveOnGameUpdate(&OrbwalkerBase::OnGameUpdateStatic);
    Events::RemoveOnProcessSpell(&OrbwalkerBase::OnProcessSpellStatic);
    Events::RemoveOnDoCast(&OrbwalkerBase::OnDoCastStatic);
    // Events::RemoveOnStopCast(&OrbwalkerBase::OnStopCastStatic);
    Events::RemoveOnMissileCreate(&OrbwalkerBase::OnMissileCreateStatic);
    Events::RemoveOnCreateObject(&OrbwalkerBase::OnCreateObjectStatic);
    Events::RemoveOnDeleteObject(&OrbwalkerBase::OnDeleteObjectStatic);
    Events::RemoveOnPlayAnimation(&OrbwalkerBase::OnPlayAnimationStatic);
    Events::RemoveOnDash(&OrbwalkerBase::OnDashStatic);
    Drawing::RemoveOnDraw(&OrbwalkerBase::OnDrawStatic);
    if (context_.fakeCursorTexture.Texture) {
        UI::Icons::ReleaseTexture(context_.fakeCursorTexture);
    }
    context_.fakeCursorTextureLoadTried = false;
    context_.fakeCursorTexturePath.clear();
    if (OrbwalkingDetail::RuntimeInstance == this) {
        OrbwalkingDetail::RuntimeInstance = nullptr;
    }
    context_.disposed = true;
}

inline bool OrbwalkerBase::IsAutoAttack(std::string name) {
    return Utils::AutoAttack::IsAutoAttack(std::move(name));
}

inline bool OrbwalkerBase::IsAutoAttackReset(std::string name) {
    return Utils::AutoAttack::IsAutoAttackReset(std::move(name));
}

inline int OrbwalkerBase::Tick() {
    return Game::TickCount();
}

} // namespace OrbwalkerKuro
