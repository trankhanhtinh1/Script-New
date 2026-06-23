#pragma once

#include "SkillshotMissile.h"

namespace SDK {

class TargetedMissile : public SkillshotMissile {
public:
    explicit TargetedMissile(const std::string& spellName)
        : SkillshotMissile(spellName) {
    }

    explicit TargetedMissile(const SpellDatabaseEntry& entry)
        : SkillshotMissile(entry) {
    }

    std::string ToString() const override {
        return "TargetedMissile: Champion=" + SData.ChampionName +
               " SpellName=" + SData.SpellName;
    }
};

} // namespace SDK
