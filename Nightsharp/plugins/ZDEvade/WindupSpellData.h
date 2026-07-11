#pragma once

// ============================================================================
// WindupSpellData.h - spell windup/cast delay data.
// ============================================================================
// Simple struct for spell windup/cast delay tracking.
// ============================================================================

#include <string>

// ── SpellSlot enum (shared with EvadeSpellData.h) ───────────────────────────
// Redefined here for standalone compilation.
enum class WindupSpellSlot {
    Q = 0,
    W = 1,
    E = 2,
    R = 3,
};

// ── WindupSpellData struct (1-1 mapping from C# SpellData) ───────────────────
struct WindupSpellData {
    std::string charName;
    std::string name;
    float spellDelay = 250.0f; // default 250ms in C# when not specified
    WindupSpellSlot spellKey = WindupSpellSlot::Q;
    std::string spellName;

    WindupSpellData() = default;
};
