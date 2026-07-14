#pragma once

#include "SpecialSpellCommon.h"

namespace Plugins::KuroEvade::SpecialSpells {

struct Nami {
    static bool ProcessCast(const CastContext& context, ProcessResult& result) {
        if (!EqualsSpell(context.Source, "NamiQ")) {
            return false;
        }

        result.Data.Runtime.MissileSpeed = static_cast<int>(
            ClampedCastDistance(context.Start3, context.End3, static_cast<float>(context.Source.Runtime.Range)) / 0.7f);
        return true;
    }

    static bool ProcessMissile(const SDK::AIBaseClient& caster,
                               const SDK::MissileClient& missile,
                               Database::SpellData& data) {
        (void)caster;
        if (!EqualsText(missile.MissileName(), "NamiQMissile")) {
            return false;
        }

        data.Runtime.MissileSpeed = static_cast<int>(
            std::min(static_cast<float>(data.Runtime.Range),
                     missile.StartPosition().Distance(missile.EndPosition())) / 0.7f);
        return true;
    }
};

} // namespace Plugins::KuroEvade::SpecialSpells
