#pragma once
#include "sdk/EzEvade/Helpers/PositionInfo.h"

namespace EzEvade {

// Shared runtime context mirroring original EzEvade static fields that are
// consumed across Evade/Core/Spells modules.
struct EvadeContext {
    static inline bool IsDodging = false;
    static inline bool DodgeOnlyDangerous = false;
    static inline bool HasLastPosInfo = false;
    static inline PositionInfo LastPosInfo = PositionInfo();
};

} // namespace EzEvade

