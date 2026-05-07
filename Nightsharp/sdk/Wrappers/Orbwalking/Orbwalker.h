#pragma once

#include "AttackData.h"
#include "OrbwalkerBase.h"
#include "../TargetSelector/TargetSelector.h"
#include "../../Core/Game.h"
#include "../../Events/SpellCastTracker.h"
#include "../../UI/Drawing.h"
#include "../../UI/UI.h"
#include "../../Utils/AutoAttack.h"

#include <Windows.h>
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <vector>

namespace SDK {

class Orbwalker {
public:
    using ActionHandler = void(*)(OrbwalkingActionArgs&);

    static void Initialize() {
        if (s_initialized) {
            return;
        }
        s_initialized = true;

        s_menu = UI::CreateMenu("orbwalker", "Orbwalker");

        s_drawingMenu = s_menu.AddMenu("drawings", "Drawings");
        s_drawingMenu.AddBool("drawAARange", "Auto-Attack Range", true);
        s_drawingMenu.AddBool("drawAARangeEnemy", "Auto-Attack Range Enemy", false);
        s_drawingMenu.AddBool("drawExtraHoldPosition", "Extra Hold Position", false);
        s_drawingMenu.AddBool("drawKillableMinion", "Killable Minions", false);
        s_drawingMenu.AddBool("drawKillableMinionFade", "Killable Minions Fade Effect", false);
        s_drawingMenu.AddColor("allyRangeColor", "Ally Range Color", ImVec4(0.15f, 0.75f, 1.0f, 0.9f));
        s_drawingMenu.AddColor("enemyRangeColor", "Enemy Range Color", ImVec4(1.0f, 0.35f, 0.25f, 0.7f));
        s_drawingMenu.AddColor("killableColor", "Killable Color", ImVec4(1.0f, 0.95f, 0.1f, 0.9f));

        s_advancedMenu = s_menu.AddMenu("advanced", "Advanced");
        s_advancedMenu.AddSeparator("separatorMovement", "Movement");
        s_advancedMenu.AddBool("movementRandomize", "Randomize Location", true);
        s_advancedMenu.AddSlider("movementExtraHold", "Extra Hold Position", 0, 0, 250);
        s_advancedMenu.AddSlider("movementMaximumDistance", "Maximum Distance", 1500, 500, 1500);
        s_advancedMenu.AddSeparator("separatorDelay", "Delay");
        s_advancedMenu.AddSlider("delayMovement", "Movement", 0, 0, 500);
        s_advancedMenu.AddSlider("delayWindup", "Windup", 80, 0, 200);
        s_advancedMenu.AddSlider("delayFarm", "Farm", 30, 0, 200);
        s_advancedMenu.AddSeparator("separatorPrioritization", "Prioritization");
        s_advancedMenu.AddBool("prioritizeFarm", "Farm Over Harass", true);
        s_advancedMenu.AddBool("prioritizeMinions", "Minions Over Objectives", false);
        s_advancedMenu.AddBool("prioritizeSmallJungle", "Small Jungle", false);
        s_advancedMenu.AddBool("prioritizeWards", "Wards", false);
        s_advancedMenu.AddBool("prioritizeSpecialMinions", "Special Minions", false);
        s_advancedMenu.AddSeparator("separatorAttack", "Attack");
        s_advancedMenu.AddBool("attackWards", "Wards", false);
        s_advancedMenu.AddBool("attackBarrels", "Barrels", false);
        s_advancedMenu.AddBool("attackClones", "Clones", false);
        s_advancedMenu.AddBool("attackSpecialMinions", "Special Minions", true);
        s_advancedMenu.AddSeparator("separatorMisc", "Miscellaneous");
        s_advancedMenu.AddBool("miscMissile", "Use Missile Checks", true);
        s_advancedMenu.AddBool("miscAttackSpeed", "Do Not Kite If Attack Speed > 2.5", true);

        s_menu.AddSeparator("separatorKeys", "Key Bindings");
        s_menu.AddKeyBind("lasthitKey", "Last Hit", 'X', KeyBindType::Press);
        s_menu.AddKeyBind("laneclearKey", "Lane Clear", 'V', KeyBindType::Press);
        s_menu.AddKeyBind("hybridKey", "Hybrid", 'C', KeyBindType::Press);
        s_menu.AddKeyBind("comboKey", "Combo", VK_SPACE, KeyBindType::Press);
        s_menu.AddKeyBind("fleeKey", "Flee", 'Z', KeyBindType::Press);
        s_menu.AddBool("enabledOption", "Enabled", true);

        Events::SpellCast::AddOnProcessSpellCast(&OnProcessSpellCast);
        Events::SpellCast::AddOnDoCast(&OnDoCast);
        Events::SpellCast::AddOnStopCast(&OnStopCast);
    }

