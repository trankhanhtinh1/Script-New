#pragma once

#include "Globals.h"
#include "Offsets.h"
#include <cstdint>

// ============================================================================
// CoreItem — New Inventory System (post-restructure)
// ============================================================================
//
// POINTER CHAIN:
//   obj + 0x4DB8                          = InventoryComponent (inline, NOT ptr deref)
//     component + 0x50 + slot_index * 8   = slot pointer (39 slots total)
//       slot + 0x10                       = item node pointer (null = empty slot)
//         item_node + 0x00               = item info pointer
//           info + 0xB4                  = item ID (XOR ENCRYPTED with __rdtsc!)
//
// SLOT LAYOUT (39 slots, index 0-38):
//   Slots 0-5:  Regular item slots (visible in inventory)
//   Slot 6:     Trinket/Ward slot
//   Slots 7+:   Internal slots (potions, jungle items, etc.)
//
// WARNING:
//   DataItemId at info+0xB4 is XOR encrypted. Raw reads give garbage.
//   Within one game session, same item type = same encrypted value.
//   Use CompareItemId() for matching, or build a lookup table.
// ============================================================================

namespace CoreItem {

    // Slot index constants
    constexpr int SLOT_ITEM_START     = 0;
    constexpr int SLOT_ITEM_END       = 5;   // 6 regular item slots (0-5)
    constexpr int SLOT_TRINKET        = 6;
    constexpr int SLOT_INTERNAL_START = 7;

    // ── Slot pointer access ──
    // Reads the raw slot pointer from the flat array at component+0x50
    inline uintptr_t GetSlotPtr(uintptr_t obj, int slotIndex) {
        if (!Globals::IsValidPtr(obj) ||
            slotIndex < 0 || slotIndex >= Offset::ItemSystem::SlotCount) {
            return 0;
        }

        const uintptr_t component = obj + Offset::ItemSystem::InventoryComponent;
        const uintptr_t slotBase  = component + Offset::ItemSystem::SlotArray;
        const uintptr_t slotAddr  = slotBase + static_cast<uintptr_t>(slotIndex) * 8;

        return Globals::Read<uintptr_t>(slotAddr);
    }

    // ── Item node access ──
    // Returns item node pointer from slot (null = empty slot)
    inline uintptr_t GetItemNode(uintptr_t obj, int slotIndex) {
        const uintptr_t slot = GetSlotPtr(obj, slotIndex);
        if (!Globals::IsValidPtr(slot)) {
            return 0;
        }

        const uintptr_t node = Globals::Read<uintptr_t>(slot + Offset::ItemSystem::ItemNode);
        return Globals::IsValidPtr(node) ? node : 0;
    }

    // ── Item info access ──
    // Returns item info pointer from item node
    inline uintptr_t GetItemInfo(uintptr_t obj, int slotIndex) {
        const uintptr_t node = GetItemNode(obj, slotIndex);
        if (!node) {
            return 0;
        }

        const uintptr_t info = Globals::Read<uintptr_t>(node + Offset::ItemSystem::ItemInfo);
        return Globals::IsValidPtr(info) ? info : 0;
    }

    // ── Has item in slot ──
    inline bool HasItem(uintptr_t obj, int slotIndex) {
        return GetItemNode(obj, slotIndex) != 0;
    }

    // ── Raw item ID (XOR encrypted!) ──
    // Returns the encrypted item ID. NOT the real Riot item ID.
    // Same item type within one game session = same encrypted value.
    inline uint32_t GetItemIdRaw(uintptr_t obj, int slotIndex) {
        const uintptr_t info = GetItemInfo(obj, slotIndex);
        if (!info) {
            return 0;
        }
        return Globals::Read<uint32_t>(info + Offset::ItemSystem::DataItemId);
    }

    // ── Compare encrypted IDs ──
    // Since all items in one session share the same XOR key,
    // equal encrypted values = equal item types.
    inline bool CompareItemId(uintptr_t objA, int slotA, uintptr_t objB, int slotB) {
        const uint32_t idA = GetItemIdRaw(objA, slotA);
        const uint32_t idB = GetItemIdRaw(objB, slotB);
        return idA != 0 && idA == idB;
    }

    // ── Match against known encrypted reference ──
    inline bool MatchesEncryptedId(uintptr_t obj, int slotIndex, uint32_t encryptedRef) {
        return encryptedRef != 0 && GetItemIdRaw(obj, slotIndex) == encryptedRef;
    }

    // ── Count equipped items (slots 0-5) ──
    inline int GetItemCount(uintptr_t obj) {
        int count = 0;
        for (int i = SLOT_ITEM_START; i <= SLOT_ITEM_END; ++i) {
            if (HasItem(obj, i)) {
                ++count;
            }
        }
        return count;
    }

    // ── Has trinket ──
    inline bool HasTrinket(uintptr_t obj) {
        return HasItem(obj, SLOT_TRINKET);
    }

    // ── Item stat readers (from info pointer — offsets need re-verification!) ──
    inline float ReadItemStat(uintptr_t obj, int slotIndex, uintptr_t statOffset) {
        const uintptr_t info = GetItemInfo(obj, slotIndex);
        if (!info) {
            return 0.0f;
        }
        return Globals::Read<float>(info + statOffset);
    }

    inline float GetItemHealth(uintptr_t obj, int slotIndex) {
        return ReadItemStat(obj, slotIndex, Offset::ItemSystem::DataHealth);
    }

    inline float GetItemArmor(uintptr_t obj, int slotIndex) {
        return ReadItemStat(obj, slotIndex, Offset::ItemSystem::DataArmor);
    }

    inline float GetItemMR(uintptr_t obj, int slotIndex) {
        return ReadItemStat(obj, slotIndex, Offset::ItemSystem::DataMR);
    }

    inline float GetItemAD(uintptr_t obj, int slotIndex) {
        return ReadItemStat(obj, slotIndex, Offset::ItemSystem::DataAD);
    }

    inline float GetItemAP(uintptr_t obj, int slotIndex) {
        return ReadItemStat(obj, slotIndex, Offset::ItemSystem::DataAP);
    }

    inline float GetItemAbilityHaste(uintptr_t obj, int slotIndex) {
        return ReadItemStat(obj, slotIndex, Offset::ItemSystem::DataAbilityHaste);
    }

    inline float GetItemAtkSpeedMult(uintptr_t obj, int slotIndex) {
        return ReadItemStat(obj, slotIndex, Offset::ItemSystem::DataAtkSpeedMult);
    }

    // ── Struct for snapshot of one slot ──
    struct ItemSlot {
        int      index    = -1;
        bool     hasItem  = false;
        uint32_t idRaw    = 0;      // encrypted
        uintptr_t infoPtr = 0;

        bool IsEmpty() const { return !hasItem; }
    };

    // ── Snapshot all visible items (slots 0-6) ──
    inline int SnapshotItems(uintptr_t obj, ItemSlot* out, int maxOut) {
        if (!out || maxOut <= 0 || !Globals::IsValidPtr(obj)) {
            return 0;
        }

        const int limit = (maxOut < 7) ? maxOut : 7; // slots 0-6
        int written = 0;

        for (int i = 0; i < limit; ++i) {
            ItemSlot& s = out[written];
            s.index   = i;
            s.hasItem = HasItem(obj, i);
            s.idRaw   = s.hasItem ? GetItemIdRaw(obj, i) : 0;
            s.infoPtr = s.hasItem ? GetItemInfo(obj, i) : 0;
            ++written;
        }

        return written;
    }

} // namespace CoreItem
