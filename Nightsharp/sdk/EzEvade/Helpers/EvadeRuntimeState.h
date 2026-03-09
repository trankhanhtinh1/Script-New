#pragma once
#include "core/Vector.h"
#include "sdk/Enums.h"

namespace EzEvade {

// Runtime state ported from EzEvade/Core/Evade.cs static fields.
// Keeps the same state model so next ported modules can wire in directly.
struct EvadeRuntimeState {
    static inline bool Enabled = true;

    static inline SDK::SpellSlotId LastSpellCast = SDK::SpellSlotId::Q;
    static inline float LastSpellCastTime = 0.0f;
    static inline float LastWindupTime = 0.0f;

    static inline float LastTickCount = 0.0f;
    static inline float LastStopEvadeTime = 0.0f;

    static inline Vec3 LastMovementBlockPos = Vec3();
    static inline float LastMovementBlockTime = 0.0f;

    static inline float LastEvadeOrderTime = 0.0f;
    static inline float LastIssueOrderGameTime = 0.0f;
    static inline float LastIssueOrderTime = 0.0f;

    static inline Vec2 LastMoveToPosition = Vec2();
    static inline Vec2 LastMoveToServerPos = Vec2();
    static inline Vec2 LastStopPosition = Vec2();

    static inline bool IsDodging = false;
    static inline bool DodgeOnlyDangerous = false;
    static inline bool DevModeOn = false;
    static inline bool HasGameEnded = false;
    static inline bool IsChanneling = false;
    static inline Vec2 ChannelPosition = Vec2();

    static inline float LastDodgingEndTime = 0.0f;
};

} // namespace EzEvade
