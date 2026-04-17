#pragma once

#include "Globals.h"
#include "Offsets.h"
#include <cstdint>

// ============================================================================
// CoreItem — Inventory system (post-restructure)
// ============================================================================
//
// POINTER CHAIN (verified via MCP IDA on the current build):
//   obj  + InventoryComponent (=0x4DB8)          = InventoryComponent (inline, NOT deref)
//     component + SlotArray (=0x50) + idx * 8    = slot pointer (SlotCount=39)
//       slot     + ItemNode  (=0x10)             = item node pointer (null = empty)
//         node   + ItemInfo  (=0x38)             = item info pointer
//           info + DataItemId(=0xB4)             = internal item id (uint32)
//
// Reference disassembly:
//   sub_5843D0:  mov rax,[rcx+10h]; mov rax,[rax+38h]; mov eax,[rax+0B4h]; ret
//   sub_26B9B0 (dtor): v12 = comp+0x50; v13 = 39; do { if (*v12) ... } while (--v13)
//
// NOTE on DataItemId encoding:
//   The value at info+0xB4 is a stable INTERNAL item id used by game code for
//   equality compares (e.g. `cmp eax, 0x801` in sub_2BE920). It is not the
//   Riot-facing item id (e.g. 3031 for Infinity Edge). You cannot convert
//   internal -> Riot without a learned catalog, because the mapping is
//   populated from runtime data (bin files loaded by the client).
//
// Public helpers:
//   - GetSlotPtr/GetItemNode/GetItemInfo   raw chain accessors
//   - GetItemIdRaw                         reads info+0xB4 (internal id)
//   - HasItemInSlot                        slot has any item (node != null)
//   - GetItemCount                         number of filled item slots (0..5)
//   - HasTrinket                           slot 6 filled
//   - MatchesInternalId                    compare internal id at slot
//   - SnapshotItems                        dump visible slots (0..6)
// ============================================================================

namespace CoreItem {

    constexpr int SLOT_ITEM_START     = Offset::ItemSystem::SlotItemBegin;
    constexpr int SLOT_ITEM_END       = Offset::ItemSystem::SlotItemEnd;
    constexpr int SLOT_TRINKET        = Offset::ItemSystem::SlotTrinket;
    constexpr int SLOT_INTERNAL_START = Offset::ItemSystem::SlotTrinket + 1;
    constexpr int SLOT_VISIBLE_COUNT  = Offset::ItemSystem::SlotVisibleCount;

    // ── Raw slot pointer (component+0x50 + index*8) ──
    inline uintptr_t GetSlotPtr(uintptr_t obj, int slotIndex) {
        if (!Globals::IsValidPtr(obj) ||
            slotIndex < 0 ||
            slotIndex >= Offset::ItemSystem::SlotCount) {
            return 0;
        }

        const uintptr_t component = obj + Offset::ItemSystem::InventoryComponent;
        const uintptr_t slotBase  = component + Offset::ItemSystem::SlotArray;
        const uintptr_t slotAddr  = slotBase + static_cast<uintptr_t>(slotIndex) * 8;

        return Globals::Read<uintptr_t>(slotAddr);
    }

    // ── Item node pointer (slot+0x10) ──
    inline uintptr_t GetItemNode(uintptr_t obj, int slotIndex) {
        const uintptr_t slot = GetSlotPtr(obj, slotIndex);
        if (!Globals::IsValidPtr(slot)) {
            return 0;
        }

        const uintptr_t node = Globals::Read<uintptr_t>(slot + Offset::ItemSystem::ItemNode);
        return Globals::IsValidPtr(node) ? node : 0;
    }

    // ── Item info pointer (node+0x38) ──
    //
    // Previous NightSharp port had this at 0x00, which read the node vtable
    // instead of the info pointer and produced garbage at info+0xB4. IDA
    // disassembly of sub_5843D0 and sub_584380 both confirm the +0x38 field.
    inline uintptr_t GetItemInfo(uintptr_t obj, int slotIndex) {
        const uintptr_t node = GetItemNode(obj, slotIndex);
        if (!node) {
            return 0;
        }

        const uintptr_t info = Globals::Read<uintptr_t>(node + Offset::ItemSystem::ItemInfo);
        return Globals::IsValidPtr(info) ? info : 0;
    }

