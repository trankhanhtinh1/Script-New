#pragma once

#include "EvadeSpellDatabase.generated.h"

#include <cstring>
#include <vector>

namespace Plugins::KuroEvade {

struct EvadeSpellDatabase final {
    static const std::vector<EvadeSpellData>& Spells() {
        return Generated::EvadeSpellDataList();
    }

    static std::vector<const EvadeSpellData*> ForChampion(const char* championName,
                                                          bool includeGlobal = true) {
        std::vector<const EvadeSpellData*> result;
        for (const auto& spell : Spells()) {
            const bool isGlobal = _stricmp(spell.ChampionName.c_str(), "AllChampions") == 0;
            const bool isChampion = championName && championName[0] &&
                _stricmp(spell.ChampionName.c_str(), championName) == 0;
            if ((includeGlobal && isGlobal) || isChampion) {
                result.push_back(&spell);
            }
        }
        return result;
    }
};

} // namespace Plugins::KuroEvade
