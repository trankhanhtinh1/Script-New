#pragma once

#include "../IPlugin.h"
#include "EvadeSpells/EvadeSpellDatabase.h"
#include "../../sdk/GameObjects/ObjectManager.h"
#include "../../sdk/UI/UI.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

namespace Plugins {
namespace EzEvadePlugin {

class Plugin final : public IPlugin {
public:
    const char* GetName() const override { return "EzEvade"; }
    const char* GetInternalId() const override { return "ezevade"; }
    const char* GetAuthor() const override { return "EzEvade/Nightsharp"; }
    PluginCategory GetCategory() const override { return PluginCategory::Utility; }
    bool AutoLoadByDefault() const override { return true; }

    void OnLoad() override {
        BuildMenu();
    }

    SDK::MenuUI::Menu* GetMenuRoot() override {
        return m_menu;
    }

private:
    SDK::MenuUI::Menu* m_menu = nullptr;
    SDK::MenuUI::Menu* m_mainMenu = nullptr;
    SDK::MenuUI::Menu* m_evadeSpellMenu = nullptr;
    SDK::MenuUI::Menu* m_keyMenu = nullptr;
    SDK::MenuUI::Menu* m_miscMenu = nullptr;
    SDK::MenuUI::Menu* m_drawMenu = nullptr;

    void BuildMenu() {
        if (m_menu) {
            return;
        }

        m_menu = SDK::Menu::Create("ezEvade", "ezEvade");
        BuildMainMenu();
        BuildEvadeSpellMenu();
        BuildKeyMenu();
        BuildMiscMenu();
        BuildDrawMenu();
    }

    void BuildMainMenu() {
        if (!m_menu || m_mainMenu) {
            return;
        }

        m_mainMenu = m_menu->AddSubMenu("Main", "Main");
        m_mainMenu->Add<SDK::MenuKeyBind>("DodgeSkillShots",
            "Dodge SkillShots",
            'K',
            SDK::KeyBindType::Toggle,
            true);
        m_mainMenu->Add<SDK::MenuKeyBind>("ActivateEvadeSpells",
            "Use Evade Spells",
            'K',
            SDK::KeyBindType::Toggle,
            true);
        m_mainMenu->Add<SDK::MenuBool>("DodgeDangerous", "Dodge Only Dangerous", false);
        m_mainMenu->Add<SDK::MenuBool>("DodgeFOWSpells", "Dodge FOW SkillShots", true);
        m_mainMenu->Add<SDK::MenuBool>("DodgeCircularSpells", "Dodge Circular SkillShots", true);
    }

    void BuildEvadeSpellMenu() {
        if (!m_menu || m_evadeSpellMenu) {
            return;
        }

        m_evadeSpellMenu = m_menu->AddSubMenu("EvadeSpells", "Evade Spells");
        const auto player = SDK::ObjectManager::Player();
        const std::string championName = player.IsValid() ? player.CharacterName() : "";

        for (const auto& spell : EzEvade::GetEvadeSpellDatabase()) {
            const bool globalSpell = SameText(spell.charName, "AllChampions");
            const bool championSpell = !championName.empty() && SameText(spell.charName, championName);
            if (!globalSpell && !championSpell && !spell.isItem) {
                continue;
            }
            CreateEvadeSpellMenu(spell);
        }
    }

