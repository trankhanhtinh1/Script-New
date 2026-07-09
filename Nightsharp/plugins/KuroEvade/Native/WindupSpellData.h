#pragma once

// ============================================================================
// WindupSpellData.h  —  C++ port of EzEvade's SpellWindupDatabase SpellData.
// ============================================================================
// Simple struct for spell windup/cast delay tracking.
// Ported 1-1 from `EzEvade/Spells/SpellWindupDatabase.cs`.
// ============================================================================

#include <string>

namespace Plugins::KuroEvade::ZD {

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

} // namespace Plugins::KuroEvade::ZD
