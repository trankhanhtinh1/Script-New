#include "SpellDatabase.h"

namespace SpellDatabase
{
    const SpellInfo* GetSpellInfo(const char* spellName)
    {
        if (!spellName) return nullptr;
        return GetSpellInfo(std::string(spellName));
    }

    bool HasSpell(const char* spellName)
    {
        if (!spellName) return false;
        return HasSpell(std::string(spellName));
    }
}