    void BuildKeyMenu() {
        if (!m_menu || m_keyMenu) {
            return;
        }

        m_keyMenu = m_menu->AddSubMenu("KeySettings", "Key Settings");
        m_keyMenu->Add<SDK::MenuBool>("DodgeDangerousKeyEnabled", "Enable Dodge Only Dangerous Keys", false);
        m_keyMenu->Add<SDK::MenuKeyBind>("DodgeDangerousKey",
            "Dodge Only Dangerous Key",
            VK_SPACE,
            SDK::KeyBindType::Press,
            false);
        m_keyMenu->Add<SDK::MenuKeyBind>("DodgeDangerousKey2",
            "Dodge Only Dangerous Key 2",
            'V',
            SDK::KeyBindType::Press,
            false);
        m_keyMenu->Add<SDK::MenuBool>("DodgeOnlyOnComboKeyEnabled", "Enable Dodge Only On Combo Key", false);
        m_keyMenu->Add<SDK::MenuKeyBind>("DodgeComboKey",
            "Dodge Only Combo Key",
            VK_SPACE,
            SDK::KeyBindType::Press,
            false);
        m_keyMenu->Add<SDK::MenuBool>("DontDodgeKeyEnabled", "Enable Don't Dodge Key", false);
        m_keyMenu->Add<SDK::MenuKeyBind>("DontDodgeKey",
            "Don't Dodge Key",
            'Z',
            SDK::KeyBindType::Press,
            false);
    }

    void BuildMiscMenu() {
        if (!m_menu || m_miscMenu) {
            return;
        }

        m_miscMenu = m_menu->AddSubMenu("MiscSettings", "Misc Settings");
        m_miscMenu->Add<SDK::MenuBool>("HigherPrecision", "Enhanced Dodge Precision", false);
        m_miscMenu->Add<SDK::MenuBool>("RecalculatePosition", "Recalculate Path", true);
        m_miscMenu->Add<SDK::MenuBool>("ContinueMovement", "Continue Last Movement", true);
        m_miscMenu->Add<SDK::MenuBool>("CalculateWindupDelay", "Calculate Windup Delay", true);
        m_miscMenu->Add<SDK::MenuBool>("CheckSpellCollision", "Check Spell Collision", false);
        m_miscMenu->Add<SDK::MenuBool>("PreventDodgingUnderTower", "Prevent Dodging Under Tower", false);
        m_miscMenu->Add<SDK::MenuBool>("PreventDodgingNearEnemy", "Prevent Dodging Near Enemies", true);
        m_miscMenu->Add<SDK::MenuBool>("AdvancedSpellDetection", "Advanced Spell Detection", false);
        m_miscMenu->Add<SDK::MenuBool>("ClickRemove", "Allow Left Click Removal", true);
        m_miscMenu->Add<SDK::MenuList>("EvadeMode",
            "Evade Profile",
            std::vector<std::string>{"Smooth", "Very Smooth", "Fastest", "Hawk", "Kurisu", "GuessWho"},
            0);
        m_miscMenu->Add<SDK::MenuBool>("ResetConfig", "Reset Evade Config", false);

        auto* limiterMenu = m_miscMenu->AddSubMenu("Limiter", "Humanizer");
        limiterMenu->Add<SDK::MenuBool>("ClickOnlyOnce", "Click Only Once", true);
        limiterMenu->Add<SDK::MenuBool>("EnableEvadeDistance", "Extended Evade", false);
        limiterMenu->Add<SDK::MenuSlider>("TickLimiter", "Tick Limiter", 100, 0, 500);
        limiterMenu->Add<SDK::MenuSlider>("SpellDetectionTime", "Spell Detection Time", 0, 0, 1000);
        limiterMenu->Add<SDK::MenuSlider>("ReactionTime", "Reaction Time", 0, 0, 500);
        limiterMenu->Add<SDK::MenuSlider>("DodgeInterval", "Dodge Interval", 0, 0, 2000);

        auto* fastEvadeMenu = m_miscMenu->AddSubMenu("FastEvade", "Fast Evade");
        fastEvadeMenu->Add<SDK::MenuBool>("FastMovementBlock", "Fast Movement Block", false);
        fastEvadeMenu->Add<SDK::MenuSlider>("FastEvadeActivationTime", "FastEvade Activation Time", 65, 0, 500);
        fastEvadeMenu->Add<SDK::MenuSlider>("SpellActivationTime", "Spell Activation Time", 400, 0, 1000);
        fastEvadeMenu->Add<SDK::MenuSlider>("RejectMinDistance", "Collision Distance Buffer", 10, 0, 100);

        auto* bufferMenu = m_miscMenu->AddSubMenu("ExtraBuffers", "Extra Buffers");
        bufferMenu->Add<SDK::MenuSlider>("ExtraPingBuffer", "Extra Ping Buffer", 65, 0, 200);
        bufferMenu->Add<SDK::MenuSlider>("ExtraCPADistance", "Extra Collision Distance", 10, 0, 150);
        bufferMenu->Add<SDK::MenuSlider>("ExtraSpellRadius", "Extra Spell Radius", 0, 0, 100);
        bufferMenu->Add<SDK::MenuSlider>("ExtraEvadeDistance", "Extra Evade Distance", 100, 0, 300);
        bufferMenu->Add<SDK::MenuSlider>("ExtraAvoidDistance", "Extra Avoid Distance", 50, 0, 300);
        bufferMenu->Add<SDK::MenuSlider>("MinComfortZone", "Min Distance to Champion", 550, 0, 1000);
    }

