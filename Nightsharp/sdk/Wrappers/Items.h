#pragma once
// ============================================================================
// Items.h — Item Wrapper (EnsoulSharp.SDK/Core/Wrappers/Items.cs port)
// ============================================================================
// Provides:
//   - Item struct: id, range, slot info
//   - HasItem(), GetItem(), UseItem(), CanUseItem()
//   - GetItemCooldown(), GetItemStacks()
//   - GetSlotByItemId() → SpellSlotId for casting
//   - Inventory iteration, gold value, etc.
//
// Usage:
//   using namespace SDK;
//   Item botrk(ItemId::BladeOfTheRuinedKing);
//   if (botrk.IsOwned() && botrk.IsReady()) {
//       botrk.Cast(target);
//   }
//
//   // Or static helpers:
//   if (Items::HasItem(ItemId::Zhonyas)) {
//       Items::UseItem(ItemId::Zhonyas);
//   }
// ============================================================================

#include "core/Globals.h"
#include "core/Offsets.h"
#include "Enums.h"
#include "Game.h"
#include "GameObjects/GameObject.h"
#include "GameObjects/SpellBook.h"
#include "GameObjects/GameObjects.h"
#include "sdk/Utils/Bypass.h"
#include "Wrappers/Damages/DamageCalc.h"  // For DamageCalc::ItemId

#include <string>
#include <vector>
#include <algorithm>

// Forward declare — we use CastSpellInternal pattern from SpellCaster
extern "C" void* spoof_call(...);

namespace SDK {

    // Alias for DamageCalc::ItemId (defined in DamageCalc.h)
    using ItemId = DamageCalc::ItemId;

    // ========================================================================
    // ItemSlotInfo — Detailed info about a single inventory slot
    // ========================================================================
    struct ItemSlotInfo {
        int Id = 0;               // Item ID (int)
        int Stacks = 0;
        int SlotIndex = -1;       // 0-6 (maps to SpellSlotId::Item1..Trinket)
        SpellSlotId SpellSlot = SpellSlotId::Item1;

        // Raw pointers for advanced usage
        uintptr_t SlotPtr = 0;
        uintptr_t InfoPtr = 0;
        uintptr_t DataPtr = 0;

        bool IsValid() const { return Id > 0 && SlotIndex >= 0; }
    };

    // Forward declaration — Items class is defined below
    class Items;

    // ========================================================================
    // Item — High-level item wrapper (EnsoulSharp: Items.Item)
    // Methods that depend on Items are defined after Items class.
    // ========================================================================
    class Item {
    public:
        int Id;
        float Range;

        Item() : Id(0), Range(0) {}
        Item(int id, float range = 0) : Id(id), Range(range) {}
        Item(ItemId id, float range = 0) : Id((int)id), Range(range) {}

        // Ownership & Ready — implemented after Items class
        inline bool IsOwned() const;
        inline bool IsOwned(const GameObject& unit) const;
        inline bool IsReady() const;

        /// Check if item is in range of target
        bool IsInRange(const GameObject& target) const {
            if (Range <= 0) return true;
            if (!GameObjects::Player.IsValid() || !target.IsValid()) return false;
            float dist = GameObjects::Player.GetPosition().Distance2D(target.GetPosition());
            return dist <= Range + GameObjects::Player.GetBoundingRadius() + target.GetBoundingRadius();
        }

        // Cast — implemented after Items class
        inline bool Cast() const;
        inline bool Cast(const GameObject& target) const;
        inline bool Cast(const Vec3& pos) const;

        // Info — implemented after Items class
        inline float GetCooldown() const;
        inline int GetStacks() const;
    };

    // ========================================================================
    // Items — Static helper class (EnsoulSharp: Items static methods)
    // ========================================================================
    class Items {
    public:
        // ====================================================================
        // Inventory Reading
        // ====================================================================

