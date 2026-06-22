#pragma once

#include "Game.h"

namespace SDK::Variables {

inline int TickCount() {
    return SDK::Game::TickCount();
}

} // namespace SDK::Variables

namespace SDK::Core::Variables {
    inline int TickCount() { return ::SDK::Variables::TickCount(); }
} // namespace SDK::Core::Variables
