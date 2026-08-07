#pragma once

#include "OrbwalkerTypes.h"

#include "../../../sdk/Core/Game.h"

#include <Windows.h>

#include <algorithm>

using namespace ::SDK;

namespace OrbwalkerKuro {

class OrbwalkerMenu {
    friend class OrbwalkerBase;
public:
    explicit OrbwalkerMenu(Menu* parentMenu)
        : parentMenu_(parentMenu) {
        Build();
    }

    bool Enabled() const { return BoolValue(enabledOption_, true); }
    bool AttackEnabled() const { return BoolValue(attackEnabledOption_, true); }
    bool MoveEnabled() const { return BoolValue(moveEnabledOption_, true); }

    MenuBool* AttackEnabledOption() const { return attackEnabledOption_; }
    MenuBool* MoveEnabledOption() const { return moveEnabledOption_; }

    void SetAttackEnabled(bool value) {
        if (attackEnabledOption_) {
            attackEnabledOption_->Set(value);
        }
    }

    void SetMoveEnabled(bool value) {
        if (moveEnabledOption_) {
            moveEnabledOption_->Set(value);
        }
    }

    bool DrawAARange() const { return BoolValue(drawAARange_, true); }
    int AARangeStyle() const { return ListValue(aaRangeStyle_, 0); }
    int AARangeTexture() const { return ListValue(aaRangeTexture_, 0); }
    int AARangeOpacityPercent() const { return SliderValue(aaRangeOpacity_, 100); }
    int AARangeRotateSpeed() const { return SliderValue(aaRangeRotateSpeed_, 8); }
    int AARangeFadeWidth() const { return SliderValue(aaRangeFadeWidth_, 200); }
    int AARangeFadeOpacityPercent() const { return SliderValue(aaRangeFadeOpacityPercent_, 70); }
    bool DrawAARangeEnemy() const { return BoolValue(drawAARangeEnemy_, false); }
    bool DrawAzirSoldierRanges() const { return BoolValue(drawAzirSoldierRanges_, true); }
    bool DrawExtraHoldPosition() const { return BoolValue(drawExtraHoldPosition_, false); }
    bool DrawKillableMinion() const { return BoolValue(drawKillableMinion_, false); }
    bool DrawKillableMinionFade() const { return BoolValue(drawKillableMinionFade_, false); }
    bool ShowFakeClick() const { return BoolValue(showFakeClick_, false); }
    bool ShowFakeCursor() const { return BoolValue(showFakeCursor_, false); }
    int FakeCursorSize() const { return SliderValue(fakeCursorSize_, 22); }

    bool MovementRandomize() const { return BoolValue(movementRandomize_, true); }
    int MovementExtraHold() const { return SliderValue(movementExtraHold_, 0); }
    int MovementMaximumDistance() const { return SliderValue(movementMaximumDistance_, 1500); }
    int DelayMovement() const { return SliderValue(delayMovement_, 60); }
    int DelayWindup() const { return SliderValue(delayWindup_, 0); }
    int DelayFarm() const { return SliderValue(delayFarm_, 30); }
    bool UseTreTrauLogic() const { return ListValue(movementLogic_, 0) == 1; }
    bool CoordinateKuroEvade() const { return BoolValue(coordinateKuroEvade_, true); }
    int EvadeHandoffGrace() const { return SliderValue(evadeHandoffGrace_, 55); }

    bool PrioritizeFarm() const { return BoolValue(prioritizeFarm_, true); }
    bool PrioritizeMinions() const { return BoolValue(prioritizeMinions_, false); }
    bool PrioritizeSmallJungle() const { return BoolValue(prioritizeSmallJungle_, false); }
    bool PrioritizeWards() const { return BoolValue(prioritizeWards_, false); }
    bool PrioritizeSpecialMinions() const { return BoolValue(prioritizeSpecialMinions_, false); }

    bool AttackWards() const { return BoolValue(attackWards_, false); }
    bool AttackBarrels() const { return BoolValue(attackBarrels_, false); }
    bool AttackClones() const { return BoolValue(attackClones_, false); }
    bool AttackSpecialMinions() const { return BoolValue(attackSpecialMinions_, true); }
    int AttackEarlyWindow() const { return SliderValue(attackEarlyWindow_, 25); }
    int AkshanPassiveMode() const { return ListValue(akshanPassiveMode_, 0); }
    bool WindWallCheck() const { return BoolValue(windWallCheck_, true); }

    bool DebugLogSpellNames() const { return BoolValue(debugLogSpellNames_, false); }
    bool DebugLogAfterAttack() const { return BoolValue(debugLogAfterAttack_, false); }

