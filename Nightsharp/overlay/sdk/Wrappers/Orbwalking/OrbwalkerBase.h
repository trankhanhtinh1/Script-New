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
        // Simple local-time approach: IssueAttack sets LastAutoAttackTick immediately.
        // No server-confirmation (PendingAttack) needed — mirrors external orbwalker.
        const int cd = static_cast<int>(FrameCache().attackDelay * 1000.0f);
        return static_cast<float>(Variables::TickCount() + (Game::Ping() / 2) + 25) >=
               static_cast<float>(LastAutoAttackTick) + static_cast<float>(cd) + extraWindup;
    }

    bool CanMove() { return CanMove(0.0f, false); }

    virtual bool CanMove(float extraWindup, bool /*disableMissileCheck*/) {
        auto player = GameObjects::Player();
        if (!player.IsValid()) return false;

        // Kalista can always move (her passive requires it)
        if (!Utils::AutoAttack::CanCancelAutoAttack(player)) return true;

        if (LastAutoAttackTick <= 0) return true;

        // Pure engine-windup timing: fire move once the client animation's
        // windup phase is complete. No ping adjustment — the client animation
        // starts the moment IssueAttack is called, so this is fully local.
        const float windupMs = FrameCache().attackWindup * 1000.0f;
        return static_cast<float>(Variables::TickCount()) >=
               static_cast<float>(LastAutoAttackTick) + windupMs + extraWindup;
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

#ifdef NIGHTSHARP_DEBUG_ORB
        {
            static int s_lastOrbwalkLog = 0;
            const int now = Variables::TickCount();
            if (now - s_lastOrbwalkLog >= 250) {
                s_lastOrbwalkLog = now;
                std::ofstream os("c:\\Users\\Public\\nightsharp_orbwalker_debug.txt", std::ios::app);
                os << "[Tick: " << now
                   << "] ORBWALK ENTRY | mode=" << static_cast<int>(ActiveMode)
                   << " | AttackState=" << (AttackState ? "1" : "0")
                   << " | LastAutoAttackTick=" << LastAutoAttackTick
                   << " | attackDelayMs=" << static_cast<int>(FrameCache().attackDelay * 1000.0f)
                   << " | windupMs=" << static_cast<int>(FrameCache().attackWindup * 1000.0f)
                   << " | sinceLastAttack=" << (now - LastAutoAttackTick) << "ms\n";
            }
        }
#endif

        bool attackReady;
        {
            NS_PROFILE("orb.CanAttack");
            attackReady = CanAttack();
        }
        if (attackReady && AttackState) {
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

#ifdef NIGHTSHARP_DEBUG_ORB
            {
                std::ofstream os("c:\\Users\\Public\\nightsharp_orbwalker_debug.txt", std::ios::app);
                os << "[Tick: " << Variables::TickCount()
                   << "] ORBWALK CanAttack=TRUE | targetValid=" << (target.IsValid() ? "YES" : "NO")
                   << " | targetName='" << (target.IsValid() ? target.CharacterName() : "") << "'"
                   << " | inRange=" << (inRange ? "YES" : "NO") << "\n";
            }
#endif

            if (inRange) {
                Attack(target);
            }
        }
        if (CanMove() && MovementState) {
#ifdef NIGHTSHARP_DEBUG_ORB
            {
                const int now = Variables::TickCount();
                std::ofstream os("c:\\Users\\Public\\nightsharp_orbwalker_debug.txt", std::ios::app);
                os << "[Tick: " << now
                   << "] MOVE | elapsed=" << (now - LastAutoAttackTick) << "ms"
                   << " | windupMs=" << static_cast<int>(FrameCache().attackWindup * 1000.0f) << "ms\n";
            }
#endif
            Move(forcedPosition.IsValid() && !forcedPosition.IsZero()
                     ? forcedPosition
                     : FrameCache().cursorPos);
        }
    }

    void ResetSwingTimer() {
        LastAutoAttackTick  = 0;
        LastCastDelay       = 0.0f;
        PendingAttack       = false;
        PendingAttackTick   = 0;
        MissileLaunched     = false;
    }

    void SetAttackState(bool state) { AttackState = state; }
    void SetMovementState(bool state) { MovementState = state; }

