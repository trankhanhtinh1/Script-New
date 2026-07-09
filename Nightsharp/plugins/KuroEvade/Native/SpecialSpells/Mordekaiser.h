#pragma once

#include "SpecialSpellCommon.h"

namespace Plugins::KuroEvade::SpecialSpells {

struct Mordekaiser {
    static bool ProcessCast(const CastContext& context, ProcessResult& result) {
        if (!EqualsSpell(context.Source, "MordekaiserE")) {
            return false;
        }

        const Vec2 casterPos = context.Caster.ServerPosition().To2D();
        const float pullStartDistance = std::min(700.0f, context.End.Distance(casterPos));
        const Vec2 pullOuter = casterPos + (context.End - casterPos).Normalized() * pullStartDistance;
        const Vec2 spellStart = pullOuter + (pullOuter - casterPos).Normalized() * 255.0f;
        const Vec2 spellEnd = spellStart + (casterPos - spellStart).Normalized() * 850.0f;
        AddExtra(result, From2D(spellStart, context.Start3.y), From2D(spellEnd, context.Start3.y), context.Source);
        result.NoProcess = true;
        return true;
    }
};

} // namespace Plugins::KuroEvade::SpecialSpells