    static Menu* GetMenu() {
        Initialize();
        return s_menu.Raw();
    }

    static OrbwalkerMode GetMode() {
        Initialize();
        return s_activeMode;
    }

    static void SetExternalControl(bool enabled) {
        Initialize();
        s_externalControl = enabled;
    }

    static bool IsExternalControlEnabled() {
        return s_externalControl;
    }

    static void SetActiveMode(OrbwalkerMode mode) {
        Initialize();
        s_activeMode = mode;
    }

    static void InvokeAction(OrbwalkingActionArgs& args) {
        Initialize();
        if (args.Type == OrbwalkingType::BeforeAttack) {
            InvokeBeforeAttack(args);
        } else if (args.Type == OrbwalkingType::AfterAttack ||
                   args.Type == OrbwalkingType::TargetSwitch) {
            InvokeAfterAttack(args);
        }
    }

    static bool OnBeforeAttack(ActionHandler handler) {
        Initialize();
        return handler && s_beforeAttackHandlers.push_back(handler);
    }

    static bool OnAfterAttack(ActionHandler handler) {
        Initialize();
        return handler && s_afterAttackHandlers.push_back(handler);
    }

    static GameObject ForceTarget() {
        return s_forceTarget;
    }

    static void ForceTarget(const GameObject& target) {
        s_forceTarget = target;
    }

    static void ResetSwingTimer() {
        s_lastAutoAttackTick = 0;
        s_missileLaunched = false;
    }

    static void SetAttackState(bool state) {
        s_attackState = state;
    }

    static void SetMovementState(bool state) {
        s_movementState = state;
    }

    static bool CanAttack(float extraWindup = 0.0f) {
        const auto player = ObjectManager::Player();
        if (!player.IsValid() || player.IsDead()) {
            return false;
        }

        if (_stricmp(player.CharacterName().c_str(), "Jhin") == 0 && player.HasBuff("JhinPassiveReload")) {
            return false;
        }

        const int now = Game::TickCount();
        return now + (Game::Ping() / 2) + 25 >=
            s_lastAutoAttackTick + static_cast<int>(player.AttackDelay() * 1000.0f + extraWindup);
    }

    static bool CanMove(float extraWindup = 0.0f, bool disableMissileCheck = false) {
        const auto player = ObjectManager::Player();
        if (!player.IsValid() || player.IsDead()) {
            return false;
        }

        if (s_missileLaunched && !disableMissileCheck) {
            return true;
        }

        const int now = Game::TickCount();
        const float configuredWindup = static_cast<float>(s_advancedMenu.Slider("delayWindup", 80));
        return now + (Game::Ping() / 2) >=
            s_lastAutoAttackTick + static_cast<int>(player.AttackCastDelay() * 1000.0f + extraWindup + configuredWindup);
    }

