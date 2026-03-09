#pragma once
#include "sdk/EzEvade/Spells/Spell.h"
#include <functional>
#include <vector>

namespace EzEvade {
namespace SpellRuntime {

using ActiveSpellListProvider = std::function<std::vector<const Spell*>()>;
inline ActiveSpellListProvider GetActiveSpells = {};

inline std::vector<const Spell*> ActiveSpells() {
    if (GetActiveSpells) {
        return GetActiveSpells();
    }
    return {};
}

} // namespace SpellRuntime
} // namespace EzEvade

