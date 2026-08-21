#pragma once

using namespace ::SDK;

namespace OrbwalkerKuro {

inline OrbwalkerBase::OrbwalkerBase(Menu* parentMenu)
    : menu_(parentMenu) {
    OrbwalkingDetail::RuntimeInstance = this;

    if (menu_.attackEnabledOption_) {
        context_.attackEnabled = menu_.attackEnabledOption_->Value;
        menu_.attackEnabledOption_->ValueChanged = [](MenuItem* sender, void* userData) {
            auto* self = static_cast<OrbwalkerBase*>(userData);
            if (self && sender) {
                self->context_.attackEnabled = sender->As<MenuBool>()->Value;
            }
        };
        menu_.attackEnabledOption_->ValueChangedUd = this;
    }

    if (menu_.moveEnabledOption_) {
        context_.moveEnabled = menu_.moveEnabledOption_->Value;
        menu_.moveEnabledOption_->ValueChanged = [](MenuItem* sender, void* userData) {
            auto* self = static_cast<OrbwalkerBase*>(userData);
            if (self && sender) {
                self->context_.moveEnabled = sender->As<MenuBool>()->Value;
            }
        };
        menu_.moveEnabledOption_->ValueChangedUd = this;
    }

    Events::AddOnGameUpdate(&OrbwalkerBase::OnGameUpdateStatic);
    Events::AddOnDoCast(&OrbwalkerBase::OnDoCastStatic);
    Events::AddOnProcessSpell(&OrbwalkerBase::OnProcessSpellStatic);
    // Events::AddOnStopCast(&OrbwalkerBase::OnStopCastStatic);
    Events::AddOnMissileCreate(&OrbwalkerBase::OnMissileCreateStatic);
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
    context_.attackReleaseSafeTick = 0;
    context_.attackCastComplete = value > 0;
}

inline int OrbwalkerBase::LastMovementTick() const { return context_.lastMovementTick; }
inline void OrbwalkerBase::LastMovementTick(int value) {
    context_.lastMovementTick = value;
    context_.lastMoveOrderTick = value;
}
inline bool OrbwalkerBase::AttackEnabled() const { return context_.attackEnabled && menu_.AttackEnabled(); }
inline void OrbwalkerBase::AttackEnabled(bool value) {
    context_.attackEnabled = value;
    menu_.SetAttackEnabled(value);
}
inline bool OrbwalkerBase::MoveEnabled() const { return context_.moveEnabled && menu_.MoveEnabled(); }
inline void OrbwalkerBase::MoveEnabled(bool value) {
    context_.moveEnabled = value;
    menu_.SetMoveEnabled(value);
}
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
        "[<b-cyan>OrbwalkerKuro</b-cyan>][<b-yellow>AAReset</b-yellow>] reason=<yellow>%s</yellow> | source=<cyan>%s</cyan> | match=<cyan>%s</cyan> | tick=%d\n"
        "  champion=<magenta>%s</magenta> | spell=<magenta>%s</magenta> (slot=%d)\n"
        "  state={lastAA=%d, pending=<green>%d</green>, confirmed=<green>%d</green>, castComplete=%d}\n",
        reason && reason[0] ? reason : "unknown",
        source && source[0] ? source : "unknown",
        matchType && matchType[0] ? matchType : "none",
        now,
        championName && championName[0] ? championName : "none",
        spellName && spellName[0] ? spellName : "none",
        spellSlot,
        context_.lastAutoAttackTick,
        context_.pendingAttack ? 1 : 0,
        context_.hasConfirmedAttack ? 1 : 0,
        context_.attackCastComplete ? 1 : 0);

    context_.lastAutoAttackTick = 0;
    context_.lastAttackOrderTick = 0;
    context_.lastAttackOrderNetworkId = 0;
    context_.lastAttackConfirmTick = 0;
    context_.lastAfterAttackStartTick = 0;
    context_.attackReleaseSafeTick = 0;
    context_.lastAutoAttackResetTick = now;
    context_.attackPauseTick = 0;
    ClearPendingAttackState();
    context_.hasConfirmedAttack = false;
    context_.attackCastComplete = false;
}

inline void OrbwalkerBase::LogAfterAttackDebug(
    const AttackableUnit& target,
    const std::string* spellNameOverride) {
    if (!menu_.DebugLogAfterAttack()) {
        return;
    }
    if (!target.IsValid()) {
        return;
    }

    const std::string& spellName =
        spellNameOverride ? *spellNameOverride : context_.lastAttackSpellName;
    NightSharpDebug::Logf(
        "[<b-cyan>OrbwalkerKuro</b-cyan>][<b-yellow>AfterAttack</b-yellow>] "
        "spell=<magenta>%s</magenta> target=<cyan>%s</cyan> net=%u",
        spellName.c_str(),
        target.CharacterName().c_str(),
        target.NetworkId());
}

inline void OrbwalkerBase::Dispose() {
    if (context_.disposed) {
        return;
    }
    Events::RemoveOnGameUpdate(&OrbwalkerBase::OnGameUpdateStatic);
    Events::RemoveOnDoCast(&OrbwalkerBase::OnDoCastStatic);
    Events::RemoveOnProcessSpell(&OrbwalkerBase::OnProcessSpellStatic);
    // Events::RemoveOnStopCast(&OrbwalkerBase::OnStopCastStatic);
    Events::RemoveOnMissileCreate(&OrbwalkerBase::OnMissileCreateStatic);
    Events::RemoveOnPlayAnimation(&OrbwalkerBase::OnPlayAnimationStatic);
    Events::RemoveOnDash(&OrbwalkerBase::OnDashStatic);
    Drawing::RemoveOnDraw(&OrbwalkerBase::OnDrawStatic);
    ClearPlantAttackSpellBlock();
    if (context_.plantAttackSpellBlockOwner) {
        CoreEvadeState::ReleaseOwner(context_.plantAttackSpellBlockOwner);
        context_.plantAttackSpellBlockOwner = {};
    }
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
inline bool OrbwalkerBase::IsPostFlashAttackGraceActive(int now) const {
    return context_.postFlashTargetNetworkId > 0 &&
           context_.postFlashAttackGraceUntilTick > now;
}

inline void OrbwalkerBase::ClearPostFlashAttackGrace() {
    context_.postFlashAttackGraceUntilTick = 0;
    context_.postFlashTargetNetworkId = 0;
}


} // namespace OrbwalkerKuro
