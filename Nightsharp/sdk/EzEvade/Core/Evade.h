#pragma once
#include "sdk/SDK.h"
#include "sdk/EzEvade/Core/EvadeHelper.h"
#include "sdk/EzEvade/EvadeSpells/EvadeSpell.h"
#include "sdk/EzEvade/Helpers/EvadeCommand.h"
#include "sdk/EzEvade/Helpers/EvadeContext.h"
#include "sdk/EzEvade/Helpers/ObjectCache.h"
#include "sdk/EzEvade/Helpers/Situation.h"
#include "sdk/EzEvade/Spells/SpellDetector.h"
#include "sdk/EzEvade/Spells/SpellDrawer.h"
#include <memory>

namespace EzEvade {

class Evade {
public:
    static inline std::shared_ptr<SDK::MenuUI::Menu> Menu = nullptr;

    Evade() {
        LoadAssembly();
    }

    static void SetEnabled(bool enabled) {
        EvadeRuntimeState::Enabled = enabled;
        if (!enabled) {
            EvadeContext::IsDodging = false;
            EvadeContext::HasLastPosInfo = false;
            EvadeRuntimeState::DodgeOnlyDangerous = false;
        }
    }

    static bool IsEnabled() {
        return EvadeRuntimeState::Enabled;
    }

    static std::shared_ptr<SDK::MenuUI::Menu> GetMenu() {
        return Menu;
    }

    static void OnRender() {
        if (!EvadeRuntimeState::Enabled) {
            return;
        }

        if (s_spellDrawer) {
            s_spellDrawer->Draw();
        }
    }

private:
    static inline std::unique_ptr<SpellDetector> s_spellDetector = nullptr;
    static inline std::unique_ptr<SpellDrawer> s_spellDrawer = nullptr;
    static inline std::unique_ptr<EvadeSpell> s_evadeSpell = nullptr;
    static inline bool s_registeredUpdate = false;
    static inline bool s_registeredDetected = false;
    static inline bool s_initialized = false;

    static void LoadAssembly() {
        if (s_initialized) {
            return;
        }
        OnGameLoad();
    }

    static void OnGameLoad() {
        Menu = SDK::MenuUI::Menu::Create("ezEvade", "ezEvade");
        BuildMenu(*Menu);
        EvadeRuntimeState::Enabled = true;

        ObjectCache::Initialize(Menu);
        s_spellDetector = std::make_unique<SpellDetector>(Menu);
        s_evadeSpell = std::make_unique<EvadeSpell>(Menu);
        s_spellDrawer = std::make_unique<SpellDrawer>(Menu);

        PositionInfo::CurrentMovePositionProvider = []() {
            return EvadeContext::HasLastPosInfo ? EvadeContext::LastPosInfo : PositionInfo::SetAllDodgeable();
        };

        if (!s_registeredDetected) {
            SpellDetector::RegisterOnProcessDetectedSpells([]() {
                OnProcessDetectedSpells();
            });
            s_registeredDetected = true;
        }

        if (!s_registeredUpdate) {
            SDK::EventSystem::OnGameUpdate([](float) {
                OnGameUpdate();
            });
            s_registeredUpdate = true;
        }

        s_initialized = true;
    }

