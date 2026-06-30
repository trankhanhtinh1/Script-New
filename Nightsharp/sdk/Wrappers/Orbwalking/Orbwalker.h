#pragma once

#include "OrbwalkerBase.h"
#include "OrbwalkerSelector.h"
#include "../../Enumerations/KeyBindType.h"
#include "../../UI/UI.h"
#include "../../UI/Drawing.h"
#include "../../Utils/AutoAttack.h"
#include "../../Wrappers/Damages/Damage.h"
#include "../../Core/Game.h"
#include "../../Core/Variables.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <random>
#include <vector>
#include <fstream>
#include <string>

#ifndef NIGHTSHARP_ORBWALKER_LOGGING
#define NIGHTSHARP_ORBWALKER_LOGGING 0
#endif

inline void LogOrb(const std::string& msg) {
#if NIGHTSHARP_ORBWALKER_LOGGING
    std::ofstream os("c:\\Users\\Public\\nightsharp_orbwalker_debug.txt", std::ios::app);
    os << "[Tick: " << SDK::Variables::TickCount() << "] " << msg << "\n";
#else
    (void)msg;
#endif
}

namespace SDK {

class Orbwalker : public OrbwalkerBase {
public:
    Menu* menu_;
    OrbwalkerSelector* Selector;
    int BlockOrdersUntilTick = 0;

    AttackableUnit GetForceTarget() const { return Selector->ForceTarget; }
    void SetForceTarget(const AttackableUnit& target) { Selector->ForceTarget = target; }

    MenuKeyBind* comboKey_ = nullptr;
    MenuKeyBind* hybridKey_ = nullptr;
    MenuKeyBind* laneClearKey_ = nullptr;
    MenuKeyBind* lastHitKey_ = nullptr;
    MenuBool* enabledOption_ = nullptr;

    explicit Orbwalker(Menu* parentMenu) {
        menu_ = new Menu("orbwalker", "Orbwalker");
        parentMenu->Add(menu_);

        auto* drawing = new Menu("drawings", "Drawings");
        drawing->Add(new MenuBool("drawAARange", "Auto-Attack Range", true));
        drawing->Add(new MenuBool("drawAARangeEnemy", "Auto-Attack Range Enemy"));
        drawing->Add(new MenuBool("drawExtraHoldPosition", "Extra Hold Position"));
        drawing->Add(new MenuBool("drawKillableMinion", "Killable Minions"));
        drawing->Add(new MenuBool("drawKillableMinionFade", "Killable Minions Fade Effect"));
        menu_->Add(drawing);

        auto* advanced = new Menu("advanced", "Advanced");

        advanced->Add(new MenuSeparator("separatorMovement", "Movement"));
        advanced->Add(new MenuBool("movementRandomize", "Randomize Location", true));
        advanced->Add(new MenuSlider("movementExtraHold", "Extra Hold Position", 0, 0, 250));
        advanced->Add(new MenuSlider("movementMaximumDistance", "Maximum Distance", 1500, 500, 1500));

        advanced->Add(new MenuSeparator("separatorDelay", "Delay"));
        advanced->Add(new MenuSlider("delayMovement", "Movement", 0, 0, 500));
        advanced->Add(new MenuSlider("delayWindup", "Windup", 80, 0, 200));
        advanced->Add(new MenuSlider("delayFarm", "Farm", 30, 0, 200));

        advanced->Add(new MenuSeparator("separatorPrioritization", "Prioritization"));
        advanced->Add(new MenuBool("prioritizeFarm", "Farm Over Harass", true));
        advanced->Add(new MenuBool("prioritizeMinions", "Minions Over Objectives"));
        advanced->Add(new MenuBool("prioritizeSmallJungle", "Small Jungle"));
        advanced->Add(new MenuBool("prioritizeWards", "Wards"));
        advanced->Add(new MenuBool("prioritizeSpecialMinions", "Special Minions"));

        advanced->Add(new MenuSeparator("separatorAttack", "Attack"));
        advanced->Add(new MenuBool("attackWards", "Wards"));
        advanced->Add(new MenuBool("attackBarrels", "Barrels"));
        advanced->Add(new MenuBool("attackClones", "Clones"));
        advanced->Add(new MenuBool("attackSpecialMinions", "Special Minions", true));

        advanced->Add(new MenuSeparator("separatorMisc", "Miscellaneous"));
        advanced->Add(new MenuBool("miscMissile", "Use Missile Checks", true));
        advanced->Add(new MenuBool("miscAttackSpeed", "Don't Kite if Attack Speed > 2.5", true));

        menu_->Add(advanced);

        menu_->Add(new MenuSeparator("separatorKeys", "Key Bindings"));
        lastHitKey_ = menu_->Add(new MenuKeyBind("lasthitKey", "Last Hit", 'X', KeyBindType::Press));
        laneClearKey_ = menu_->Add(new MenuKeyBind("laneclearKey", "Lane Clear", 'V', KeyBindType::Press));
        hybridKey_ = menu_->Add(new MenuKeyBind("hybridKey", "Hybrid", 'C', KeyBindType::Press));
        comboKey_ = menu_->Add(new MenuKeyBind("comboKey", "Combo", VK_SPACE, KeyBindType::Press));
        enabledOption_ = menu_->Add(new MenuBool("enabledOption", "Enabled", true));

        Selector = new OrbwalkerSelector(this, menu_);

        menu_->MenuValueChanged = OnMenuValueChanged;
        menu_->MenuValueChangedUd = this;

        SetEnabled(enabledOption_->Value);
    }

