#pragma once

#include "OrbwalkerContext.h"
#include "OrbwalkerDatabase.h"
#include "OrbwalkerEventBus.h"
#include "OrbwalkerMenu.h"

#include "../../Core/Game.h"
#include "../../Events/Events.h"
#include "../../GameObjects/GameObjects.h"
#include "../../../Core/CoreControl.h"

#include <algorithm>
#include <cfloat>
#include <string>

namespace SDK {

class OrbwalkerBase : public IOrbwalker {
public:
    explicit OrbwalkerBase(Menu* parentMenu);
    ~OrbwalkerBase() override;

    AttackableUnit ForceTarget() const override;
    void ForceTarget(const AttackableUnit& target) override;
    AttackableUnit LastTarget() const override;
    OrbwalkingMode ActiveMode() const override;
    int LastAutoAttackTick() const override;
    void LastAutoAttackTick(int value) override;
    int LastMovementTick() const override;
    void LastMovementTick(int value) override;
    bool AttackEnabled() const override;
    void AttackEnabled(bool value) override;
    bool MoveEnabled() const override;
    void MoveEnabled(bool value) override;
    void SetOrbwalkerPosition(const Vector3& position) override;
    void SetPauseTime(int time) override;
    void SetServerPauseTime(int time) override;
    void SetAttackPauseTime(int time) override;
    void SetAttackServerPauseTime(int time) override;
    void SetMovePauseTime(int time) override;
    void SetMoveServerPauseTime(int time) override;
    AttackableUnit GetTarget() override;
    bool CanAttack() override;
    bool CanAttack(float extraWindup) override;
    bool CanMove() override;
    bool CanMove(float extraWindup, bool disableMissileCheck) override;
    bool Attack(const AttackableUnit& target) override;
    void Move(const Vector3& position) override;
    void Orbwalk(const AttackableUnit& target, const Vector3& position = {}) override;
    void ResetAutoAttackTimer() override;
    void Dispose() override;

    static bool IsAutoAttack(std::string name);
    static bool IsAutoAttackReset(std::string name);

protected:
    static int Tick();
    float GetAutoAttackRange(const AttackableUnit& target) const;

private:
    static constexpr int kMoveDelayMs = 45;
    static constexpr int kAttackRetryDelayMs = 45;
    static constexpr int kPendingTimeoutBaseMs = 150;
    static constexpr float kMaxPingLeadMs = 90.0f;
    static constexpr float kAttackSafetyMs = 5.0f;
    static constexpr float kMoveSafetyMs = 18.0f;
    static constexpr float kDefaultAttackDelayMs = 625.0f;
    static constexpr float kDefaultAttackWindupMs = 300.0f;

    static void OnGameUpdateStatic(const Events::GameUpdateEventArgs& args);
    static void OnProcessSpellStatic(const Events::ProcessSpellEventArgs& args);
    static void OnDoCastStatic(const Events::ProcessSpellEventArgs& args);

    void OnGameUpdate();
    void OnProcessSpell(const Events::ProcessSpellEventArgs& args);
    void OnDoCast(const Events::ProcessSpellEventArgs& args);
    bool IsLocalAutoAttack(const Events::ProcessSpellEventArgs& args) const;
    AttackableUnit ResolveAttackTarget(const Events::ProcessSpellEventArgs& args) const;
    void ExpirePendingAttack();
    int PendingAttackTimeoutMs() const;
    float OneWayPingMs() const;
    float AttackSafetyMs() const;
    float MoveSafetyMs() const;
    void SnapshotAttackTimings(const AIHeroClient& player);
    float GetAttackDelayMs(const AIHeroClient& player) const;
    float GetAttackWindupMs(const AIHeroClient& player) const;

    OrbwalkerMenu menu_;
    OrbwalkerRuntimeContext context_ = {};
};

} // namespace SDK

#include "OrbwalkerLifecycle.inl"
#include "OrbwalkerTargeting.inl"
#include "OrbwalkerActions.inl"
#include "OrbwalkerEventHandlers.inl"