    static GameObject GetTarget() {
        Initialize();
        const auto player = ObjectManager::Player();
        if (!player.IsValid()) {
            return GameObject();
        }

        if (IsAttackCandidate(s_forceTarget, player)) {
            return s_forceTarget;
        }

        if ((s_activeMode == OrbwalkerMode::Harass || s_activeMode == OrbwalkerMode::LaneClear) &&
            !s_advancedMenu.Bool("prioritizeFarm", true)) {
            auto hero = GetHeroTarget();
            if (hero.IsValid()) {
                return hero;
            }
        }

        if (s_activeMode == OrbwalkerMode::LastHit ||
            s_activeMode == OrbwalkerMode::Harass ||
            s_activeMode == OrbwalkerMode::LaneClear) {
            auto minion = GetFarmTarget();
            if (minion.IsValid()) {
                return minion;
            }
        }

        if (s_activeMode != OrbwalkerMode::LastHit) {
            auto hero = GetHeroTarget();
            if (hero.IsValid()) {
                return hero;
            }
        }

        if (s_activeMode == OrbwalkerMode::LaneClear) {
            auto structure = GetStructureTarget();
            if (structure.IsValid()) {
                return structure;
            }
        }

        return GameObject();
    }

    static void Attack(const GameObject& target) {
        Initialize();
        const int now = Game::TickCount();
        if (s_blockOrdersUntilTick > now) {
            return;
        }

        const auto player = ObjectManager::Player();
        if (!IsAttackCandidate(target, player, target.IsNeutral())) {
            DebugAttackState("attack-filter", target);
            return;
        }

        OrbwalkingActionArgs args = {};
        args.Target = target;
        args.Position = target.Position();
        args.Process = true;
        args.Type = OrbwalkingType::BeforeAttack;
        InvokeBeforeAttack(args);
        if (!args.Process) {
            return;
        }

        if (player.IssueOrder(GameObjectOrder::AttackUnit, target)) {
            s_lastAutoAttackCommandTick = now;
            s_lastAutoAttackTick = now - (Game::Ping() / 2);
            s_missileLaunched = false;
            s_lastMovementOrderTick = 0;
            s_lastTarget = target;
            s_blockOrdersUntilTick = now + 70 + std::min(60, Game::Ping());
            DebugAttackState("attack-order", target, true);
        } else {
            DebugAttackState("attack-issue-false", target);
        }
    }

    static void Move(const Vector3& position) {
        Initialize();
        const int now = Game::TickCount();
        if (s_blockOrdersUntilTick > now || !position.IsValid() || position.IsZero()) {
            return;
        }

        const int delay = s_advancedMenu.Slider("delayMovement", 0);
        if (now - s_lastMovementOrderTick < delay) {
            return;
        }

        const auto player = ObjectManager::Player();
        if (!player.IsValid()) {
            return;
        }

        Vector3 movePos = position;
        const float maxDistance = static_cast<float>(s_advancedMenu.Slider("movementMaximumDistance", 1500));
        if (player.Position().Distance2D(movePos) > maxDistance) {
            movePos = player.Position().Extend(movePos, maxDistance);
        }

        const int extraHold = s_advancedMenu.Slider("movementExtraHold", 0);
        if (extraHold > 0 && player.Position().Distance2D(movePos) <= static_cast<float>(extraHold)) {
            movePos = player.Position();
        }

        if (movePos.DistanceSqr2D(s_lastMovePosition) < 25.0f * 25.0f && now - s_lastMovementOrderTick < 250) {
            return;
        }

        if (player.IssueOrder(GameObjectOrder::MoveTo, movePos)) {
            s_lastMovementOrderTick = now;
            s_lastMovePosition = movePos;
        }
    }

    static void Orbwalk(const GameObject& forcedTarget = GameObject(), const Vector3& forcedPosition = Vector3()) {
        const auto player = ObjectManager::Player();
        if (!player.IsValid()) {
            return;
        }

        if (CanAttack() && s_attackState) {
            GameObject target = forcedTarget.IsValid() ? forcedTarget : GetTarget();
            if (IsAttackCandidate(target, player, target.IsNeutral())) {
                Attack(target);
            } else {
                DebugAttackState("no-target", target);
            }
        }

        if (CanMove(0.0f, !s_advancedMenu.Bool("miscMissile", true)) && s_movementState) {
            Move(forcedPosition.IsValid() && !forcedPosition.IsZero() ? forcedPosition : Game::CursorPos());
        }
    }

