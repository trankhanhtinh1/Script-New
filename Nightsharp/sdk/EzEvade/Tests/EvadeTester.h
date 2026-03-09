#pragma once
#include "sdk/SDK.h"
#include "sdk/EzEvade/Helpers/ObjectCache.h"
#include "sdk/EzEvade/Utils/ConsolePrinter.h"

namespace EzEvade {

class EvadeTester {
public:
    explicit EvadeTester(const std::shared_ptr<SDK::MenuUI::Menu>& mainMenu) {
        if (!mainMenu) {
            return;
        }

        auto testMenu = mainMenu->AddSubMenu("Test", "Test");
        testMenu->Add<SDK::MenuUI::MenuBool>("TestWall", "TestWall", true);
        testMenu->Add<SDK::MenuUI::MenuBool>("TestPath", "TestPath", true);
        testMenu->Add<SDK::MenuUI::MenuBool>("TestTracker", "TestTracker", false);
        testMenu->Add<SDK::MenuUI::MenuBool>("TestHeroPos", "TestHeroPos", true);
        testMenu->Add<SDK::MenuUI::MenuBool>("DrawHeroPos", "DrawHeroPos", true);
        testMenu->Add<SDK::MenuUI::MenuBool>("TestSpellEndTime", "TestSpellEndTime", true);
        testMenu->Add<SDK::MenuUI::MenuBool>("ShowBuffs", "ShowBuffs", true);
        testMenu->Add<SDK::MenuUI::MenuBool>("ShowDashInfo", "ShowDashInfo", true);
        testMenu->Add<SDK::MenuUI::MenuBool>("ShowProcessSpell", "ShowProcessSpell", true);
        testMenu->Add<SDK::MenuUI::MenuBool>("ShowDoCastInfo", "ShowDoCastInfo", true);
        testMenu->Add<SDK::MenuUI::MenuBool>("ShowMissileInfo", "ShowMissileInfo", true);
        testMenu->Add<SDK::MenuUI::MenuBool>("ShowWindupTime", "ShowWindupTime", true);
        testMenu->Add<SDK::MenuUI::MenuKeyBind>("TestMoveTo", "TestMoveTo", 'L', SDK::MenuUI::KeyBindType::Toggle);
        testMenu->Add<SDK::MenuUI::MenuBool>("EvadeTesterPing", "EvadeTesterPing", false);

        ObjectCache::Menu.AddMenuToCache(testMenu);
        ConsolePrinter::Print("EvadeTester loaded");
    }
};

} // namespace EzEvade