    OrbwalkingMode ActiveMode() const {
        if (!Enabled() || Game::IsChatOpen() || Game::IsShopOpen()) {
            return OrbwalkingMode::None;
        }
        return IsKeyActive(lastHitKey_, 'X')
            ? OrbwalkingMode::LastHit
            : (IsKeyActive(laneClearKey_, 'V') || IsKeyActive(fastLaneClearKey_, 'U'))
            ? OrbwalkingMode::LaneClear
            : IsKeyActive(hybridKey_, 'C')
            ? OrbwalkingMode::Hybrid
            : IsKeyActive(comboKey_, VK_SPACE)
            ? OrbwalkingMode::Combo
            : OrbwalkingMode::None;
    }

    bool FastLaneClear() const { return IsKeyActive(fastLaneClearKey_, 'U'); }
    bool IsFastLaneClearKeyPressed() const { return fastLaneClearKey_ && fastLaneClearKey_->Active; }

    bool IsComboKeyPressed() const { return comboKey_ && comboKey_->Active; }
    bool IsLaneClearKeyPressed() const { return (laneClearKey_ && laneClearKey_->Active) || (fastLaneClearKey_ && fastLaneClearKey_->Active); }
    bool IsLastHitKeyPressed() const { return lastHitKey_ && lastHitKey_->Active; }
    bool IsHybridKeyPressed() const { return hybridKey_ && hybridKey_->Active; }

private:
    static bool BoolValue(const MenuBool* value, bool fallback) {
        return value ? value->Value : fallback;
    }

    static int SliderValue(const MenuSlider* value, int fallback) {
        return value ? value->Value : fallback;
    }

    static int ListValue(const MenuList* value, int fallback) {
        return value ? value->Index : fallback;
    }

    static bool IsKeyActive(const MenuKeyBind* key, int fallbackKey) {
        return key && key->Active;
    }

