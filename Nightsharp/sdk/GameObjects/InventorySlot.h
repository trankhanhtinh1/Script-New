#pragma once

#include "../Core/Objects.h"
#include "../Data/GameData.h"

#include <algorithm>
#include <vector>

namespace SDK {

class InventorySlot {
public:
    InventorySlot() = default;
    explicit InventorySlot(const ::CoreItem::ItemSlot& slot)
        : slot_(slot) {}

    bool IsValid() const {
        return slot_.hasItem && slot_.id > 0;
    }

    bool IsEmpty() const {
        return !slot_.hasItem;
    }

    int Index() const {
        return slot_.index;
    }

    int SlotIndex() const {
        return slot_.index;
    }

    SDK::SpellSlot SpellSlotId() const {
        const int nativeSlot = ::CoreItem::ToSpellSlotIndex(slot_.index);
        return nativeSlot >= 0
            ? static_cast<SDK::SpellSlot>(nativeSlot)
            : SDK::SpellSlot::Unknown;
    }

    SDK::SpellSlot GetSpellSlot() const {
        return SpellSlotId();
    }

    int Id() const {
        return slot_.id;
    }

    const char* IdText() const {
        return slot_.idText;
    }

    uintptr_t SlotAddress() const {
        return slot_.slotPtr;
    }

    uintptr_t ItemNodeAddress() const {
        return slot_.nodePtr;
    }

    uintptr_t ItemInfoAddress() const {
        return slot_.infoPtr;
    }

    SDK::Data::ItemInfo* ItemData() const {
        return SDK::Data::GameData::GetItemInfoById(slot_.id);
    }

    SDK::Data::ItemInfo* Data() const {
        return ItemData();
    }

    const ::CoreItem::ItemSlot& Native() const {
        return slot_;
    }

private:
    ::CoreItem::ItemSlot slot_ = {};
};

inline std::vector<InventorySlot> AIBaseClient::InventoryItems() const {
    std::vector<InventorySlot> result;
    if (!IsValid()) {
        return result;
    }

    ::CoreItem::ItemSlot slots[::CoreItem::kVisibleSlotCount] = {};
    const int count = ::CoreItem::SnapshotItems(
        Address(),
        slots,
        ::CoreItem::kVisibleSlotCount);

    result.reserve(static_cast<std::size_t>(std::max(0, count)));
    for (int i = 0; i < count; ++i) {
        if (slots[i].hasItem) {
            result.emplace_back(slots[i]);
        }
    }
    return result;
}

} // namespace SDK
