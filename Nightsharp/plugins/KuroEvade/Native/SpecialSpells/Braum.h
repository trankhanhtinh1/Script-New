#pragma once

#include "SpecialSpellCommon.h"

namespace Plugins::KuroEvade::SpecialSpells {

struct Braum {
    static bool ProcessCast(const CastContext& context, ProcessResult& result) {
        if (!EqualsSpell(context.Source, "BraumRWrapper") || !context.Lookup) {
            return false;
        }

        if (const auto* range = context.Lookup("BraumRRange")) {
            AddExtra(result, context.Start3, context.Start3, *range);
        }
        return true;
    }
};

} // namespace Plugins::KuroEvade::SpecialSpells
