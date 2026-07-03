#pragma once

#include "BaseSpell.h"

namespace SDK {

class Targeted : public BaseSpell {
public:
    explicit Targeted(const std::string& spellName)
        : BaseSpell(spellName) {
    }

    explicit Targeted(const SpellDatabaseEntry& entry)
        : BaseSpell(entry) {
    }

    std::string ToString() const override {
        return "Targeted: Champion=" + SData.ChampionName +
               " SpellName=" + SData.SpellName;
    }
};

} // namespace SDK