        /// Get all item IDs owned by a unit
        static std::vector<int> GetItemIds(const GameObject& unit) {
            std::vector<int> result;
            uintptr_t addr = unit.address;
            if (!Globals::IsValidPtr(addr)) return result;

            uintptr_t itemListBase = addr + Offset::GameObject::ItemList;
            for (int i = 0; i < 7; i++) {
                uintptr_t slotPtr = Globals::Read<uintptr_t>(itemListBase + i * 8);
                if (!Globals::IsValidPtr(slotPtr)) continue;

                uintptr_t infoPtr = Globals::Read<uintptr_t>(slotPtr + Offset::ItemSystem::SlotInfo);
                if (!Globals::IsValidPtr(infoPtr)) continue;

                uintptr_t dataPtr = Globals::Read<uintptr_t>(infoPtr + Offset::ItemSystem::InfoData);
                if (!Globals::IsValidPtr(dataPtr)) continue;

                int itemId = Globals::Read<int>(dataPtr + Offset::ItemSystem::DataItemId);
                if (itemId > 0)
                    result.push_back(itemId);
            }
            return result;
        }

        /// Get all item IDs owned by local player
        static std::vector<int> GetItemIds() {
            return GetItemIds(GameObjects::Player);
        }

        /// Get detailed item slot info for a unit
        static std::vector<ItemSlotInfo> GetInventory(const GameObject& unit) {
            std::vector<ItemSlotInfo> result;
            uintptr_t addr = unit.address;
            if (!Globals::IsValidPtr(addr)) return result;

            uintptr_t itemListBase = addr + Offset::GameObject::ItemList;
            for (int i = 0; i < 7; i++) {
                uintptr_t slotPtr = Globals::Read<uintptr_t>(itemListBase + i * 8);
                if (!Globals::IsValidPtr(slotPtr)) continue;

                uintptr_t infoPtr = Globals::Read<uintptr_t>(slotPtr + Offset::ItemSystem::SlotInfo);
                if (!Globals::IsValidPtr(infoPtr)) continue;

                uintptr_t dataPtr = Globals::Read<uintptr_t>(infoPtr + Offset::ItemSystem::InfoData);
                if (!Globals::IsValidPtr(dataPtr)) continue;

                int itemId = Globals::Read<int>(dataPtr + Offset::ItemSystem::DataItemId);
                if (itemId <= 0) continue;

                ItemSlotInfo info;
                info.Id = itemId;
                info.SlotIndex = i;
                info.SpellSlot = IndexToSlot(i);
                info.SlotPtr = slotPtr;
                info.InfoPtr = infoPtr;
                info.DataPtr = dataPtr;
                info.Stacks = Globals::Read<int>(infoPtr + Offset::ItemSystem::InfoStacks);

                result.push_back(info);
            }
            return result;
        }

        /// Get detailed inventory for local player
        static std::vector<ItemSlotInfo> GetInventory() {
            return GetInventory(GameObjects::Player);
        }

        // ====================================================================
        // HasItem
        // ====================================================================

        /// Check if unit has item by ID
        static bool HasItem(const GameObject& unit, int itemId) {
            auto items = GetItemIds(unit);
            for (int id : items)
                if (id == itemId) return true;
            return false;
        }

        static bool HasItem(const GameObject& unit, ItemId itemId) {
            return HasItem(unit, (int)itemId);
        }

        /// Check if local player has item
        static bool HasItem(int itemId) {
            return HasItem(GameObjects::Player, itemId);
        }

        static bool HasItem(ItemId itemId) {
            return HasItem((int)itemId);
        }

        // ====================================================================
        // GetItem — Find item slot info
        // ====================================================================

        /// Find item slot info by item ID on a unit
        static ItemSlotInfo GetItem(const GameObject& unit, int itemId) {
            auto inventory = GetInventory(unit);
            for (auto& slot : inventory) {
                if (slot.Id == itemId) return slot;
            }
            return ItemSlotInfo(); // Invalid
        }

        static ItemSlotInfo GetItem(const GameObject& unit, ItemId itemId) {
            return GetItem(unit, (int)itemId);
        }

        /// Find item slot info on local player
        static ItemSlotInfo GetItem(int itemId) {
            return GetItem(GameObjects::Player, itemId);
        }

        static ItemSlotInfo GetItem(ItemId itemId) {
            return GetItem((int)itemId);
        }

