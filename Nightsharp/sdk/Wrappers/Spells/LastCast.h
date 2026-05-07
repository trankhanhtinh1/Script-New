#pragma once

#include "../../Enumerations/SpellSlot.h"
#include "../../Core/Game.h"
#include "../../Core/Objects.h"

#include <string>

namespace SDK::LastCast {

    inline const struct CastPacketEntry* LastCastPacketSent = nullptr;
    inline const struct CastPacketEntry* LastCastedSpell = nullptr;

    struct CastPacketEntry {
        SpellSlot Slot = SpellSlot::Unknown;
        Vector3 StartPosition = {};
        Vector3 EndPosition = {};
        int TargetNetworkId = 0;
        int Tick = 0;
        std::string Name = {};

        bool IsValid() const {
            return Slot != SpellSlot::Unknown;
        }
    };

    inline CastPacketEntry& LastCastPacketSentState() {
        static CastPacketEntry s_state = {};
        return s_state;
    }

    inline CastPacketEntry& LastCastedSpellState() {
        static CastPacketEntry s_state = {};
        return s_state;
    }

    inline void NotifyLocalSpellCast(SpellSlot slot,
                                     const Vector3& start,
                                     const Vector3& end,
                                     int targetNetId,
                                     const std::string& name = {}) {
        CastPacketEntry entry = {};
        entry.Slot = slot;
        entry.StartPosition = start;
        entry.EndPosition = end;
        entry.TargetNetworkId = targetNetId;
        entry.Tick = Game::TickCount();
        entry.Name = name;
        LastCastPacketSentState() = entry;
        LastCastedSpellState() = entry;
        LastCastPacketSent = &LastCastPacketSentState();
        LastCastedSpell = &LastCastedSpellState();
    }

    inline const CastPacketEntry* GetLastCastPacketSent() {
        const auto& entry = LastCastPacketSentState();
        return entry.IsValid() ? &entry : nullptr;
    }

    inline const CastPacketEntry* GetLastCastedSpell() {
        const auto& entry = LastCastedSpellState();
        return entry.IsValid() ? &entry : nullptr;
    }

    inline void Reset() {
        LastCastPacketSentState() = CastPacketEntry{};
        LastCastedSpellState() = CastPacketEntry{};
        LastCastPacketSent = nullptr;
        LastCastedSpell = nullptr;
    }

} // namespace SDK::LastCast
