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

    const SDK::Data::ItemDatabase::ItemDataEntry* DatabaseEntry() const {
        return SDK::Data::GameData::GetItemDataById(slot_.id);
    }

    const SDK::Data::ItemDatabase::ItemDataEntry* CDragonData() const {
        return DatabaseEntry();
    }

    bool IsActiveItem() const {
        const auto* entry = DatabaseEntry();
        return entry && entry->Active;
    }

    bool HasCooldownData() const {
        const auto* entry = DatabaseEntry();
        return entry && entry->HasCooldown();
    }

    float CooldownMin() const {
        const auto* entry = DatabaseEntry();
        return entry ? entry->CooldownMin : 0.0f;
    }

    float CooldownMax() const {
        const auto* entry = DatabaseEntry();
        return entry ? entry->CooldownMax : 0.0f;
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

inline float AIBaseClient::BonusHealth() const {
    if (!IsValid()) {
        return 0.0f;
    }

    float bonusHealth = 0.0f;
    for (const auto& slot : InventoryItems()) {
        if (const auto* entry = slot.CDragonData()) {
            bonusHealth += std::max(entry->Stats.health, 0.0f);
        } else if (const auto* info = slot.ItemData()) {
            bonusHealth += std::max(info->health, 0.0f);
        }
    }

    if (IsHero()) {
        const AIHeroClient hero(Address());
        const int level = std::clamp(hero.Level(), 1, 18);
        const auto runeManager = hero.RuneManager();

        // Stat shard: +65 Health.
        if (runeManager.HasRune(5011)) {
            bonusHealth += 65.0f;
        }

        // Stat shard: +10 to +180 Health based on level.
        if (runeManager.HasRune(5001)) {
            bonusHealth += 10.0f + (170.0f / 17.0f) * static_cast<float>(level - 1);
        }
    }

    return std::max(bonusHealth, 0.0f);
}

} // namespace SDK