    static void Update() {
        Initialize();
        if (s_externalControl) {
            UpdateKillableCache();
            return;
        }
        s_activeMode = ReadActiveMode();

        const auto player = ObjectManager::Player();
        if (!player.IsValid() || player.IsDead() || s_activeMode == OrbwalkerMode::None ||
            Game::IsChatOpen() || Game::IsShopOpen()) {
            UpdateKillableCache();
            return;
        }

        Orbwalk();
        UpdateKillableCache();
    }

    static void Render() {
        Initialize();
        if (s_externalControl) {
            return;
        }
        const auto player = ObjectManager::Player();
        if (!player.IsValid() || player.IsDead()) {
            return;
        }

        if (s_drawingMenu.Bool("drawAARange", true)) {
            Drawing::DrawCircle(player.Position(), player.GetRealAutoAttackRange(),
                ToColor(s_drawingMenu.Raw()->GetColorValue("allyRangeColor", ImVec4(0.15f, 0.75f, 1.0f, 0.9f))),
                1.6f, 28, true);
        }

        if (s_drawingMenu.Bool("drawAARangeEnemy", false)) {
            const auto color = ToColor(s_drawingMenu.Raw()->GetColorValue("enemyRangeColor", ImVec4(1.0f, 0.35f, 0.25f, 0.7f)));
            for (const auto& hero : ObjectManager::EnemyHeroes()) {
                if (!hero.IsValid() || hero.IsDead() || !hero.IsVisible()) {
                    continue;
                }
                Drawing::DrawCircle(hero.Position(), hero.GetRealAutoAttackRange(player), color, 1.2f, 22, true);
            }
        }

        if (s_drawingMenu.Bool("drawKillableMinion", false)) {
            const auto color = ToColor(s_drawingMenu.Raw()->GetColorValue("killableColor", ImVec4(1.0f, 0.95f, 0.1f, 0.9f)));
            for (const auto address : s_killableMinions) {
                AIMinionClient minion(address);
                if (!minion.IsValidTarget()) {
                    continue;
                }
                Drawing::DrawCircle(minion.Position(), minion.BoundingRadius() + 18.0f, color, 1.4f, 16, true);
            }
        }

        if (s_drawingMenu.Bool("drawExtraHoldPosition", false)) {
            const Vector3 cursor = Game::CursorPos();
            if (cursor.IsValid() && !cursor.IsZero()) {
                Drawing::DrawCircle(cursor, static_cast<float>(std::max(25, s_advancedMenu.Slider("movementExtraHold", 0))),
                    IM_COL32(255, 255, 255, 160), 1.0f, 16, true);
            }
        }
    }

private:
    static inline bool s_initialized = false;
    static inline bool s_externalControl = false;
    static inline UI::MenuNode s_menu = {};
    static inline UI::MenuNode s_drawingMenu = {};
    static inline UI::MenuNode s_advancedMenu = {};
    static inline OrbwalkerMode s_activeMode = OrbwalkerMode::None;
    static inline bool s_attackState = true;
    static inline bool s_movementState = true;
    static inline bool s_missileLaunched = false;
    static inline int s_lastAutoAttackCommandTick = 0;
    static inline int s_lastAutoAttackTick = 0;
    static inline int s_lastMovementOrderTick = 0;
    static inline int s_blockOrdersUntilTick = 0;
    static inline int s_totalAutoAttacks = 0;
    static inline int s_lastKillableCacheTick = 0;
    static inline int s_lastAttackDebugTick = 0;
    static inline int s_lastEnemyHeroCount = 0;
    static inline int s_lastHeroInRangeCount = 0;
    static inline int s_lastEnemyMinionCount = 0;
    static inline int s_lastMinionInRangeCount = 0;
    static inline GameObject s_lastTarget = {};
    static inline GameObject s_forceTarget = {};
    static inline Vector3 s_lastMovePosition = {};
    static inline std::vector<uintptr_t> s_killableMinions = {};
    static inline MenuUI::FixedList<ActionHandler, 32> s_beforeAttackHandlers = {};
    static inline MenuUI::FixedList<ActionHandler, 32> s_afterAttackHandlers = {};

