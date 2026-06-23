#pragma once

#include "../../Enumerations/SpellSlot.h"

#include <cstdint>

namespace SDK {

class LastCastPacketSentEntry {
public:
    LastCastPacketSentEntry() = default;

    LastCastPacketSentEntry(SpellSlot slot, int tick, std::uint32_t targetNetworkId)
        : Slot(slot),
          TargetNetworkId(targetNetworkId),
          Tick(tick) {
    }

    SpellSlot Slot = SpellSlot::Unknown;
    std::uint32_t TargetNetworkId = 0;
    int Tick = 0;
};

} // namespace SDK
