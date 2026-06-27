#pragma once

#include "OrbwalkingActionArgs.h"
#include "../../Enumerations/OrbwalkingMode.h"
#include "../../Events/Events.h"
#include "../../Extensions/Unit.h"
#include "../../GameObjects/GameObjects.h"
#include "../../Utils/AutoAttack.h"
#include "../../Core/Game.h"
#include "../../Core/Variables.h"
#include "../../../core/CoreControl.h"

#include <algorithm>
#include <cmath>

namespace SDK {

class OrbwalkerBase {
public:
    static constexpr int kMaxActionHandlers = 32;
    using ActionHandler = void(*)(OrbwalkingActionArgs&);

    explicit OrbwalkerBase() {
        Ptr() = this;
    }

    virtual ~OrbwalkerBase() = default;

    virtual void Attack(AttackableUnit target) = 0;
    virtual void Move(Vector3 position) = 0;
    virtual AttackableUnit GetTarget() = 0;
    virtual bool ShouldWait() = 0;

    OrbwalkingMode ActiveMode = OrbwalkingMode::None;

    struct PerFrameCache {
        int tick = 0;
        float attackDelay = 0.0f;
        float attackWindup = 0.0f;
        Vector3 cursorPos = {};
    };

    static PerFrameCache& FrameCache() {
        static PerFrameCache c;
        return c;
    }

    static void RefreshCache() {
        auto& fc = FrameCache();
        int now = Variables::TickCount();
        if (fc.tick == now) return;
        fc.tick = now;
        const auto player = GameObjects::Player();
        if (player.IsValid()) {
            fc.attackDelay = CoreControl::GetAttackDelay(player.Address());
            fc.attackWindup = CoreControl::GetAttackWindup(player.Address());
        }
        fc.cursorPos = Game::CursorPosRaw();
    }

    bool Enabled() const { return enabled_; }

    virtual void SetEnabled(bool value) {
        if (enabled_ == value) return;
        if (value) {
            Events::hook.OnProcessSpell += &OnProcessSpellHandler;
            Events::hook.OnDoCast += &OnDoCastHandler;
            Events::hook.OnBuffAdd += &OnBuffAddHandler;
            Events::hook.OnStopCast += &OnStopCastHandler;
            Events::hook.OnUpdate += &OnGameUpdateHandler;
        } else {
            Events::hook.OnProcessSpell -= &OnProcessSpellHandler;
            Events::hook.OnDoCast -= &OnDoCastHandler;
            Events::hook.OnBuffAdd -= &OnBuffAddHandler;
            Events::hook.OnStopCast -= &OnStopCastHandler;
            Events::hook.OnUpdate -= &OnGameUpdateHandler;
        }
        enabled_ = value;
    }

    bool OnAction(ActionHandler handler) {
        if (!handler) return false;
        for (int i = 0; i < actionHandlerCount_; ++i) {
            if (actionHandlers_[i] == handler) return true;
        }
        if (actionHandlerCount_ >= kMaxActionHandlers) return false;
        actionHandlers_[actionHandlerCount_++] = handler;
        return true;
    }

    bool RemoveAction(ActionHandler handler) {
        if (!handler) return false;
        for (int i = 0; i < actionHandlerCount_; ++i) {
            if (actionHandlers_[i] == handler) {
                for (int j = i; j + 1 < actionHandlerCount_; ++j) {
                    actionHandlers_[j] = actionHandlers_[j + 1];
                }
                actionHandlers_[--actionHandlerCount_] = nullptr;
                return true;
            }
        }
        return false;
    }

    void InvokeAction(OrbwalkingActionArgs& args) {
        for (int i = 0; i < actionHandlerCount_; ++i) {
            if (actionHandlers_[i]) {
                actionHandlers_[i](args);
            }
        }
    }

    bool CanAttack() { return CanAttack(0.0f); }

    virtual bool CanAttack(float extraWindup) {
        return Variables::TickCount() + (Game::Ping() / 2) + 25
               >= lastAutoAttackTick
                  + static_cast<int>(FrameCache().attackDelay * 1000.0f)
                  + static_cast<int>(extraWindup);
    }

    bool CanMove() { return CanMove(0.0f, false); }

    virtual bool CanMove(float extraWindup, bool disableMissileCheck) {
        if (MissileLaunched && !disableMissileCheck) {
            return true;
        }
        const auto player = GameObjects::Player();
        return !Utils::AutoAttack::CanCancelAutoAttack(player)
               || (Variables::TickCount() + (Game::Ping() / 2)
                   >= LastAutoAttackTick
                      + static_cast<int>(FrameCache().attackWindup * 1000.0f)
                      + static_cast<int>(extraWindup));
    }

    bool CanOrbwalk(const AttackableUnit& target, float range = 0.0f) {
        return CanOrbwalk(target, range, 0.0f);
    }

    bool CanOrbwalk(const AttackableUnit& target, float range, float extraWindup) {
        const float effectiveRange = range > 0.0f ? range : Utils::AutoAttack::GetRealAutoAttackRange(target);
        return Extensions::IsValidTarget(target, effectiveRange) && CanAttack(extraWindup);
    }

    OrbwalkingMode GetActiveMode() const { return ActiveMode; }

    float GetMyRange(const AttackableUnit& target) const {
        return Utils::AutoAttack::GetRealAutoAttackRange(target);
    }

