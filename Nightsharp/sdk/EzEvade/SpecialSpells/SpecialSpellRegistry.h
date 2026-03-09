#pragma once

namespace EzEvade {
struct SpellData;

namespace SpecialSpells {

void LoadSpecialSpellPlugins(bool devMode);
void LoadSpecialSpell(SpellData& spellData);

} // namespace SpecialSpells
} // namespace EzEvade