    static void BuildMenu(SDK::MenuUI::Menu& root) {
        auto mainMenu = root.AddSubMenu("Main", "Main");
        mainMenu->Add<SDK::MenuUI::MenuKeyBind>("DodgeSkillShots", "Dodge SkillShots", 'K', SDK::MenuUI::KeyBindType::Toggle, true);
        mainMenu->Add<SDK::MenuUI::MenuKeyBind>("ActivateEvadeSpells", "Use Evade Spells", 'K', SDK::MenuUI::KeyBindType::Toggle, true);
        mainMenu->Add<SDK::MenuUI::MenuBool>("DodgeDangerous", "Dodge Only Dangerous", false);
        mainMenu->Add<SDK::MenuUI::MenuBool>("DodgeFOWSpells", "Dodge FOW SkillShots", true);
        mainMenu->Add<SDK::MenuUI::MenuBool>("DodgeCircularSpells", "Dodge Circular SkillShots", true);

        auto keyMenu = root.AddSubMenu("KeySettings", "Key Settings");
        keyMenu->Add<SDK::MenuUI::MenuBool>("DodgeDangerousKeyEnabled", "Enable Dodge Only Dangerous Keys", false);
        keyMenu->Add<SDK::MenuUI::MenuKeyBind>("DodgeDangerousKey", "Dodge Only Dangerous Key", VK_SPACE, SDK::MenuUI::KeyBindType::Press);
        keyMenu->Add<SDK::MenuUI::MenuKeyBind>("DodgeDangerousKey2", "Dodge Only Dangerous Key 2", 'V', SDK::MenuUI::KeyBindType::Press);
        keyMenu->Add<SDK::MenuUI::MenuBool>("DodgeOnlyOnComboKeyEnabled", "Enable Dodge Only On Combo Key", false);
        keyMenu->Add<SDK::MenuUI::MenuKeyBind>("DodgeComboKey", "Dodge Only Combo Key", VK_SPACE, SDK::MenuUI::KeyBindType::Press);
        keyMenu->Add<SDK::MenuUI::MenuBool>("DontDodgeKeyEnabled", "Enable Don't Dodge Key", false);
        keyMenu->Add<SDK::MenuUI::MenuKeyBind>("DontDodgeKey", "Don't Dodge Key", 'Z', SDK::MenuUI::KeyBindType::Press);

        auto miscMenu = root.AddSubMenu("MiscSettings", "Misc Settings");
        miscMenu->Add<SDK::MenuUI::MenuBool>("HigherPrecision", "Enhanced Dodge Precision", false);
        miscMenu->Add<SDK::MenuUI::MenuBool>("RecalculatePosition", "Recalculate Path", true);
        miscMenu->Add<SDK::MenuUI::MenuBool>("ContinueMovement", "Continue Last Movement", true);
        miscMenu->Add<SDK::MenuUI::MenuBool>("CalculateWindupDelay", "Calculate Windup Delay", true);
        miscMenu->Add<SDK::MenuUI::MenuBool>("CheckSpellCollision", "Check Spell Collision", false);
        miscMenu->Add<SDK::MenuUI::MenuBool>("PreventDodgingUnderTower", "Prevent Dodging Under Tower", false);
        miscMenu->Add<SDK::MenuUI::MenuBool>("PreventDodgingNearEnemy", "Prevent Dodging Near Enemies", true);
        miscMenu->Add<SDK::MenuUI::MenuBool>("AdvancedSpellDetection", "Advanced Spell Detection", false);
        miscMenu->Add<SDK::MenuUI::MenuBool>("ClickRemove", "Allow Left Click Removal", true);
        miscMenu->Add<SDK::MenuUI::MenuList>("EvadeMode", "Evade Profile",
                                             std::vector<std::string>{ "Smooth", "Very Smooth", "Fastest", "Hawk", "Kurisu", "GuessWho" }, 0);
        miscMenu->Add<SDK::MenuUI::MenuBool>("ResetConfig", "Reset Evade Config", false);

        auto limiterMenu = miscMenu->AddSubMenu("Limiter", "Humanizer");
        limiterMenu->Add<SDK::MenuUI::MenuBool>("ClickOnlyOnce", "Click Only Once", true);
        limiterMenu->Add<SDK::MenuUI::MenuBool>("EnableEvadeDistance", "Extended Evade", false);
        limiterMenu->Add<SDK::MenuUI::MenuSlider>("TickLimiter", "Tick Limiter", 100, 0, 500);
        limiterMenu->Add<SDK::MenuUI::MenuSlider>("SpellDetectionTime", "Spell Detection Time", 0, 0, 1000);
        limiterMenu->Add<SDK::MenuUI::MenuSlider>("ReactionTime", "Reaction Time", 0, 0, 500);
        limiterMenu->Add<SDK::MenuUI::MenuSlider>("DodgeInterval", "Dodge Interval", 0, 0, 2000);

        auto fastEvadeMenu = miscMenu->AddSubMenu("FastEvade", "Fast Evade");
        fastEvadeMenu->Add<SDK::MenuUI::MenuBool>("FastMovementBlock", "Fast Movement Block", false);
        fastEvadeMenu->Add<SDK::MenuUI::MenuSlider>("FastEvadeActivationTime", "FastEvade Activation Time", 65, 0, 500);
        fastEvadeMenu->Add<SDK::MenuUI::MenuSlider>("SpellActivationTime", "Spell Activation Time", 400, 0, 1000);
        fastEvadeMenu->Add<SDK::MenuUI::MenuSlider>("RejectMinDistance", "Collision Distance Buffer", 10, 0, 100);

        auto bufferMenu = miscMenu->AddSubMenu("ExtraBuffers", "Extra Buffers");
        bufferMenu->Add<SDK::MenuUI::MenuSlider>("ExtraPingBuffer", "Extra Ping Buffer", 65, 0, 200);
        bufferMenu->Add<SDK::MenuUI::MenuSlider>("ExtraCPADistance", "Extra Collision Distance", 10, 0, 150);
        bufferMenu->Add<SDK::MenuUI::MenuSlider>("ExtraSpellRadius", "Extra Spell Radius", 0, 0, 100);
        bufferMenu->Add<SDK::MenuUI::MenuSlider>("ExtraEvadeDistance", "Extra Evade Distance", 100, 0, 300);
        bufferMenu->Add<SDK::MenuUI::MenuSlider>("ExtraAvoidDistance", "Extra Avoid Distance", 50, 0, 300);
        bufferMenu->Add<SDK::MenuUI::MenuSlider>("MinComfortZone", "Min Distance to Champion", 550, 0, 1000);
    }

