#pragma once

#include "Drawing.h"
#include "../Core/Game.h"

namespace SDK::Cursor {

    inline Vector3 WorldPosition() {
        return Game::CursorPos();
    }

    inline Vector2 ScreenPosition() {
        POINT pt{};
        if (::GetCursorPos(&pt)) {
            return Vector2(static_cast<float>(pt.x), static_cast<float>(pt.y));
        }
        return {};
    }

    inline bool WorldToScreen(const Vector3& world, Vector2& screen) {
        return Drawing::WorldToScreen(world, screen);
    }

} // namespace SDK::Cursor
