#pragma once

#include "SpecialSpellCommon.h"

namespace Plugins::KuroEvade::SpecialSpells {

struct Ziggs {
    static bool ProcessCast(const CastContext& context, ProcessResult& result) {
        if (EqualsSpell(context.Source, "ZiggsR")) {
            const float range = std::max(1.0f, static_cast<float>(context.Source.sdk.Range));
            const float distance = ClampedCastDistance(context.Start3, context.End3, range);
            result.Data.sdk.Delay = static_cast<int>(1500.0f + 1500.0f * distance / range);
            return true;
        }

        if (!EqualsSpell(context.Source, "ZiggsQ") || !context.Lookup) {
            return false;
        }

        const auto* second = context.Lookup("ZiggsQSpell2");
        const auto* third = context.Lookup("ZiggsQSpell3");
        if (!second || !third) {
            return true;
        }

        Generated::SpellDataEntry spell2 = *second;
        Generated::SpellDataEntry spell3 = *third;
        const float firstDistance = ClampedCastDistance(
            context.Start3, context.End3, static_cast<float>(context.Source.sdk.Range));
        const float secondDistance = firstDistance * 0.4f;
        const float thirdDistance = secondDistance * 0.69f;
        const Vec2 firstEnd = context.Start + context.Direction * firstDistance;
        const Vec2 secondEnd = firstEnd + context.Direction * secondDistance;
        const Vec2 thirdEnd = secondEnd + context.Direction * thirdDistance;

        spell2.sdk.MissileSpeed = static_cast<int>(secondDistance * 1000.0f / 480.0f);
        spell3.sdk.MissileSpeed = static_cast<int>(thirdDistance * 1000.0f / 430.0f);
        spell2.sdk.Delay = static_cast<int>(context.Source.sdk.Delay + firstDistance * 1000.0f /
            std::max(1.0f, static_cast<float>(context.Source.sdk.MissileSpeed)));
        spell3.sdk.Delay = static_cast<int>(spell2.sdk.Delay + secondDistance * 1000.0f /
            std::max(1.0f, static_cast<float>(spell2.sdk.MissileSpeed)));

        AddExtra(result, From2D(firstEnd, context.Start3.y), From2D(secondEnd, context.Start3.y), spell2);
        AddExtra(result, From2D(secondEnd, context.Start3.y), From2D(thirdEnd, context.Start3.y), spell3);
        return true;
    }
};

} // namespace Plugins::KuroEvade::SpecialSpells
