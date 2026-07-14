#pragma once

#include "SpecialSpellCommon.h"

namespace Plugins::KuroEvade::SpecialSpells {

struct Malphite {
    static bool ProcessCast(const CastContext& context, ProcessResult& result) {
        if (!EqualsSpell(context.Source, "UFSlash") &&
            !EqualsSpell(context.Source, "MalphiteR")) {
            return false;
        }

        result.Data.Runtime.MissileSpeed = 1600 + static_cast<int>(context.Caster.MoveSpeed());
        return true;
    }
};

} // namespace Plugins::KuroEvade::SpecialSpells
