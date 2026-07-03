#pragma once

// ============================================================================
// ItemInfo.h - Static item stat block loaded from ItemData.json
// ----------------------------------------------------------------------------
// Direct port of LView/ItemInfo.h. All fields preserved 1:1 so existing
// ItemData.json files keep parsing without changes.
// ============================================================================

namespace SDK::Data {

struct ItemInfo {
    int   id                   = 0;
    float cost                 = 0.0f;
    float movementSpeed        = 0.0f;
    float health               = 0.0f;
    float crit                 = 0.0f;
    float abilityPower         = 0.0f;
    float mana                 = 0.0f;
    float armour               = 0.0f;
    float magicResist          = 0.0f;
    float physicalDamage       = 0.0f;
    float attackSpeed          = 0.0f;
    float lifeSteal            = 0.0f;
    float hpRegen              = 0.0f;
    float movementSpeedPercent = 0.0f;
};

} // namespace SDK::Data
