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
#include "../../../FpsDropDebug.h"

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
            Events::hook.OnPlayAnimation += &OnPlayAnimationHandler;
            Events::hook.OnProcessCastSpell += &OnProcessCastSpellHandler;
            Events::hook.OnMissileCreate += &OnMissileCreateHandler;
            Events::hook.OnBuffAdd += &OnBuffAddHandler;
            Events::hook.OnStopCast += &OnStopCastHandler;
            Events::hook.OnUpdate += &OnGameUpdateHandler;
        } else {
            Events::hook.OnProcessSpell -= &OnProcessSpellHandler;
            Events::hook.OnDoCast -= &OnDoCastHandler;
            Events::hook.OnPlayAnimation -= &OnPlayAnimationHandler;
            Events::hook.OnProcessCastSpell -= &OnProcessCastSpellHandler;
            Events::hook.OnMissileCreate -= &OnMissileCreateHandler;
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
        int cd = static_cast<int>(FrameCache().attackDelay * 1000.0f);
        int required = LastAutoAttackTick + cd + static_cast<int>(extraWindup);
        bool res = Variables::TickCount() + (Game::Ping() / 2) + 25 >= required;

        if (res && Variables::TickCount() >= LastAutoAttackTick + 100 && Variables::TickCount() < LastAutoAttackTick + 500) {
            std::ofstream os("c:\\Users\\Public\\nightsharp_orbwalker_debug.txt", std::ios::app);
            os << "[Tick: " << Variables::TickCount() << "] CanAttack() -> TRUE | attackDelay: " << FrameCache().attackDelay << "s (" << cd << "ms) | LastAttackTick: " << LastAutoAttackTick << " | extraWindup: " << extraWindup << "\n";
        }
        return res;
    }

    bool CanMove() { return CanMove(0.0f, false); }

    virtual bool CanMove(float extraWindup, bool disableMissileCheck) {
        auto player = GameObjects::Player();
        if (!player.IsValid()) {
            return false;
        }

        int windupMs = static_cast<int>(FrameCache().attackWindup * 1000.0f);
        int requiredTick = LastAutoAttackTick + windupMs + static_cast<int>(extraWindup) + (Game::Ping() / 2) + 25; 

        return !Utils::AutoAttack::CanCancelAutoAttack(player) || (Variables::TickCount() >= requiredTick);
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
        NS_PROFILE("orb.Orbwalk.total");
        {
            NS_PROFILE("orb.CanAttack");
            bool canAttack = CanAttack();
            // (probe ends here so the timing excludes GetTarget/Attack)
            (void)canAttack;
        }
        if (CanAttack() && AttackState) {
            AttackableUnit target;
            {
                NS_PROFILE("orb.GetTarget");
                target = forcedTarget.IsValid() ? forcedTarget : GetTarget();
            }
            bool inRange;
            {
                NS_PROFILE("orb.InAutoAttackRange");
                inRange = target.IsValid() && Utils::AutoAttack::InAutoAttackRange(target);
            }
            if (inRange) {
                std::ofstream os("c:\\Users\\Public\\nightsharp_orbwalker_debug.txt", std::ios::app);
                os << "[Tick: " << Variables::TickCount() << "] CHECK: CanAttack() -> TRUE, InRange -> TRUE. Calling Attack().\n";
                Attack(target);
            } else {
                std::ofstream os("c:\\Users\\Public\\nightsharp_orbwalker_debug.txt", std::ios::app);
                os << "[Tick: " << Variables::TickCount() << "] CHECK: CanAttack() -> TRUE, but No Target In Range.\n";
            }
        }
        if (CanMove() && MovementState) {
            std::ofstream os("c:\\Users\\Public\\nightsharp_orbwalker_debug.txt", std::ios::app);
            os << "[Tick: " << Variables::TickCount() << "] CHECK: CanMove() -> TRUE. Calling Move().\n";
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
        static int lastUpdateTick = 0;
        int now = Variables::TickCount();
        if (now >= lastUpdateTick && now - lastUpdateTick < 30) return;
        lastUpdateTick = now;

        auto* self = Ptr();
        if (!self || !self->enabled_) {
            return;
        }

        // Activate section profiling for the entire tick while an orbwalker
        // key is held (ActiveMode != None). Idle frames (no key held) pay zero
        // overhead because every NS_PROFILE probe is a no-op when
        // SectionsActive == false. The toggle is restored on exit so manual
        // F12 activation still works outside the orbwalker tick.
        const bool wasActive = NightSharpPerf::SectionsActive;
        const bool keyHeld = (self->ActiveMode != OrbwalkingMode::None);
        if (keyHeld) {
            if (!wasActive) NightSharpPerf::ResetSections();
            NightSharpPerf::SectionsActive = true;
        }

        NS_PROFILE("orb.OnGameUpdateHandler.total");
        {
            NS_PROFILE("orb.RefreshCache");
            RefreshCache();
        }
        {
            NS_PROFILE("orb.Player");
            const auto player = GameObjects::Player();
            if (!player.IsValid() || player.IsDead()) {
                if (keyHeld) {
                    NightSharpPerf::DumpSections();
                    NightSharpPerf::SectionsActive = wasActive;
                }
                return;
            }
            {
                NS_PROFILE("orb.IsCastingInterruptableSpell");
                if (Extensions::IsCastingInterruptableSpell(player, true)) {
                    if (keyHeld) {
                        NightSharpPerf::DumpSections();
                        NightSharpPerf::SectionsActive = wasActive;
                    }
                    return;
                }
            }
        }
        if (self->ActiveMode != self->InActiveMode) {
            self->Orbwalk();
        }
        if (keyHeld) {
            NightSharpPerf::DumpSections();
            NightSharpPerf::SectionsActive = wasActive;
        }
    }

    static void OnProcessSpellHandler(const Events::ProcessSpellEventArgs& args) {
        auto* self = Ptr();
        if (!self || !self->enabled_) return;

        std::ofstream os("c:\\Users\\Public\\nightsharp_orbwalker_debug.txt", std::ios::app);
        os << "[Tick: " << Variables::TickCount() << "] EVENT TRIGGERED: OnProcessSpell | SenderNetId: " << args.Sender.NetworkId << " | PlayerNetId: " << GameObjects::Player().NetworkId() << " | Name: '" << args.SpellName << "' | TargetId: " << args.TargetNetworkId << " | Slot: " << args.Slot << " | IsAuto: " << (args.IsAutoAttack ? "TRUE" : "FALSE") << "\n";

        if (!args.Sender.IsValid()
            || args.Sender.NetworkId != GameObjects::Player().NetworkId()) {
            return;
        }

        self->OnProcessSpellDelayed(args);
    }

    static void OnPlayAnimationHandler(const Events::PlayAnimationEventArgs& args) {
        auto* self = Ptr();
        if (!self || !self->enabled_) return;

        if (args.Sender.NetworkId == GameObjects::Player().NetworkId()) {
            std::ofstream os("c:\\Users\\Public\\nightsharp_orbwalker_debug.txt", std::ios::app);
            os << "[Tick: " << Variables::TickCount() << "] EVENT: OnPlayAnimation | Name: '" << args.Animation << "'\n";
        }
    }

    static void OnProcessCastSpellHandler(const Events::CastSpellEventArgs& args) {
        auto* self = Ptr();
        if (!self || !self->enabled_) return;

        if (args.Sender.NetworkId == GameObjects::Player().NetworkId()) {
            std::ofstream os("c:\\Users\\Public\\nightsharp_orbwalker_debug.txt", std::ios::app);
            os << "[Tick: " << Variables::TickCount() << "] EVENT TRIGGERED: OnProcessCastSpell | SenderNetId: " << args.Sender.NetworkId << " | Slot: " << args.Slot << "\n";
        }
    }

    static void OnMissileCreateHandler(const Events::ObjectEventArgs& args) {
        auto* self = Ptr();
        if (!self || !self->enabled_) return;

        if (args.Source.NetworkId == GameObjects::Player().NetworkId()) {
            std::ofstream os("c:\\Users\\Public\\nightsharp_orbwalker_debug.txt", std::ios::app);
            os << "[Tick: " << Variables::TickCount() << "] EVENT: OnMissileCreate | SpellName: '" << args.SpellName << "' | MissileName: '" << args.MissileName << "'\n";
        }
    }

    static void OnDoCastHandler(const Events::ProcessSpellEventArgs& args) {
        auto* self = Ptr();
        if (!self || !self->enabled_) return;

        std::ofstream os("c:\\Users\\Public\\nightsharp_orbwalker_debug.txt", std::ios::app);
        os << "[Tick: " << Variables::TickCount() << "] EVENT TRIGGERED: OnDoCast | SenderNetId: " << args.Sender.NetworkId << " | PlayerNetId: " << GameObjects::Player().NetworkId() << " | Name: '" << args.SpellName << "' | Slot: " << args.Slot << " | IsAuto: " << (args.IsAutoAttack ? "TRUE" : "FALSE") << "\n";

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

        std::ofstream os2("c:\\Users\\Public\\nightsharp_orbwalker_debug.txt", std::ios::app);
        os2 << "[Tick: " << Variables::TickCount() << "] EVENT: OnDoCast (Windup Finished)\n";

        self->MissileLaunched = true;
        self->LastMovementOrderTick = 0;
        ++self->TotalAutoAttacks;

        auto target = ObjectManager::GetUnitByNetworkId<AttackableUnit>(
            static_cast<int>(args.TargetNetworkId));

        OrbwalkingActionArgs eventArgs = {};
        eventArgs.Target = target;
        eventArgs.Sender = sender;
        eventArgs.Type = OrbwalkingType::AfterAttack;
        self->InvokeAction(eventArgs);
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

        auto target = ObjectManager::GetUnitByNetworkId<AttackableUnit>(
            static_cast<int>(args.TargetNetworkId));
        if (!target.IsValid()) {
            target = LastTarget.IsValid() ? LastTarget : GetTarget();
        }
        const AIBaseClient sender(args.Sender.Ptr);

        std::ofstream os("c:\\Users\\Public\\nightsharp_orbwalker_debug.txt", std::ios::app);
        os << "[Tick: " << Variables::TickCount() << "] EVENT: OnProcessSpell (Windup Started)\n";

        LastAutoAttackTick = Variables::TickCount() - (Game::Ping() / 2);
        MissileLaunched = false;

        if (target.IsValid()) {
            if (!target.Compare(LastTarget)) {
                OrbwalkingActionArgs switchArgs = {};
                switchArgs.Target = target;
                switchArgs.Type = OrbwalkingType::TargetSwitch;
                InvokeAction(switchArgs);
                LastTarget = target;
            }

            OrbwalkingActionArgs attackArgs = {};
            attackArgs.Target = target;
            attackArgs.Sender = sender;
            attackArgs.Type = OrbwalkingType::OnAttack;
            InvokeAction(attackArgs);
        }
    }
};

} // namespace SDK
