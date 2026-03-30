#pragma once

#include "../Database/SpellDatabase.h"
#include "../../../Core/Objects.h"

#include <string>

namespace SDK::SpellTracker::Detector {

inline const Data::SpellData* ResolveSpell(const AIBaseClient& caster, const std::string& spellName) {
    return SpellDatabase::FindBySpellName(caster.CharacterName(), spellName);
}

inline const Data::SpellData* ResolveMissile(const MissileClient& missile) {
    if (!missile.IsValid()) {
        return nullptr;
    }
    return SpellDatabase::FindBySpellName("", missile.Name());
}

inline bool IsSkillshot(const AIBaseClient& caster, const std::string& spellName) {
    const auto* entry = ResolveSpell(caster, spellName);
    return entry && entry->IsSkillshot();
}

inline bool IsTargeted(const AIBaseClient& caster, const std::string& spellName) {
    const auto* entry = ResolveSpell(caster, spellName);
    return entry && !entry->IsSkillshot();
}

inline float ResolveRadius(const AIBaseClient& caster, const std::string& spellName, float fallback = 0.0f) {
    const auto* entry = ResolveSpell(caster, spellName);
    return entry ? entry->radius : fallback;
}

inline float ResolveRange(const AIBaseClient& caster, const std::string& spellName, float fallback = 0.0f) {
    const auto* entry = ResolveSpell(caster, spellName);
    return entry ? entry->range : fallback;
}

} // namespace SDK::SpellTracker::Detector