    static void OnGameUpdate() {
        if (!EvadeRuntimeState::Enabled) {
            return;
        }

        ObjectCache::Refresh();

        if (EvadeContext::IsDodging && !Situation::ShouldDodge()) {
            EvadeContext::IsDodging = false;
        }

        if (s_evadeSpell) {
            s_evadeSpell->UseEvadeSpell();
        }

        CheckDodgeOnlyDangerous();
    }

    static void OnProcessDetectedSpells() {
        if (!EvadeRuntimeState::Enabled) {
            return;
        }

        ObjectCache::Refresh();

        if (!ObjectCache::Menu.GetKey("DodgeSkillShots", true)) {
            EvadeContext::LastPosInfo = PositionInfo::SetAllUndodgeable();
            EvadeContext::HasLastPosInfo = true;
            if (s_evadeSpell) {
                s_evadeSpell->UseEvadeSpell();
            }
            return;
        }

        const bool inDanger = Position::CheckDangerousPos(ObjectCache::MyHeroCache.ServerPos2D, 0.0f)
            || Position::CheckDangerousPos(ObjectCache::MyHeroCache.ServerPos2DExtra, 0.0f);

        if (inDanger) {
            if (s_evadeSpell && s_evadeSpell->PreferEvadeSpell()) {
                EvadeContext::LastPosInfo = PositionInfo::SetAllUndodgeable();
                EvadeContext::HasLastPosInfo = true;
            } else {
                PositionInfo posInfo = EvadeHelper::GetBestPosition();
                EvadeContext::LastPosInfo = PositionInfoExtensions::CompareLastMovePos(posInfo);
                EvadeContext::HasLastPosInfo = true;
            }
        } else {
            EvadeContext::LastPosInfo = PositionInfo::SetAllDodgeable();
            EvadeContext::HasLastPosInfo = true;
        }

        EvadeContext::IsDodging = inDanger;
        if (inDanger && Situation::ShouldDodge() && !EvadeContext::LastPosInfo.Position.IsZero()) {
            EvadeCommand::MoveTo(EvadeContext::LastPosInfo.Position);
        }
    }

    static void CheckDodgeOnlyDangerous() {
        if (!EvadeRuntimeState::Enabled) {
            EvadeRuntimeState::DodgeOnlyDangerous = false;
            return;
        }

        const bool dodgeOnlyDangerous = Situation::IsDodgeDangerousEnabled();
        if (!EvadeRuntimeState::DodgeOnlyDangerous && dodgeOnlyDangerous && s_spellDetector) {
            s_spellDetector->RemoveNonDangerousSpells();
        }
        EvadeRuntimeState::DodgeOnlyDangerous = dodgeOnlyDangerous;
    }
};

} // namespace EzEvade