    ~Orbwalker() override {
        SetEnabled(false);
        delete Selector;
        Selector = nullptr;
    }

    static void OnMenuValueChanged(MenuValueChangedEventArgs args, void* ud) {
        auto* self = static_cast<Orbwalker*>(ud);
        if (!self || !self->Enabled()) return;
        auto* kb = dynamic_cast<MenuKeyBind*>(args.Item);
        if (!kb) return;
        if (self->comboKey_->Active) {
            self->ActiveMode = OrbwalkingMode::Combo;
        } else if (self->hybridKey_->Active) {
            self->ActiveMode = OrbwalkingMode::Hybrid;
        } else if (self->laneClearKey_->Active) {
            self->ActiveMode = OrbwalkingMode::LaneClear;
        } else if (self->lastHitKey_->Active) {
            self->ActiveMode = OrbwalkingMode::LastHit;
        } else {
            self->ActiveMode = OrbwalkingMode::None;
        }
    }

    void SetEnabled(bool value) override {
        if (Enabled() == value) return;
        if (value) {
            SDK::Drawing::OnDraw += &OnDrawingDrawHandler;
        } else {
            SDK::Drawing::OnDraw -= &OnDrawingDrawHandler;
        }
        OrbwalkerBase::SetEnabled(value);
        if (Selector) {
            Selector->SetEnabled(value);
        }
        if (enabledOption_) {
            enabledOption_->Value = value;
        }
    }

    void Attack(AttackableUnit target) override {
        NS_PROFILE("orb.Attack.total");
        if (BlockOrdersUntilTick - Variables::TickCount() > 0) return;

        auto gTarget = target.IsValid() ? target : GetTarget();
        if (!gTarget.IsValid() || !Utils::AutoAttack::InAutoAttackRange(gTarget)) return;

        OrbwalkingActionArgs eventArgs = {};
        eventArgs.Target = gTarget;
        eventArgs.Position = gTarget.Position();
        eventArgs.Process = true;
        eventArgs.Type = OrbwalkingType::BeforeAttack;
        InvokeAction(eventArgs);

        if (eventArgs.Process) {
            if (Utils::AutoAttack::CanCancelAutoAttack(GameObjects::Player())) {
                MissileLaunched = false;
            }

            {
                NS_PROFILE("orb.CoreControl.IssueAttack");
                if (CoreControl::IssueAttack(gTarget.Address(), gTarget.Position())) {
                    LogOrb("ACTION: IssueAttack (SUCCESS) -> Target: " + gTarget.CharacterName()
                        + " | Tick: " + std::to_string(Variables::TickCount()));
                    LastAutoAttackCommandTick = Variables::TickCount();
                    // Set LastAutoAttackTick immediately (local time).
                    // CanAttack/CanMove use this directly — no server confirmation needed.
                    LastAutoAttackTick = Variables::TickCount();
                    LastTarget = gTarget;
                } else {
                    LogOrb("ACTION: IssueAttack (FAILED)");
                }
            }

            BlockOrdersUntilTick = Variables::TickCount() + 70 + (std::min)(60, Game::Ping());
        }
    }

    bool CanAttack(float extraWindup) override {
        float extraDelay = 0.0f;
        const auto player = GameObjects::Player();

        if (player.CharacterName() == "Graves") {
            if (!player.HasBuff("gravesbasicattackammo1")) return false;
            float attackDelay = FrameCache().attackDelay * 1000.0f;
            extraDelay = (attackDelay * 1.0740296828f) - 716.2381256175f - attackDelay;
        } else if (player.CharacterName() == "Jhin" && player.HasBuff("JhinPassiveReload")) {
            return false;
        }

        return OrbwalkerBase::CanAttack(extraWindup + extraDelay);
    }

