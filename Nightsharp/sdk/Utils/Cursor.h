#pragma once

#include "../UI/Cursor.h"

namespace SDK::Utils::Cursor {

inline Vector3 WorldPosition() {
    return SDK::Cursor::WorldPosition();
}

inline Vector2 Position() {
    return SDK::Cursor::ScreenPosition();
}

inline Vector2 ScreenPosition() {
    return SDK::Cursor::ScreenPosition();
}

} // namespace SDK::Utils::Cursor

