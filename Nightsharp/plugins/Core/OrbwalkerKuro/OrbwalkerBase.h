#pragma once

#include "AzirSoldierSupport.h"
#include "OrbwalkerContext.h"
#include "OrbwalkerEventBus.h"
#include "OrbwalkerMenu.h"
#include "../../../Core/KuroCombatCoordinator.h"
#include "../KuroTargetSelector/KuroTargetActionGate.h"

#include "../../../sdk/Core/Game.h"
#include "../../../sdk/Core/Hud.h"
#include "../../../sdk/Enumerations/SpellSlot.h"
#include "../../../sdk/Enumerations/ChampionId.h"
#include "../../../sdk/Events/Events.h"
#include "../../../sdk/Events/Dash.h"
#include "../../../sdk/GameObjects/GameObjects.h"
#include "../../../sdk/Math/Collision.h"
#include "../../../sdk/Math/HealthPrediction.h"
#include "../../../sdk/UI/Drawing.h"
#include "../../../sdk/UI/Icons.h"
#include "../../../sdk/Utils/AssetInstaller.h"
#include "../../../sdk/Utils/AutoAttack.h"
#include "../../../sdk/Utils/DelayAction.h"
#include "../../../sdk/Wrappers/Damages/Damage.h"
#include "../../../sdk/Wrappers/TargetSelector/TargetSelector.h"
#include "../../../sdk/Extensions/AIBaseClientExtensions.h"
#include "../../../core/CoreControl.h"
#include "../../../core/CoreAIHeroClient.h"
#include "../../../DebugLog.h"
#include "../../../FpsDropDebug.h"

#include <algorithm>
#include <cmath>
#include <cfloat>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace ::SDK;

namespace OrbwalkerKuro {

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
    enum class AutoAttackResetMatch {
        None,
        SpellName,
        ChampionSlot,
    };

    static constexpr int kMoveDelayMs = 25;
    static constexpr int kMoveDuplicateDelayMs = 85;
    static constexpr int kAttackOrderDelayMs = 45;
    static constexpr int kAttackRetryDelayMs = 45;
    static constexpr int kPendingEventGraceMs = 150;
    static constexpr int kDuplicateAttackEventMs = 80;
    static constexpr int kPostFlashAttackGraceMs = 260;

    static constexpr float kMoveDuplicateDistance = 55.0f;
    static constexpr float kMaxPingLeadMs = 90.0f;
    // Attack safety is a fraction of the current attack cycle, clamped so it
    // stays sane at both extremes: see AttackSafetyMs().
    static constexpr float kAttackSafetyMs = 35.0f;
    static constexpr float kMinAttackSafetyMs = 12.0f;
    static constexpr float kAttackSafetyRatio = 0.05f;
    static constexpr float kMoveSafetyMs = 10.0f;
    static constexpr float kRangedPreCastMoveSafetyMs = 35.0f;
    static constexpr float kDefaultAttackDelayMs = 625.0f;
    static constexpr float kDefaultAttackWindupMs = 300.0f;
    static constexpr float kMinAttackSpeedMod = 0.2f;
    static constexpr float kMaxAttackSpeedMod = 6.0f;
    static constexpr float kCalibrationMinShift = 0.05f;

    static void OnGameUpdateStatic(const Events::GameUpdateEventArgs& args);
    static void OnProcessSpellStatic(const Events::ProcessSpellEventArgs& args);
    static void OnDoCastStatic(const Events::ProcessSpellEventArgs& args);
    static void OnStopCastStatic(const Events::StopCastEventArgs& args);
    static void OnMissileCreateStatic(const Events::ObjectEventArgs& args);
    static void OnDrawStatic();
    static void OnPlayAnimationStatic(const Events::PlayAnimationEventArgs& args);
    static void OnDashStatic(const Events::Dash::DashArgs& args);

