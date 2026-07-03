#pragma once

#include <cstdint>

namespace SDK {

enum class SpellSlot : std::int32_t {
    Q = 0,
    W = 1,
    E = 2,
    R = 3,
    Summoner1 = 4,
    Summoner2 = 5,
    Item1 = 6,
    Item2 = 7,
    Item3 = 8,
    Item4 = 9,
    Item5 = 10,
    Item6 = 11,
    Trinket = 12,
    Recall = 13,
    BasicAttack = 64,
    Unknown = -1,
};

} // namespace SDK
