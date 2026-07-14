#pragma once

#include "SpecialSpellCommon.h"

namespace Plugins::KuroEvade::SpecialSpells {

struct Pyke {
    static bool ProcessCast(const CastContext& context, ProcessResult& result) {
        if (!EqualsSpell(context.Source, "PykeR")) {
            return false;
        }

        const float castDistance = ClampedCastDistance(
            context.Start3, context.End3, static_cast<float>(context.Source.Runtime.Range));
        const Vec2 center = context.Start + context.Direction * castDistance;
        const Vec2 axisX(1.0f, 0.0f);
        const Vec2 axisY(0.0f, 1.0f);
        Database::SpellData armData = context.Source;
        armData.Runtime.SpellType = SDK::SpellType::SkillshotLine;
        armData.Runtime.MissileSpeed = INT_MAX;
        armData.Runtime.Range = 710;
        armData.UseEndPosition = true;

        AddExtra(result,
                 From2D(center - axisX * 250.0f + axisY * 250.0f, context.Start3.y),
                 From2D(center + axisX * 250.0f - axisY * 250.0f, context.Start3.y),
                 armData);
        AddExtra(result,
                 From2D(center - axisX * 250.0f - axisY * 250.0f, context.Start3.y),
                 From2D(center + axisX * 250.0f + axisY * 250.0f, context.Start3.y),
                 armData);
        result.NoProcess = true;
        return true;
    }
};

} // namespace Plugins::KuroEvade::SpecialSpells
