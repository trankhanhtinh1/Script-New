#pragma once

#include "../IPlugin.h"
#include "../../Core/CoreControl.h"
#include "../../Core/CoreRuntime.h"
#include "../../DebugLog.h"
#include "../../SDK/SDK.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

namespace Plugins {

class EnsoulsharpOrbWalker final : public SDK::IOrbwalker {
public:
    explicit EnsoulsharpOrbWalker(SDK::Menu* parentMenu) {
        BuildMenu(parentMenu);
    }

    ~EnsoulsharpOrbWalker() override {
        Dispose();
        DestroyMenu();
    }

    bool Enable() {
        if (eventsBound_) {
            return true;
        }

        disposed_ = false;
        s_instance = this;
        updateEventBound_ =
            SDK::Events::hook.OnUpdate += &EnsoulsharpOrbWalker::OnGameUpdate;
        processSpellEventBound_ =
            SDK::Events::hook.OnProcessSpell += &EnsoulsharpOrbWalker::OnProcessSpell;
        doCastEventBound_ =
            SDK::Events::hook.OnDoCast += &EnsoulsharpOrbWalker::OnDoCast;
        buffAddEventBound_ =
            SDK::Events::hook.OnBuffAdd += &EnsoulsharpOrbWalker::OnBuffAdd;
        stopCastEventBound_ =
            SDK::Events::hook.OnStopCast += &EnsoulsharpOrbWalker::OnStopCast;
        drawEventBound_ =
            SDK::Drawing::OnDraw += &EnsoulsharpOrbWalker::OnDraw;

        eventsBound_ =
            updateEventBound_ &&
            processSpellEventBound_ &&
            doCastEventBound_ &&
            buffAddEventBound_ &&
            stopCastEventBound_ &&
            drawEventBound_;
        if (!eventsBound_) {
            Dispose();
            return false;
        }

        return true;
    }

    SDK::AttackableUnit ForceTarget() const override { return forceTarget_; }
    void ForceTarget(const SDK::AttackableUnit& target) override { forceTarget_ = target; }
    SDK::AttackableUnit LastTarget() const override { return lastTarget_; }
    SDK::OrbwalkingMode ActiveMode() const override { return MenuActiveMode(); }

    int LastAutoAttackTick() const override { return lastAutoAttackTick_; }
    void LastAutoAttackTick(int value) override {
        lastAutoAttackTick_ = value;
        missileLaunched_ = false;
        lastAttackInstantWindup_ = false;
        lastAttackInstantWindupCasted_ = false;
    }

    bool IsAutoAttacking() override {
        return IsWindingUp();
    }

    bool IsWindingUp() override {
        const auto player = SDK::GameObjects::Player();
        if (!player.IsValid() || player.IsDead()) {
            return false;
        }
        if (IsWaitingForAttackConfirm()) {
            return true;
        }
        if (lastAutoAttackTick_ <= 0) {
            return false;
        }
        if ((missileLaunched_ && !lastAttackInstantWindup_) ||
            (lastAttackInstantWindup_ && lastAttackInstantWindupCasted_)) {
            return false;
        }
        return Tick() < AttackCastReadyTick(player);
    }

    bool IsAttackCastComplete() override {
        return lastAutoAttackTick_ > 0 &&
               (missileLaunched_ ||
                (lastAttackInstantWindup_ && lastAttackInstantWindupCasted_));
    }

    int AttackCastDelayRemaining() override {
        const auto player = SDK::GameObjects::Player();
        if (!player.IsValid() || player.IsDead() || !IsWindingUp()) {
            return 0;
        }
        return std::max(0, AttackCastReadyTick(player) - Tick());
    }

    int NextAttackReadyTick() override {
        const auto player = SDK::GameObjects::Player();
        if (!attackState_ || !player.IsValid() || player.IsDead()) {
            return 0;
        }
        return AttackReadyTick(player);
    }

    int AttackCooldownRemaining() override {
        const int readyTick = NextAttackReadyTick();
        return readyTick > 0 ? std::max(0, readyTick - Tick()) : 0;
    }

    int LastMovementTick() const override { return lastMovementOrderTick_; }
    void LastMovementTick(int value) override { lastMovementOrderTick_ = value; }

    bool AttackEnabled() const override { return attackState_; }
    void AttackEnabled(bool value) override { attackState_ = value; }
    bool MoveEnabled() const override { return movementState_; }
    void MoveEnabled(bool value) override { movementState_ = value; }
    void SetOrbwalkerPosition(const SDK::Vector3& position) override { orbwalkerPosition_ = position; }

    void SetPauseTime(int time) override { blockOrdersUntilTick_ = Tick() + std::max(0, time); }
    void SetServerPauseTime(int time) override { SetPauseTime(time - SDK::Game::Ping() / 2); }
    void SetAttackPauseTime(int time) override { attackPauseUntilTick_ = Tick() + std::max(0, time); }
    void SetAttackServerPauseTime(int time) override { SetAttackPauseTime(time - SDK::Game::Ping() / 2); }
    void SetMovePauseTime(int time) override { movePauseUntilTick_ = Tick() + std::max(0, time); }
    void SetMoveServerPauseTime(int time) override { SetMovePauseTime(time - SDK::Game::Ping() / 2); }

    SDK::AttackableUnit GetTarget() override {
        return GetTarget(MenuActiveMode());
    }

    bool CanAttack() override { return CanAttack(0.0f); }

    bool CanAttack(float extraWindup) override {
        const int now = Tick();
        if (!attackState_ || now < blockOrdersUntilTick_ || now < attackPauseUntilTick_) {
            return false;
        }

        const auto player = SDK::GameObjects::Player();
        if (!player.IsValid() || player.IsDead()) {
            return false;
        }

        float extraAttackDelay = 0.0f;
        if (EqualsIgnoreCase(player.CharacterName(), "Graves")) {
            if (!player.HasBuff("gravesbasicattackammo1")) {
                return false;
            }

            const float attackDelay = AttackDelayMs(player);
            extraAttackDelay =
                (attackDelay * 1.0740296828f) - 716.2381256175f - attackDelay;
        } else if (EqualsIgnoreCase(player.CharacterName(), "Jhin") &&
                   player.HasBuff("JhinPassiveReload")) {
            return false;
        }

        return static_cast<float>(now + SDK::Game::Ping() / 2 + 25) >=
               static_cast<float>(lastAutoAttackTick_) +
                   AttackDelayMs(player) +
                   extraWindup +
                   extraAttackDelay;
    }

    bool CanMove() override { return CanMove(0.0f, false); }

    bool CanMove(float extraWindup, bool disableMissileCheck) override {
        const int now = Tick();
        if (!movementState_ || now < blockOrdersUntilTick_ || now < movePauseUntilTick_) {
            return false;
        }

        const auto player = SDK::GameObjects::Player();
        if (!player.IsValid() || player.IsDead()) {
            return false;
        }

        const bool missileCheckDisabled = disableMissileCheck || !MiscMissile();
        if (missileLaunched_ && !missileCheckDisabled && !lastAttackInstantWindup_) {
            return true;
        }

        if (lastAttackInstantWindup_ && lastAttackInstantWindupCasted_) {
            return true;
        }

        return !SDK::Utils::AutoAttack::CanCancelAutoAttack(player) ||
               static_cast<float>(now + SDK::Game::Ping() / 2) >=
                   static_cast<float>(lastAutoAttackTick_) +
                       AttackWindupMs(player) +
                       extraWindup +
                       static_cast<float>(DelayWindup());
    }

    bool Attack(const SDK::AttackableUnit& target) override {
        if (Tick() < blockOrdersUntilTick_) {
            return false;
        }

        SDK::AttackableUnit attackTarget = target.IsValid() ? target : GetTarget();
        if (!InAutoAttackRange(attackTarget)) {
            return false;
        }

        SDK::OrbwalkingActionArgs beforeArgs(
            SDK::OrbwalkingType::BeforeAttack,
            attackTarget,
            attackTarget.Position(),
            kOrbwalkerName);
        SDK::OrbwalkingDetail::FireBeforeAttack(beforeArgs);
        if (!beforeArgs.Process) {
            return false;
        }

        const auto player = SDK::GameObjects::Player();
        if (SDK::Utils::AutoAttack::CanCancelAutoAttack(player)) {
            missileLaunched_ = false;
        }

        const int now = Tick();
        if (!CoreControl::IssueAttack(attackTarget.Address(), attackTarget.Position(), true)) {
            return false;
        }

        lastAutoAttackCommandTick_ = now;
        lastTarget_ = attackTarget;
        lastAttackInstantWindup_ = IsRengarInstantWindupAttack(player);
        lastAttackInstantWindupCasted_ = false;
        blockOrdersUntilTick_ = now + 70 + std::min(60, SDK::Game::Ping());
        return true;
    }

