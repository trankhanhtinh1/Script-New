#pragma once

#include "OrbwalkerContext.h"
#include "OrbwalkerEventBus.h"
#include "OrbwalkerMenu.h"
#include "../KuroCombatCoordinator.h"

#include "../../../sdk/Core/Game.h"
#include "../../../sdk/Core/Hud.h"
#include "../../../sdk/Enumerations/SpellSlot.h"
#include "../../../sdk/Events/Events.h"
#include "../../../sdk/GameObjects/GameObjects.h"
#include "../../../sdk/Math/HealthPrediction.h"
#include "../../../sdk/UI/Drawing.h"
#include "../../../sdk/UI/Icons.h"
#include "../../../sdk/Utils/AssetInstaller.h"
#include "../../../sdk/Utils/AutoAttack.h"
#include "../../../sdk/Wrappers/Damages/Damage.h"
#include "../../../sdk/Wrappers/TargetSelector/TargetSelector.h"
#include "../../../core/CoreControl.h"

#include <algorithm>
#include <cmath>
#include <cfloat>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace OrbwalkerKuro {

using namespace ::SDK;

class OrbwalkerBase : public IOrbwalker {
public:
    explicit OrbwalkerBase(Menu* parentMenu);
    ~OrbwalkerBase() override;

    void DebugPrint(const char* text);
    void ClearDebugConsole();

    AttackableUnit ForceTarget() const override;
    void ForceTarget(const AttackableUnit& target) override;
    AttackableUnit LastTarget() const override;
    OrbwalkingMode ActiveMode() const override;
    int LastAutoAttackTick() const override;
    void LastAutoAttackTick(int value) override;
    bool IsAutoAttacking() override;
    bool IsWindingUp() override;
    bool IsAttackCastComplete() override;
    int AttackCastDelayRemaining() override;
    int NextAttackReadyTick() override;
    int AttackCooldownRemaining() override;
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
    static void OnProcessCastSpellStatic(const Events::CastSpellEventArgs& args);
    static void OnDoCastStatic(const Events::ProcessSpellEventArgs& args);
    static void OnStopCastStatic(const Events::StopCastEventArgs& args);
    static void OnMissileCreateStatic(const Events::ObjectEventArgs& args);
    static void OnDrawStatic();
    static void OnDebugDrawStatic();

    void OnGameUpdate();
    void OnProcessSpell(const Events::ProcessSpellEventArgs& args);
    void OnProcessCastSpell(const Events::CastSpellEventArgs& args);
    void OnDoCast(const Events::ProcessSpellEventArgs& args);
    void OnStopCast(const Events::StopCastEventArgs& args);
    void OnMissileCreate(const Events::ObjectEventArgs& args);
    void OnDraw();
    void OnDebugDraw();
    bool IsLocalAutoAttack(const Events::ProcessSpellEventArgs& args) const;
    bool IsLocalAutoAttackReset(const Events::ProcessSpellEventArgs& args) const;
    bool IsLocalAutoAttackResetSlot(const ::Core::Events::ObjectInfo& sender, int slot) const;
    bool IsLocalAutoAttackMissile(const Events::ObjectEventArgs& args) const;
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
    bool EvadeOwnsActions(int now) const;
    bool EvadeBlocksMovement(int now) const;
    bool EvadeBlocksAttack(int now) const;
    float AttackSafetyMs() const;
    float MoveSafetyMs() const;
    void SnapshotAttackTimings(const AIHeroClient& player);
    int AttackCastReadyTick(const AIHeroClient& player);
    int AttackReadyTick(const AIHeroClient& player);
    void PushDebugConsoleLine(const char* text, int tick);
    void DrawLiveAttackDebugOverlay();
    void TryShowFakeClick(Hud::ClickType type, const Vector3& position, int now, int& lastTick);
    void TrackFakeCursorClick(const Vector3& position, int now);
    bool EnsureFakeCursorTexture();
    void DrawFakeCursorFallback(ImDrawList* draw, const Vec2& position, float size) const;
    void DrawFakeCursor();
    void DrawFakeVisuals();

    OrbwalkerMenu menu_;
    OrbwalkerRuntimeContext context_ = {};
};

} // namespace OrbwalkerKuro

#include "OrbwalkerLifecycle.inl"
#include "OrbwalkerTargeting.inl"
#include "OrbwalkerActions.inl"
#include "OrbwalkerVisuals.inl"
#include "OrbwalkerEventHandlers.inl"
