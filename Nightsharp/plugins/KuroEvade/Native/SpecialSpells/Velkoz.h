#pragma once

#include "SpecialSpellCommon.h"

namespace Plugins::KuroEvade::SpecialSpells {

struct Velkoz {
    static bool ProcessCast(const CastContext& context, ProcessResult& result) {
        if (!EqualsSpell(context.Source, "VelkozE")) {
            return false;
        }

        result.Data.sdk.MissileSpeed = static_cast<int>(std::ceil(
            ClampedCastDistance(context.Start3, context.End3, static_cast<float>(context.Source.sdk.Range)) / 0.55f));
        return true;
    }
};

} // namespace Plugins::KuroEvade::SpecialSpells
