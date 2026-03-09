#pragma once
// ============================================================================
// LastCastedSpellEntry.h — Port of EnsoulSharp.SDK LastCastedSpellEntry.cs
// ============================================================================
// Holds information about the last casted spell a unit did.
// ============================================================================

#include <string>

namespace SDK {

class LastCastedSpellEntry {
public:
    /// Name of the spell last casted
    std::string Name;

    /// Target NetworkId (0 if no target)
    unsigned int TargetNetId = 0;

    /// Start time of the cast (game time in seconds)
    float StartTime = 0.0f;

    /// End time of the cast (game time in seconds)
    float EndTime = 0.0f;

    /// Whether the entry contains valid data
    bool IsValid = false;

    /// Spell slot (0=Q, 1=W, 2=E, 3=R, 4=D, 5=F, ...)
    int Slot = -1;

    /// Start position of the spell cast
    Vec3 StartPos;

    /// End position of the spell cast
    Vec3 EndPos;

    // Default constructor (empty / invalid)
    LastCastedSpellEntry() = default;

    // Constructor from spell cast data
    LastCastedSpellEntry(const std::string& name, int slot,
                         unsigned int targetNetId,
                         float startTime, float endTime,
                         const Vec3& startPos, const Vec3& endPos)
        : Name(name), Slot(slot), TargetNetId(targetNetId),
          StartTime(startTime), EndTime(endTime),
          StartPos(startPos), EndPos(endPos), IsValid(true) {}

    /// Time since this spell was cast (seconds)
    float TimeSinceCast(float gameTime) const {
        return gameTime - StartTime;
    }
};

} // namespace SDK