        // ====================================================================
        // GetSlotByItemId — Get SpellSlotId for the item
        // ====================================================================

        /// Returns the SpellSlotId for the given item, or SpellSlotId::Item1 if not found
        static SpellSlotId GetSlotByItemId(int itemId) {
            auto info = GetItem(itemId);
            return info.IsValid() ? info.SpellSlot : SpellSlotId::Item1;
        }

        static SpellSlotId GetSlotByItemId(ItemId itemId) {
            return GetSlotByItemId((int)itemId);
        }

        // ====================================================================
        // Cooldown & Stacks
        // ====================================================================

        /// Get remaining cooldown for item (via SpellBook item slot)
        static float GetItemCooldown(int itemId) {
            auto info = GetItem(itemId);
            if (!info.IsValid()) return -1.0f;

            SpellBook sb(GameObjects::Player.address);
            if (!sb.IsValid()) return -1.0f;

            SpellSlot slot = sb.GetSpell(info.SpellSlot);
            return slot.GetRemainingCooldown();
        }

        static float GetItemCooldown(ItemId itemId) {
            return GetItemCooldown((int)itemId);
        }

        /// Get item charges/stacks
        static int GetItemStacks(int itemId) {
            auto info = GetItem(itemId);
            return info.IsValid() ? info.Stacks : 0;
        }

        static int GetItemStacks(ItemId itemId) {
            return GetItemStacks((int)itemId);
        }

        // ====================================================================
        // CanUseItem — Ready check (owned + off cooldown)
        // ====================================================================

        static bool CanUseItem(int itemId) {
            auto info = GetItem(itemId);
            if (!info.IsValid()) return false;

            SpellBook sb(GameObjects::Player.address);
            if (!sb.IsValid()) return false;

            SpellSlot slot = sb.GetSpell(info.SpellSlot);
            return slot.IsReady();
        }

        static bool CanUseItem(ItemId itemId) {
            return CanUseItem((int)itemId);
        }

        // ====================================================================
        // UseItem — Cast item active (uses CastSpell via SpellBook)
        // ====================================================================

        /// Use item (self-cast or targeted)
        static bool UseItem(int itemId, const GameObject* target = nullptr) {
            auto info = GetItem(itemId);
            if (!info.IsValid()) return false;

            SpellBook sb(GameObjects::Player.address);
            if (!sb.IsValid()) return false;

            SpellSlot slot = sb.GetSpell(info.SpellSlot);
            if (!slot.IsReady()) return false;

            // Use the same CastSpell pattern as SpellCaster
            Vec3 castPos;
            if (target && target->IsValid()) {
                castPos = target->GetPosition();
            } else {
                castPos = GameObjects::Player.GetPosition();
            }

            return CastItemSpell(info.SpellSlot, castPos);
        }

        static bool UseItem(ItemId itemId, const GameObject* target = nullptr) {
            return UseItem((int)itemId, target);
        }

        /// Use item at specific position
        static bool UseItemAtPos(int itemId, const Vec3& pos) {
            auto info = GetItem(itemId);
            if (!info.IsValid()) return false;

            SpellBook sb(GameObjects::Player.address);
            if (!sb.IsValid()) return false;

            SpellSlot slot = sb.GetSpell(info.SpellSlot);
            if (!slot.IsReady()) return false;

            return CastItemSpell(info.SpellSlot, pos);
        }

        static bool UseItemAtPos(ItemId itemId, const Vec3& pos) {
            return UseItemAtPos((int)itemId, pos);
        }

        // ====================================================================
        // Item Count
        // ====================================================================

        /// Count how many of a specific item a unit owns (usually 1)
        static int GetItemCount(const GameObject& unit, int itemId) {
            auto items = GetItemIds(unit);
            int count = 0;
            for (int id : items)
                if (id == itemId) count++;
            return count;
        }

        static int GetItemCount(int itemId) {
            return GetItemCount(GameObjects::Player, itemId);
        }

        // ====================================================================
        // Utility
        // ====================================================================

