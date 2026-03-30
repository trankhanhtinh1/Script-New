#pragma once

namespace SDK {

enum class GameObjectOrder : int {
    MoveTo = 0,
    AttackUnit = 1,
    AttackMove = 2,
    HoldPosition = 3,
    Stop = 4
};

} // namespace SDK
