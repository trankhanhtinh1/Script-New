#pragma once

#include "OrbwalkerSelector.h"

#include <cstdio>
#include <string>

namespace SDK {

class Orbwalker {
public:
    using EventHandler = void(*)(OrbwalkingActionArgs&);

    struct EventSlot {
        bool (*Add)(EventHandler) = nullptr;
        bool (*Remove)(EventHandler) = nullptr;

        bool operator+=(EventHandler handler) const {
            return Add ? Add(handler) : false;
        }

        bool operator-=(EventHandler handler) const {
            return Remove ? Remove(handler) : false;
        }

        bool operator()(EventHandler handler) const {
            return (*this += handler);
        }
    };

    explicit Orbwalker(Menu* parentMenu)
        : sdkImplementation_(parentMenu) {
        AddOrbwalker("SDK", &sdkImplementation_);
        SetOrbwalker("SDK");
    }

    ~Orbwalker() {
        sdkImplementation_.Dispose();
        auto it = OrbwalkingDetail::Implementations.find("SDK");
        if (it != OrbwalkingDetail::Implementations.end() && it->second == &sdkImplementation_) {
            OrbwalkingDetail::Implementations.erase(it);
        }
        if (OrbwalkingDetail::Implementation == &sdkImplementation_) {
            OrbwalkingDetail::Implementation = nullptr;
            OrbwalkingDetail::SelectedImplementationName.clear();
        }
    }

    static IOrbwalker* Implementation() {
        return OrbwalkingDetail::Implementation;
    }

    static bool AddOrbwalker(const std::string& name, IOrbwalker* implementation) {
        if (name.empty() || !implementation ||
            OrbwalkingDetail::Implementations.find(name) != OrbwalkingDetail::Implementations.end()) {
            return false;
        }
        OrbwalkingDetail::Implementations.emplace(name, implementation);
        if (!OrbwalkingDetail::Implementation) {
            OrbwalkingDetail::Implementation = implementation;
            OrbwalkingDetail::SelectedImplementationName = name;
        }
        return true;
    }

    static bool SetOrbwalker(const std::string& name) {
        auto it = OrbwalkingDetail::Implementations.find(name);
        if (it == OrbwalkingDetail::Implementations.end() || !it->second) {
            return false;
        }
        OrbwalkingDetail::Implementation = it->second;
        OrbwalkingDetail::SelectedImplementationName = name;
        return true;
    }

    static IOrbwalker* GetOrbwalker(const std::string& name) {
        auto it = OrbwalkingDetail::Implementations.find(name);
        return it != OrbwalkingDetail::Implementations.end() ? it->second : nullptr;
    }

    static const std::string& CurrentOrbwalkerName() {
        return OrbwalkingDetail::SelectedImplementationName;
    }

    static AttackableUnit ForceTarget() {
        return Current() ? Current()->ForceTarget() : AttackableUnit();
    }

    static void ForceTarget(const AttackableUnit& target) {
        if (Current()) {
            Current()->ForceTarget(target);
        }
    }

    static AttackableUnit LastTarget() {
        return Current() ? Current()->LastTarget() : AttackableUnit();
    }

    static OrbwalkingMode ActiveMode() {
        return Current() ? Current()->ActiveMode() : OrbwalkingMode::None;
    }

    static int LastAutoAttackTick() {
        return Current() ? Current()->LastAutoAttackTick() : 0;
    }

    static void LastAutoAttackTick(int value) {
        if (Current()) {
            Current()->LastAutoAttackTick(value);
        }
    }

    /// True while the local player is still in the auto-attack windup.
    /// Use this to avoid casting/moving before the attack has fired.
    static bool IsAutoAttacking() {
        return Current() && Current()->IsAutoAttacking();
    }

    /// True from attack order/attack confirm until the windup is done.
    /// Same practical check as IsAutoAttacking(), but named for timing logic.
    static bool IsWindingUp() {
        return Current() && Current()->IsWindingUp();
    }

    /// True after the current auto attack has fired.
    /// For ranged champions this means missile/do-cast was seen; for instant attacks, cast is complete.
    static bool IsAttackCastComplete() {
        return Current() && Current()->IsAttackCastComplete();
    }