        /// Check if any enemy has a specific item
        static bool AnyEnemyHasItem(int itemId) {
            for (auto& hero : GameObjects::EnemyHeroes) {
                if (!hero.IsValid() || !hero.IsAlive()) continue;
                if (HasItem(hero, itemId)) return true;
            }
            return false;
        }

        /// Check if any ally has a specific item
        static bool AnyAllyHasItem(int itemId) {
            for (auto& hero : GameObjects::AllyHeroes) {
                if (!hero.IsValid() || !hero.IsAlive()) continue;
                if (HasItem(hero, itemId)) return true;
            }
            return false;
        }

        /// Get total completed items count (excluding trinket slot 6 which is index 6)
        static int GetCompletedItemCount(const GameObject& unit) {
            int count = 0;
            uintptr_t addr = unit.address;
            if (!Globals::IsValidPtr(addr)) return 0;

            uintptr_t itemListBase = addr + Offset::GameObject::ItemList;
            for (int i = 0; i < 6; i++) { // Skip trinket (slot 6)
                uintptr_t slotPtr = Globals::Read<uintptr_t>(itemListBase + i * 8);
                if (!Globals::IsValidPtr(slotPtr)) continue;

                uintptr_t infoPtr = Globals::Read<uintptr_t>(slotPtr + Offset::ItemSystem::SlotInfo);
                if (!Globals::IsValidPtr(infoPtr)) continue;

                uintptr_t dataPtr = Globals::Read<uintptr_t>(infoPtr + Offset::ItemSystem::InfoData);
                if (!Globals::IsValidPtr(dataPtr)) continue;

                int id = Globals::Read<int>(dataPtr + Offset::ItemSystem::DataItemId);
                if (id > 0) count++;
            }
            return count;
        }

        static int GetCompletedItemCount() {
            return GetCompletedItemCount(GameObjects::Player);
        }

    private:
        // ====================================================================
        // Internal: Map inventory index (0-6) to SpellSlotId
        // ====================================================================
        static SpellSlotId IndexToSlot(int idx) {
            switch (idx) {
                case 0: return SpellSlotId::Item1;
                case 1: return SpellSlotId::Item2;
                case 2: return SpellSlotId::Item3;
                case 3: return SpellSlotId::Item4;
                case 4: return SpellSlotId::Item5;
                case 5: return SpellSlotId::Item6;
                case 6: return SpellSlotId::Trinket;
                default: return SpellSlotId::Item1;
            }
        }

