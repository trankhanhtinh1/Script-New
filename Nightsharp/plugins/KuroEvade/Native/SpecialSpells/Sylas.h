#pragma once

#include "SpecialSpellCommon.h"

namespace Plugins::KuroEvade::SpecialSpells {

struct Sylas {
    static bool ProcessCast(const CastContext& context, ProcessResult& result) {
        if (!EqualsSpell(context.Source, "SylasQ")) {
            return false;
        }

        const float distance = std::min(
            static_cast<float>(context.Source.sdk.Range),
            std::max(context.Start.Distance(context.End), 175.0f));
        const Vec2 center = context.Start + context.Direction * distance;
        const Vec2 perpendicular = Perpendicular((center - context.Start).Normalized());
        const Vec2 leftStart = context.Start + perpendicular * 100.0f;
        const Vec2 leftEnd = leftStart + (center - leftStart).Normalized() *
            static_cast<float>(context.Source.sdk.Range);
        const Vec2 rightStart = context.Start - perpendicular * 100.0f;
        const Vec2 rightEnd = rightStart + (center - rightStart).Normalized() *
            static_cast<float>(context.Source.sdk.Range);

        AddExtra(result, From2D(leftStart, context.Start3.y), From2D(leftEnd, context.Start3.y), context.Source);
        AddExtra(result, From2D(rightStart, context.Start3.y), From2D(rightEnd, context.Start3.y), context.Source);
        result.NoProcess = true;
        return true;
    }
};

} // namespace Plugins::KuroEvade::SpecialSpells
