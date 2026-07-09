#pragma once

#include "KuroEvadeDatabase.generated.h"

namespace Plugins::KuroEvade::SpellDatabase {

inline const std::vector<Generated::SpellDataEntry>& Spells() {
    return Generated::SpellData();
}

} // namespace Plugins::KuroEvade::SpellDatabase

