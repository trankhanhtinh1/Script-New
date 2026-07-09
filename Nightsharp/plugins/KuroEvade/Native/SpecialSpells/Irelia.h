#pragma once

#include "SpecialSpellCommon.h"

namespace Plugins::KuroEvade::SpecialSpells {

struct Irelia {
    static bool ProcessCast(const CastContext& context, ProcessResult& result) {
        (void)context;
        (void)result;
        return false;
    }
};

} // namespace Plugins::KuroEvade::SpecialSpells