    void Move(const SDK::Vector3& position) override {
        const int now = Tick();
        if (now < blockOrdersUntilTick_ ||
            !movementState_ ||
            !position.IsValid() ||
            position.IsZero()) {
            return;
        }

        const auto player = SDK::GameObjects::Player();
        if (!player.IsValid() || player.IsDead()) {
            return;
        }

        if (now - lastMovementOrderTick_ < DelayMovement()) {
            return;
        }

        SDK::Vector3 movePosition = position;
        const SDK::Vector3 playerPosition = player.Position();

        if (BoolValue(miscAttackSpeed_, true) &&
            CoreControl::GetAttackDelay(player.Address()) < (1.0f / 2.6f) &&
            (totalAutoAttacks_ % 3) != 0 &&
            !CanMove(500.0f, true) &&
            movementState_) {
            return;
        }

        if (movePosition.Distance2D(playerPosition) < static_cast<float>(MovementExtraHold())) {
            if (!player.GetWaypoints().empty()) {
                SDK::OrbwalkingActionArgs stopArgs(
                    SDK::OrbwalkingType::StopMovement,
                    {},
                    playerPosition,
                    kOrbwalkerName);
                SDK::OrbwalkingDetail::FireBeforeMove(stopArgs);
                if (stopArgs.Process && CoreControl::StopMoving(true)) {
                    lastMovementOrderTick_ = now - 70;
                }
            }
            return;
        }

        if (movePosition.Distance2D(playerPosition) <
            player.BoundingRadius() + 100.0f) {
            movePosition = playerPosition.Extend(
                movePosition,
                player.BoundingRadius() + (RandomFloat(0.6f, 1.0f) + 0.2f) * 400.0f);
        }

        const int maximumDistance = MovementMaximumDistance();
        if (movePosition.Distance2D(playerPosition) > static_cast<float>(maximumDistance)) {
            movePosition = playerPosition.Extend(
                movePosition,
                static_cast<float>(maximumDistance + 25 - RandomInt(0, 50)));
        }

        if (MovementRandomize() && player.Distance(movePosition) > 350.0f) {
            const float angle = 2.0f * kPi * RandomFloat(0.0f, 1.0f);
            const float radius = player.BoundingRadius() / 2.0f;
            movePosition.x += radius * std::cos(angle);
            movePosition.z += radius * std::sin(angle);
            movePosition.y = SDK::NavMesh::GetHeightForPosition(movePosition);
        }

        const float pathAngle = CurrentPathAngle(player, movePosition);
        if (now - lastMovementOrderTick_ < 70 + std::min(60, SDK::Game::Ping()) &&
            pathAngle < 60.0f) {
            return;
        }
        if (pathAngle >= 60.0f && now - lastMovementOrderTick_ < 60) {
            return;
        }

        SDK::OrbwalkingActionArgs moveArgs(
            SDK::OrbwalkingType::Movement,
            {},
            movePosition,
            kOrbwalkerName);
        SDK::OrbwalkingDetail::FireBeforeMove(moveArgs);
        if (!moveArgs.Process || moveArgs.Position.IsZero()) {
            return;
        }

        if (CoreControl::IssueMove(moveArgs.Position, true)) {
            lastMovementOrderTick_ = now;
        }
    }

    void Orbwalk(const SDK::AttackableUnit& target,
                 const SDK::Vector3& position = {}) override {
        if (attackState_ && CanAttack()) {
            const SDK::AttackableUnit attackTarget =
                target.IsValid() ? target : GetTarget();
            if (InAutoAttackRange(attackTarget) && Attack(attackTarget)) {
                return;
            }
        }

        if (movementState_ && CanMove()) {
            const SDK::Vector3 movePosition =
                position.IsValid() && !position.IsZero()
                    ? position
                    : SDK::Game::CursorPosRaw();
            Move(movePosition);
        }
    }

    bool ShouldWait() override {
        const auto player = SDK::GameObjects::Player();
        if (!player.IsValid() || player.IsDead()) {
            return false;
        }

        const int predictionTime =
            static_cast<int>(AttackDelayMs(player) * kLaneClearWaitTime);
        for (const auto& minion : GetEnemyMinions()) {
            const float damage = SDK::Damage::GetAutoAttackDamage(player, minion);
            if (damage <= 0.0f) {
                continue;
            }

            const float predictedHealth = SDK::HealthPrediction::GetPrediction(
                minion,
                predictionTime,
                DelayFarm(),
                SDK::HealthPredictionType::Simulated);
            if (predictedHealth < damage) {
                return true;
            }
        }

        return false;
    }

    void ResetAutoAttackTimer() override {
        lastAutoAttackTick_ = 0;
        lastAutoAttackCommandTick_ = 0;
        missileLaunched_ = false;
        lastAttackInstantWindup_ = false;
        lastAttackInstantWindupCasted_ = false;
    }

    void Dispose() override {
        if (disposed_) {
            return;
        }

        if (drawEventBound_) {
            SDK::Drawing::OnDraw -= &EnsoulsharpOrbWalker::OnDraw;
            drawEventBound_ = false;
        }
        if (stopCastEventBound_) {
            SDK::Events::hook.OnStopCast -= &EnsoulsharpOrbWalker::OnStopCast;
            stopCastEventBound_ = false;
        }
        if (buffAddEventBound_) {
            SDK::Events::hook.OnBuffAdd -= &EnsoulsharpOrbWalker::OnBuffAdd;
            buffAddEventBound_ = false;
        }
        if (doCastEventBound_) {
            SDK::Events::hook.OnDoCast -= &EnsoulsharpOrbWalker::OnDoCast;
            doCastEventBound_ = false;
        }
        if (processSpellEventBound_) {
            SDK::Events::hook.OnProcessSpell -= &EnsoulsharpOrbWalker::OnProcessSpell;
            processSpellEventBound_ = false;
        }
        if (updateEventBound_) {
            SDK::Events::hook.OnUpdate -= &EnsoulsharpOrbWalker::OnGameUpdate;
            updateEventBound_ = false;
        }
        eventsBound_ = false;

        if (s_instance == this) {
            s_instance = nullptr;
        }
        disposed_ = true;
    }

private:
    static constexpr const char* kOrbwalkerName = "EnsoulsharpOrb";
    static constexpr float kLaneClearWaitTime = 2.0f;
    static constexpr float kPi = 3.14159265358979323846f;

    static inline EnsoulsharpOrbWalker* s_instance = nullptr;

    SDK::Menu* parentMenu_ = nullptr;
    SDK::Menu* menu_ = nullptr;
    SDK::Menu* drawingsMenu_ = nullptr;
    SDK::Menu* advancedMenu_ = nullptr;

    SDK::MenuBool* enabledOption_ = nullptr;
    SDK::MenuBool* drawAARange_ = nullptr;
    SDK::MenuBool* drawAARangeEnemy_ = nullptr;
    SDK::MenuBool* drawExtraHoldPosition_ = nullptr;
    SDK::MenuBool* drawKillableMinion_ = nullptr;
    SDK::MenuBool* drawKillableMinionFade_ = nullptr;
    SDK::MenuBool* movementRandomize_ = nullptr;
    SDK::MenuSlider* movementExtraHold_ = nullptr;
    SDK::MenuSlider* movementMaximumDistance_ = nullptr;
    SDK::MenuSlider* delayMovement_ = nullptr;
    SDK::MenuSlider* delayWindup_ = nullptr;
    SDK::MenuSlider* delayFarm_ = nullptr;
    SDK::MenuBool* prioritizeFarm_ = nullptr;
    SDK::MenuBool* prioritizeMinions_ = nullptr;
    SDK::MenuBool* prioritizeSmallJungle_ = nullptr;
    SDK::MenuBool* prioritizeWards_ = nullptr;
    SDK::MenuBool* prioritizeSpecialMinions_ = nullptr;
    SDK::MenuBool* attackWards_ = nullptr;
    SDK::MenuBool* attackBarrels_ = nullptr;
    SDK::MenuBool* attackClones_ = nullptr;
    SDK::MenuBool* attackSpecialMinions_ = nullptr;
    SDK::MenuBool* miscMissile_ = nullptr;
    SDK::MenuBool* miscAttackSpeed_ = nullptr;
    SDK::MenuKeyBind* lastHitKey_ = nullptr;
    SDK::MenuKeyBind* laneClearKey_ = nullptr;
    SDK::MenuKeyBind* hybridKey_ = nullptr;
    SDK::MenuKeyBind* comboKey_ = nullptr;

    SDK::AttackableUnit forceTarget_ = {};
    SDK::AttackableUnit lastTarget_ = {};
    SDK::AttackableUnit laneClearMinion_ = {};
    SDK::Vector3 orbwalkerPosition_ = {};

    int blockOrdersUntilTick_ = 0;
    int attackPauseUntilTick_ = 0;
    int movePauseUntilTick_ = 0;
    int lastAutoAttackCommandTick_ = 0;
    int lastAutoAttackTick_ = 0;
    int lastMovementOrderTick_ = 0;
    int totalAutoAttacks_ = 0;
    bool attackState_ = true;
    bool movementState_ = true;
    bool missileLaunched_ = false;
    bool lastAttackInstantWindup_ = false;
    bool lastAttackInstantWindupCasted_ = false;


