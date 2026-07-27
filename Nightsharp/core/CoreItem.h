#pragma once

#include "Globals.h"
#include "offset.h"

#include <algorithm>
#include <cstdint>
#include <cstring>

namespace CoreItem {

inline constexpr int kItemSlotStart = 0;
inline constexpr int kItemSlotEnd = 5;
inline constexpr int kTrinketSlot = 6;
inline constexpr int kInternalSlotStart = 7;
inline constexpr int kVisibleSlotCount = 7;
inline constexpr int kMaxSlots = Offset::ItemRuntime::SlotCount;

struct ItemSlot {
    int index = -1;
    bool hasItem = false;
    int id = 0;
    uintptr_t slotPtr = 0;
    uintptr_t nodePtr = 0;
    uintptr_t infoPtr = 0;
    char idText[16] = {};

    bool IsEmpty() const { return !hasItem; }
    bool IsVisibleSlot() const { return index >= kItemSlotStart && index <= kTrinketSlot; }
    bool IsTrinket() const { return index == kTrinketSlot; }
};

inline bool IsSlotIndexValid(int slotIndex) {
    return slotIndex >= 0 && slotIndex < kMaxSlots;
}

inline bool IsVisibleSlot(int slotIndex) {
    return slotIndex >= kItemSlotStart && slotIndex <= kTrinketSlot;
}

inline int ToSpellSlotIndex(int slotIndex) {
    if (slotIndex >= kItemSlotStart && slotIndex <= kItemSlotEnd) {
        return 6 + slotIndex;
    }
    if (slotIndex == kTrinketSlot) {
        return 12;
    }
    return -1;
}

inline uintptr_t InventoryComponent(uintptr_t object) {
    return Globals::IsValidPtr(object)
        ? object + Offset::ItemRuntime::InventoryComponent
        : 0;
}

inline uintptr_t SlotArray(uintptr_t object) {
    const uintptr_t component = InventoryComponent(object);
    return component ? component + Offset::ItemRuntime::SlotArray : 0;
}

inline uintptr_t GetSlotPtr(uintptr_t object, int slotIndex) {
    if (!Globals::IsValidPtr(object) || !IsSlotIndexValid(slotIndex)) {
        return 0;
    }

    const uintptr_t slotAddress =
        SlotArray(object) + static_cast<uintptr_t>(slotIndex) * sizeof(uintptr_t);
    const uintptr_t slot = Globals::Read<uintptr_t>(slotAddress);
    return Globals::IsValidPtr(slot) ? slot : 0;
}

inline uintptr_t GetItemNode(uintptr_t object, int slotIndex) {
    const uintptr_t slot = GetSlotPtr(object, slotIndex);
    if (!slot) {
        return 0;
    }

    const uintptr_t node = Globals::Read<uintptr_t>(slot + Offset::ItemRuntime::ItemNode);
    return Globals::IsValidPtr(node) ? node : 0;
}

inline uintptr_t GetItemInfo(uintptr_t object, int slotIndex) {
    const uintptr_t node = GetItemNode(object, slotIndex);
    if (!node) {
        return 0;
    }

    const uintptr_t info = Globals::Read<uintptr_t>(node + Offset::ItemRuntime::ItemInfo);
    return Globals::IsValidPtr(info) ? info : 0;
}

inline bool ReadItemIdText(uintptr_t info, char* out, int maxOut) {
    if (!out || maxOut <= 1) {
        return false;
    }
    out[0] = 0;

    if (!Globals::IsValidPtr(info)) {
        return false;
    }

    char text[32] = {};
    if (!Globals::ReadRuntimeStringField(
            info + Offset::ItemRuntime::DataItemIdString,
            text,
            static_cast<int>(sizeof(text)))) {
        return false;
    }

    int count = 0;
    while (text[count] >= '0' && text[count] <= '9' && count < maxOut - 1) {
        out[count] = text[count];
        ++count;
    }
    out[count] = 0;
    return count > 0;
}

inline int ParseItemId(const char* text) {
    if (!text || !text[0]) {
        return 0;
    }

    int value = 0;
    for (int i = 0; text[i]; ++i) {
        if (text[i] < '0' || text[i] > '9') {
            return 0;
        }
        value = value * 10 + (text[i] - '0');
        if (value > 999999) {
            return 0;
        }
    }
    return value;
}

inline int GetItemIdFromInfo(uintptr_t info) {
    char text[16] = {};
    return ReadItemIdText(info, text, static_cast<int>(sizeof(text)))
        ? ParseItemId(text)
        : 0;
}

inline int GetItemId(uintptr_t object, int slotIndex) {
    return GetItemIdFromInfo(GetItemInfo(object, slotIndex));
}

inline bool HasItem(uintptr_t object, int slotIndex) {
    return GetItemNode(object, slotIndex) != 0;
}

// ARAM/Arena bán bản sao của mỗi trang bị dưới một id riêng bằng id gốc cộng
// 220000 (Sheen 3057 -> 223057, Iceborn Gauntlet 6662 -> 226662, Shadowflame
// 4645 -> 224645). So khớp id thô khiến HasItemId(3057) trả false khi người chơi
// đang cầm 223057, tức MỌI passive/logic phụ thuộc trang bị im lặng ở các chế độ
// đó. Chuẩn hóa cả hai vế về id gốc để mọi call site khớp mà không phải tự xử lý.
//
// Chỉ xử lý dải 22xxxx đã đối chiếu được với ItemData.h. Các biến thể khác
// (124011, 326621, 443060, 447111, ...) ghép chuỗi chứ không theo phép cộng cố
// định nên không quy về được, cố ý bỏ qua.
inline int NormalizeItemId(int itemId) {
    return (itemId > 220000 && itemId < 230000) ? itemId - 220000 : itemId;
}

inline bool HasItemId(uintptr_t object, int itemId, bool includeTrinket = true) {
    if (!Globals::IsValidPtr(object) || itemId <= 0) {
        return false;
    }

    const int wanted = NormalizeItemId(itemId);
    const int end = includeTrinket ? kTrinketSlot : kItemSlotEnd;
    for (int i = kItemSlotStart; i <= end; ++i) {
        if (NormalizeItemId(GetItemId(object, i)) == wanted) {
            return true;
        }
    }
    return false;
}

inline int GetItemCount(uintptr_t object) {
    int count = 0;
    for (int i = kItemSlotStart; i <= kItemSlotEnd; ++i) {
        if (HasItem(object, i)) {
            ++count;
        }
    }
    return count;
}

inline bool HasTrinket(uintptr_t object) {
    return HasItem(object, kTrinketSlot);
}

inline ItemSlot ReadSlot(uintptr_t object, int slotIndex) {
    ItemSlot out{};
    out.index = slotIndex;
    if (!Globals::IsValidPtr(object) || !IsSlotIndexValid(slotIndex)) {
        return out;
    }

    out.slotPtr = GetSlotPtr(object, slotIndex);
    if (!out.slotPtr) {
        return out;
    }

    out.nodePtr = GetItemNode(object, slotIndex);
    out.hasItem = out.nodePtr != 0;
    if (!out.hasItem) {
        return out;
    }

    out.infoPtr = GetItemInfo(object, slotIndex);
    if (out.infoPtr) {
        ReadItemIdText(out.infoPtr, out.idText, static_cast<int>(sizeof(out.idText)));
        out.id = ParseItemId(out.idText);
    }
    return out;
}

inline int SnapshotItems(uintptr_t object, ItemSlot* out, int maxOut) {
    if (!out || maxOut <= 0 || !Globals::IsValidPtr(object)) {
        return 0;
    }

    const int limit = std::min(maxOut, kVisibleSlotCount);
    for (int i = 0; i < limit; ++i) {
        out[i] = ReadSlot(object, i);
    }
    return limit;
}

inline int SnapshotAllSlots(uintptr_t object, ItemSlot* out, int maxOut) {
    if (!out || maxOut <= 0 || !Globals::IsValidPtr(object)) {
        return 0;
    }

    const int limit = std::min(maxOut, kMaxSlots);
    for (int i = 0; i < limit; ++i) {
        out[i] = ReadSlot(object, i);
    }
    return limit;
}

} // namespace CoreItem
