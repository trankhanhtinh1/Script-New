#pragma once

#include "SpecialSpellCommon.h"

namespace Plugins::KuroEvade::SpecialSpells {

struct Taric {
    static bool ProcessCast(const CastContext& context, ProcessResult& result) {
        if (!EqualsSpell(context.Source, "TaricE")) {
            return false;
        }

        for (const auto& hero : SDK::GameObjects::Heroes()) {
            if (!hero.IsValid() || hero.IsDead()) {
                continue;
            }

            const uintptr_t buffCaster = hero.GetBuffCaster("taricwleashactive");
            if (!buffCaster) {
                continue;
            }

            const SDK::GameObject caster(buffCaster);
            if (caster.IsValid() && caster.NetworkId() == context.Caster.NetworkId()) {
                AddExtra(result, hero.Position(), context.End3, context.Source);
            }
        }

        return true;
    }
};

} // namespace Plugins::KuroEvade::SpecialSpells
