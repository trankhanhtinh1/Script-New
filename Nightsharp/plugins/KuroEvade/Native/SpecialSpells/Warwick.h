#pragma once

#include "SpecialSpellCommon.h"

namespace Plugins::KuroEvade::SpecialSpells {

struct Warwick {
    static bool ProcessCast(const CastContext& context, ProcessResult& result) {
        if (!EqualsSpell(context.Source, "WarwickR")) {
            return false;
        }

        result.Data.Runtime.Range = static_cast<int>(std::ceil(2.5f * context.Caster.MoveSpeed()));
        return true;
    }
};

} // namespace Plugins::KuroEvade::SpecialSpells