    void Orbwalk(const AttackableUnit& forcedTarget = AttackableUnit(),
                 const Vector3& forcedPosition = Vector3()) {
        if (CanAttack() && AttackState) {
            auto target = forcedTarget.IsValid() ? forcedTarget : GetTarget();
            if (target.IsValid() && Utils::AutoAttack::InAutoAttackRange(target)) {
                Attack(target);
            }
        }
        if (CanMove() && MovementState) {
            Move(forcedPosition.IsValid() && !forcedPosition.IsZero()
                     ? forcedPosition
                     : FrameCache().cursorPos);
        }
    }

    void ResetSwingTimer() {
        LastAutoAttackTick = 0;
    }

    void SetAttackState(bool state) { AttackState = state; }
    void SetMovementState(bool state) { MovementState = state; }

public:
    // Public read access (matching C# { get; protected set; })
    int LastAutoAttackCommandTick = 0;
    int LastAutoAttackTick = 0;
    int LastMovementOrderTick = 0;
    int TotalAutoAttacks = 0;
    AttackableUnit LastTarget = {};

protected:
    OrbwalkingMode InActiveMode = OrbwalkingMode::None;
    bool AttackState = true;
    bool MovementState = true;
    bool MissileLaunched = false;

public:
    static OrbwalkerBase*& Ptr() {
        static OrbwalkerBase* ptr = nullptr;
        return ptr;
    }

private:
    bool enabled_ = false;
    ActionHandler actionHandlers_[kMaxActionHandlers] = {};
    int actionHandlerCount_ = 0;

public:
    static void OnGameUpdateHandler(const Events::GameUpdateEventArgs&) {
        RefreshCache();
        auto* self = Ptr();
        if (!self || !self->enabled_) return;
        const auto player = GameObjects::Player();
        if (!player.IsValid() || player.IsDead()
            || Extensions::IsCastingInterruptableSpell(player, true)) {
            return;
        }
        if (self->ActiveMode != self->InActiveMode) {
            self->Orbwalk();
        }
    }

    static void OnProcessSpellHandler(const Events::ProcessSpellEventArgs& args) {
        auto* self = Ptr();
        if (!self || !self->enabled_) return;
        if (!args.Sender.IsValid()
            || args.Sender.NetworkId != GameObjects::Player().NetworkId()) {
            return;
        }
        self->OnProcessSpellDelayed(args);
    }

    static void OnDoCastHandler(const Events::ProcessSpellEventArgs& args) {
        auto* self = Ptr();
        if (!self || !self->enabled_) return;
        if (!args.Sender.IsValid()
            || args.Sender.NetworkId != GameObjects::Player().NetworkId()) {
            return;
        }
        const AIBaseClient sender(args.Sender.Ptr);
        if (!sender.IsValid() || !sender.IsMe()) return;

        if (Utils::AutoAttack::IsAutoAttackReset(args.SpellName)) {
            self->ResetSwingTimer();
            return;
        }

        if (!args.IsAutoAttack && !Utils::AutoAttack::IsAutoAttack(args.SpellName)) {
            return;
        }

        auto target = ObjectManager::GetUnitByNetworkId<AttackableUnit>(
            static_cast<int>(args.TargetNetworkId));
        if (target.IsValid()) {
            self->LastAutoAttackTick = Variables::TickCount() - (Game::Ping() / 2);
            self->MissileLaunched = false;
            self->LastMovementOrderTick = 0;
            ++self->TotalAutoAttacks;

            if (!target.Compare(self->LastTarget)) {
                OrbwalkingActionArgs switchArgs = {};
                switchArgs.Target = target;
                switchArgs.Type = OrbwalkingType::TargetSwitch;
                self->InvokeAction(switchArgs);
                self->LastTarget = target;
            }

            OrbwalkingActionArgs attackArgs = {};
            attackArgs.Target = target;
            attackArgs.Sender = sender;
            attackArgs.Type = OrbwalkingType::OnAttack;
            self->InvokeAction(attackArgs);
        }
    }

    static void OnBuffAddHandler(const Events::BuffEventArgs& args) {
        auto* self = Ptr();
        if (!self || !self->enabled_) return;
        if (!args.Sender.IsValid()
            || args.Sender.NetworkId != GameObjects::Player().NetworkId()) {
            return;
        }
        if (std::strcmp(args.BuffName, "sonapassiveattack") == 0) {
            self->ResetSwingTimer();
        }
    }

    static void OnStopCastHandler(const Events::StopCastEventArgs& args) {
        auto* self = Ptr();
        if (!self || !self->enabled_) return;
        if (!args.Sender.IsValid()
            || args.Sender.NetworkId != GameObjects::Player().NetworkId()) {
            return;
        }
        if (args.DestroyMissile && args.KeepAnimationPlaying) {
            self->ResetSwingTimer();
        }
    }

    void OnProcessSpellDelayed(const Events::ProcessSpellEventArgs& args) {
        if (Utils::AutoAttack::IsAutoAttackReset(args.SpellName)) {
            ResetSwingTimer();
            return;
        }

        if (!args.IsAutoAttack && !Utils::AutoAttack::IsAutoAttack(args.SpellName)) {
            return;
        }

        MissileLaunched = true;

        auto target = ObjectManager::GetUnitByNetworkId<AttackableUnit>(
            static_cast<int>(args.TargetNetworkId));
        const AIBaseClient sender(args.Sender.Ptr);

        OrbwalkingActionArgs eventArgs = {};
        eventArgs.Target = target;
        eventArgs.Sender = sender;
        eventArgs.Type = OrbwalkingType::AfterAttack;
        InvokeAction(eventArgs);
    }
};

} // namespace SDK
