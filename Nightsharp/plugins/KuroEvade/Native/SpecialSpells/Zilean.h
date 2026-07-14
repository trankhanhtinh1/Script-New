#pragma once

#include "SpecialSpellCommon.h"

namespace Plugins::KuroEvade::SpecialSpells {

struct Zilean {
    static bool ProcessCast(const CastContext& context, ProcessResult& result) {
        if (!EqualsSpell(context.Source, "ZileanQ")) {
            return false;
        }

        result.Data.Runtime.MissileSpeed = static_cast<int>(
            ClampedCastDistance(context.Start3, context.End3, static_cast<float>(context.Source.Runtime.Range)) / 0.45f);
        return true;
    }
};

} // namespace Plugins::KuroEvade::SpecialSpells