    void BuildDrawMenu() {
        if (!m_menu || m_drawMenu) {
            return;
        }

        m_drawMenu = m_menu->AddSubMenu("Draw", "Draw");
        m_drawMenu->Add<SDK::MenuBool>("DrawSkillShots", "Draw SkillShots", true);
        m_drawMenu->Add<SDK::MenuBool>("ShowStatus", "Show Evade Status", true);
        m_drawMenu->Add<SDK::MenuBool>("DrawSpellPos", "Draw Spell Position", false);
        m_drawMenu->Add<SDK::MenuBool>("DrawEvadePosition", "Draw Evade Position", false);
    }

    static std::string NormalizeKey(const std::string& value) {
        std::string key;
        key.reserve(value.size());
        for (const char ch : value) {
            if (std::isalnum(static_cast<unsigned char>(ch))) {
                key.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
            }
        }
        return key;
    }

    static bool SameText(const std::string& left, const std::string& right) {
        return NormalizeKey(left) == NormalizeKey(right);
    }

    static const char* SlotText(EzEvade::SpellSlotId slot) {
        switch (slot) {
        case EzEvade::SpellSlotId::Q:
            return "Q";
        case EzEvade::SpellSlotId::W:
            return "W";
        case EzEvade::SpellSlotId::E:
            return "E";
        case EzEvade::SpellSlotId::R:
            return "R";
        case EzEvade::SpellSlotId::Summoner1:
        case EzEvade::SpellSlotId::Summoner2:
            return "Summoner";
        default:
            return "Item";
        }
    }

    static int DefaultSpellMode(const EzEvade::EvadeSpellData& spell) {
        return spell.dangerlevel > 3 ? 0 : 1;
    }

    void CreateEvadeSpellMenu(const EzEvade::EvadeSpellData& spell) {
        if (!m_evadeSpellMenu) {
            return;
        }

        std::string menuName = spell.name + " (" + SlotText(spell.spellKey) + ") Settings";
        if (spell.isItem) {
            menuName = spell.name + " Settings";
        }

        auto* spellMenu = m_evadeSpellMenu->AddSubMenu(
            spell.charName + spell.name + "EvadeSpellSettings",
            menuName);
        if (!spellMenu) {
            return;
        }

        spellMenu->Add<SDK::MenuBool>(spell.name + "UseEvadeSpell", "Use Spell", true);
        spellMenu->Add<SDK::MenuList>(spell.name + "EvadeSpellDangerLevel",
            "Danger Level",
            std::vector<std::string>{"Low", "Normal", "High", "Extreme"},
            (std::max)(0, (std::min)(3, spell.dangerlevel - 1)));
        spellMenu->Add<SDK::MenuList>(spell.name + "EvadeSpellMode",
            "Spell Mode",
            std::vector<std::string>{"Undodgeable", "Activation Time", "Always"},
            DefaultSpellMode(spell));
    }
};

} // namespace EzEvadePlugin
} // namespace Plugins
