#pragma once

#include "SpellData.h"

namespace Plugins::KuroEvade {

struct SpecialSpellEventArgs {
    bool NoProcess = false;
    bool HasSpellDataOverride = false;
    SpellDataEntry SpellData;
};

} // namespace Plugins::KuroEvade

