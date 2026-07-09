#pragma once

#include "SpecialSpellCommon.h"

namespace Plugins::KuroEvade::SpecialSpells {

struct Nami {
    static bool ProcessCast(const CastContext& context, ProcessResult& result) {
        if (!EqualsSpell(context.Source, "NamiQ")) {
            return false;
        }

        result.Data.sdk.MissileSpeed = static_cast<int>(
            ClampedCastDistance(context.Start3, context.End3, static_cast<float>(context.Source.sdk.Range)) / 0.7f);
        return true;
    }

    static bool ProcessMissile(const SDK::AIBaseClient& caster,
                               const SDK::MissileClient& missile,
                               Generated::SpellDataEntry& data) {
        (void)caster;
        if (!EqualsText(missile.MissileName(), "NamiQMissile")) {
            return false;
        }

        data.sdk.MissileSpeed = static_cast<int>(
            std::min(static_cast<float>(data.sdk.Range),
                     missile.StartPosition().Distance(missile.EndPosition())) / 0.7f);
        return true;
    }
};

} // namespace Plugins::KuroEvade::SpecialSpells