    bool CanMove(float extraWindup, bool disableMissileCheck) override {
        int localWindup = 0;
        const auto player = GameObjects::Player();

        if (player.CharacterName() == "Rengar"
            && (player.HasBuff("RengarQ") || player.HasBuff("RengarQEmp"))) {
            localWindup = 200;
        }

        auto* advanced = menu_->GetSubMenu("advanced");
        int windupDelay = advanced ? advanced->Get<MenuSlider>("delayWindup")->Value : 0;
        bool noMissileCheck = disableMissileCheck
            || (advanced && !advanced->Get<MenuBool>("miscMissile")->Value);

        return OrbwalkerBase::CanMove(
            extraWindup + static_cast<float>(localWindup + windupDelay),
            noMissileCheck);
    }

    AttackableUnit GetTarget() override {
        return Selector->GetTarget(ActiveMode);
    }

    bool ShouldWait() override {
        return Selector->ShouldWait();
    }

    void Move(Vector3 position) override {
        NS_PROFILE("orb.Move.total");
        if (BlockOrdersUntilTick - Variables::TickCount() > 0) return;
        if (!position.IsValid()) return;

        auto* advanced = menu_->GetSubMenu("advanced");

        int moveDelay = advanced ? advanced->Get<MenuSlider>("delayMovement")->Value : 0;
        if (Variables::TickCount() - LastMovementOrderTick < moveDelay) return;

        if (advanced && advanced->Get<MenuBool>("miscAttackSpeed")->Value) {
            float attackDelay;
            {
                NS_PROFILE("orb.CoreControl.GetAttackDelay");
                attackDelay = CoreControl::GetAttackDelay(GameObjects::Player().Address());
            }
            if (attackDelay < 1.0f / 2.6f && TotalAutoAttacks % 3 != 0
                && !CanMove(500.0f, true) && !MovementState) {
                return;
            }
        }

        int extraHold = advanced ? advanced->Get<MenuSlider>("movementExtraHold")->Value : 0;
        float holdDist = GameObjects::Player().BoundingRadius() + static_cast<float>(extraHold);
        if (position.Distance(GameObjects::Player().Position()) < holdDist) {
            if (!GameObjects::Player().Path().empty()) {
                OrbwalkingActionArgs stopArgs = {};
                stopArgs.Position = GameObjects::Player().Position();
                stopArgs.Process = true;
                stopArgs.Type = OrbwalkingType::StopMovement;
                InvokeAction(stopArgs);
                if (stopArgs.Process) {
                    NS_PROFILE("orb.CoreControl.StopMoving");
                    CoreControl::StopMoving();
                    LastMovementOrderTick = Variables::TickCount() - 70;
                }
            }
            return;
        }

        float playerDist = position.Distance(GameObjects::Player().Position());
        float bounding = GameObjects::Player().BoundingRadius();

        auto fastRandFloat = [](unsigned int seed) -> float {
            seed ^= seed << 13;
            seed ^= seed >> 17;
            seed ^= seed << 5;
            return static_cast<float>(seed % 10000u) / 10000.0f;
        };

        if (playerDist < bounding + 100.0f) {
            float randFactor = 0.8f + fastRandFloat(static_cast<unsigned>(Variables::TickCount()) ^ 0xA5A5u) * 0.4f;
            position = GameObjects::Player().Position().Extend(
                position, bounding + randFactor * 400.0f);
            playerDist = position.Distance(GameObjects::Player().Position());
        }

        int maxDist = advanced ? advanced->Get<MenuSlider>("movementMaximumDistance")->Value : 1500;
        if (playerDist > static_cast<float>(maxDist)) {
            unsigned int r = static_cast<unsigned>(Variables::TickCount() + 1) ^ 0x5A5Au;
            r ^= r << 13; r ^= r >> 17; r ^= r << 5;
            int randInt = static_cast<int>(r % 52u);
            position = GameObjects::Player().Position().Extend(
                position,
                static_cast<float>(maxDist + 25 - randInt));
            playerDist = position.Distance(GameObjects::Player().Position());
        }

        if (advanced && advanced->Get<MenuBool>("movementRandomize")->Value
            && playerDist > 350.0f) {
            float rAngle = fastRandFloat(static_cast<unsigned>(Variables::TickCount() + 2) ^ 0xF0F0u) * (2.0f * 3.14159265358979323846f);
            float radius = bounding / 2.0f;
            float x = position.x + radius * std::cos(rAngle);
            float z = position.z + radius * std::sin(rAngle);
            position = Vector3(x, position.y, z);
        }

        // 1:1 EnsoulSharp path angle movement throttling
        std::vector<Vector3> waypoints;
        {
            NS_PROFILE("orb.Player.GetWaypoints");
            waypoints = GameObjects::Player().GetWaypoints();
        }
        if (waypoints.size() > 1) {
            Vector3 v1 = waypoints[1] - waypoints[0];
            Vector3 v2 = position - GameObjects::Player().Position();
            float angle = v1.AngleBetween(v2);
            if (angle < 60.0f && Variables::TickCount() - LastMovementOrderTick < 70 + (std::min)(60, Game::Ping())) {
                return;
            }
            if (angle >= 60.0f && Variables::TickCount() - LastMovementOrderTick < 60) {
                return;
            }
        } else {
            if (Variables::TickCount() - LastMovementOrderTick < 70 + (std::min)(60, Game::Ping())) {
                return;
            }
        }

        OrbwalkingActionArgs eventArgs = {};
        eventArgs.Position = position;
        eventArgs.Process = true;
        eventArgs.Type = OrbwalkingType::Movement;
        InvokeAction(eventArgs);

        if (eventArgs.Process) {
            LogOrb("ACTION: Move (SUCCESS) -> Position: " + std::to_string(eventArgs.Position.x) + ", " + std::to_string(eventArgs.Position.z));
            NS_PROFILE("orb.CoreControl.IssueMove");
            CoreControl::IssueMove(eventArgs.Position);
            LastMovementOrderTick = Variables::TickCount();
        }
    }

private:
    static void OnDrawingDrawHandler() {
        auto* self = static_cast<Orbwalker*>(Ptr());
        if (!self || !self->Enabled()) return;
        self->OnDrawingDraw();
    }