    void Build() {
        if (!parentMenu_) {
            return;
        }

        if (parentMenu_->Root) {
            menu_ = parentMenu_;
        } else {
            menu_ = parentMenu_->AddSubMenu(new Menu("orbwalker", "Orbwalker"));
        }
        if (!menu_) {
            return;
        }

        drawingsMenu_ = menu_->AddSubMenu(new Menu("drawings", "Drawings"));
        if (drawingsMenu_) {
            drawAARange_ = drawingsMenu_->Add(new MenuBool("drawAARange", "Auto-Attack Range", true));
            aaRangeStyle_ = drawingsMenu_->Add(new MenuList("aaRangeStyle", "AA Range Drawing Style", { "Default (Vector)", "Texture Ring" }, 0));
            aaRangeTexture_ = drawingsMenu_->Add(new MenuList("aaRangeTexture", "AA Range Texture", { "Demacia Trophy", "Hextech AoE", "Bilgewater Shield", "Olaf Circle" }, 0));
            aaRangeOpacity_ = drawingsMenu_->Add(new MenuSlider("aaRangeOpacity", "Texture Opacity (%)", 100, 10, 100));
            aaRangeRotateSpeed_ = drawingsMenu_->Add(new MenuSlider("aaRangeRotateSpeed", "Texture Rotation Speed", 8, 1, 20));
            aaRangeFadeWidth_ = drawingsMenu_->Add(new MenuSlider(
                "aaRangeFadeWidth", "AA Range Fade Width", 200, 1, 600));
            aaRangeFadeOpacityPercent_ = drawingsMenu_->Add(new MenuSlider(
                "aaRangeFadeOpacityPercent", "AA Range Max Opacity (%)", 10, 0, 100));
            drawAARangeEnemy_ = drawingsMenu_->Add(new MenuBool("drawAARangeEnemy", "Auto-Attack Range Enemy", false));
            drawAzirSoldierRanges_ = drawingsMenu_->Add(new MenuBool(
                "drawAzirSoldierRanges", "Azir Sand Soldier Ranges", true));
            drawExtraHoldPosition_ = drawingsMenu_->Add(new MenuBool("drawExtraHoldPosition", "Extra Hold Position", false));
            drawKillableMinion_ = drawingsMenu_->Add(new MenuBool("drawKillableMinion", "Killable Minions", false));
            drawKillableMinionFade_ = drawingsMenu_->Add(new MenuBool("drawKillableMinionFade", "Killable Minions Fade Effect", false));
            showFakeClick_ = drawingsMenu_->Add(new MenuBool("ShowFakeClick", "Show Fake Click", false));
            showFakeCursor_ = drawingsMenu_->Add(new MenuBool("ShowFakeCursor", "Show Fake Cursor", false));
            fakeCursorSize_ = drawingsMenu_->Add(new MenuSlider("FakeCursorSize", "Fake Cursor Size", 22, 12, 42));
        }

        // ── Movement sub-menu ────────────────────────────────────────────────
        movementMenu_ = menu_->AddSubMenu(new Menu("movement", "Movement"));
        if (movementMenu_) {
            movementMenu_->Add(new MenuSeparator("separatorLogic", "Logic"));
            movementLogic_ = movementMenu_->Add(new MenuList("movementLogic", "Movement Logic", { "Kuro", "TreTrau" }, 0));

            movementMenu_->Add(new MenuSeparator("separatorEvade", "KuroEvade Coordination"));
            coordinateKuroEvade_ = movementMenu_->Add(new MenuBool(
                "coordinateKuroEvade", "Let KuroEvade Own Actions While Dodging", true));
            evadeHandoffGrace_ = movementMenu_->Add(new MenuSlider(
                "evadeHandoffGrace", "Movement Handoff Grace (ms)", 55, 0, 150));

            movementMenu_->Add(new MenuSeparator("separatorMovement", "Movement"));
            movementRandomize_ = movementMenu_->Add(new MenuBool("movementRandomize", "Randomize Location", true));
            movementExtraHold_ = movementMenu_->Add(new MenuSlider("movementExtraHold", "Extra Hold Position", 0, 0, 250));
            movementMaximumDistance_ = movementMenu_->Add(new MenuSlider("movementMaximumDistance", "Maximum Distance", 1500, 500, 1500));

            movementMenu_->Add(new MenuSeparator("separatorDelay", "Delay"));
            delayMovement_ = movementMenu_->Add(new MenuSlider("delayMovement", "Movement", 60, 0, 500));
            delayWindup_ = movementMenu_->Add(new MenuSlider("delayWindup", "Windup", 0, 0, 200));
            delayFarm_ = movementMenu_->Add(new MenuSlider("delayFarm", "Farm", 30, 0, 200));
        }

        // ── Targeting sub-menu ───────────────────────────────────────────────
        targetingMenu_ = menu_->AddSubMenu(new Menu("targeting", "Targeting"));
        if (targetingMenu_) {
            targetingMenu_->Add(new MenuSeparator("separatorPrioritization", "Prioritization"));
            prioritizeFarm_ = targetingMenu_->Add(new MenuBool("prioritizeFarm", "Farm Over Harass", true));
            prioritizeMinions_ = targetingMenu_->Add(new MenuBool("prioritizeMinions", "Minions Over Objectives", false));
            prioritizeSmallJungle_ = targetingMenu_->Add(new MenuBool("prioritizeSmallJungle", "Small Jungle", false));
            prioritizeWards_ = targetingMenu_->Add(new MenuBool("prioritizeWards", "Wards", false));
            prioritizeSpecialMinions_ = targetingMenu_->Add(new MenuBool("prioritizeSpecialMinions", "Special Minions", false));

            targetingMenu_->Add(new MenuSeparator("separatorAttack", "Attack"));
            attackWards_ = targetingMenu_->Add(new MenuBool("attackWards", "Wards", false));
            attackBarrels_ = targetingMenu_->Add(new MenuBool("attackBarrels", "Barrels", false));
            attackClones_ = targetingMenu_->Add(new MenuBool("attackClones", "Clones", false));
            attackSpecialMinions_ = targetingMenu_->Add(new MenuBool("attackSpecialMinions", "Special Minions", true));
            attackEarlyWindow_ = targetingMenu_->Add(new MenuSlider("attackEarlyWindow", "Attack Early Window (ms)", 25, 0, 250));

            targetingMenu_->Add(new MenuSeparator("separatorCollision", "Collision"));
            windWallCheck_ = targetingMenu_->Add(new MenuBool(
                "windWallCheck", "Wind Wall Check (Yasuo/Samira/Mel)", true));

            targetingMenu_->Add(new MenuSeparator("separatorAkshan", "Akshan Passive"));
            akshanPassiveMode_ = targetingMenu_->Add(new MenuList(
                "akshanPassiveMode", "Akshan Passive",
                { "Always 2-Hit", "Always 1-Hit", "Smart" }, 0));
        }

        // ── Debug sub-menu ───────────────────────────────────────────────────
        debugMenu_ = menu_->AddSubMenu(new Menu("debug", "Debug"));
        if (debugMenu_) {
            debugLogSpellNames_ = debugMenu_->Add(new MenuBool(
                "debugLogSpellNames", "Log Player Spell Names (ProcessSpell)", false));
            debugLogAfterAttack_ = debugMenu_->Add(new MenuBool(
                "debugLogAfterAttack", "Log AfterAttack (spell + target)", false));
        }

        menu_->Add(new MenuSeparator("separatorKeys", "Key Bindings"));
        lastHitKey_ = menu_->Add(new MenuKeyBind("lasthitKey", "Last Hit", 'X', KeyBindType::Press));
        laneClearKey_ = menu_->Add(new MenuKeyBind("laneclearKey", "Lane Clear", 'V', KeyBindType::Press));
        fastLaneClearKey_ = menu_->Add(new MenuKeyBind("fastLaneClearKey", "Fast Lane Clear", 'U', KeyBindType::Toggle));
        hybridKey_ = menu_->Add(new MenuKeyBind("hybridKey", "Hybrid", 'C', KeyBindType::Press));
        comboKey_ = menu_->Add(new MenuKeyBind("comboKey", "Combo", VK_SPACE, KeyBindType::Press));

        menu_->Add(new MenuSeparator("separatorControls", "Controls"));
        attackEnabledOption_ = menu_->Add(new MenuBool("attackEnabled", "Attack Enable", true));
        moveEnabledOption_ = menu_->Add(new MenuBool("moveEnabled", "Move Enable", true));
        enabledOption_ = menu_->Add(new MenuBool("enabledOption", "Enabled", true));
    }

