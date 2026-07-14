#pragma once

#include "SpecialSpellCommon.h"

namespace Plugins::KuroEvade::SpecialSpells {

struct Zac {
    static bool ProcessCast(const CastContext& context, ProcessResult& result) {
        if (!EqualsSpell(context.Source, "ZacE")) {
            return false;
        }

        static constexpr int ranges[] = { 1200, 1350, 1500, 1650, 1800 };
        const int level = std::clamp(context.Caster.GetSpell(SDK::SpellSlot::E).Level(), 1, 5);
        result.Data.Runtime.Range = ranges[level - 1];
        return true;
    }
};

} // namespace Plugins::KuroEvade::SpecialSpells