    bool eventsBound_ = false;
    bool updateEventBound_ = false;
    bool processSpellEventBound_ = false;
    bool doCastEventBound_ = false;
    bool buffAddEventBound_ = false;
    bool stopCastEventBound_ = false;
    bool drawEventBound_ = false;
    bool disposed_ = false;
    bool ownsMenu_ = false;

    void BuildMenu(SDK::Menu* parentMenu) {
        if (!parentMenu) {
            return;
        }

        parentMenu_ = parentMenu;
        menu_ = parentMenu->GetSubMenu("ensoulsharpOrb");
        if (!menu_) {
            menu_ = parentMenu->AddSubMenu(new SDK::Menu("ensoulsharpOrb", "EnsoulsharpOrb"));
            ownsMenu_ = menu_ != nullptr;
        }
        if (!menu_) {
            return;
        }

        drawingsMenu_ = menu_->GetSubMenu("drawings");
        if (!drawingsMenu_) {
            drawingsMenu_ = menu_->AddSubMenu(new SDK::Menu("drawings", "Drawings"));
        }
        if (drawingsMenu_) {
            drawAARange_ = EnsureBool(drawingsMenu_, "drawAARange", "Auto-Attack Range", true);
            drawAARangeEnemy_ = EnsureBool(drawingsMenu_, "drawAARangeEnemy", "Auto-Attack Range Enemy", false);
            drawExtraHoldPosition_ = EnsureBool(drawingsMenu_, "drawExtraHoldPosition", "Extra Hold Position", false);
            drawKillableMinion_ = EnsureBool(drawingsMenu_, "drawKillableMinion", "Killable Minions", false);
            drawKillableMinionFade_ = EnsureBool(drawingsMenu_, "drawKillableMinionFade", "Killable Minions Fade Effect", false);
        }

        advancedMenu_ = menu_->GetSubMenu("advanced");
        if (!advancedMenu_) {
            advancedMenu_ = menu_->AddSubMenu(new SDK::Menu("advanced", "Advanced"));
        }
        if (advancedMenu_) {
            EnsureSeparator(advancedMenu_, "separatorMovement", "Movement");
            movementRandomize_ = EnsureBool(advancedMenu_, "movementRandomize", "Randomize Location", true);
            movementExtraHold_ = EnsureSlider(advancedMenu_, "movementExtraHold", "Extra Hold Position", 0, 0, 250);
            movementMaximumDistance_ = EnsureSlider(advancedMenu_, "movementMaximumDistance", "Maximum Distance", 1500, 500, 1500);

            EnsureSeparator(advancedMenu_, "separatorDelay", "Delay");
            delayMovement_ = EnsureSlider(advancedMenu_, "delayMovement", "Movement", 0, 0, 500);
            delayWindup_ = EnsureSlider(advancedMenu_, "delayWindup", "Windup", 80, 0, 200);
            delayFarm_ = EnsureSlider(advancedMenu_, "delayFarm", "Farm", 30, 0, 200);

            EnsureSeparator(advancedMenu_, "separatorPrioritization", "Prioritization");
            prioritizeFarm_ = EnsureBool(advancedMenu_, "prioritizeFarm", "Farm Over Harass", true);
            prioritizeMinions_ = EnsureBool(advancedMenu_, "prioritizeMinions", "Minions Over Objectives", false);
            prioritizeSmallJungle_ = EnsureBool(advancedMenu_, "prioritizeSmallJungle", "Small Jungle", false);
            prioritizeWards_ = EnsureBool(advancedMenu_, "prioritizeWards", "Wards", false);
            prioritizeSpecialMinions_ = EnsureBool(advancedMenu_, "prioritizeSpecialMinions", "Special Minions", false);

            EnsureSeparator(advancedMenu_, "separatorAttack", "Attack");
            attackWards_ = EnsureBool(advancedMenu_, "attackWards", "Wards", false);
            attackBarrels_ = EnsureBool(advancedMenu_, "attackBarrels", "Barrels", false);
            attackClones_ = EnsureBool(advancedMenu_, "attackClones", "Clones", false);
            attackSpecialMinions_ = EnsureBool(advancedMenu_, "attackSpecialMinions", "Special Minions", true);

            EnsureSeparator(advancedMenu_, "separatorMisc", "Miscellaneous");
            miscMissile_ = EnsureBool(advancedMenu_, "miscMissile", "Use Missile Checks", true);
            miscAttackSpeed_ = EnsureBool(advancedMenu_, "miscAttackSpeed", "Don't Kite if Attack Speed > 2.5", true);
        }

        EnsureSeparator(menu_, "separatorKeys", "Key Bindings");
        lastHitKey_ = EnsureKeyBind(menu_, "lasthitKey", "Last Hit", 'X');
        laneClearKey_ = EnsureKeyBind(menu_, "laneclearKey", "Lane Clear", 'V');
        hybridKey_ = EnsureKeyBind(menu_, "hybridKey", "Hybrid", 'C');
        comboKey_ = EnsureKeyBind(menu_, "comboKey", "Combo", VK_SPACE);
        enabledOption_ = EnsureBool(menu_, "enabledOption", "Enabled", true);
    }

    void DestroyMenu() {
        if (ownsMenu_ && parentMenu_ && menu_) {
            SDK::Menu* pluginMenu = menu_;
            for (int i = 0; i < parentMenu_->Components.size(); ++i) {
                if (parentMenu_->Components[i] != pluginMenu) {
                    continue;
                }

                // Detach only the plugin submenu; the SDK root/parent menu stays alive.
                parentMenu_->Components.erase(i);
                pluginMenu->Parent = nullptr;
                delete pluginMenu;
                break;
            }
        }

        parentMenu_ = nullptr;
        menu_ = nullptr;
        drawingsMenu_ = nullptr;
        advancedMenu_ = nullptr;
        enabledOption_ = nullptr;
        drawAARange_ = nullptr;
        drawAARangeEnemy_ = nullptr;
        drawExtraHoldPosition_ = nullptr;
        drawKillableMinion_ = nullptr;
        drawKillableMinionFade_ = nullptr;
        movementRandomize_ = nullptr;
        movementExtraHold_ = nullptr;
        movementMaximumDistance_ = nullptr;
        delayMovement_ = nullptr;
        delayWindup_ = nullptr;
        delayFarm_ = nullptr;
        prioritizeFarm_ = nullptr;
        prioritizeMinions_ = nullptr;
        prioritizeSmallJungle_ = nullptr;
        prioritizeWards_ = nullptr;
        prioritizeSpecialMinions_ = nullptr;
        attackWards_ = nullptr;
        attackBarrels_ = nullptr;
        attackClones_ = nullptr;
        attackSpecialMinions_ = nullptr;
        miscMissile_ = nullptr;
        miscAttackSpeed_ = nullptr;
        lastHitKey_ = nullptr;
        laneClearKey_ = nullptr;
        hybridKey_ = nullptr;
        comboKey_ = nullptr;
        ownsMenu_ = false;
    }

    static SDK::MenuBool* EnsureBool(
        SDK::Menu* menu,
        const char* name,
        const char* displayName,
        bool value) {
        if (auto* existing = menu->Get<SDK::MenuBool>(name)) {
            return existing;
        }
        return menu->Add(new SDK::MenuBool(name, displayName, value));
    }

    static SDK::MenuSlider* EnsureSlider(
        SDK::Menu* menu,
        const char* name,
        const char* displayName,
        int value,
        int minValue,
        int maxValue) {
        if (auto* existing = menu->Get<SDK::MenuSlider>(name)) {
            return existing;
        }
        return menu->Add(new SDK::MenuSlider(name, displayName, value, minValue, maxValue));
    }

    static SDK::MenuKeyBind* EnsureKeyBind(
        SDK::Menu* menu,
        const char* name,
        const char* displayName,
        int key) {
        if (auto* existing = menu->Get<SDK::MenuKeyBind>(name)) {
            return existing;
        }
        return menu->Add(new SDK::MenuKeyBind(
            name,
            displayName,
            key,
            SDK::KeyBindType::Press));
    }

    static SDK::MenuSeparator* EnsureSeparator(
        SDK::Menu* menu,
        const char* name,
        const char* displayName) {
        if (auto* existing = menu->Get<SDK::MenuSeparator>(name)) {
            return existing;
        }
        return menu->Add(new SDK::MenuSeparator(name, displayName));
    }

    SDK::OrbwalkingMode MenuActiveMode() const {
        if (!BoolValue(enabledOption_, true) ||
            SDK::Game::IsChatOpen() ||
            SDK::Game::IsShopOpen()) {
            return SDK::OrbwalkingMode::None;
        }

        return KeyActive(lastHitKey_, 'X')
            ? SDK::OrbwalkingMode::LastHit
            : KeyActive(laneClearKey_, 'V')
            ? SDK::OrbwalkingMode::LaneClear
            : KeyActive(hybridKey_, 'C')
            ? SDK::OrbwalkingMode::Hybrid
            : KeyActive(comboKey_, VK_SPACE)
            ? SDK::OrbwalkingMode::Combo
            : SDK::OrbwalkingMode::None;
    }