    void OnGameUpdate();
    void ReconcileApheliosReturnMissile();
    void ReconcileRetainedObjects();
    void OnProcessSpell(const Events::ProcessSpellEventArgs& args);
    void OnDoCast(const Events::ProcessSpellEventArgs& args);
    void OnStopCast(const Events::StopCastEventArgs& args);
    void OnMissileCreate(const Events::ObjectEventArgs& args);
    void OnDraw();
    void OnPlayAnimation(const Events::PlayAnimationEventArgs& args);
    void OnDash(const Events::Dash::DashArgs& args);
    bool IsLocalAutoAttack(const Events::ProcessSpellEventArgs& args) const;
    AutoAttackResetMatch GetLocalAutoAttackResetMatch(
        const Events::ProcessSpellEventArgs& args) const;
    bool IsLocalAutoAttackResetSlot(const ::Core::Events::ObjectInfo& sender, int slot) const;
    bool IsLocalAutoAttackMissile(const Events::ObjectEventArgs& args) const;
    bool IsSpecialAfterAttack(const std::string& nameLower) const;
    AttackableUnit ResolveAttackTarget(const Events::ProcessSpellEventArgs& args) const;
    AttackableUnit ResolveAttackTarget(const Events::ObjectEventArgs& args) const;
    void ClearDoCastMoveGate();
    void ClearPendingAttackState();
    void ResetAutoAttackTimerWithReason(
        const char* reason,
        const char* source,
        const char* matchType,
        const char* championName = "",
        const char* spellName = "",
        int spellSlot = -1,
        const char* senderName = "",
        const char* missileName = "",
        std::uint32_t senderNetworkId = 0,
        std::uint32_t sourceNetworkId = 0);
    void ExpirePendingAttack();
    void CheckAttackCastedBefore();
    int PendingAttackTimeoutMs();
    int DoCastMoveGateTimeoutMs();
    float OneWayPingMs() const;
    float AttackPingLeadMs() const;
    float ChampionExtraAttackDelayMs(const AIHeroClient& player) const;
    bool ChampionRequiresDoCastBeforeMove(const AIHeroClient& player) const;
    bool ChampionCanAttack(const AIHeroClient& player) const;
    bool EvadeOwnsActions(int now) const;
    bool EvadeBlocksMovement(int now) const;
    bool EvadeBlocksAttack(int now) const;
    bool IsPostFlashAttackGraceActive(int now) const;
    void ClearPostFlashAttackGrace();

    float AttackSafetyMs() const;
    float MoveSafetyMs() const;
    float LiveAttackSpeedMod(const AIHeroClient& player) const;
    void ReadAttackTimingsFromMemory(const AIHeroClient& player);
    int AttackCastDoneTick(const AIHeroClient& player);
    int AttackReadyTick(const AIHeroClient& player);
    void TryShowFakeClick(Hud::ClickType type, const Vector3& position, int now, int& lastTick);
    void TrackFakeCursorClick(const Vector3& position, int now);
    bool EnsureFakeCursorTexture();
    void DrawFakeCursorFallback(ImDrawList* draw, const Vec2& position, float size) const;
    void DrawAutoAttackRangeFade(const AIHeroClient& player);
    void DrawAzirSoldierRanges(const AIHeroClient& player);
    void DrawFakeCursor();
    void DrawFakeVisuals();

    OrbwalkerMenu menu_;
    OrbwalkerRuntimeContext context_ = {};
};

inline float GetRealAutoAttackRange(const AIBaseClient& sender, const AttackableUnit& target = AttackableUnit()) {
    if (!sender.IsValid()) {
        return 0.0f;
    }

    float result = sender.AttackRange() + sender.BoundingRadius();
    if (target.IsValid() && !target.IsDead()) {
        const AIBaseClient targetBase(target.Handle());
        const SDK::ChampionId senderChampionId = SDK::ChampionIdFromName(sender.CharacterName().c_str());
        if (senderChampionId == SDK::ChampionId::Caitlyn &&
            (targetBase.HasBuff("CaitlynWSnare") || targetBase.HasBuff("CaitlynEMissile"))) {
            result = 1300.0f;
        } else if (senderChampionId == SDK::ChampionId::Aphelios &&
                   targetBase.HasBuff("aphelioscalibrumbonusrangedebuff") &&
                   sender.HasBuff("aphelioscalibrumbonusrangebuff")) {
            result = 1800.0f;
        }
        result += target.BoundingRadius();
    }

    return result;
}

inline float GetRealAutoAttackRange(const AttackableUnit& target) {
    const auto player = GameObjects::Player();
    if (target.Compare(player)) {
        return GetRealAutoAttackRange(player, AttackableUnit());
    }
    return GetRealAutoAttackRange(player, target);
}

} // namespace OrbwalkerKuro

#include "OrbwalkerLifecycle.inl"
#include "OrbwalkerAzir.inl"
#include "OrbwalkerTargeting.inl"
#include "OrbwalkerActions.inl"
#include "OrbwalkerVisuals.inl"
#include "OrbwalkerEventHandlers.inl"