    Menu* parentMenu_ = nullptr;
    Menu* menu_ = nullptr;
    Menu* drawingsMenu_ = nullptr;
    Menu* movementMenu_ = nullptr;
    Menu* targetingMenu_ = nullptr;
    Menu* debugMenu_ = nullptr;


    MenuBool* drawAARange_ = nullptr;
    MenuList* aaRangeStyle_ = nullptr;
    MenuList* aaRangeTexture_ = nullptr;
    MenuSlider* aaRangeOpacity_ = nullptr;
    MenuSlider* aaRangeRotateSpeed_ = nullptr;
    MenuSlider* aaRangeFadeWidth_ = nullptr;
    MenuSlider* aaRangeFadeOpacityPercent_ = nullptr;
    MenuBool* drawAARangeEnemy_ = nullptr;
    MenuBool* drawAzirSoldierRanges_ = nullptr;
    MenuBool* drawExtraHoldPosition_ = nullptr;
    MenuBool* drawKillableMinion_ = nullptr;
    MenuBool* drawKillableMinionFade_ = nullptr;
    MenuBool* showFakeClick_ = nullptr;
    MenuBool* showFakeCursor_ = nullptr;
    MenuSlider* fakeCursorSize_ = nullptr;

    MenuList* movementLogic_ = nullptr;
    MenuBool* coordinateKuroEvade_ = nullptr;
    MenuSlider* evadeHandoffGrace_ = nullptr;
    MenuBool* movementRandomize_ = nullptr;
    MenuSlider* movementExtraHold_ = nullptr;
    MenuSlider* movementMaximumDistance_ = nullptr;
    MenuSlider* delayMovement_ = nullptr;
    MenuSlider* delayWindup_ = nullptr;
    MenuSlider* delayFarm_ = nullptr;

    MenuBool* prioritizeFarm_ = nullptr;
    MenuBool* prioritizeMinions_ = nullptr;
    MenuBool* prioritizeSmallJungle_ = nullptr;
    MenuBool* prioritizeWards_ = nullptr;
    MenuBool* prioritizeSpecialMinions_ = nullptr;

    MenuBool* attackWards_ = nullptr;
    MenuBool* attackBarrels_ = nullptr;
    MenuBool* attackClones_ = nullptr;
    MenuBool* attackSpecialMinions_ = nullptr;
    MenuSlider* attackEarlyWindow_ = nullptr;
    MenuList* akshanPassiveMode_ = nullptr;
    MenuBool* windWallCheck_ = nullptr;

    MenuBool* debugLogSpellNames_ = nullptr;
    MenuBool* debugLogAfterAttack_ = nullptr;

    MenuKeyBind* lastHitKey_ = nullptr;
    MenuKeyBind* laneClearKey_ = nullptr;
    MenuKeyBind* fastLaneClearKey_ = nullptr;
    MenuKeyBind* hybridKey_ = nullptr;
    MenuKeyBind* comboKey_ = nullptr;
    MenuBool* attackEnabledOption_ = nullptr;
    MenuBool* moveEnabledOption_ = nullptr;
    MenuBool* enabledOption_ = nullptr;
};

} // namespace OrbwalkerKuro
