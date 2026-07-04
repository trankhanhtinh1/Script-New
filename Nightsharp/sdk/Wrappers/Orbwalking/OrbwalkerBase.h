#pragma once

#include "OrbwalkerContext.h"
#include "OrbwalkerDatabase.h"
#include "OrbwalkerEventBus.h"
#include "OrbwalkerMenu.h"

#include "../../Core/Game.h"
#include "../../Events/Events.h"
#include "../../GameObjects/GameObjects.h"
#include "../../Math/HealthPrediction.h"
#include "../../UI/Drawing.h"
#include "../../Utils/AutoAttack.h"
#include "../Damages/Damage.h"
#include "../TargetSelector/TargetSelector.h"
#include "../../../Core/CoreControl.h"

#include <algorithm>
#include <cmath>
#include <cfloat>
#include <cctype>
#include <string>
#include <vector>

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
    bool ShouldWait() override;
    void ResetAutoAttackTimer() override;
    void Dispose() override;

    static bool IsAutoAttack(std::string name);
    static bool IsAutoAttackReset(std::string name);

protected:
    static int Tick();
    float GetAutoAttackRange(const AttackableUnit& target) const;

private:
    static constexpr int kMoveDelayMs = 25;
    static constexpr int kMoveDuplicateDelayMs = 85;
    static constexpr int kAttackOrderDelayMs = 45;
    static constexpr int kAttackRetryDelayMs = 45;
    static constexpr int kPendingEventGraceMs = 150;
    static constexpr int kDuplicateAttackEventMs = 80;
    static constexpr float kMoveDuplicateDistance = 55.0f;
    static constexpr float kMaxPingLeadMs = 90.0f;
    static constexpr float kAttackSafetyMs = 35.0f;
    static constexpr float kHighAttackSpeedSafetyMs = 30.0f;
    static constexpr float kMoveSafetyMs = 10.0f;
    static constexpr float kRangedPreCastMoveSafetyMs = 35.0f;
    static constexpr float kDefaultAttackDelayMs = 625.0f;
    static constexpr float kDefaultAttackWindupMs = 300.0f;

    static void OnGameUpdateStatic(const Events::GameUpdateEventArgs& args);
    static void OnProcessSpellStatic(const Events::ProcessSpellEventArgs& args);
    static void OnDoCastStatic(const Events::ProcessSpellEventArgs& args);
    static void OnStopCastStatic(const Events::StopCastEventArgs& args);
    static void OnDrawStatic();

    void OnGameUpdate();
    void OnProcessSpell(const Events::ProcessSpellEventArgs& args);
    void OnDoCast(const Events::ProcessSpellEventArgs& args);
    void OnStopCast(const Events::StopCastEventArgs& args);
    void OnDraw();
    bool IsLocalAutoAttack(const Events::ProcessSpellEventArgs& args) const;
    bool IsLocalAutoAttackReset(const Events::ProcessSpellEventArgs& args) const;
    AttackableUnit ResolveAttackTarget(const Events::ProcessSpellEventArgs& args) const;
    void ClearDoCastMoveGate();
    void ClearPendingAttackState();
    void ExpirePendingAttack();
    int PendingAttackTimeoutMs() const;
    int DoCastMoveGateTimeoutMs() const;
    float OneWayPingMs() const;
    float ChampionExtraAttackDelayMs(const AIHeroClient& player) const;
    bool ChampionRequiresDoCastBeforeMove(const AIHeroClient& player) const;
    bool ChampionCanAttack(const AIHeroClient& player) const;
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
