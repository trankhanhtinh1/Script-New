#pragma once

namespace CoreEvadeState {

inline bool StrictEvadeActive = false;
inline bool StrictAttackBlockActive = false;
inline int ComboBlockUntilTick = 0;
inline int SpellCastBypassDepth = 0;
using SpellBlockPredicate = bool(*)(int slot);
inline SpellBlockPredicate SpellBlocker = nullptr;

inline void SetStrictEvadeActive(bool active) {
    StrictEvadeActive = active;
    StrictAttackBlockActive = active;
}

// KuroEvade follows the source menu semantics: spell interception remains
// active while evading, while auto-attacks are only blocked above the
// configured danger level. Legacy callers of SetStrictEvadeActive retain the
// original all-or-nothing behavior.
inline void SetEvadeInterventionState(bool active, bool blockAttacks) {
    StrictEvadeActive = active;
    StrictAttackBlockActive = active && blockAttacks;
}

inline void BlockComboUntil(int tick) {
    if (tick > ComboBlockUntilTick) ComboBlockUntilTick = tick;
}

inline void ClearComboBlock(int) {
    ComboBlockUntilTick = 0;
}

inline void ClearAll() {
    StrictEvadeActive = false;
    StrictAttackBlockActive = false;
    ComboBlockUntilTick = 0;
    SpellCastBypassDepth = 0;
    SpellBlocker = nullptr;
}

inline void SetSpellBlockPredicate(SpellBlockPredicate predicate) {
    SpellBlocker = predicate;
}

inline bool IsComboBlocked(int now) {
    return StrictAttackBlockActive || now < ComboBlockUntilTick;
}

inline bool AreSpellCastsBlocked(int now, int slot = -1) {
    if ((!StrictEvadeActive && now >= ComboBlockUntilTick) ||
        SpellCastBypassDepth > 0) {
        return false;
    }
    return !SpellBlocker || slot < 0 || SpellBlocker(slot);
}

struct SpellCastBypassScope {
    SpellCastBypassScope() { ++SpellCastBypassDepth; }
    ~SpellCastBypassScope() { if (SpellCastBypassDepth > 0) --SpellCastBypassDepth; }
};

} // namespace CoreEvadeState
