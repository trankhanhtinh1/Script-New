#pragma once
// ============================================================================
// LastCastPacketSentEntry.h — Port of EnsoulSharp.SDK LastCastPacketSentEntry.cs
// ============================================================================
// Records the last spell cast command sent by the LOCAL player.
// (OnCastSpell event — before the server confirms it)
// ============================================================================

#include <string>

namespace SDK {

class LastCastPacketSentEntry {
public:
    /// Spell slot (0=Q, 1=W, 2=E, 3=R, 4=D, 5=F)
    int Slot = -1;

    /// Game time when the cast command was sent (seconds)
    float Tick = 0.0f;

    /// Target NetworkId (0 if ground-targeted / no target)
    unsigned int TargetNetworkId = 0;

    /// Whether the entry is valid
    bool IsValid = false;

    // Default constructor
    LastCastPacketSentEntry() = default;

    // Constructor
    LastCastPacketSentEntry(int slot, float tick, unsigned int targetNetId)
        : Slot(slot), Tick(tick), TargetNetworkId(targetNetId), IsValid(true) {}
};

} // namespace SDK