    static bool BoolValue(const SDK::MenuBool* value, bool fallback) {
        return value ? value->Value : fallback;
    }

    static int SliderValue(const SDK::MenuSlider* value, int fallback) {
        return value ? value->Value : fallback;
    }

    static bool KeyActive(const SDK::MenuKeyBind* key, int fallbackKey) {
        const int vk = key ? key->Key : fallbackKey;
        return (key && key->Active) ||
               ((::GetAsyncKeyState(vk) & 0x8000) != 0) ||
               ((::GetAsyncKeyState(fallbackKey) & 0x8000) != 0);
    }

    bool MovementRandomize() const { return BoolValue(movementRandomize_, true); }
    int MovementExtraHold() const { return SliderValue(movementExtraHold_, 0); }
    int MovementMaximumDistance() const { return SliderValue(movementMaximumDistance_, 1500); }
    int DelayMovement() const { return SliderValue(delayMovement_, 60); }
    int DelayWindup() const { return SliderValue(delayWindup_, 80); }
    int DelayFarm() const { return SliderValue(delayFarm_, 30); }
    bool PrioritizeFarm() const { return BoolValue(prioritizeFarm_, true); }
    bool PrioritizeMinions() const { return BoolValue(prioritizeMinions_, false); }
    bool PrioritizeSmallJungle() const { return BoolValue(prioritizeSmallJungle_, false); }
    bool PrioritizeWards() const { return BoolValue(prioritizeWards_, false); }
    bool PrioritizeSpecialMinions() const { return BoolValue(prioritizeSpecialMinions_, false); }
    bool AttackWards() const { return BoolValue(attackWards_, false); }
    bool AttackBarrels() const { return BoolValue(attackBarrels_, false); }
    bool AttackClones() const { return BoolValue(attackClones_, false); }
    bool AttackSpecialMinions() const { return BoolValue(attackSpecialMinions_, true); }
    bool MiscMissile() const { return BoolValue(miscMissile_, true); }

    static int Tick() {
        return SDK::Game::TickCount();
    }

    static bool EqualsIgnoreCase(const std::string& a, const char* b) {
        return b && _stricmp(a.c_str(), b) == 0;
    }

    static bool EqualsIgnoreCase(const char* a, const char* b) {
        return a && b && _stricmp(a, b) == 0;
    }

    static float AttackDelayMs(const SDK::AIBaseClient& unit) {
        return std::max(1.0f, CoreControl::GetAttackDelay(unit.Address()) * 1000.0f);
    }

    static float AttackWindupMs(const SDK::AIBaseClient& unit) {
        return std::max(1.0f, CoreControl::GetAttackWindup(unit.Address()) * 1000.0f);
    }

    bool IsWaitingForAttackConfirm() const {
        const int now = Tick();
        return lastAutoAttackCommandTick_ > lastAutoAttackTick_ &&
               now - lastAutoAttackCommandTick_ >= 0 &&
               now - lastAutoAttackCommandTick_ <= 500 + SDK::Game::Ping();
    }

    int AttackCastReadyTick(const SDK::AIHeroClient& player) const {
        if (IsWaitingForAttackConfirm()) {
            return static_cast<int>(std::ceil(
                static_cast<float>(lastAutoAttackCommandTick_) +
                AttackWindupMs(player) +
                static_cast<float>(DelayWindup())));
        }
        if (lastAutoAttackTick_ <= 0) {
            return Tick();
        }
        return static_cast<int>(std::ceil(
            static_cast<float>(lastAutoAttackTick_) +
            AttackWindupMs(player) +
            static_cast<float>(DelayWindup()) -
            static_cast<float>(SDK::Game::Ping()) * 0.5f));
    }

    int AttackReadyTick(const SDK::AIHeroClient& player) const {
        const int now = Tick();
        int readyTick = std::max(blockOrdersUntilTick_, attackPauseUntilTick_);
        if (lastAutoAttackTick_ > 0) {
            float extraAttackDelay = 0.0f;
            if (EqualsIgnoreCase(player.CharacterName(), "Graves")) {
                const float attackDelay = AttackDelayMs(player);
                extraAttackDelay =
                    (attackDelay * 1.0740296828f) - 716.2381256175f - attackDelay;
            }
            readyTick = std::max(readyTick, static_cast<int>(std::ceil(
                static_cast<float>(lastAutoAttackTick_) +
                AttackDelayMs(player) +
                extraAttackDelay -
                static_cast<float>(SDK::Game::Ping()) * 0.5f -
                25.0f)));
        }
        return std::max(now, readyTick);
    }

    static bool IsRengarInstantWindupAttack(const SDK::AIHeroClient& player) {
        return EqualsIgnoreCase(player.CharacterName(), "Rengar") &&
               (player.HasBuff("RengarQ") ||
                player.HasBuff("RengarQEmp") ||
                player.HasBuff("rengarqbase") ||
                player.HasBuff("rengarqemp"));
    }

    static float RandomFloat(float minValue, float maxValue) {
        static std::uint32_t seed = 0x31415926u;
        seed = seed * 1664525u + 1013904223u + static_cast<std::uint32_t>(Tick());
        const float unit = static_cast<float>(seed & 0x00FFFFFFu) / 16777215.0f;
        return minValue + (maxValue - minValue) * unit;
    }

    static int RandomInt(int minValue, int maxValue) {
        return minValue + static_cast<int>(
            RandomFloat(0.0f, 1.0f) * static_cast<float>(maxValue - minValue + 1));
    }

    static float PathLength(const std::vector<SDK::Vector3>& path) {
        float length = 0.0f;
        for (std::size_t i = 1; i < path.size(); ++i) {
            length += path[i - 1].Distance2D(path[i]);
        }
        return length;
    }

    static float CurrentPathAngle(
        const SDK::AIHeroClient& player,
        const SDK::Vector3& movePosition) {
        const auto currentPath = player.GetWaypoints();
        if (currentPath.size() <= 1 || PathLength(currentPath) <= 100.0f) {
            return 0.0f;
        }

        const SDK::Vector3 currentDirection = currentPath[1] - currentPath[0];
        const SDK::Vector3 wantedDirection = movePosition - player.Position();
        return currentDirection.AngleBetween(wantedDirection);
    }

    static bool IsGangplankBarrel(const SDK::AIMinionClient& minion) {
        return EqualsIgnoreCase(minion.CharacterName(), "gangplankbarrel");
    }

    static bool IsIgnoredMinion(const SDK::AIMinionClient& minion) {
        return EqualsIgnoreCase(minion.CharacterName(), "jarvanivstandard");
    }

    static bool IsValidTarget(const SDK::AttackableUnit& target, float range) {
        const auto player = SDK::GameObjects::Player();
        if (!player.IsValid() || !target.IsValid()) {
            return false;
        }
        if ((!target.IsEnemy() && target.Team() != SDK::GameObjectTeam::Neutral) ||
            (!target.IsZombie() && target.IsDead())) {
            return false;
        }
        if (!target.IsVisible() || !target.IsTargetable() || target.IsInvulnerable()) {
            return false;
        }
        if (range < FLT_MAX * 0.5f &&
            player.Position().DistanceSqr2D(target.Position()) > range * range) {
            return false;
        }
        return true;
    }

    static bool IsValidMinionTarget(const SDK::AIMinionClient& minion, float range) {
        return minion.IsValid() &&
               !minion.IsPlant() &&
               !IsIgnoredMinion(minion) &&
               IsValidTarget(SDK::AttackableUnit(minion.Handle()), range);
    }

    static bool InAutoAttackRange(const SDK::AttackableUnit& target) {
        return target.IsValid() && IsValidTarget(target, AutoAttackRange(target));
    }

    static float AutoAttackRange(const SDK::AttackableUnit& target) {
        const auto player = SDK::GameObjects::Player();
        return SDK::Utils::AutoAttack::GetRealAutoAttackRange(player, target);
    }

    void AddUniqueMinion(std::vector<SDK::AIMinionClient>& minions,
                         const SDK::AIMinionClient& minion) const {
        const int netId = minion.NetworkId();
        const auto exists = std::any_of(
            minions.begin(),
            minions.end(),
            [&](const SDK::AIMinionClient& item) {
                return item.NetworkId() == netId;
            });
        if (!exists) {
            minions.push_back(minion);
        }
    }

