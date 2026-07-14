#pragma once

#include "SpecialSpellCommon.h"

namespace Plugins::KuroEvade::SpecialSpells {

struct Lucian {
    static bool ProcessCast(const CastContext& context, ProcessResult& result) {
        if (!EqualsSpell(context.Source, "LucianQ")) {
            return false;
        }

        result.Data.Runtime.Delay = std::max(250, 400 - (std::max(1, context.Caster.Level()) - 1) * 10);
        result.Data.Runtime.Range = 900;
        return true;
    }
};

} // namespace Plugins::KuroEvade::SpecialSpells
