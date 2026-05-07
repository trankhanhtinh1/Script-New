#pragma once

// ============================================================================
// CoreItem.h - Hero inventory access (post-restructure layout)
// ----------------------------------------------------------------------------
// Pointer chain (verified for the NEW inventory system, 26.6+):
//
//   obj + InventoryComponent          = InventoryComponent object (inline)
//   component + SlotArray + slot*8    = slot pointer (SlotCount = 39 slots total)
//     slot + ItemNode                 = item node pointer (null = empty slot)
//       node + ItemInfo               = item info pointer
//         info + DataItemId           = item ID (XOR-encrypted with __rdtsc!)
//
// SLOT LAYOUT:
//   0 .. 5   Regular item slots (visible inventory)
//   6        Trinket / ward slot
//   7 ..     Internal slots (potions, jungle item, etc.)
//
// IMPORTANT:
//   DataItemId is XOR-encrypted per game session. Raw reads return garbage.
//   Use `CompareItemId` / `MatchesEncryptedId` instead of hard-coded constants.
// ============================================================================

#include "Globals.h"
#include "offset.h"

#include <cstdint>

namespace CoreItem {

    constexpr int SLOT_ITEM_START     = 0;
    constexpr int SLOT_ITEM_END       = 5;   // inclusive, 6 item slots (0..5)
    constexpr int SLOT_TRINKET        = 6;
    constexpr int SLOT_INTERNAL_START = 7;

    // ── Pointer chain walkers ──

    inline uintptr_t GetSlotPtr(uintptr_t obj, int slotIndex) {
        if (!Globals::IsValidPtr(obj) ||
            slotIndex < 0 || slotIndex >= Offset::ItemRuntime::SlotCount) {
            return 0;
        }

        const uintptr_t component = obj + Offset::ItemRuntime::InventoryComponent;
        const uintptr_t slotBase  = component + Offset::ItemRuntime::SlotArray;
        const uintptr_t slotAddr  = slotBase + static_cast<uintptr_t>(slotIndex) * 8;

        return Globals::Read<uintptr_t>(slotAddr);
    }

    inline uintptr_t GetItemNode(uintptr_t obj, int slotIndex) {
        const uintptr_t slot = GetSlotPtr(obj, slotIndex);
        if (!Globals::IsValidPtr(slot)) return 0;

        const uintptr_t node = Globals::Read<uintptr_t>(slot + Offset::ItemRuntime::ItemNode);
        return Globals::IsValidPtr(node) ? node : 0;
    }

    inline uintptr_t GetItemInfo(uintptr_t obj, int slotIndex) {
        const uintptr_t node = GetItemNode(obj, slotIndex);
        if (!node) return 0;

        const uintptr_t info = Globals::Read<uintptr_t>(node + Offset::ItemRuntime::ItemInfo);
        return Globals::IsValidPtr(info) ? info : 0;
    }

    inline bool HasItem(uintptr_t obj, int slotIndex) {
        return GetItemNode(obj, slotIndex) != 0;
    }

    // ── Encrypted item ID ──

    inline uint32_t GetItemIdRaw(uintptr_t obj, int slotIndex) {
        const uintptr_t info = GetItemInfo(obj, slotIndex);
        if (!info) return 0;
        return Globals::Read<uint32_t>(info + Offset::ItemRuntime::DataItemId);
    }

    inline bool CompareItemId(uintptr_t objA, int slotA, uintptr_t objB, int slotB) {
        const uint32_t idA = GetItemIdRaw(objA, slotA);
        const uint32_t idB = GetItemIdRaw(objB, slotB);
        return idA != 0 && idA == idB;
    }

    inline bool MatchesEncryptedId(uintptr_t obj, int slotIndex, uint32_t encryptedRef) {
        return encryptedRef != 0 && GetItemIdRaw(obj, slotIndex) == encryptedRef;
    }

    // ── Inventory summary ──

    inline int GetItemCount(uintptr_t obj) {
        int count = 0;
        for (int i = SLOT_ITEM_START; i <= SLOT_ITEM_END; ++i) {
            if (HasItem(obj, i)) ++count;
        }
        return count;
    }

    inline bool HasTrinket(uintptr_t obj) {
        return HasItem(obj, SLOT_TRINKET);
    }

    // ── Item stat readers (per-slot, reads from info ptr) ──

    inline float ReadItemStat(uintptr_t obj, int slotIndex, uintptr_t statOffset) {
        const uintptr_t info = GetItemInfo(obj, slotIndex);
        if (!info) return 0.0f;
        return Globals::Read<float>(info + statOffset);
    }

    inline float GetItemHealth(uintptr_t obj, int slotIndex) {
        return ReadItemStat(obj, slotIndex, Offset::ItemRuntime::DataHealth);
    }

    inline float GetItemArmor(uintptr_t obj, int slotIndex) {
        return ReadItemStat(obj, slotIndex, Offset::ItemRuntime::DataArmor);
    }

    inline float GetItemMR(uintptr_t obj, int slotIndex) {
        return ReadItemStat(obj, slotIndex, Offset::ItemRuntime::DataMR);
    }

    inline float GetItemAD(uintptr_t obj, int slotIndex) {
        return ReadItemStat(obj, slotIndex, Offset::ItemRuntime::DataAD);
    }

    inline float GetItemAP(uintptr_t obj, int slotIndex) {
        return ReadItemStat(obj, slotIndex, Offset::ItemRuntime::DataAP);
    }

    inline float GetItemAbilityHaste(uintptr_t obj, int slotIndex) {
        return ReadItemStat(obj, slotIndex, Offset::ItemRuntime::DataAbilityHaste);
    }

    inline float GetItemAtkSpeedMult(uintptr_t obj, int slotIndex) {
        return ReadItemStat(obj, slotIndex, Offset::ItemRuntime::DataAtkSpeedMult);
    }

    // ── Snapshot ──

    struct ItemSlot {
        int       index   = -1;
        bool      hasItem = false;
        uint32_t  idRaw   = 0;    // encrypted
        uintptr_t infoPtr = 0;

        bool IsEmpty() const { return !hasItem; }
    };

    // Snapshot visible item slots (0 .. 6 inclusive = 7 entries).
    inline int SnapshotItems(uintptr_t obj, ItemSlot* out, int maxOut) {
        if (!out || maxOut <= 0 || !Globals::IsValidPtr(obj)) {
            return 0;
        }

        const int limit = (maxOut < 7) ? maxOut : 7;
        for (int i = 0; i < limit; ++i) {
            ItemSlot& s = out[i];
            s.index   = i;
            s.hasItem = HasItem(obj, i);
            s.idRaw   = s.hasItem ? GetItemIdRaw(obj, i) : 0;
            s.infoPtr = s.hasItem ? GetItemInfo(obj, i) : 0;
        }
        return limit;
    }

} // namespace CoreItem
