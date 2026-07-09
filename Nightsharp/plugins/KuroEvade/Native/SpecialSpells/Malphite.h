#pragma once

#include "SpecialSpellCommon.h"

namespace Plugins::KuroEvade::SpecialSpells {

struct Malphite {
    static bool ProcessCast(const CastContext& context, ProcessResult& result) {
        if (!EqualsSpell(context.Source, "UFSlash")) {
            return false;
        }

        result.Data.sdk.MissileSpeed = 1600 + static_cast<int>(context.Caster.MoveSpeed());
        return true;
    }
};

} // namespace Plugins::KuroEvade::SpecialSpells
