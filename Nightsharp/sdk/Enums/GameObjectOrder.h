#pragma once

namespace SDK {

enum class GameObjectOrder : int {
    HoldPosition = 1,
    MoveTo = 2,
    AttackUnit = 3,
    PetAttack = 5,
    PetMove = 6,
    AttackMove = 7,
    PetReturn = 9,
    Stop = 10,
    PetStop = 11
};

} // namespace SDK