    static ImU32 ToColor(const ImVec4& color) {
        return IM_COL32(
            static_cast<int>(std::clamp(color.x, 0.0f, 1.0f) * 255.0f),
            static_cast<int>(std::clamp(color.y, 0.0f, 1.0f) * 255.0f),
            static_cast<int>(std::clamp(color.z, 0.0f, 1.0f) * 255.0f),
            static_cast<int>(std::clamp(color.w, 0.0f, 1.0f) * 255.0f));
    }

    static OrbwalkerMode ReadActiveMode() {
        if (!s_menu.Bool("enabledOption", true)) {
            return OrbwalkerMode::None;
        }
        if (s_menu.KeyActive("comboKey", false)) return OrbwalkerMode::Combo;
        if (s_menu.KeyActive("hybridKey", false)) return OrbwalkerMode::Harass;
        if (s_menu.KeyActive("laneclearKey", false)) return OrbwalkerMode::LaneClear;
        if (s_menu.KeyActive("lasthitKey", false)) return OrbwalkerMode::LastHit;
        if (s_menu.KeyActive("fleeKey", false)) return OrbwalkerMode::Flee;
        return OrbwalkerMode::None;
    }

    static void InvokeBeforeAttack(OrbwalkingActionArgs& args) {
        for (const auto& handler : s_beforeAttackHandlers) {
            if (handler) {
                handler(args);
            }
        }
    }

    static void InvokeAfterAttack(OrbwalkingActionArgs& args) {
        for (const auto& handler : s_afterAttackHandlers) {
            if (handler) {
                handler(args);
            }
        }
    }

    static const char* ModeName(OrbwalkerMode mode) {
        switch (mode) {
        case OrbwalkerMode::Combo: return "Combo";
        case OrbwalkerMode::Harass: return "Harass";
        case OrbwalkerMode::LaneClear: return "LaneClear";
        case OrbwalkerMode::LastHit: return "LastHit";
        case OrbwalkerMode::Flee: return "Flee";
        default: return "None";
        }
    }

    static void AppendOrbwalkerDebug(const char* text) {
        if (!text || !*text) {
            return;
        }

        HANDLE hFile = CreateFileA(
            "C:\\Users\\Public\\ns_orbwalker_debug.txt",
            FILE_APPEND_DATA,
            FILE_SHARE_READ,
            nullptr,
            OPEN_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);
        if (hFile == INVALID_HANDLE_VALUE) {
            return;
        }

        DWORD written = 0;
        WriteFile(hFile, text, static_cast<DWORD>(lstrlenA(text)), &written, nullptr);
        CloseHandle(hFile);
    }

    static bool IsAttackCandidate(const GameObject& unit,
                                  const AIBaseClient& player,
                                  bool allowNeutral = false) {
        if (!player.IsValid() || !unit.IsValid() || unit.IsDead()) {
            return false;
        }
        if (!allowNeutral && !unit.IsEnemy()) {
            return false;
        }
        if (unit.Health() <= 0.0f || unit.MaxHealth() <= 0.0f) {
            return false;
        }

        const float range = player.GetRealAutoAttackRange(unit) + 45.0f;
        return player.Distance(unit) <= range;
    }