    static void DrawCircleWorld(const Vec3& center, float radius, std::uint32_t color, float thickness) {
        Vec2 screen;
        if (!SDK::Drawing::WorldToScreen(center, screen)) return;
        Vec2 edge;
        if (!SDK::Drawing::WorldToScreen(Vec3(center.x + radius, center.y, center.z), edge)) return;
        float screenRadius = std::max(1.0f, screen.Distance(edge));
        if (!std::isfinite(screenRadius) || screenRadius <= 0.0f || screenRadius >= 10000.0f) return;
        SDK::Drawing::DrawCircle(screen, screenRadius, thickness, color, 64);
    }

    void OnDrawingDraw() {
        const auto player = GameObjects::Player();
        if (!player.IsValid() || player.IsDead()) return;

        auto* drawings = menu_->GetSubMenu("drawings");
        if (!drawings) return;

        if (drawings->Get<MenuBool>("drawAARange")->Value) {
            float aaRange = Utils::AutoAttack::GetRealAutoAttackRange(player);
            DrawCircleWorld(player.Position(), aaRange, 0xFF00BFFF, 2.0f);
        }

        if (drawings->Get<MenuBool>("drawExtraHoldPosition")->Value) {
            auto* advanced = menu_->GetSubMenu("advanced");
            float holdRadius = player.BoundingRadius()
                + (advanced ? static_cast<float>(advanced->Get<MenuSlider>("movementExtraHold")->Value) : 0.0f);
            DrawCircleWorld(player.Position(), holdRadius, 0xFF800080, 2.0f);
        }

        if (drawings->Get<MenuBool>("drawAARangeEnemy")->Value) {
            for (const auto& enemy : GameObjects::EnemyHeroes()) {
                if (!enemy.IsValid() || enemy.IsDead() || !enemy.IsVisible()) continue;
                float rangeOnPlayer = Utils::AutoAttack::GetRealAutoAttackRange(enemy, AttackableUnit(player.Address()));
                DrawCircleWorld(enemy.Position(), rangeOnPlayer, 0xFF00BFFF, 1.0f);
            }
        }

        if (drawings->Get<MenuBool>("drawKillableMinion")->Value) {
            float aaRange = Utils::AutoAttack::GetRealAutoAttackRange(player);
            auto minions = Selector->GetEnemyMinions(aaRange * 2.0f);
            bool fade = drawings->Get<MenuBool>("drawKillableMinionFade")->Value;

            for (const auto& m : minions) {
                float aaDmg = Damage::GetAutoAttackDamage(
                    AIHeroClient(player.Address()), AIBaseClient(m.Address()), true);
                float health = m.Health();
                if (health >= (fade ? aaDmg * 2.0f : aaDmg)) continue;

                if (fade) {
                    int value = 255 - static_cast<int>(health * 2.0f);
                    value = (std::max)(0, (std::min)(255, value));
                    uint32_t blue = static_cast<uint32_t>(255 - value);
                    DrawCircleWorld(m.Position(), m.BoundingRadius() * 2.0f,
                        0xFF00FF00 | blue, 1.0f);
                } else {
                    DrawCircleWorld(m.Position(), m.BoundingRadius() * 2.0f,
                        0xFF00FF00, 1.0f);
                }
            }
        }
    }
};

} // namespace SDK