public:
    // Public read access (matching C# { get; protected set; })
    int LastAutoAttackCommandTick = 0;
    int LastAutoAttackTick        = 0;  // Set from OnProcessSpell (server-side AA start)
    int LastMovementOrderTick     = 0;
    int TotalAutoAttacks          = 0;
    AttackableUnit LastTarget     = {};

    // Pending attack state: set by Attack() on IssueAttack success, cleared by OnProcessSpell.
    // AttackClockTick() picks max(LastAutoAttackTick, PendingAttackTick) to prevent spam.
    bool  PendingAttack    = false;
    int   PendingAttackTick = 0;

    // Actual cast delay captured from OnProcessSpell CastDelay field.
    // More accurate than engine windup; used by CanMove() for the animation window.
    float LastCastDelay = 0.0f;

protected:
    OrbwalkingMode InActiveMode = OrbwalkingMode::None;
    bool AttackState    = true;
    bool MovementState  = true;
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
        // Start of orbwalker loop
        NS_PROFILE("orb.OnGameUpdateHandler.total");
        {
            NS_PROFILE("orb.RefreshCache");
            RefreshCache();
        }
        {
            NS_PROFILE("orb.Player");
            const auto player = GameObjects::Player();
            if (!player.IsValid() || player.IsDead()) {
                return;
            }
            {
                NS_PROFILE("orb.IsCastingInterruptableSpell");
                if (Extensions::IsCastingInterruptableSpell(player, true)) {
                    return;
                }
            }
        }
        if (self->ActiveMode != self->InActiveMode) {
            self->Orbwalk();
        }
    }

    static void OnProcessSpellHandler(const Events::ProcessSpellEventArgs& args) {
        auto* self = Ptr();
        if (!self || !self->enabled_) return;

#ifdef NIGHTSHARP_DEBUG_ORB
        std::ofstream os("c:\\Users\\Public\\nightsharp_orbwalker_debug.txt", std::ios::app);
        os << "[Tick: " << Variables::TickCount() << "] EVENT TRIGGERED: OnProcessSpell | SenderNetId: " << args.Sender.NetworkId << " | PlayerNetId: " << GameObjects::Player().NetworkId() << " | Name: '" << args.SpellName << "' | TargetId: " << args.TargetNetworkId << " | Slot: " << args.Slot << " | IsAuto: " << (args.IsAutoAttack ? "TRUE" : "FALSE") << "\n";
#endif

        if (!args.Sender.IsValid()
            || args.Sender.NetworkId != GameObjects::Player().NetworkId()) {
            return;
        }

        self->OnProcessSpellDelayed(args);
    }

    static void OnPlayAnimationHandler(const Events::PlayAnimationEventArgs& args) {
        auto* self = Ptr();
        if (!self || !self->enabled_) return;

#ifdef NIGHTSHARP_DEBUG_ORB
        if (args.Sender.NetworkId == GameObjects::Player().NetworkId()) {
            std::ofstream os("c:\\Users\\Public\\nightsharp_orbwalker_debug.txt", std::ios::app);
            os << "[Tick: " << Variables::TickCount() << "] EVENT: OnPlayAnimation | Name: '" << args.Animation << "'\n";
        }
#endif
    }

    static void OnProcessCastSpellHandler(const Events::CastSpellEventArgs& args) {
        auto* self = Ptr();
        if (!self || !self->enabled_) return;

#ifdef NIGHTSHARP_DEBUG_ORB
        if (args.Sender.NetworkId == GameObjects::Player().NetworkId()) {
            std::ofstream os("c:\\Users\\Public\\nightsharp_orbwalker_debug.txt", std::ios::app);
            os << "[Tick: " << Variables::TickCount() << "] EVENT TRIGGERED: OnProcessCastSpell | SenderNetId: " << args.Sender.NetworkId << " | Slot: " << args.Slot << "\n";
        }
#endif
    }

    static void OnMissileCreateHandler(const Events::ObjectEventArgs& args) {
        auto* self = Ptr();
        if (!self || !self->enabled_) return;

#ifdef NIGHTSHARP_DEBUG_ORB
        if (args.Source.NetworkId == GameObjects::Player().NetworkId()) {
            std::ofstream os("c:\\Users\\Public\\nightsharp_orbwalker_debug.txt", std::ios::app);
            os << "[Tick: " << Variables::TickCount() << "] EVENT: OnMissileCreate | SpellName: '" << args.SpellName << "' | MissileName: '" << args.MissileName << "'\n";
        }
#endif
    }

    static void OnDoCastHandler(const Events::ProcessSpellEventArgs& args) {
        auto* self = Ptr();
        if (!self || !self->enabled_) return;

#ifdef NIGHTSHARP_DEBUG_ORB
        {
            std::ofstream os("c:\\Users\\Public\\nightsharp_orbwalker_debug.txt", std::ios::app);
            os << "[Tick: " << Variables::TickCount() << "] EVENT TRIGGERED: OnDoCast | SenderNetId: " << args.Sender.NetworkId
               << " | PlayerNetId: " << GameObjects::Player().NetworkId()
               << " | Name: '" << args.SpellName << "' | Slot: " << args.Slot
               << " | IsAuto: " << (args.IsAutoAttack ? "TRUE" : "FALSE") << "\n";
        }
#endif

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
            // Not our auto-attack, not a reset — ignore completely.
            return;
        }

        // OnDoCast = missile has left the bow (windup finished, projectile spawned).
        // Set MissileLaunched to enable the 60% fast-path in CanMove().
        // Clamp LastAutoAttackTick from PendingAttackTick if it was set later
        // (local issue time may be later than server-event time due to ping).
        self->MissileLaunched = true;
        if (self->PendingAttack && self->LastAutoAttackTick < self->PendingAttackTick) {
            self->LastAutoAttackTick = self->PendingAttackTick;
        }
        self->PendingAttack     = false;
        self->PendingAttackTick = 0;
        self->LastMovementOrderTick = 0;
        ++self->TotalAutoAttacks;