    /// Milliseconds left before the current auto attack is considered fired/cancel-safe.
    /// Returns 0 when no attack is currently winding up.
    static int AttackCastDelayRemaining() {
        return Current() ? Current()->AttackCastDelayRemaining() : 0;
    }

    /// Game tick when the next auto attack should be ready.
    /// Includes attack pause/server pause and orbwalker safety delay; returns 0 if unavailable.
    static int NextAttackReadyTick() {
        return Current() ? Current()->NextAttackReadyTick() : 0;
    }

    /// Milliseconds until CanAttack() is expected to become true.
    /// Returns 0 when the attack is ready or timing is unavailable.
    static int AttackCooldownRemaining() {
        return Current() ? Current()->AttackCooldownRemaining() : 0;
    }

    static int LastMovementTick() {
        return Current() ? Current()->LastMovementTick() : 0;
    }

    static void LastMovementTick(int value) {
        if (Current()) {
            Current()->LastMovementTick(value);
        }
    }

    static bool AttackEnabled() {
        return Current() ? Current()->AttackEnabled() : false;
    }

    static void AttackEnabled(bool value) {
        if (Current()) {
            Current()->AttackEnabled(value);
        }
    }

    static bool MoveEnabled() {
        return Current() ? Current()->MoveEnabled() : false;
    }

    static void MoveEnabled(bool value) {
        if (Current()) {
            Current()->MoveEnabled(value);
        }
    }

    static void SetOrbwalkerPosition(const Vector3& position) {
        if (Current()) {
            Current()->SetOrbwalkerPosition(position);
        }
    }

    static void SetPauseTime(int time) {
        if (Current()) {
            Current()->SetPauseTime(time);
        }
    }

    static void SetServerPauseTime(int time) {
        if (Current()) {
            Current()->SetServerPauseTime(time);
        }
    }

    static void SetAttackPauseTime(int time) {
        if (Current()) {
            Current()->SetAttackPauseTime(time);
        }
    }

    static void SetAttackServerPauseTime(int time) {
        if (Current()) {
            Current()->SetAttackServerPauseTime(time);
        }
    }

    static void SetMovePauseTime(int time) {
        if (Current()) {
            Current()->SetMovePauseTime(time);
        }
    }

    static void SetMoveServerPauseTime(int time) {
        if (Current()) {
            Current()->SetMoveServerPauseTime(time);
        }
    }

    static AttackableUnit GetTarget() {
        return Current() ? Current()->GetTarget() : AttackableUnit();
    }

    static bool CanAttack() {
        return Current() && Current()->CanAttack();
    }

    static bool CanAttack(float extraWindup) {
        return Current() && Current()->CanAttack(extraWindup);
    }

    static bool CanMove() {
        return Current() && Current()->CanMove();
    }

    static bool CanMove(float extraWindup, bool disableMissileCheck = false) {
        return Current() && Current()->CanMove(extraWindup, disableMissileCheck);
    }

    static bool Attack(const AttackableUnit& target) {
        return Current() && Current()->Attack(target);
    }

    static void Move(const Vector3& position) {
        if (Current()) {
            Current()->Move(position);
        }
    }

    static void Orbwalk(const AttackableUnit& target, const Vector3& position = {}) {
        if (Current()) {
            Current()->Orbwalk(target, position);
        }
    }

    static bool ShouldWait() {
        return Current() && Current()->ShouldWait();
    }

    static void ResetAutoAttackTimer() {
        if (Current()) {
            Current()->ResetAutoAttackTimer();
        }
    }

    static bool IsAutoAttack(const std::string& spellName) {
        return OrbwalkerBase::IsAutoAttack(spellName);
    }

    static bool IsAutoAttackReset(const std::string& spellName) {
        return OrbwalkerBase::IsAutoAttackReset(spellName);
    }

    static void DebugPrint(const char* text) {
        if (OrbwalkingDetail::RuntimeInstance) {
            OrbwalkingDetail::RuntimeInstance->DebugPrint(text);
        }
    }

    static void DebugPrint(const std::string& text) {
        DebugPrint(text.c_str());
    }