    static bool IsSpecialMinionName(const SDK::AIMinionClient& minion) {
        const std::string name = minion.CharacterName();
        return EqualsIgnoreCase(name, "annietibbers") ||
               EqualsIgnoreCase(name, "elisespiderling") ||
               EqualsIgnoreCase(name, "heimertyellow") ||
               EqualsIgnoreCase(name, "heimertblue") ||
               EqualsIgnoreCase(name, "ivernminion") ||
               EqualsIgnoreCase(name, "malzaharvoidling") ||
               EqualsIgnoreCase(name, "shacobox") ||
               EqualsIgnoreCase(name, "teemomushroom") ||
               EqualsIgnoreCase(name, "yorickghoulmelee") ||
               EqualsIgnoreCase(name, "yorickbigghoul") ||
               EqualsIgnoreCase(name, "zyrathornplant") ||
               EqualsIgnoreCase(name, "zyragraspingplant");
    }

    static bool IsCloneName(const SDK::AIMinionClient& minion) {
        const std::string name = minion.CharacterName();
        return EqualsIgnoreCase(name, "leblanc") ||
               EqualsIgnoreCase(name, "monkeyking") ||
               EqualsIgnoreCase(name, "neeko") ||
               EqualsIgnoreCase(name, "shaco");
    }

    std::vector<SDK::AIMinionClient> GetEnemyMinions(float range = 0.0f) const {
        std::vector<SDK::AIMinionClient> result;
        for (const auto& minion : SDK::GameObjects::EnemyMinions()) {
            const float effectiveRange = range > 0.0f ? range : AutoAttackRange(minion);
            if (IsValidMinionTarget(minion, effectiveRange) && !IsIgnoredMinion(minion)) {
                AddUniqueMinion(result, minion);
            }
        }
        return result;
    }

    static void OrderEnemyMinions(std::vector<SDK::AIMinionClient>& minions) {
        std::stable_sort(
            minions.begin(),
            minions.end(),
            [](const SDK::AIMinionClient& left, const SDK::AIMinionClient& right) {
                const bool leftSiege = SDK::HasFlag(left.GetMinionType(), SDK::MinionTypes::Siege);
                const bool rightSiege = SDK::HasFlag(right.GetMinionType(), SDK::MinionTypes::Siege);
                if (leftSiege != rightSiege) {
                    return leftSiege;
                }

                const bool leftSuper = SDK::HasFlag(left.GetMinionType(), SDK::MinionTypes::Super);
                const bool rightSuper = SDK::HasFlag(right.GetMinionType(), SDK::MinionTypes::Super);
                if (leftSuper != rightSuper) {
                    return !leftSuper;
                }

                if (std::fabs(left.Health() - right.Health()) > FLT_EPSILON) {
                    return left.Health() < right.Health();
                }

                return left.MaxHealth() > right.MaxHealth();
            });
    }

    void OrderJungleMinions(std::vector<SDK::AIMinionClient>& minions) const {
        std::stable_sort(
            minions.begin(),
            minions.end(),
            [&](const SDK::AIMinionClient& left, const SDK::AIMinionClient& right) {
                return PrioritizeSmallJungle()
                    ? left.MaxHealth() < right.MaxHealth()
                    : left.MaxHealth() > right.MaxHealth();
            });
    }

    std::vector<SDK::AIMinionClient> GetMinionsForMode(SDK::OrbwalkingMode mode) const {
        const bool includeOrdinaryMinions = mode != SDK::OrbwalkingMode::Combo;
        const bool attackWards = AttackWards();
        const bool attackClones = AttackClones();
        const bool attackSpecialMinions = AttackSpecialMinions();
        const bool prioritizeWards = PrioritizeWards();
        const bool prioritizeSpecialMinions = PrioritizeSpecialMinions();

        std::vector<SDK::AIMinionClient> ordinaryList;
        std::vector<SDK::AIMinionClient> specialList;
        std::vector<SDK::AIMinionClient> cloneList;
        std::vector<SDK::AIMinionClient> wardList;
        std::vector<SDK::AIMinionClient> finalList;

        for (const auto& minion : SDK::GameObjects::EnemyMinions()) {
            if (!IsValidMinionTarget(minion, AutoAttackRange(minion))) {
                continue;
            }

            if (includeOrdinaryMinions && minion.IsMinion()) {
                AddUniqueMinion(ordinaryList, minion);
            } else if (attackSpecialMinions && IsSpecialMinionName(minion)) {
                AddUniqueMinion(specialList, minion);
            } else if (attackClones && IsCloneName(minion)) {
                AddUniqueMinion(cloneList, minion);
            }
        }

        if (attackSpecialMinions) {
            for (const auto& minion : SDK::GameObjects::EnemySpecialMinions()) {
                if (IsValidMinionTarget(minion, AutoAttackRange(minion))) {
                    AddUniqueMinion(specialList, minion);
                }
            }
        }

        if (attackClones) {
            for (const auto& clone : SDK::GameObjects::EnemyClones()) {
                if (IsValidMinionTarget(clone, AutoAttackRange(clone))) {
                    AddUniqueMinion(cloneList, clone);
                }
            }
        }

        if (includeOrdinaryMinions) {
            OrderEnemyMinions(ordinaryList);

            std::vector<SDK::AIMinionClient> jungleList;
            for (const auto& jungle : SDK::GameObjects::Jungle()) {
                if (IsValidMinionTarget(jungle, AutoAttackRange(jungle)) &&
                    !IsGangplankBarrel(jungle)) {
                    AddUniqueMinion(jungleList, jungle);
                }
            }
            OrderJungleMinions(jungleList);
            for (const auto& jungle : jungleList) {
                AddUniqueMinion(ordinaryList, jungle);
            }
        }

        if (attackWards) {
            for (const auto& ward : SDK::GameObjects::EnemyWards()) {
                if (IsValidMinionTarget(ward, AutoAttackRange(ward))) {
                    AddUniqueMinion(wardList, ward);
                }
            }
        }

        auto append = [&](const std::vector<SDK::AIMinionClient>& values) {
            for (const auto& value : values) {
                AddUniqueMinion(finalList, value);
            }
        };

        if (attackWards && prioritizeWards &&
            attackSpecialMinions && prioritizeSpecialMinions) {
            append(wardList);
            append(specialList);
            append(ordinaryList);
        } else if (attackSpecialMinions && prioritizeSpecialMinions) {
            append(specialList);
            append(ordinaryList);
            append(wardList);
        } else if (attackWards && prioritizeWards) {
            append(wardList);
            append(ordinaryList);
            append(specialList);
        } else {
            append(ordinaryList);
            append(specialList);
            append(wardList);
        }

        if (AttackBarrels()) {
            for (const auto& jungle : SDK::GameObjects::Jungle()) {
                if (IsValidMinionTarget(jungle, AutoAttackRange(jungle)) &&
                    jungle.Health() <= 1.0f &&
                    IsGangplankBarrel(jungle)) {
                    AddUniqueMinion(finalList, jungle);
                }
            }
        }

        if (attackClones) {
            append(cloneList);
        }

        finalList.erase(
            std::remove_if(
                finalList.begin(),
                finalList.end(),
                [](const SDK::AIMinionClient& minion) {
                    return IsIgnoredMinion(minion);
                }),
            finalList.end());
        return finalList;
    }

    SDK::AttackableUnit GetHeroTarget() const {
        const auto player = SDK::GameObjects::Player();
        if (!player.IsValid()) {
            return {};
        }

        if (auto* selector = SDK::TargetSelector::Instance()) {
            const auto hero = selector->GetTarget(-1.0f, SDK::DamageType::Physical);
            const SDK::AttackableUnit target(hero.Handle());
            if (InAutoAttackRange(target)) {
                return target;
            }
        }

        SDK::AttackableUnit best;
        float bestDistance = FLT_MAX;
        for (const auto& hero : SDK::GameObjects::EnemyHeroes()) {
            const SDK::AttackableUnit target(hero.Handle());
            if (!InAutoAttackRange(target)) {
                continue;
            }

            const float distance = player.Distance(hero);
            if (distance < bestDistance) {
                best = target;
                bestDistance = distance;
            }
        }

        return best;
    }

    bool CanLastHitMinion(const SDK::AIMinionClient& minion) const {
        const auto player = SDK::GameObjects::Player();
        const float damage = SDK::Damage::GetAutoAttackDamage(player, minion);
        if (damage <= 0.0f) {
            return false;
        }

        if (minion.Health() < damage) {
            return true;
        }

        if (minion.MaxHealth() <= 10.0f) {
            return minion.Health() <= 1.0f;
        }

        const int timeToHit = static_cast<int>(
            std::max(0.0f, SDK::Utils::AutoAttack::GetTimeToHit(minion)));
        const float predictedHealth = SDK::HealthPrediction::GetPrediction(
            minion,
            timeToHit,
            DelayFarm());
        return predictedHealth < damage;
    }

    SDK::AttackableUnit GetKillableMinion(
        const std::vector<SDK::AIMinionClient>& minions) const {
        SDK::AIMinionClient best;
        float bestHealth = FLT_MAX;
        for (const auto& minion : minions) {
            if (minion.Health() < bestHealth && CanLastHitMinion(minion)) {
                best = minion;
                bestHealth = minion.Health();
            }
        }
        return best.IsValid() ? SDK::AttackableUnit(best.Handle()) : SDK::AttackableUnit();
    }

