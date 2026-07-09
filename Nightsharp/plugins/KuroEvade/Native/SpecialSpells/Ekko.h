#pragma once

#include "SpecialSpellCommon.h"

namespace Plugins::KuroEvade::SpecialSpells {

struct Ekko {
    static bool ProcessCast(const CastContext& context, ProcessResult& result) {
        if (!EqualsSpell(context.Source, "EkkoR")) {
            return false;
        }

        for (const auto& minion : SDK::ObjectManager::Get<SDK::AIMinionClient>()) {
            if (!minion.IsValid() || minion.IsDead() || !minion.IsEnemy()) {
                continue;
            }

            const std::string name = ToLower(minion.Name());
            const std::string characterName = ToLower(minion.CharacterName());
            if (name == "ekko" || characterName == "ekko") {
                AddExtra(result, context.Start3, minion.Position(), context.Source);
            }
        }

        result.NoProcess = true;
        return true;
    }
};

} // namespace Plugins::KuroEvade::SpecialSpells
