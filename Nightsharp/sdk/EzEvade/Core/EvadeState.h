#pragma once
#include "../../core/Vector.h"
// =========================================================================
// EvadeState.h — Shared state variables for the Evade system
//   Extracted from Evade.h to avoid circular includes
//   These correspond to C# Evade.cs static fields (lines 16-67)
// =========================================================================

namespace EzEvade {

// Forward declare EvadeCommand to avoid circular include
class EvadeCommand;

namespace EvadeState {

    // C# line 29
    inline float lastWindupTime = 0;

    // C# line 46-47
    inline float lastSpellCastTime = 0;
    inline int   lastSpellCast = -1;       // SpellSlot

    // C# line 48-54
    inline bool isDodging = false;
    inline bool dodgeOnlyDangerous = false;
    inline bool devModeOn = false;
    inline bool hasGameEnded = false;
    inline bool isChanneling = false;

    // C# line 42-44  (move tracking)
    inline Vec2 lastMoveToPosition = {0, 0};
    inline Vec2 lastMoveToServerPos = {0, 0};
    inline Vec2 lastStopPosition = {0, 0};

    // C# line 58: lastEvadeCommand — stored as opaque bytes to avoid circular includes
    // Access via EvadeState::lastEvadeCommand (cast in EvadeCommand.cpp)
    // Size = sizeof(EvadeCommand) rounded up
    struct EvadeCommandStorage {
        char data[256] = {};  // large enough for EvadeCommand struct
    };
    inline EvadeCommandStorage lastEvadeCommandStorage;
    inline EvadeCommandStorage lastSpellEvadeCommandStorage;

    // Typed access (implemented in EvadeCommand.cpp via reinterpret_cast)
    // Usage: EvadeState::lastEvadeCommand = cmd;
    // These are actually references — defined below for use by code that includes EvadeCommand.h
    // For code that only has EvadeState.h, the storage is available via lastEvadeCommandStorage

} // namespace EvadeState
} // namespace EzEvade
