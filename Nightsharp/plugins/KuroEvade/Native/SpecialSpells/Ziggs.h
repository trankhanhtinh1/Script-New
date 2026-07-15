#pragma once

#include "SpecialSpellCommon.h"

namespace Plugins::KuroEvade::SpecialSpells {

struct Ziggs {
    static bool HasProjectileTerminationDependents(
            const std::string& primary) {
        return _stricmp(primary.c_str(), "ZiggsQ") == 0 ||
            _stricmp(primary.c_str(), "ZiggsQBounce1") == 0;
    }

    static bool IsProjectileTerminationDependent(
            const std::string& primary,
            const std::string& dependent) {
        if (_stricmp(primary.c_str(), "ZiggsQ") == 0) {
            return _stricmp(dependent.c_str(), "ZiggsQBounce1") == 0 ||
                _stricmp(dependent.c_str(), "ZiggsQBounce2") == 0;
        }
        return _stricmp(primary.c_str(), "ZiggsQBounce1") == 0 &&
            _stricmp(dependent.c_str(), "ZiggsQBounce2") == 0;
    }

    static bool ProcessCast(const CastContext& context, ProcessResult& result) {
        if (EqualsSpell(context.Source, "ZiggsR")) {
            const float range = std::max(1.0f, static_cast<float>(context.Source.Runtime.Range));
            const float distance = ClampedCastDistance(context.Start3, context.End3, range);
            result.Data.Runtime.Delay = static_cast<int>(1500.0f + 1500.0f * distance / range);
            return true;
        }

        if (!EqualsSpell(context.Source, "ZiggsQ") || !context.Lookup) {
            return false;
        }

        const auto* second = context.Lookup("ZiggsQBounce1");
        const auto* third = context.Lookup("ZiggsQBounce2");
        if (!second || !third) {
            return true;
        }

        Database::SpellData spell2 = *second;
        Database::SpellData spell3 = *third;
        const float firstDistance = ClampedCastDistance(
            context.Start3, context.End3, static_cast<float>(context.Source.Runtime.Range));
        const float secondDistance = firstDistance * 0.4f;
        const float thirdDistance = secondDistance * 0.69f;
        const Vec2 firstEnd = context.Start + context.Direction * firstDistance;
        const Vec2 secondEnd = firstEnd + context.Direction * secondDistance;
        const Vec2 thirdEnd = secondEnd + context.Direction * thirdDistance;

        constexpr float bounceTravelMs = 480.0f;
        constexpr float landingPauseMs = 500.0f;
        spell2.Range = std::max(1.0f, secondDistance);
        spell3.Range = std::max(1.0f, thirdDistance);
        spell2.MissileSpeed = std::max(1.0f,
            secondDistance * 1000.0f / bounceTravelMs);
        spell3.MissileSpeed = std::max(1.0f,
            thirdDistance * 1000.0f / bounceTravelMs);
        spell2.Delay = static_cast<int>(std::lround(
            static_cast<float>(context.Source.Delay) +
            firstDistance * 1000.0f / std::max(
                1.0f, context.Source.MissileSpeed) + landingPauseMs));
        spell3.Delay = static_cast<int>(std::lround(
            static_cast<float>(spell2.Delay) + bounceTravelMs +
            landingPauseMs));
        spell2.Finalize();
        spell3.Finalize();

        AddExtra(result, From2D(firstEnd, context.Start3.y), From2D(secondEnd, context.Start3.y), spell2);
        AddExtra(result, From2D(secondEnd, context.Start3.y), From2D(thirdEnd, context.Start3.y), spell3);
        return true;
    }
};

} // namespace Plugins::KuroEvade::SpecialSpells
