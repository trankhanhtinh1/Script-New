#pragma once

#include "SpecialSpellCommon.h"

namespace Plugins::KuroEvade::SpecialSpells {

struct Maokai {
    static bool ProcessCast(const CastContext& context, ProcessResult& result) {
        if (!EqualsSpell(context.Source, "MaokaiQ") || !context.Lookup) {
            return false;
        }

        if (const auto* range = context.Lookup("MaokaiQRange")) {
            AddExtra(result, context.Start3, context.Start3, *range);
        }
        return true;
    }
};

} // namespace Plugins::KuroEvade::SpecialSpells
