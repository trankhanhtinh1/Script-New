#pragma once

#include <cstdint>
#include <unordered_map>

// ============================================================================
// ItemCatalog — Runtime mapping of Riot item ids to game-internal ids.
// ============================================================================
//
// The engine stores a stable INTERNAL item hash at `info+0xB4` (see CoreItem.h).
// That value is not the Riot-facing item id. Since the mapping is loaded from
// client data on disk and is not in the executable, we either:
//   (a) learn at runtime by observing a known slot (e.g. after a shop purchase),
//   (b) use a unique buff name as a side-channel for items that carry one, or
//   (c) let the user preload a table for the current patch.
//
// Public surface:
//   ItemCatalog::Learn(riotId, internalId)      remember a mapping
//   ItemCatalog::GetInternal(riotId)            0 if unknown
//   ItemCatalog::GetRiot(internalId)            0 if unknown
//   ItemCatalog::GetUniqueBuffFor(riotId)       nullptr if no reliable buff
//
// The buff table only contains items whose buff name is UNIQUE to that item
// (e.g. "InfinityEdge" identifies 3031, but "sheen" does not distinguish
// 3057 Sheen from 3078 Trinity Force).
// ============================================================================

namespace ItemCatalog {

    struct UniqueBuffEntry {
        int riotId;
        const char* buffName;
    };

    // Items that can be disambiguated via a unique buff name alone.
    // Keep this list conservative — a false-positive would inflate damage
    // predictions. Only list buffs that never appear without the item.
    inline const UniqueBuffEntry kUniqueBuffItems[] = {
        { 3031, "InfinityEdge"                 }, // Infinity Edge
        { 3078, "TrinityForce"                 }, // Trinity Force (prefer unique over "sheen")
        { 3025, "itemfrozenfist"               }, // Iceborn Gauntlet
        { 3100, "lichbane"                     }, // Lich Bane
        { 3748, "itemtitanichydracleavebuff"   }, // Titanic Hydra active
        { 3303, "hydaborusactiveattack"        }, // Profane Hydra active
    };

    inline const char* GetUniqueBuffFor(int riotId) {
        for (const auto& e : kUniqueBuffItems) {
            if (e.riotId == riotId) return e.buffName;
        }
        return nullptr;
    }

    inline std::unordered_map<int, uint32_t>& RiotToInternalMap() {
        static std::unordered_map<int, uint32_t> s;
        return s;
    }

    inline std::unordered_map<uint32_t, int>& InternalToRiotMap() {
        static std::unordered_map<uint32_t, int> s;
        return s;
    }

    inline void Learn(int riotId, uint32_t internalId) {
        if (riotId <= 0 || internalId == 0) return;
        RiotToInternalMap()[riotId] = internalId;
        InternalToRiotMap()[internalId] = riotId;
    }

    inline uint32_t GetInternal(int riotId) {
        auto& m = RiotToInternalMap();
        auto it = m.find(riotId);
        return it != m.end() ? it->second : 0;
    }

    inline int GetRiot(uint32_t internalId) {
        auto& m = InternalToRiotMap();
        auto it = m.find(internalId);
        return it != m.end() ? it->second : 0;
    }

    inline void Clear() {
        RiotToInternalMap().clear();
        InternalToRiotMap().clear();
    }

} // namespace ItemCatalog
