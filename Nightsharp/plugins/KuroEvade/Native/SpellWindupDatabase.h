#pragma once

#include "SpellWindupDatabase.generated.h"

#include <cstring>
#include <vector>

namespace Plugins::KuroEvade {

struct SpellWindupDatabase final {
    using Entry = Generated::SpellWindupEntry;

    static const std::vector<Entry>& Spells() {
        return Generated::SpellWindupData();
    }

    static const Entry* Find(const char* championName,
                             const char* spellName,
                             SDK::SpellSlot slot) {
        for (const auto& entry : Spells()) {
            if (championName && championName[0] &&
                _stricmp(entry.ChampionName.c_str(), championName) != 0) {
                continue;
            }
            if (slot != SDK::SpellSlot::Unknown && entry.Slot != slot) {
                continue;
            }
            if (spellName && spellName[0] && !entry.SpellName.empty() &&
                _stricmp(entry.SpellName.c_str(), spellName) != 0) {
                continue;
            }
            return &entry;
        }
        return nullptr;
    }
};

} // namespace Plugins::KuroEvade
