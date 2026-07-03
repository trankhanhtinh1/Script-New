#pragma once

#include <cstdint>

namespace SDK {

enum class CastType : std::int32_t {
    EnemyChampions,
    EnemyMinions,
    EnemyTurrets,
    AllyChampions,
    AllyMinions,
    AllyTurrets,
    HeroPets,
    Position,
    Direction,
    Self,
    Charging,
    Toggle,
    Channel,
    Activate,
    ImpossibleToCast,
};

} // namespace SDK
