#pragma once

#include "../Core/Objects.h"
#include "../GameObjects/InventorySlot.h"
#include "AIBaseClientExtensions.h"
#include "../../Core/CoreItem.h"
#include "../../Core/CoreSpellBook.h"
#include "../../Core/CoreSpellDataInst.h"

namespace SDK {

// ── Item helpers (delegates to CoreItem + Spellbook) ──

inline SDK::SpellSlot GetItemSlot(const AIHeroClient& source, int itemId) {
    if (!source.IsValid() || itemId <= 0) {
        return SDK::SpellSlot::Unknown;
    }
    const auto items = source.InventoryItems();
    for (const auto& slot : items) {
        if (slot.Id() == itemId) {
            return slot.GetSpellSlot();
        }
    }
    return SDK::SpellSlot::Unknown;
}

inline bool CanUseItem(const AIHeroClient& source, int itemId) {
    if (!source.IsValid()) return false;
    const SDK::SpellSlot spellSlot = GetItemSlot(source, itemId);
    if (spellSlot == SDK::SpellSlot::Unknown) return false;

    const auto spell = source.Spellbook().GetSpell(spellSlot);
    if (!spell.IsValid()) return false;
    return spell.State(0.0f) == CoreSpellBook::State_Ready;
}

inline bool UseItem(const AIHeroClient& source, int itemId) {
    if (!source.IsValid()) return false;
    const SDK::SpellSlot spellSlot = GetItemSlot(source, itemId);
    if (spellSlot == SDK::SpellSlot::Unknown) return false;

    return source.Spellbook().CastSpell(spellSlot);
}

inline bool UseItem(const AIHeroClient& source, int itemId, const AIBaseClient& target) {
    if (!source.IsValid() || !target.IsValid()) return false;
    const SDK::SpellSlot spellSlot = GetItemSlot(source, itemId);
    if (spellSlot == SDK::SpellSlot::Unknown) return false;

    return source.Spellbook().CastSpell(spellSlot, target.Address());
}

inline bool UseItem(const AIHeroClient& source, int itemId, Vector3 position) {
    if (!source.IsValid()) return false;
    const SDK::SpellSlot spellSlot = GetItemSlot(source, itemId);
    if (spellSlot == SDK::SpellSlot::Unknown) return false;

    return source.Spellbook().CastSpell(spellSlot, position);
}

} // namespace SDK
