#pragma once
#include "../Spells/SpellData.h"

namespace EzEvade {
namespace SpecialSpells {

class ChampionPlugin {
public:
    virtual ~ChampionPlugin() = default;
    virtual void LoadSpecialSpell(SpellData& spellData) = 0;
};

} // namespace SpecialSpells
} // namespace EzEvade
