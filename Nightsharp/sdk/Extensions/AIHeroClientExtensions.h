#pragma once

#include "../Core/Objects.h"
#include "../GameObjects/InventorySlot.h"
#include "AIBaseClientExtensions.h"
#include "../../Core/CoreItem.h"
#include "../Enumerations/ItemId.h"
#include "../../Core/CoreSpellBook.h"
#include "../../Core/CoreSpellDataInst.h"

namespace SDK {

// ── Item helpers (delegates to CoreItem + Spellbook) ──

inline ::SDK::SpellSlot GetItemSlot(const AIHeroClient& source, int itemId) {
    if (!source.IsValid() || itemId <= 0) {
        return ::SDK::SpellSlot::Unknown;
    }
    // Chuẩn hóa id giống CoreItem::HasItemId: ARAM/Arena bán bản sao trang bị
    // dưới id = 220000 + gốc, so khớp thô sẽ không tìm ra slot nên CanUseItem/
    // UseItem im lặng thất bại ở các chế độ đó.
    const int wanted = ::CoreItem::NormalizeItemId(itemId);
    const auto items = source.InventoryItems();
    for (const auto& slot : items) {
        if (::CoreItem::NormalizeItemId(slot.Id()) == wanted) {
            return slot.GetSpellSlot();
        }
    }
    return ::SDK::SpellSlot::Unknown;
}

inline bool CanUseItem(const AIHeroClient& source, int itemId) {
    if (!source.IsValid()) return false;
    const ::SDK::SpellSlot spellSlot = GetItemSlot(source, itemId);
    if (spellSlot == ::SDK::SpellSlot::Unknown) return false;

    const auto spell = source.Spellbook().GetSpell(spellSlot);
    if (!spell.IsValid()) return false;
    return spell.State(0.0f) == CoreSpellBook::State_Ready;
}

inline bool UseItem(const AIHeroClient& source, int itemId) {
    if (!source.IsValid()) return false;
    const ::SDK::SpellSlot spellSlot = GetItemSlot(source, itemId);
    if (spellSlot == ::SDK::SpellSlot::Unknown) return false;

    return source.Spellbook().CastSpell(spellSlot);
}

inline bool UseItem(const AIHeroClient& source, int itemId, const AIBaseClient& target) {
    if (!source.IsValid() || !target.IsValid()) return false;
    const ::SDK::SpellSlot spellSlot = GetItemSlot(source, itemId);
    if (spellSlot == ::SDK::SpellSlot::Unknown) return false;

    return source.Spellbook().CastSpell(spellSlot, target.Address());
}

inline bool UseItem(const AIHeroClient& source, int itemId, Vector3 position) {
    if (!source.IsValid()) return false;
    const ::SDK::SpellSlot spellSlot = GetItemSlot(source, itemId);
    if (spellSlot == ::SDK::SpellSlot::Unknown) return false;

    return source.Spellbook().CastSpell(spellSlot, position);
}
inline bool CanUseItem(const AIHeroClient& source, ::SDK::ItemId itemId) {
    return CanUseItem(source, ::SDK::ItemIdValue(itemId));
}

inline bool UseItem(const AIHeroClient& source, ::SDK::ItemId itemId) {
    return UseItem(source, ::SDK::ItemIdValue(itemId));
}

inline bool UseItem(const AIHeroClient& source, ::SDK::ItemId itemId,
                    const AIBaseClient& target) {
    return UseItem(source, ::SDK::ItemIdValue(itemId), target);
}

inline bool UseItem(const AIHeroClient& source, ::SDK::ItemId itemId,
                    Vector3 position) {
    return UseItem(source, ::SDK::ItemIdValue(itemId), position);
}

} // namespace SDK