    static void DebugAttackState(const char* reason,
                                 const GameObject& target = GameObject(),
                                 bool force = false) {
        const int now = Game::TickCount();
        if (!force && now - s_lastAttackDebugTick < 1000) {
            return;
        }
        s_lastAttackDebugTick = now;

        const auto player = ObjectManager::Player();
        char buffer[512] = {};
        std::snprintf(
            buffer,
            sizeof(buffer),
            "[NightSharp][Orbwalker] %s mode=%s canAttack=%d atkState=%d moveState=%d heroes=%d/%d minions=%d/%d target=0x%llX targetValid=%d enemy=%d visible=%d targetable=%d hp=%.1f dist=%.1f range=%.1f tick=%d lastAA=%d\r\n",
            reason ? reason : "state",
            ModeName(s_activeMode),
            CanAttack() ? 1 : 0,
            s_attackState ? 1 : 0,
            s_movementState ? 1 : 0,
            s_lastHeroInRangeCount,
            s_lastEnemyHeroCount,
            s_lastMinionInRangeCount,
            s_lastEnemyMinionCount,
            static_cast<unsigned long long>(target.Address()),
            target.IsValid() ? 1 : 0,
            target.IsValid() && target.IsEnemy() ? 1 : 0,
            target.IsValid() && target.IsVisible() ? 1 : 0,
            target.IsValid() && target.IsTargetable() ? 1 : 0,
            target.IsValid() ? target.Health() : 0.0f,
            player.IsValid() && target.IsValid() ? player.Distance(target) : 0.0f,
            player.IsValid() && target.IsValid() ? player.GetRealAutoAttackRange(target) : 0.0f,
            now,
            s_lastAutoAttackTick);
        AppendOrbwalkerDebug(buffer);
    }

    static GameObject GetHeroTarget() {
        const auto player = ObjectManager::Player();
        s_lastEnemyHeroCount = 0;
        s_lastHeroInRangeCount = 0;
        if (!player.IsValid()) {
            return GameObject();
        }

        auto selected = TargetSelector::GetTarget(-1.0f, DamageType::Physical);
        if (IsAttackCandidate(selected, player)) {
            return selected;
        }

        GameObject best = {};
        float bestScore = FLT_MAX;
        for (const auto& hero : ObjectManager::EnemyHeroes()) {
            ++s_lastEnemyHeroCount;
            if (!IsAttackCandidate(hero, player)) {
                continue;
            }
            ++s_lastHeroInRangeCount;
            const float score = hero.Health() + player.Distance(hero) * 0.15f;
            if (score < bestScore) {
                best = hero;
                bestScore = score;
            }
        }
        return best;
    }

    static GameObject GetFarmTarget() {
        const auto player = ObjectManager::Player();
        GameObject best = {};
        float bestHealth = FLT_MAX;

        s_lastEnemyMinionCount = 0;
        s_lastMinionInRangeCount = 0;

        auto consider = [&](const GameObject& unit, bool requireKillable, bool allowNeutral) {
            ++s_lastEnemyMinionCount;
            if (!IsAttackCandidate(unit, player, allowNeutral)) {
                return;
            }
            ++s_lastMinionInRangeCount;

            const float damage = player.GetAutoAttackDamage(unit);
            const bool killable = unit.MaxHealth() <= 10.0f ? unit.Health() <= 1.0f : unit.Health() <= damage;
            if (requireKillable && !killable) {
                return;
            }

            if (unit.Health() < bestHealth) {
                best = unit;
                bestHealth = unit.Health();
            }
        };

        const bool lastHitMode = s_activeMode == OrbwalkerMode::LastHit || s_activeMode == OrbwalkerMode::Harass;
        for (const auto& minion : ObjectManager::EnemyMinions()) {
            consider(minion, lastHitMode, false);
        }

        if (!best.IsValid() && s_activeMode == OrbwalkerMode::LaneClear) {
            for (const auto& jungle : ObjectManager::JungleMinions()) {
                consider(jungle, false, true);
            }
        }

        return best;
    }

