#pragma once

#include "SpecialSpellCommon.h"

namespace Plugins::KuroEvade::SpecialSpells {

struct Draven {
    static bool ProcessCast(const CastContext& context, ProcessResult& result) {
        if (!EqualsSpell(context.Source, "DravenRDoublecast")) {
            return false;
        }

        result.RemoveSpellNames.push_back("DravenRCast");
        result.NoProcess = true;
        return true;
    }
};

} // namespace Plugins::KuroEvade::SpecialSpells