    bool ShouldWaitUnderTurret(const SDK::AIMinionClient& noneKillableMinion = {}) const {
        const auto player = SDK::GameObjects::Player();
        if (!player.IsValid() || player.IsDead()) {
            return false;
        }

        for (const auto& minion : GetEnemyMinions()) {
            if (noneKillableMinion.IsValid() &&
                noneKillableMinion.NetworkId() == minion.NetworkId()) {
                continue;
            }
            if (!InAutoAttackRange(SDK::AttackableUnit(minion.Handle()))) {
                continue;
            }

            const float damage = SDK::Damage::GetAutoAttackDamage(player, minion);
            if (damage <= 0.0f) {
                continue;
            }

            const int predictionTime = static_cast<int>(
                AttackDelayMs(player) + SDK::Utils::AutoAttack::GetTimeToHit(minion));
            const float predictedHealth = SDK::HealthPrediction::GetPrediction(
                minion,
                predictionTime,
                DelayFarm(),
                SDK::HealthPredictionType::Simulated);
            if (predictedHealth < damage) {
                return true;
            }
        }

        return false;
    }

    static float TurretProjectileSpeed(const SDK::AIBaseClient& turret) {
        return turret.IsMelee() ? FLT_MAX : 2000.0f;
    }

    static float TurretDamage(const SDK::AIBaseClient& turret,
                              const SDK::AIMinionClient& minion) {
        return SDK::Prediction::Health::GetAutoAttackDamage(turret, minion);
    }

    struct UnderTurretResult {
        bool handled = false;
        SDK::AttackableUnit target = {};
    };

    UnderTurretResult GetUnderTurretTarget(
        const std::vector<SDK::AIMinionClient>& minions) const {
        (void)minions;
        // REMOVED: Turret farm logic disabled by user request.
        return {};
        /*
        std::vector<SDK::AIMinionClient> turretMinions;
        for (const auto& minion : minions) {
            if (minion.IsMinion() && minion.IsUnderAllyTurret()) {
                turretMinions.push_back(minion);
            }
        }

        if (turretMinions.empty()) {
            return {};
        }

        UnderTurretResult result;
        result.handled = true;

        SDK::AIMinionClient turretMinion;
        for (const auto& minion : turretMinions) {
            if (SDK::HealthPrediction::HasTurretAggro(minion)) {
                turretMinion = minion;
                break;
            }
        }

        const auto player = SDK::GameObjects::Player();
        if (turretMinion.IsValid()) {
            SDK::AIMinionClient farmUnderTurretMinion;
            SDK::AIMinionClient noneKillableMinion;
            int hpLeftBeforeDie = 0;
            int hpLeft = 0;
            int turretAttackCount = 0;
            const SDK::AIBaseClient turret =
                SDK::HealthPrediction::GetAggroTurret(turretMinion);

            if (turret.IsValid()) {
                const int turretStartTick =
                    SDK::HealthPrediction::TurretAggroStartTick(turretMinion);
                const float projectileSpeed = TurretProjectileSpeed(turret) + 70.0f;
                const int travelTime = projectileSpeed >= FLT_MAX * 0.5f
                    ? 0
                    : static_cast<int>(
                        1000.0f *
                        std::max(0.0f, turretMinion.Distance(turret) - turret.BoundingRadius()) /
                        std::max(1.0f, projectileSpeed));
                const int turretLandTick =
                    turretStartTick + static_cast<int>(AttackWindupMs(turret)) + travelTime;
                const float turretAttackDelay = AttackDelayMs(turret);

                for (float i = static_cast<float>(turretLandTick + 50);
                     i < static_cast<float>(turretLandTick) + (3.0f * turretAttackDelay) + 50.0f;
                     i += turretAttackDelay) {
                    const int time = static_cast<int>(i) - Tick() + (SDK::Game::Ping() / 2);
                    const int predictedHealth = static_cast<int>(
                        SDK::HealthPrediction::GetPrediction(
                            turretMinion,
                            time > 0 ? time : 0,
                            70,
                            SDK::HealthPredictionType::Simulated));
                    if (predictedHealth > 0) {
                        hpLeft = predictedHealth;
                        ++turretAttackCount;
                        continue;
                    }

                    hpLeftBeforeDie = hpLeft;
                    hpLeft = 0;
                    break;
                }

                if (hpLeft == 0 && turretAttackCount != 0 && hpLeftBeforeDie != 0) {
                    const int damage = static_cast<int>(
                        SDK::Damage::GetAutoAttackDamage(player, turretMinion));
                    const int hits = damage > 0 ? hpLeftBeforeDie / damage : 0;
                    const float playerAttackDelay = AttackDelayMs(player);
                    const int timeBeforeDie =
                        turretLandTick +
                        ((turretAttackCount + 1) * static_cast<int>(turretAttackDelay)) -
                        Tick();
                    const int attackReadyTick =
                        lastAutoAttackTick_ + static_cast<int>(playerAttackDelay);
                    const int attackReadyGate = Tick() + (SDK::Game::Ping() / 2) + 25;
                    const int timeUntilAttackReady =
                        attackReadyTick > attackReadyGate
                            ? attackReadyTick - attackReadyGate
                            : 0;
                    const float timeToLandAttack =
                        SDK::Utils::AutoAttack::GetTimeToHit(turretMinion);
                    const float neededTime =
                        (static_cast<float>(hits) * playerAttackDelay) +
                        static_cast<float>(timeUntilAttackReady) +
                        timeToLandAttack;

                    if (hits >= 1 && neededTime < static_cast<float>(timeBeforeDie)) {
                        farmUnderTurretMinion = turretMinion;
                    } else if (hits >= 1 && neededTime > static_cast<float>(timeBeforeDie)) {
                        noneKillableMinion = turretMinion;
                    }
                } else if (hpLeft == 0 &&
                           turretAttackCount == 0 &&
                           hpLeftBeforeDie == 0) {
                    noneKillableMinion = turretMinion;
                }

                if (ShouldWaitUnderTurret(noneKillableMinion)) {
                    return result;
                }
                if (farmUnderTurretMinion.IsValid()) {
                    result.target = SDK::AttackableUnit(farmUnderTurretMinion.Handle());
                    return result;
                }

                for (const auto& minion : turretMinions) {
                    if (minion.NetworkId() == turretMinion.NetworkId() ||
                        SDK::HealthPrediction::HasMinionAggro(minion)) {
                        continue;
                    }

                    const int turretDamage = static_cast<int>(TurretDamage(turret, minion));
                    const int playerDamage = static_cast<int>(
                        SDK::Damage::GetAutoAttackDamage(player, minion));
                    if (turretDamage > 0 &&
                        playerDamage > 0 &&
                        (static_cast<int>(minion.Health()) % turretDamage) > playerDamage) {
                        result.target = SDK::AttackableUnit(minion.Handle());
                        return result;
                    }
                }
            }

            return result;
        }

        if (ShouldWaitUnderTurret()) {
            return result;
        }

        for (const auto& minion : turretMinions) {
            if (SDK::HealthPrediction::HasMinionAggro(minion)) {
                continue;
            }

            SDK::AIBaseClient turret;
            for (const auto& allyTurret : SDK::GameObjects::AllyTurrets()) {
                if (allyTurret.IsValid() &&
                    !allyTurret.IsDead() &&
                    allyTurret.Position().DistanceSqr2D(minion.Position()) <= 950.0f * 950.0f) {
                    turret = allyTurret;
                    break;
                }
            }
            if (!turret.IsValid()) {
                continue;
            }

            const int turretDamage = static_cast<int>(TurretDamage(turret, minion));
            const int playerDamage = static_cast<int>(
                SDK::Damage::GetAutoAttackDamage(player, minion));
            if (turretDamage > 0 &&
                playerDamage > 0 &&
                (static_cast<int>(minion.Health()) % turretDamage) > playerDamage) {
                result.target = SDK::AttackableUnit(minion.Handle());
                return result;
            }
        }

        return result;
        */
    }

