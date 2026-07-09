#pragma once

#include "KuroEvadeDatabase.generated.h"

namespace Plugins::KuroEvade {

struct SpecialSpellEventArgs {
    bool NoProcess = false;
    bool HasSpellDataOverride = false;
    Generated::SpellDataEntry SpellData;
};

} // namespace Plugins::KuroEvade

