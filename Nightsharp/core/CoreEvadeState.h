#pragma once

namespace CoreEvadeState {

inline bool StrictEvadeActive = false;
inline int ComboBlockUntilTick = 0;
inline int SpellCastBypassDepth = 0;

inline void SetStrictEvadeActive(bool active) {
    StrictEvadeActive = active;
}

inline void BlockComboUntil(int tick) {
    if (tick > ComboBlockUntilTick) ComboBlockUntilTick = tick;
}

inline void ClearComboBlock(int) {
    ComboBlockUntilTick = 0;
}

inline void ClearAll() {
    StrictEvadeActive = false;
    ComboBlockUntilTick = 0;
    SpellCastBypassDepth = 0;
}

inline bool IsComboBlocked(int now) {
    return StrictEvadeActive || now < ComboBlockUntilTick;
}

inline bool AreSpellCastsBlocked(int now) {
    return IsComboBlocked(now) && SpellCastBypassDepth <= 0;
}

struct SpellCastBypassScope {
    SpellCastBypassScope() { ++SpellCastBypassDepth; }
    ~SpellCastBypassScope() { if (SpellCastBypassDepth > 0) --SpellCastBypassDepth; }
};

} // namespace CoreEvadeState