    SDK::AttackableUnit GetTarget(SDK::OrbwalkingMode mode) {
        const auto player = SDK::GameObjects::Player();
        if (!player.IsValid() || player.IsDead() || mode == SDK::OrbwalkingMode::None) {
            return {};
        }

        if ((mode == SDK::OrbwalkingMode::Hybrid ||
             mode == SDK::OrbwalkingMode::LaneClear) &&
            !PrioritizeFarm()) {
            const SDK::AttackableUnit hero = GetHeroTarget();
            if (hero.IsValid()) {
                return hero;
            }
        }

        const auto minions = GetMinionsForMode(mode);

        const SDK::AttackableUnit killable = GetKillableMinion(minions);
        if ((mode == SDK::OrbwalkingMode::LaneClear ||
             mode == SDK::OrbwalkingMode::Hybrid ||
             mode == SDK::OrbwalkingMode::LastHit) &&
            killable.IsValid()) {
            return killable;
        }

        if (forceTarget_.IsValid() && InAutoAttackRange(forceTarget_)) {
            return forceTarget_;
        }

        // REMOVED: Turret/structure targeting disabled by user request.
        /*
        if (mode == SDK::OrbwalkingMode::LaneClear &&
            (!PrioritizeMinions() || minions.empty())) {
            for (const auto& turret : SDK::GameObjects::EnemyTurrets()) {
                const SDK::AttackableUnit target(turret.Handle());
                if (InAutoAttackRange(target)) {
                    return target;
                }
            }
            for (const auto& inhibitor : SDK::GameObjects::EnemyInhibitors()) {
                const SDK::AttackableUnit target(inhibitor.Handle());
                if (InAutoAttackRange(target)) {
                    return target;
                }
            }
            const auto nexus = SDK::GameObjects::EnemyNexus();
            const SDK::AttackableUnit nexusTarget(nexus.Handle());
            if (InAutoAttackRange(nexusTarget)) {
                return nexusTarget;
            }
        }
        */

        if (mode != SDK::OrbwalkingMode::LastHit) {
            const SDK::AttackableUnit hero = GetHeroTarget();
            if (hero.IsValid()) {
                return hero;
            }
        }

        if (mode == SDK::OrbwalkingMode::LaneClear ||
            mode == SDK::OrbwalkingMode::Hybrid) {
            for (const auto& minion : minions) {
                if (minion.Team() == SDK::GameObjectTeam::Neutral) {
                    return SDK::AttackableUnit(minion.Handle());
                }
            }
        }

        if (mode == SDK::OrbwalkingMode::LaneClear ||
            mode == SDK::OrbwalkingMode::Hybrid ||
            mode == SDK::OrbwalkingMode::LastHit) {
            const UnderTurretResult underTurret = GetUnderTurretTarget(minions);
            if (underTurret.handled) {
                return underTurret.target;
            }
        }

        if (mode == SDK::OrbwalkingMode::LaneClear) {
            if (!ShouldWait()) {
                const SDK::AIMinionClient current(laneClearMinion_.Handle());
                if (current.IsValid() && InAutoAttackRange(SDK::AttackableUnit(current.Handle()))) {
                    if (current.MaxHealth() <= 10.0f) {
                        return SDK::AttackableUnit(current.Handle());
                    }

                    const float predictedHealth = SDK::HealthPrediction::GetPrediction(
                        current,
                        static_cast<int>(AttackDelayMs(player) * kLaneClearWaitTime),
                        DelayFarm(),
                        SDK::HealthPredictionType::Simulated);
                    const float damage = SDK::Damage::GetAutoAttackDamage(player, current);
                    if (predictedHealth >= 2.0f * damage ||
                        std::fabs(predictedHealth - current.Health()) < FLT_EPSILON) {
                        return SDK::AttackableUnit(current.Handle());
                    }
                }

                for (const auto& minion : minions) {
                    if (minion.Team() == SDK::GameObjectTeam::Neutral) {
                        continue;
                    }
                    if (minion.MaxHealth() <= 10.0f) {
                        laneClearMinion_ = SDK::AttackableUnit(minion.Handle());
                        return SDK::AttackableUnit(minion.Handle());
                    }

                    const float predictedHealth = SDK::HealthPrediction::GetPrediction(
                        minion,
                        static_cast<int>(AttackDelayMs(player) * kLaneClearWaitTime),
                        DelayFarm(),
                        SDK::HealthPredictionType::Simulated);
                    const float damage = SDK::Damage::GetAutoAttackDamage(player, minion);
                    if (predictedHealth >= 2.0f * damage ||
                        std::fabs(predictedHealth - minion.Health()) < FLT_EPSILON) {
                        laneClearMinion_ = SDK::AttackableUnit(minion.Handle());
                        return SDK::AttackableUnit(minion.Handle());
                    }
                }
            }
        }

        if (mode == SDK::OrbwalkingMode::Combo &&
            !minions.empty() &&
            !HasEnemyHeroNearAutoAttackRange()) {
            return SDK::AttackableUnit(minions.front().Handle());
        }

        return {};
    }

    bool HasEnemyHeroNearAutoAttackRange() const {
        for (const auto& enemy : SDK::GameObjects::EnemyHeroes()) {
            if (!enemy.IsValid() || enemy.IsDead()) {
                continue;
            }
            const float range = SDK::Utils::AutoAttack::GetRealAutoAttackRange(enemy) * 2.0f;
            if (SDK::Extensions::IsValidTarget(enemy, range)) {
                return true;
            }
        }
        return false;
    }

    SDK::AttackableUnit ResolveAttackTarget(
        const SDK::Events::ProcessSpellEventArgs& args) const {
        if (args.Target.IsValid()) {
            return SDK::AttackableUnit(args.Target.Ptr);
        }
        if (args.TargetNetworkId != 0 && args.TargetNetworkId != 0xFFFFFFFFu) {
            return SDK::ObjectManager::GetUnitByNetworkId<SDK::AttackableUnit>(
                static_cast<int>(args.TargetNetworkId));
        }
        return {};
    }

    void HandleGameUpdate() {
        const auto player = SDK::GameObjects::Player();
        if (!player.IsValid() ||
            player.IsDead() ||
            SDK::Extensions::IsCastingInterruptableSpell(player, true)) {
            return;
        }

        const SDK::OrbwalkingMode mode = MenuActiveMode();
        if (mode == SDK::OrbwalkingMode::None) {
            return;
        }

        const SDK::Vector3 position =
            orbwalkerPosition_.IsValid() && !orbwalkerPosition_.IsZero()
                ? orbwalkerPosition_
                : SDK::Game::CursorPosRaw();
        Orbwalk(GetTarget(mode), position);
    }

    void HandleProcessSpell(const SDK::Events::ProcessSpellEventArgs& args) {
        if (!SDK::Events::IsLocalPlayer(args.Sender)) {
            return;
        }

        if (SDK::Game::Ping() <= 30) {
            SDK::Utils::DelayAction::Add(30, [args]() {
                if (s_instance) {
                    s_instance->HandleProcessSpellDelayed(args);
                }
            });
            return;
        }

        HandleProcessSpellDelayed(args);
    }

    void HandleProcessSpellDelayed(const SDK::Events::ProcessSpellEventArgs& args) {
        const bool autoAttack =
            args.IsAutoAttack || SDK::Utils::AutoAttack::IsAutoAttack(args.SpellName);
        const bool attackReset = SDK::Utils::AutoAttack::IsAutoAttackReset(args.SpellName);
        if (attackReset && !autoAttack) {
            ResetAutoAttackTimer();
            return;
        }

        if (!autoAttack) {
            return;
        }

        missileLaunched_ = true;
        const SDK::AttackableUnit target = ResolveAttackTarget(args);
        SDK::OrbwalkingActionArgs afterArgs(
            SDK::OrbwalkingType::AfterAttack,
            target,
            target.IsValid() ? target.Position() : SDK::Vector3(),
            kOrbwalkerName);
        SDK::OrbwalkingDetail::FireAfterAttack(afterArgs);
    }

    void HandleDoCast(const SDK::Events::ProcessSpellEventArgs& args) {
        if (!SDK::Events::IsLocalPlayer(args.Sender)) {
            return;
        }

        const bool autoAttack =
            args.IsAutoAttack || SDK::Utils::AutoAttack::IsAutoAttack(args.SpellName);
        if (autoAttack) {
            const SDK::AttackableUnit target = ResolveAttackTarget(args);
            lastAutoAttackTick_ = Tick() - SDK::Game::Ping() / 2;
            missileLaunched_ = false;
            lastMovementOrderTick_ = 0;
            if (lastAttackInstantWindup_) {
                lastAttackInstantWindupCasted_ = true;
            }
            ++totalAutoAttacks_;

            if (target.IsValid()) {
                if (!target.Compare(lastTarget_)) {
                    lastTarget_ = target;
                }

                SDK::OrbwalkingActionArgs attackArgs(
                    SDK::OrbwalkingType::OnAttack,
                    target,
                    target.Position(),
                    kOrbwalkerName);
                SDK::OrbwalkingDetail::FireOnAttack(attackArgs);
            }
        }

        if (SDK::Utils::AutoAttack::IsAutoAttackReset(args.SpellName)) {
            ResetAutoAttackTimer();
        }
    }

    void HandleBuffAdd(const SDK::Events::BuffEventArgs& args) {
        if (SDK::Events::IsLocalPlayer(args.Sender) &&
            EqualsIgnoreCase(args.BuffName, "sonapassiveattack")) {
            ResetAutoAttackTimer();
        }
    }

    void HandleStopCast(const SDK::Events::StopCastEventArgs& args) {
        if (SDK::Events::IsLocalPlayer(args.Sender) &&
            args.DestroyMissile &&
            args.KeepAnimationPlaying) {
            ResetAutoAttackTimer();
        }
    }

