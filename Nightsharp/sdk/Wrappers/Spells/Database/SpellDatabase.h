#pragma once

#include "../../../Data/Database.h"

#include <cstring>
#include <string>
#include <vector>

namespace SDK::SpellDatabase {

    using Entry = Data::SpellData;

    inline std::vector<Data::SpellData>& Entries() {
        return Data::GetSpellDatabase();
    }

    inline const Data::SpellData* FindBySpellName(const std::string& championName, const std::string& spellName) {
        for (const auto& entry : Entries()) {
            if (!championName.empty() && _stricmp(entry.charName.c_str(), championName.c_str()) != 0) {
                continue;
            }
            if (_stricmp(entry.spellName.c_str(), spellName.c_str()) == 0) {
                return &entry;
            }
        }
        return nullptr;
    }

} // namespace SDK::SpellDatabase
