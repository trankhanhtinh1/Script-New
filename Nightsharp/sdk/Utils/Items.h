#pragma once

#include "../Extensions/AIHeroClientExtensions.h"
#include "../GameObjects/GameObjects.h"

#include <string_view>

namespace SDK::Items {

inline bool ItemNameEquals(const InventorySlot& slot, const char* name) {
    if (!slot.IsValid() || !name) {
        return false;
    }

    const auto* entry = slot.DatabaseEntry();
    return entry && entry->Name == std::string_view(name);
}

inline bool HasItem(const AIHeroClient& source, int itemId) {
    return source.IsValid() && source.HasItem(itemId);
}

inline bool HasItem(const AIHeroClient& source, const char* name) {
    if (!source.IsValid() || !name) {
        return false;
    }

    const auto items = source.InventoryItems();
    for (const auto& slot : items) {
        if (ItemNameEquals(slot, name)) {
            return true;
        }
    }
    return false;
}

inline bool CanUseItem(const AIHeroClient& source, int itemId) {
    return SDK::CanUseItem(source, itemId);
}

inline bool CanUseItem(const AIHeroClient& source, const char* name) {
    if (!source.IsValid() || !name) {
        return false;
    }

    const auto items = source.InventoryItems();
    for (const auto& slot : items) {
        if (!ItemNameEquals(slot, name)) {
            continue;
        }

        const auto spell = source.Spellbook().GetSpell(slot.GetSpellSlot());
        return spell.IsValid() &&
               spell.State(0.0f) == CoreSpellBook::State_Ready;
    }
    return false;
}

inline InventorySlot GetWardSlot(const AIHeroClient& source) {
    constexpr int wardIds[] = {
        3340, // Stealth_Ward
        3859, // Targons_Buckler
        3860, // Bulwark_of_the_Mountain
        3855, // Runesteel_Spaulders
        4641, // Stirring_Wardstone
        3857, // Pauldrons_of_Whiterock
        4643, // Vigilant_Wardstone
        4638, // Watchful_Wardstone
        2055  // Control_Ward
    };

    for (const int wardId : wardIds) {
        if (!SDK::Items::CanUseItem(source, wardId)) {
            continue;
        }

        const auto items = source.InventoryItems();
        for (const auto& slot : items) {
            if (slot.Id() == wardId) {
                return slot;
            }
        }
    }
    return InventorySlot();
}

inline bool UseItem(const AIHeroClient& source, int itemId) {
    const auto player = GameObjects::Player();
    if (!source.IsValid() || !source.Compare(player)) {
        return false;
    }

    return SDK::UseItem(player, itemId);
}

inline bool UseItem(const AIHeroClient& source, int itemId,
                    const AIBaseClient& target) {
    const auto player = GameObjects::Player();
    if (!source.IsValid() || !source.Compare(player)) {
        return false;
    }

    if (!target.IsValid()) {
        return SDK::UseItem(player, itemId);
    }
    return SDK::UseItem(player, itemId, target);
}

inline bool UseItem(const AIHeroClient& source, int itemId, Vector3 position) {
    const auto player = GameObjects::Player();
    if (!source.IsValid() || !source.Compare(player) || position.IsZero()) {
        return false;
    }

    return SDK::UseItem(player, itemId, position);
}

inline bool UseItem(const AIHeroClient& source, int itemId, Vector2 position) {
    return SDK::Items::UseItem(source, itemId, Vector3::From2D(position));
}

inline bool UseItem(const AIHeroClient& source, const char* name,
                    const AIHeroClient& target = AIHeroClient()) {
    const auto player = GameObjects::Player();
    if (!source.IsValid() || !source.Compare(player) || !name) {
        return false;
    }

    const auto items = player.InventoryItems();
    for (const auto& slot : items) {
        if (!ItemNameEquals(slot, name)) {
            continue;
        }

        if (!target.IsValid()) {
            return player.Spellbook().CastSpell(slot.GetSpellSlot());
        }
        return player.Spellbook().CastSpell(slot.GetSpellSlot(), target);
    }
    return false;
}

} // namespace SDK::Items