    void Draw() const {
        const auto player = SDK::GameObjects::Player();
        if (!BoolValue(enabledOption_, true) || !player.IsValid() || player.IsDead()) {
            return;
        }

        if (BoolValue(drawAARange_, true)) {
            SDK::Drawing::DrawCircle(
                player.Position(),
                SDK::Utils::AutoAttack::GetRealAutoAttackRange(player),
                0xFF00BFFFu,
                1.5f,
                64);
        }

        if (BoolValue(drawExtraHoldPosition_, false)) {
            SDK::Drawing::DrawCircle(
                player.Position(),
                player.BoundingRadius() + static_cast<float>(MovementExtraHold()),
                0xFF800080u,
                1.5f,
                48);
        }

        if (BoolValue(drawAARangeEnemy_, false)) {
            for (const auto& enemy : SDK::GameObjects::EnemyHeroes()) {
                if (!enemy.IsValid() || enemy.IsDead() || !enemy.IsVisible()) {
                    continue;
                }
                SDK::Drawing::DrawCircle(
                    enemy.Position(),
                    SDK::Utils::AutoAttack::GetRealAutoAttackRange(enemy, player),
                    0xFF00BFFFu,
                    1.5f,
                    64);
            }
        }

        if (!BoolValue(drawKillableMinion_, false)) {
            return;
        }

        const float range = SDK::Utils::AutoAttack::GetRealAutoAttackRange(player) * 2.0f;
        const float rangeSqr = range * range;
        for (const auto& minion : SDK::GameObjects::EnemyMinions()) {
            if (!IsValidMinionTarget(minion, range) ||
                player.Position().DistanceSqr2D(minion.Position()) > rangeSqr) {
                continue;
            }

            const float damage = SDK::Damage::GetAutoAttackDamage(player, minion);
            if (damage <= 0.0f) {
                continue;
            }

            if (BoolValue(drawKillableMinionFade_, false)) {
                if (minion.Health() >= damage * 2.0f) {
                    continue;
                }
                const int blue = static_cast<int>(
                    std::clamp(255.0f - minion.Health() * 2.0f, 0.0f, 255.0f));
                SDK::Drawing::DrawCircle(
                    minion.Position(),
                    minion.BoundingRadius() * 2.0f,
                    0xFF00FF00u | static_cast<std::uint32_t>(blue),
                    1.5f,
                    32);
            } else if (CanLastHitMinion(minion)) {
                SDK::Drawing::DrawCircle(
                    minion.Position(),
                    minion.BoundingRadius() * 2.0f,
                    0xFF00FF00u,
                    1.5f,
                    32);
            }
        }
    }

    static void OnGameUpdate(const SDK::Events::GameUpdateEventArgs&) {
        if (s_instance) {
            s_instance->HandleGameUpdate();
        }
    }

    static void OnProcessSpell(const SDK::Events::ProcessSpellEventArgs& args) {
        if (s_instance) {
            s_instance->HandleProcessSpell(args);
        }
    }

    static void OnDoCast(const SDK::Events::ProcessSpellEventArgs& args) {
        if (s_instance) {
            s_instance->HandleDoCast(args);
        }
    }

    static void OnBuffAdd(const SDK::Events::BuffEventArgs& args) {
        if (s_instance) {
            s_instance->HandleBuffAdd(args);
        }
    }

    static void OnStopCast(const SDK::Events::StopCastEventArgs& args) {
        if (s_instance) {
            s_instance->HandleStopCast(args);
        }
    }

    static void OnDraw() {
        if (s_instance) {
            s_instance->Draw();
        }
    }
};

class EnsoulsharpOrbPlugin final : public IPlugin {
public:
    const char* GetName() const override { return kOrbwalkerName; }
    const char* GetInternalId() const override { return "core.ensoulsharp_orb"; }
    const char* GetAuthor() const override { return "NightSharp"; }
    PluginCategory GetCategory() const override { return PluginCategory::Core; }
    bool AutoLoadByDefault() const override { return false; }

    bool CanLoad() const override {
        return CoreRuntime::EnsureInitialized() &&
               SDK::Variables::EnsoulSharpMenu != nullptr &&
               SDK::Orbwalker::GetOrbwalker(kSdkOrbwalkerName) != nullptr;
    }

    bool LoadSucceeded() const override {
        return m_orbwalker &&
               SDK::Orbwalker::GetOrbwalker(kOrbwalkerName) == m_orbwalker.get() &&
               SDK::Orbwalker::Implementation() == m_orbwalker.get();
    }

    void OnLoad() override {
        m_previousOrbwalker = SDK::Orbwalker::CurrentOrbwalkerName();

        if (!m_orbwalker) {
            m_orbwalker = std::make_unique<EnsoulsharpOrbWalker>(
                SDK::Variables::EnsoulSharpMenu);
        }

        auto existing = SDK::Orbwalker::GetOrbwalker(kOrbwalkerName);
        if (existing && existing != m_orbwalker.get()) {
            ClearCurrentOrbwalkerIf(existing);
            SDK::OrbwalkingDetail::Implementations.erase(kOrbwalkerName);
            existing = nullptr;
        }

        if (!existing &&
            !SDK::Orbwalker::AddOrbwalker(kOrbwalkerName, m_orbwalker.get())) {
            NightSharpDebug::Logf("[EnsoulsharpOrb] failed to register orbwalker");
            m_orbwalker.reset();
            return;
        }

        if (!m_orbwalker->Enable()) {
            NightSharpDebug::Logf("[EnsoulsharpOrb] failed to bind events");
            SDK::OrbwalkingDetail::Implementations.erase(kOrbwalkerName);
            ClearCurrentOrbwalkerIf(m_orbwalker.get());
            m_orbwalker.reset();
            return;
        }

        if (!SDK::Orbwalker::SetOrbwalker(kOrbwalkerName)) {
            NightSharpDebug::Logf("[EnsoulsharpOrb] failed to select orbwalker");
            SDK::OrbwalkingDetail::Implementations.erase(kOrbwalkerName);
            ClearCurrentOrbwalkerIf(m_orbwalker.get());
            m_orbwalker.reset();
            return;
        }

        SDK::OrbwalkingDetail::RuntimeInstance = nullptr;
        NightSharpDebug::Logf("[EnsoulsharpOrb] loaded");
    }

    void OnUnload() override {
        SDK::IOrbwalker* provider = m_orbwalker.get();
        const bool wasCurrent =
            SDK::Orbwalker::CurrentOrbwalkerName() == kOrbwalkerName ||
            SDK::Orbwalker::Implementation() == provider;

        if (wasCurrent) {
            RestorePreviousOrbwalker();
        }

        SDK::OrbwalkingDetail::Implementations.erase(kOrbwalkerName);
        ClearCurrentOrbwalkerIf(provider);
        m_orbwalker.reset();

        NightSharpDebug::Logf("[EnsoulsharpOrb] unloaded");
    }

private:
    static constexpr const char* kOrbwalkerName = "EnsoulsharpOrb";
    static constexpr const char* kSdkOrbwalkerName = "SDK";

    void RestorePreviousOrbwalker() {
        if (!m_previousOrbwalker.empty() &&
            m_previousOrbwalker != kOrbwalkerName &&
            SDK::Orbwalker::SetOrbwalker(m_previousOrbwalker)) {
            SyncRuntimeToCurrent();
            return;
        }

        if (SDK::Orbwalker::SetOrbwalker(kSdkOrbwalkerName)) {
            SyncRuntimeToSdk();
            return;
        }

        ClearCurrentOrbwalker();
    }

    static void SyncRuntimeToCurrent() {
        if (auto* base = dynamic_cast<SDK::OrbwalkerBase*>(
                SDK::Orbwalker::Implementation())) {
            SDK::OrbwalkingDetail::RuntimeInstance = base;
            return;
        }
        SDK::OrbwalkingDetail::RuntimeInstance = nullptr;
    }

    static void SyncRuntimeToSdk() {
        if (auto* base = dynamic_cast<SDK::OrbwalkerBase*>(
                SDK::Orbwalker::GetOrbwalker(kSdkOrbwalkerName))) {
            SDK::OrbwalkingDetail::RuntimeInstance = base;
            return;
        }
        SDK::OrbwalkingDetail::RuntimeInstance = nullptr;
    }

    static void ClearCurrentOrbwalker() {
        SDK::OrbwalkingDetail::Implementation = nullptr;
        SDK::OrbwalkingDetail::SelectedImplementationName.clear();
        SDK::OrbwalkingDetail::RuntimeInstance = nullptr;
    }

    static void ClearCurrentOrbwalkerIf(SDK::IOrbwalker* provider) {
        if (provider && SDK::Orbwalker::Implementation() == provider) {
            ClearCurrentOrbwalker();
        }
    }

    std::unique_ptr<EnsoulsharpOrbWalker> m_orbwalker;
    std::string m_previousOrbwalker = kSdkOrbwalkerName;
};

} // namespace Plugins