    static GameObject GetStructureTarget() {
        const auto player = ObjectManager::Player();
        if (s_advancedMenu.Bool("prioritizeMinions", false)) {
            for (const auto& minion : ObjectManager::EnemyMinions()) {
                if (IsAttackCandidate(minion, player)) {
                    return GameObject();
                }
            }
        }

        for (const auto& turret : ObjectManager::EnemyTurrets()) {
            if (IsAttackCandidate(turret, player)) {
                return turret;
            }
        }
        for (const auto& inhibitor : ObjectManager::EnemyInhibitors()) {
            if (IsAttackCandidate(inhibitor, player)) {
                return inhibitor;
            }
        }
        const auto nexus = ObjectManager::EnemyNexus();
        if (IsAttackCandidate(nexus, player)) {
            return nexus;
        }
        return GameObject();
    }

    static void UpdateKillableCache() {
        if (!s_drawingMenu.Bool("drawKillableMinion", false)) {
            s_killableMinions.clear();
            return;
        }

        const int now = Game::TickCount();
        if (now - s_lastKillableCacheTick < 150) {
            return;
        }
        s_lastKillableCacheTick = now;
        s_killableMinions.clear();

        const auto player = ObjectManager::Player();
        if (!player.IsValid()) {
            return;
        }

        for (const auto& minion : ObjectManager::EnemyMinions()) {
            if (!IsAttackCandidate(minion, player)) {
                continue;
            }
            if (minion.Health() <= player.GetAutoAttackDamage(minion)) {
                s_killableMinions.push_back(minion.Address());
                if (s_killableMinions.size() >= 12) {
                    break;
                }
            }
        }
    }

    static void OnProcessSpellCast(const AIBaseClient& sender, const Events::SpellCast::ProcessSpellCastEventArgs& args) {
        if (!sender.IsValid() || !sender.IsMe()) {
            return;
        }

        if (Utils::AutoAttack::IsAutoAttackReset(args.SpellName)) {
            ResetSwingTimer();
            return;
        }

        if (!args.IsAutoAttack && !Utils::AutoAttack::IsAutoAttack(args.SpellName)) {
            return;
        }

        const int now = Game::TickCount();
        s_lastAutoAttackTick = now - (Game::Ping() / 2);
        s_missileLaunched = false;
        s_lastMovementOrderTick = 0;

        GameObject target = ObjectManager::GetByNetId(args.TargetNetworkId);
        if (target.IsValid() && !target.Compare(s_lastTarget)) {
            OrbwalkingActionArgs switchArgs = {};
            switchArgs.Target = target;
            switchArgs.Sender = sender;
            switchArgs.Type = OrbwalkingType::TargetSwitch;
            InvokeAfterAttack(switchArgs);
            s_lastTarget = target;
        }
    }

    static void OnDoCast(const AIBaseClient& sender, const Events::SpellCast::ProcessSpellCastEventArgs& args) {
        if (!sender.IsValid() || !sender.IsMe()) {
            return;
        }

        if (Utils::AutoAttack::IsAutoAttackReset(args.SpellName)) {
            ResetSwingTimer();
            return;
        }

        if (!args.IsAutoAttack && !Utils::AutoAttack::IsAutoAttack(args.SpellName)) {
            return;
        }

        s_missileLaunched = true;
        ++s_totalAutoAttacks;

        OrbwalkingActionArgs afterArgs = {};
        afterArgs.Target = ObjectManager::GetByNetId(args.TargetNetworkId);
        afterArgs.Sender = sender;
        afterArgs.Type = OrbwalkingType::AfterAttack;
        InvokeAfterAttack(afterArgs);
    }

    static void OnStopCast(const AIBaseClient& sender, const Events::SpellCast::StopCastEventArgs& args) {
        (void)args;
        if (sender.IsValid() && sender.IsMe()) {
            ResetSwingTimer();
        }
    }
};

} // namespace SDK