    template <typename... Args>
    static void DebugPrint(const char* fmt, Args... args) {
        char buffer[kOrbwalkerDebugConsoleLineLength] = {};
        _snprintf_s(buffer, sizeof(buffer), _TRUNCATE, fmt ? fmt : "", args...);
        DebugPrint(buffer);
    }

    static void DebugClear() {
        if (OrbwalkingDetail::RuntimeInstance) {
            OrbwalkingDetail::RuntimeInstance->ClearDebugConsole();
        }
    }

    static bool AddOnBeforeAttack(EventHandler handler) {
        return OrbwalkingDetail::BeforeAttackHandlers.Add(handler);
    }

    static bool RemoveOnBeforeAttack(EventHandler handler) {
        return OrbwalkingDetail::BeforeAttackHandlers.Remove(handler);
    }

    static bool AddOnAttack(EventHandler handler) {
        return OrbwalkingDetail::AttackHandlers.Add(handler);
    }

    static bool RemoveOnAttack(EventHandler handler) {
        return OrbwalkingDetail::AttackHandlers.Remove(handler);
    }

    static bool AddOnAfterAttack(EventHandler handler) {
        return OrbwalkingDetail::AfterAttackHandlers.Add(handler);
    }

    static bool RemoveOnAfterAttack(EventHandler handler) {
        return OrbwalkingDetail::AfterAttackHandlers.Remove(handler);
    }

    static bool AddOnBeforeMove(EventHandler handler) {
        return OrbwalkingDetail::BeforeMoveHandlers.Add(handler);
    }

    static bool RemoveOnBeforeMove(EventHandler handler) {
        return OrbwalkingDetail::BeforeMoveHandlers.Remove(handler);
    }

    static bool AddOnNonKillableMinion(EventHandler handler) {
        return OrbwalkingDetail::NonKillableMinionHandlers.Add(handler);
    }

    static bool RemoveOnNonKillableMinion(EventHandler handler) {
        return OrbwalkingDetail::NonKillableMinionHandlers.Remove(handler);
    }

    static OrbwalkingActionArgs FireBeforeAttack(const AttackableUnit& target,
                                                 const char* orbwalkerName = "SDK") {
        OrbwalkingActionArgs args(OrbwalkingType::BeforeAttack, target, {}, orbwalkerName);
        OrbwalkingDetail::FireBeforeAttack(args);
        return args;
    }

    static void FireOnAttack(const AttackableUnit& target,
                             const char* orbwalkerName = "SDK") {
        OrbwalkingActionArgs args(OrbwalkingType::OnAttack, target, {}, orbwalkerName);
        OrbwalkingDetail::FireOnAttack(args);
    }

    static void FireAfterAttack(const AttackableUnit& target,
                                const char* orbwalkerName = "SDK") {
        OrbwalkingActionArgs args(OrbwalkingType::AfterAttack, target, {}, orbwalkerName);
        OrbwalkingDetail::FireAfterAttack(args);
    }

    static OrbwalkingActionArgs FirePreMove(const Vector3& position,
                                            const char* orbwalkerName = "SDK") {
        OrbwalkingActionArgs args(OrbwalkingType::Movement, {}, position, orbwalkerName);
        OrbwalkingDetail::FireBeforeMove(args);
        return args;
    }

    static void FireNonKillableMinion(const AttackableUnit& target,
                                      const char* orbwalkerName = "SDK") {
        OrbwalkingActionArgs args(OrbwalkingType::NonKillableMinion, target, {}, orbwalkerName);
        OrbwalkingDetail::FireNonKillableMinion(args);
    }

    inline static EventSlot OnBeforeAttack{ &AddOnBeforeAttack, &RemoveOnBeforeAttack };
    inline static EventSlot OnAttack{ &AddOnAttack, &RemoveOnAttack };
    inline static EventSlot OnAfterAttack{ &AddOnAfterAttack, &RemoveOnAfterAttack };
    inline static EventSlot OnBeforeMove{ &AddOnBeforeMove, &RemoveOnBeforeMove };
    inline static EventSlot OnNonKillableMinion{ &AddOnNonKillableMinion, &RemoveOnNonKillableMinion };

private:
    static IOrbwalker* Current() {
        return OrbwalkingDetail::Implementation;
    }

    OrbwalkerSelector sdkImplementation_;
};

} // namespace SDK
