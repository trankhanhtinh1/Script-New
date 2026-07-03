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
        if (Game::IsChatOpen() || Game::IsShopOpen()) {
            return OrbwalkingMode::None;
        }
        return IsKeyActive(comboKey_, VK_SPACE)
            ? OrbwalkingMode::Combo
            : OrbwalkingMode::None;
    }

private:
    static bool IsKeyActive(const MenuKeyBind* key, int fallbackKey) {
        const int vk = key ? key->Key : fallbackKey;
        return (key && key->Active) ||
               ((::GetAsyncKeyState(vk) & 0x8000) != 0) ||
               ((::GetAsyncKeyState(fallbackKey) & 0x8000) != 0);
    }

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

        comboKey_ = keysMenu_->Add(new MenuKeyBind("combo", "Combo", VK_SPACE, KeyBindType::Press));
    }

    Menu* parentMenu_ = nullptr;
    Menu* menu_ = nullptr;
    Menu* keysMenu_ = nullptr;
    MenuKeyBind* comboKey_ = nullptr;
};

} // namespace SDK
