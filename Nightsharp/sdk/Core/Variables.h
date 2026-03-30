#pragma once

#include "Constants.h"
#include "Game.h"
#include "../Wrappers/Orbwalking/Orbwalker.h"
#include "../Wrappers/TargetSelector/TargetSelector.h"

namespace SDK::Variables {

    inline int GameTimeTickCount() {
        return SDK::Game::TickCount();
    }

    inline const char* GameVersionString() {
        return Constants::Patch();
    }

    inline const char* AssemblyVersion() {
        return Constants::Patch();
    }

    inline SDK::OrbwalkerMode OrbwalkerActiveMode() {
        return SDK::Orbwalker::GetMode();
    }

    inline SDK::Menu* OrbwalkerMenu() {
        return SDK::Orbwalker::GetMenu();
    }

    inline SDK::Menu* TargetSelectorMenu() {
        return SDK::TargetSelector::GetMenu();
    }

} // namespace SDK::Variables