        // ====================================================================
        // Internal: Cast item spell via CastSpellSafe
        //   Same pattern as SpellCaster::CastSpellInternal but for item slots
        // ====================================================================
        static bool CastItemSpell(SpellSlotId slot, const Vec3& pos) {
            auto& player = GameObjects::Player;
            if (!player.IsValid()) return false;

            // Find spoof_call trampoline (FF 23 = jmp [rbx])
            void* trampoline = GetTrampoline();
            if (!trampoline) return false;

            // Get SpellBook → SpellSlot for item
            uintptr_t spellBookAddr = player.address + Offset::SpellBook::Offset;
            uintptr_t spellSlotAddr = Globals::Read<uintptr_t>(
                spellBookAddr + Offset::SpellBook::SpellSlotArray + (int)slot * 8);
            if (!Globals::IsValidPtr(spellSlotAddr)) return false;

            uintptr_t spellInfoPtr = Globals::Read<uintptr_t>(
                spellSlotAddr + Offset::SpellBook::SlotSpellInfo);
            if (!Globals::IsValidPtr(spellInfoPtr)) return false;

            uintptr_t spellInput = Globals::Read<uintptr_t>(
                spellSlotAddr + Offset::SpellBook::SlotSpellInput);
            if (!Globals::IsValidPtr(spellInput)) return false;

            // Get HUD
            uintptr_t hudInstance = Globals::Read<uintptr_t>(
                Globals::base + Offset::Global::HudInstance);
            if (!Globals::IsValidPtr(hudInstance)) return false;

            uintptr_t hudSpellInfo = Globals::Read<uintptr_t>(
                hudInstance + Offset::Hud::SpellInfo);
            if (!Globals::IsValidPtr(hudSpellInfo)) return false;

            uintptr_t hudInput = Globals::Read<uintptr_t>(
                hudInstance + Offset::Hud::Input);

            // Save original values
            Vec3 origStartPos = Globals::Read<Vec3>(spellInput + Offset::SpellBook::InputStartPos);
            Vec3 origEndPos = Globals::Read<Vec3>(spellInput + Offset::SpellBook::InputEndPos);

            // Write target position
            Vec3 startPos = player.GetPosition();
            Globals::Write<Vec3>(spellInput + Offset::SpellBook::InputStartPos, startPos);
            Globals::Write<Vec3>(spellInput + Offset::SpellBook::InputEndPos, pos);

            // Write mouse world position
            Vec3 origMouse = {};
            bool savedMouse = false;
            if (Globals::IsValidPtr(hudInput)) {
                origMouse = Globals::Read<Vec3>(hudInput + Offset::Hud::MouseWorldPos);
                Globals::Write<Vec3>(hudInput + Offset::Hud::MouseWorldPos, pos);
                savedMouse = true;
            }

            // Chimera pattern: item actives use the same CastSpellFlag -> CastSpellSafe
            // pipeline as normal spells on the current patch.
            Bypass::PrepareCastSpell();

            using fnCastSpell = void(__fastcall*)(uintptr_t, uintptr_t);
            fnCastSpell fn = reinterpret_cast<fnCastSpell>(
                Globals::base + Offset::Function::CastSpellSafe);

            bool success = false;
            __try {
                spoof_call(trampoline, fn, hudSpellInfo, spellInfoPtr);
                success = true;
            } __except(1) {
                success = false;
            }

            if (!success) {
                Globals::Write<uint8_t>(Globals::base + Offset::Flag::CastSpellFlag, 0);
            }

            // Restore
            Globals::Write<Vec3>(spellInput + Offset::SpellBook::InputStartPos, origStartPos);
            Globals::Write<Vec3>(spellInput + Offset::SpellBook::InputEndPos, origEndPos);
            if (savedMouse && Globals::IsValidPtr(hudInput)) {
                Globals::Write<Vec3>(hudInput + Offset::Hud::MouseWorldPos, origMouse);
            }

            return success;
        }

        // ====================================================================
        // Internal: Find trampoline for spoof_call (FF 23 = jmp [rbx])
        // ====================================================================
        static void* GetTrampoline() {
            static void* trampoline = nullptr;
            if (!trampoline) {
                MODULEINFO modInfo{};
                HMODULE hMod = GetModuleHandleA(nullptr);
                if (hMod && GetModuleInformation(GetCurrentProcess(), hMod, &modInfo, sizeof(modInfo))) {
                    uint8_t* base = (uint8_t*)modInfo.lpBaseOfDll;
                    size_t size = modInfo.SizeOfImage;
                    for (size_t i = 0; i + 1 < size; i++) {
                        if (base[i] == 0xFF && base[i + 1] == 0x23) {
                            trampoline = &base[i];
                            break;
                        }
                    }
                }
            }
            return trampoline;
        }
    };

    // ========================================================================
    // Item method implementations (depend on Items class)
    // ========================================================================
    inline bool Item::IsOwned() const { return Items::HasItem(Id); }
    inline bool Item::IsOwned(const GameObject& unit) const { return Items::HasItem(unit, Id); }
    inline bool Item::IsReady() const { return Items::CanUseItem(Id); }
    inline bool Item::Cast() const { return Items::UseItem(Id); }
    inline bool Item::Cast(const GameObject& target) const {
        if (!IsReady()) return false;
        if (Range > 0 && !IsInRange(target)) return false;
        return Items::UseItem(Id, &target);
    }
    inline bool Item::Cast(const Vec3& pos) const {
        if (!IsReady()) return false;
        return Items::UseItemAtPos(Id, pos);
    }
    inline float Item::GetCooldown() const { return Items::GetItemCooldown(Id); }
    inline int Item::GetStacks() const { return Items::GetItemStacks(Id); }

} // namespace SDK
