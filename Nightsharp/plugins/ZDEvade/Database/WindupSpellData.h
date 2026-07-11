#pragma once
#include <string>

namespace ZDEvade {

enum class WindupSpellSlot {
    Q = 0,
    W = 1,
    E = 2,
    R = 3,
};

struct WindupSpellData {
    std::string charName;
    std::string name;
    float spellDelay = 250.0f;
    WindupSpellSlot spellKey = WindupSpellSlot::Q;
    std::string spellName;

    WindupSpellData() = default;
};

} // namespace ZDEvade
