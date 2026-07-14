#pragma once

#include "SpecialSpellCommon.h"

namespace Plugins::KuroEvade::SpecialSpells {

struct Heimerdinger {
    static bool ProcessCast(const CastContext& context, ProcessResult& result) {
        if (!EqualsSpell(context.Source, "HeimerdingerEUlt") || !context.Lookup) {
            return false;
        }

        const auto* ult2 = context.Lookup("HeimerdingerESpell_ult2");
        const auto* ult3 = context.Lookup("HeimerdingerESpell_ult3");
        if (!ult2 || !ult3) {
            return true;
        }

        const float firstDistance = ClampedCastDistance(
            context.Start3, context.End3, static_cast<float>(context.Source.Runtime.Range));
        const Vec2 firstEnd = context.Start + context.Direction * firstDistance;
        const int baseStartTick = SDK::Variables::TickCount() + context.Source.Runtime.Delay +
            static_cast<int>(firstDistance * 1000.0f /
                std::max(1.0f, static_cast<float>(context.Source.Runtime.MissileSpeed)));

        AddExtra(result,
                 From2D(firstEnd, context.Start3.y),
                 From2D(firstEnd + context.Direction * static_cast<float>(context.Source.Runtime.Radius), context.Start3.y),
                 *ult2,
                 baseStartTick);
        AddExtra(result,
                 From2D(firstEnd + context.Direction * static_cast<float>(context.Source.Runtime.Radius), context.Start3.y),
                 From2D(firstEnd + context.Direction *
                     static_cast<float>(context.Source.Runtime.Radius + ult2->Runtime.Radius), context.Start3.y),
                 *ult3,
                 baseStartTick + static_cast<int>(context.Source.Runtime.Radius * 1000.0f /
                     std::max(1.0f, static_cast<float>(ult2->Runtime.MissileSpeed))));
        return true;
    }
};

} // namespace Plugins::KuroEvade::SpecialSpells