    // ── Slot has an item? (node != null) ──
    inline bool HasItemInSlot(uintptr_t obj, int slotIndex) {
        return GetItemNode(obj, slotIndex) != 0;
    }

    // ── Read the internal item id stored at info+0xB4. ──
    //
    // This is a uint32 hash stable within one patch/build but not equal to
    // the Riot-facing item id. Use it for equality compares against a
    // learned reference value (see MatchesInternalId).
    inline uint32_t GetItemIdRaw(uintptr_t obj, int slotIndex) {
        const uintptr_t info = GetItemInfo(obj, slotIndex);
        if (!info) {
            return 0;
        }
        return Globals::Read<uint32_t>(info + Offset::ItemSystem::DataItemId);
    }

    // ── Compare item internal id across two slots (same-session equivalence). ──
    inline bool SameItem(uintptr_t objA, int slotA, uintptr_t objB, int slotB) {
        const uint32_t a = GetItemIdRaw(objA, slotA);
        const uint32_t b = GetItemIdRaw(objB, slotB);
        return a != 0 && a == b;
    }

    // ── Compare against a known reference internal id. ──
    //
    // The reference id must be captured at runtime (either by walking a known
    // unit's inventory when it holds the target item, or by a learned catalog
    // that maps RiotId -> internalId).
    inline bool MatchesInternalId(uintptr_t obj, int slotIndex, uint32_t internalRef) {
        return internalRef != 0 && GetItemIdRaw(obj, slotIndex) == internalRef;
    }

    // ── Check whether any visible slot (0..6) carries the given internal id. ──
    inline bool HasInternalId(uintptr_t obj, uint32_t internalRef) {
        if (internalRef == 0 || !Globals::IsValidPtr(obj)) {
            return false;
        }
        for (int i = 0; i < SLOT_VISIBLE_COUNT; ++i) {
            if (GetItemIdRaw(obj, i) == internalRef) {
                return true;
            }
        }
        return false;
    }

    // ── Count filled regular item slots (0..5). ──
    inline int GetItemCount(uintptr_t obj) {
        int count = 0;
        for (int i = SLOT_ITEM_START; i <= SLOT_ITEM_END; ++i) {
            if (HasItemInSlot(obj, i)) {
                ++count;
            }
        }
        return count;
    }

    inline bool HasTrinket(uintptr_t obj) {
        return HasItemInSlot(obj, SLOT_TRINKET);
    }

    // ── Stat readers (info+offset); offsets still need CE re-verification ──
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

    // ── Snapshot of one slot (0..6). ──
    struct ItemSlot {
        int       index   = -1;
        bool      hasItem = false;
        uint32_t  idRaw   = 0;       // internal id @ info+0xB4
        uintptr_t infoPtr = 0;

        bool IsEmpty() const { return !hasItem; }
    };

    // ── Dump visible slots (0..6) into out[]. Returns number of entries written. ──
    inline int SnapshotItems(uintptr_t obj, ItemSlot* out, int maxOut) {
        if (!out || maxOut <= 0 || !Globals::IsValidPtr(obj)) {
            return 0;
        }

        const int limit = (maxOut < SLOT_VISIBLE_COUNT) ? maxOut : SLOT_VISIBLE_COUNT;
        int written = 0;

        for (int i = 0; i < limit; ++i) {
            ItemSlot& s = out[written];
            s.index   = i;
            s.hasItem = HasItemInSlot(obj, i);
            s.idRaw   = s.hasItem ? GetItemIdRaw(obj, i) : 0;
            s.infoPtr = s.hasItem ? GetItemInfo(obj, i) : 0;
            ++written;
        }

        return written;
    }

} // namespace CoreItem