#ifdef NIGHTSHARP_DEBUG_ORB
        {
            std::ofstream os("c:\\Users\\Public\\nightsharp_orbwalker_debug.txt", std::ios::app);
            os << "[Tick: " << Variables::TickCount() << "] EVENT: OnDoCast (Missile Launched)"
               << " | LastAutoAttackTick: " << self->LastAutoAttackTick << "\n";
        }
#endif

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
        if (args.ForceStop && !args.HasBeenCast && args.Spellbook) {
            const auto activeSlot = Globals::Read<uint8_t>(
                args.Spellbook + Offset::SpellBookLayout::ActiveSlot);
            if (activeSlot == 64) {
#ifdef NIGHTSHARP_DEBUG_ORB
                std::ofstream os("c:\\Users\\Public\\nightsharp_orbwalker_debug.txt", std::ios::app);
                os << "[Tick: " << Variables::TickCount()
                   << "] OnStopCast: AA ForceStop (slot=64) -> ResetSwingTimer()\n";
#endif
                self->ResetSwingTimer();
            }
        }

    }

    void OnProcessSpellDelayed(const Events::ProcessSpellEventArgs& args) {
        if (Utils::AutoAttack::IsAutoAttackReset(args.SpellName)) {
            ResetSwingTimer();
            return;
        }

        if (!args.IsAutoAttack && !Utils::AutoAttack::IsAutoAttack(args.SpellName)) {
            // Strictly ignore non-auto spells — no 150ms fallback.
            return;
        }

        // OnProcessSpell = windup has started (game registered the attack server-side).
        // Record the server-adjusted tick and save the per-attack cast delay.
        int serverTick = Variables::TickCount() - (Game::Ping() / 2);
        // Clamp: if PendingAttackTick is later (we issued faster than server ack), prefer it.
        if (PendingAttack && PendingAttackTick > serverTick) {
            serverTick = PendingAttackTick;
        }
        LastAutoAttackTick = serverTick;
        PendingAttack      = false;
        PendingAttackTick  = 0;
        MissileLaunched    = false;

        auto target = ObjectManager::GetUnitByNetworkId<AttackableUnit>(
            static_cast<int>(args.TargetNetworkId));
        if (!target.IsValid()) {
            target = LastTarget.IsValid() ? LastTarget : GetTarget();
        }
        const AIBaseClient sender(args.Sender.Ptr);

#ifdef NIGHTSHARP_DEBUG_ORB
        {
            std::ofstream os("c:\\Users\\Public\\nightsharp_orbwalker_debug.txt", std::ios::app);
            os << "[Tick: " << Variables::TickCount() << "] EVENT: OnProcessSpell"
               << " | serverTick: " << serverTick
               << " | Name: '" << args.SpellName << "'\n";
        }
#endif

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
