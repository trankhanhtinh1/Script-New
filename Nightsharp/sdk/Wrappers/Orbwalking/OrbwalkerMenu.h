#pragma once

#include "OrbwalkerTypes.h"

#include "../../Core/Game.h"

#include <Windows.h>

namespace SDK {

class OrbwalkerMenu {
public:
    explicit OrbwalkerMenu(Menu* parentMenu)
        : parentMenu_(parentMenu) {
        Build();
    }

    OrbwalkingMode ActiveMode() const {
        const bool active = orbwalkKey_
            ? orbwalkKey_->Active
            : ((::GetAsyncKeyState(VK_SPACE) & 0x8000) != 0);
        if (!active) {
            return OrbwalkingMode::None;
        }
        if (Game::IsChatOpen() || Game::IsShopOpen()) {
            return OrbwalkingMode::None;
        }
        return OrbwalkingMode::Combo;
    }

private:
    void Build() {
        if (!parentMenu_) {
            return;
        }

        menu_ = parentMenu_->AddSubMenu(new Menu("orbwalker", "Orbwalker"));
        if (!menu_) {
            return;
        }

        keysMenu_ = menu_->AddSubMenu(new Menu("keys", "Keys"));
        if (!keysMenu_) {
            return;
        }

        orbwalkKey_ = keysMenu_->Add(new MenuKeyBind(
            "orbwalk",
            "Orbwalk",
            VK_SPACE,
            KeyBindType::Hold));
    }

    Menu* parentMenu_ = nullptr;
    Menu* menu_ = nullptr;
    Menu* keysMenu_ = nullptr;
    MenuKeyBind* orbwalkKey_ = nullptr;
};

} // namespace SDK
