#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace CoreEvadeState {

using SpellBlockPredicate = bool(*)(int slot);

// Slot zero is reserved for the source-compatible legacy API. The remaining
// slots are explicit owners. NightSharp dispatches these callbacks serially;
// callers must not access this state concurrently.
inline constexpr std::size_t OwnerCapacity = 8;
inline constexpr std::size_t LegacyOwner = 0;

struct OwnerToken {
    std::size_t slot = OwnerCapacity;
    std::uint32_t generation = 0;

    explicit operator bool() const {
        return slot > LegacyOwner &&
            slot < OwnerCapacity &&
            generation != 0;
    }
};

struct OwnerState {
    bool acquired = false;
    bool active = false;
    bool blockAttacks = false;
    int comboUntilTick = 0;
    SpellBlockPredicate spellBlocker = nullptr;
    std::uint32_t generation = 1;
};

inline bool StrictEvadeActive = false;
inline bool StrictAttackBlockActive = false;
inline int ComboBlockUntilTick = 0;
inline int SpellCastBypassDepth = 0;
inline SpellBlockPredicate SpellBlocker = nullptr;
inline std::array<OwnerState, OwnerCapacity> OwnerStates = {};

inline std::uint32_t NextGeneration(std::uint32_t generation) {
    ++generation;
    return generation == 0 ? 1 : generation;
}

inline void RecomputeAggregates() {
    bool evadeActive = OwnerStates[LegacyOwner].active;
    bool attackBlockActive =
        OwnerStates[LegacyOwner].active &&
        OwnerStates[LegacyOwner].blockAttacks;
    int comboUntilTick = OwnerStates[LegacyOwner].comboUntilTick;

    for (std::size_t slot = LegacyOwner + 1;
         slot < OwnerCapacity;
         ++slot) {
        const OwnerState& owner = OwnerStates[slot];
        if (!owner.acquired) continue;
        evadeActive = evadeActive || owner.active;
        attackBlockActive =
            attackBlockActive || (owner.active && owner.blockAttacks);
        if (owner.comboUntilTick > comboUntilTick)
            comboUntilTick = owner.comboUntilTick;
    }

    StrictEvadeActive = evadeActive;
    StrictAttackBlockActive = attackBlockActive;
    ComboBlockUntilTick = comboUntilTick;
}

inline bool IsCurrentOwner(const OwnerToken& token) {
    return token &&
        OwnerStates[token.slot].acquired &&
        OwnerStates[token.slot].generation == token.generation;
}

inline OwnerToken AcquireOwner() {
    for (std::size_t slot = LegacyOwner + 1;
         slot < OwnerCapacity;
         ++slot) {
        OwnerState& owner = OwnerStates[slot];
        if (owner.acquired) continue;
        owner.acquired = true;
        owner.active = false;
        owner.blockAttacks = false;
        owner.comboUntilTick = 0;
        owner.spellBlocker = nullptr;
        if (owner.generation == 0) owner.generation = 1;
        return {slot, owner.generation};
    }
    return {};
}

inline bool SetOwnerState(const OwnerToken& token,
                          bool active,
                          bool blockAttacks,
                          int comboUntilTick) {
    if (!IsCurrentOwner(token)) return false;
    OwnerState& owner = OwnerStates[token.slot];
    owner.active = active;
    owner.blockAttacks = active && blockAttacks;
    owner.comboUntilTick = comboUntilTick;
    RecomputeAggregates();
    return true;
}

inline bool SetOwnerSpellBlockPredicate(
    const OwnerToken& token,
    SpellBlockPredicate predicate) {
    if (!IsCurrentOwner(token)) return false;
    OwnerStates[token.slot].spellBlocker = predicate;
    return true;
}

inline bool ReleaseOwner(const OwnerToken& token) {
    if (!IsCurrentOwner(token)) return false;
    OwnerState& owner = OwnerStates[token.slot];
    owner.acquired = false;
    owner.active = false;
    owner.blockAttacks = false;
    owner.comboUntilTick = 0;
    owner.spellBlocker = nullptr;
    owner.generation = NextGeneration(owner.generation);
    RecomputeAggregates();
    return true;
}

inline void SetStrictEvadeActive(bool active) {
    OwnerState& legacy = OwnerStates[LegacyOwner];
    legacy.active = active;
    legacy.blockAttacks = active;
    RecomputeAggregates();
}

// KuroEvade follows the source menu semantics: spell interception remains
// active while evading, while auto-attacks are only blocked above the
// configured danger level. Legacy callers of SetStrictEvadeActive retain the
// original all-or-nothing behavior.
inline void SetEvadeInterventionState(bool active, bool blockAttacks) {
    OwnerState& legacy = OwnerStates[LegacyOwner];
    legacy.active = active;
    legacy.blockAttacks = active && blockAttacks;
    RecomputeAggregates();
}

inline void BlockComboUntil(int tick) {
    OwnerState& legacy = OwnerStates[LegacyOwner];
    if (tick > legacy.comboUntilTick) legacy.comboUntilTick = tick;
    RecomputeAggregates();
}

inline void ClearComboBlock(int) {
    OwnerStates[LegacyOwner].comboUntilTick = 0;
    RecomputeAggregates();
}

// Process-shutdown reset. This intentionally invalidates every outstanding
// explicit token as well as clearing the reserved legacy owner.
inline void ClearAll() {
    for (std::size_t slot = 0; slot < OwnerCapacity; ++slot) {
        OwnerState& owner = OwnerStates[slot];
        owner.acquired = false;
        owner.active = false;
        owner.blockAttacks = false;
        owner.comboUntilTick = 0;
        owner.spellBlocker = nullptr;
        owner.generation = NextGeneration(owner.generation);
    }
    SpellCastBypassDepth = 0;
    SpellBlocker = nullptr;
    RecomputeAggregates();
}

inline void SetSpellBlockPredicate(SpellBlockPredicate predicate) {
    SpellBlocker = predicate;
    OwnerStates[LegacyOwner].spellBlocker = predicate;
}

inline bool IsComboBlocked(int now) {
    return StrictAttackBlockActive || now < ComboBlockUntilTick;
}

inline bool AreSpellCastsBlocked(int now, int slot = -1) {
    if (SpellCastBypassDepth > 0) return false;

    for (std::size_t ownerSlot = 0;
         ownerSlot < OwnerCapacity;
         ++ownerSlot) {
        const OwnerState& owner = OwnerStates[ownerSlot];
        if (ownerSlot != LegacyOwner && !owner.acquired) continue;
        if (!owner.active && now >= owner.comboUntilTick) continue;
        const SpellBlockPredicate predicate =
            ownerSlot == LegacyOwner ? SpellBlocker : owner.spellBlocker;
        if (!predicate || slot < 0 || predicate(slot)) return true;
    }
    return false;
}

struct SpellCastBypassScope {
    SpellCastBypassScope() { ++SpellCastBypassDepth; }
    ~SpellCastBypassScope() { if (SpellCastBypassDepth > 0) --SpellCastBypassDepth; }
};

} // namespace CoreEvadeState
