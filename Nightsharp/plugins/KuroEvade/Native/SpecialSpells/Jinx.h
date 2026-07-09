#pragma once

#include "SpecialSpellCommon.h"

namespace Plugins::KuroEvade::SpecialSpells {

struct Jinx {
    static bool ProcessCast(const CastContext& context, ProcessResult& result) {
        if (!EqualsSpell(context.Source, "JinxW")) {
            return false;
        }

        result.Data.sdk.Delay = static_cast<int>(
            std::max(400.0f, 600.0f - context.Caster.AttackSpeedMod() / 2.5f * 200.0f));
        return true;
    }

    static bool ProcessMissile(const SDK::AIBaseClient& caster,
                               const SDK::MissileClient& missile,
                               Generated::SpellDataEntry& data) {
        if (!EqualsText(missile.MissileName(), "JinxWMissile")) {
            return false;
        }

        data.sdk.Delay = static_cast<int>(600.0f - caster.AttackSpeedMod() / 2.5f * 200.0f);
        return true;
    }
};

} // namespace Plugins::KuroEvade::SpecialSpells
