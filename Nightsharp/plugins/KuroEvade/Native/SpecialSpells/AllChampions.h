#pragma once

#include "SpecialSpellCommon.h"

namespace Plugins::KuroEvade::SpecialSpells {

struct AllChampions {
    static bool ProcessCast(const CastContext& context, ProcessResult& result) {
        (void)context;
        (void)result;
        return false;
    }

    static bool ProcessMissile(const SDK::AIBaseClient& caster,
                               const SDK::MissileClient& missile,
                               Database::SpellData& data) {
        (void)caster;
        const std::string missileName = missile.MissileName();
        if (!NameContains(missileName.c_str(), "HowlingGaleSpell")) {
            return false;
        }

        const float distance = missile.StartPosition().Distance(missile.EndPosition());
        data.Runtime.MissileSpeed = static_cast<int>(std::max(0.0f, std::min(1166.0f, distance / 1.5f)));
        return true;
    }
};

} // namespace Plugins::KuroEvade::SpecialSpells
