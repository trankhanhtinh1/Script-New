#pragma once

#include "SpecialSpellCommon.h"

namespace Plugins::KuroEvade::SpecialSpells {

struct Zed {
    static bool ProcessCast(const CastContext& context, ProcessResult& result) {
        if (!EqualsSpell(context.Source, "ZedQ")) {
            return false;
        }

        for (const auto& emitter : SDK::ObjectManager::Get<SDK::EffectEmitter>()) {
            if (!emitter.IsValid() || !emitter.IsEnemy()) {
                continue;
            }

            const std::string name = ToLower(emitter.Name());
            if (name.find("zed_") != std::string::npos &&
                (name.find("_w_cloneswap_buf") != std::string::npos ||
                 name.find("_r_cloneswap_buf") != std::string::npos)) {
                AddExtra(result, emitter.Position(), context.End3, context.Source);
            }
        }

        return true;
    }
};

} // namespace Plugins::KuroEvade::SpecialSpells
