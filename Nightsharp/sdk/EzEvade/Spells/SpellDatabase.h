#pragma once
#include "SpellData.h"
#include <algorithm>
#include <string>
#include <vector>
#include <unordered_map>

// ============================================================================
// EzEvade SpellDatabase — Offensive skillshot & targeted spell database
//
// Ported from SpellDatabase.lua for NightSharp Evade system.
// Patch target: 26.5 (2026-03-03)
//
// Usage:
//   auto& db = EzEvade::GetSpellDatabase();
//   auto spells = EzEvade::GetSpellsForChampion("Morgana");
//   auto* spell = EzEvade::GetSpellByName("MorganaQ");
// ============================================================================

namespace EzEvade {

// Default evade percentages matching SpellDatabase.lua menu_settings
namespace EvadeDefaults {
    constexpr int All          = 100;  // menu_settings_all
    constexpr int OnlyKillMe   = 0;    // menu_settings_only_kill_me
    constexpr int Pct50        = 50;
    constexpr int Pct40        = 40;
    constexpr int Pct30        = 30;
    constexpr int Pct20        = 20;
    constexpr int Disabled     = -1;   // menu_settings_disabled
}

// ============================================================================
// Compact builder helpers for inline data
// ============================================================================
namespace detail {

inline bool EqualsIgnoreCase(const std::string& left, const std::string& right) {
    if (left.size() != right.size()) {
        return false;
    }

    for (size_t i = 0; i < left.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(left[i])) !=
            std::tolower(static_cast<unsigned char>(right[i]))) {
            return false;
        }
    }

    return true;
}

inline SpellData MakeSkillshot(
    const char* champ, const char* name, const char* display,
    SkillshotType type, bool missile, bool cc, int danger, int evadePct = 0)
{
    SpellData d;
    d.charName       = champ;
    d.spellName      = name;
    d.displayName    = display;
    d.type           = type;
    d.isMissile      = missile;
    d.isCC           = cc;
    d.dangerLevel    = danger;
    d.defaultEvadePct = evadePct;
    d.defaultEnabled = (evadePct >= 0);
    return d;
}

// Shorthand aliases
inline SpellData SS(const char* c, const char* n, const char* d,
    bool mis, bool cc, int dng, int pct = 0)
{
    return MakeSkillshot(c, n, d, SkillshotType::Line, mis, cc, dng, pct);
}

} // namespace detail

// ============================================================================
// Build the full spell database
// Defined in SpellDatabase_Skillshots.inl and SpellDatabase_Targeted.inl
// ============================================================================
static std::vector<SpellData> BuildSpellDatabase() {
    using namespace detail;
    using namespace EvadeDefaults;
    std::vector<SpellData> db;
    db.reserve(800);

    // ==== Macros for ultra-compact entry ====
    // S(champ, name, display, missile, cc, danger, evade%)
    #define S(C,N,D,M,CC,DNG,PCT) db.push_back(SS(C,N,D,M,CC,DNG,PCT));

    // Include generated data files
    #include "SpellDatabase_Skillshots.inl"
    #include "SpellDatabase_Targeted.inl"

    #undef S

    // Sort by champion name then spell name
    std::stable_sort(db.begin(), db.end(), [](const SpellData& a, const SpellData& b) {
        if (a.charName != b.charName) return a.charName < b.charName;
        return a.spellName < b.spellName;
    });

    return db;
}

// ============================================================================
// Global database access
// ============================================================================
inline std::vector<SpellData>& GetSpellDatabase() {
    static std::vector<SpellData> db = BuildSpellDatabase();
    return db;
}

// Get all spells for a champion
inline std::vector<const SpellData*> GetSpellsForChampion(const std::string& champName) {
    std::vector<const SpellData*> result;
    for (auto& s : GetSpellDatabase()) {
        if (detail::EqualsIgnoreCase(s.charName, champName))
            result.push_back(&s);
    }
    return result;
}

// Get spell by internal name
inline const SpellData* GetSpellByName(const std::string& spellName) {
    for (auto& s : GetSpellDatabase()) {
        if (detail::EqualsIgnoreCase(s.spellName, spellName))
            return &s;
        for (auto& extra : s.extraSpellNames) {
            if (detail::EqualsIgnoreCase(extra, spellName)) return &s;
        }
    }
    return nullptr;
}

// Get spell by missile name
inline const SpellData* GetSpellByMissile(const std::string& missileName) {
    for (auto& s : GetSpellDatabase()) {
        if (detail::EqualsIgnoreCase(s.missileSpellName, missileName))
            return &s;
        if (s.isMissile && detail::EqualsIgnoreCase(s.spellName, missileName))
            return &s;
        for (auto& m : s.extraMissileNames) {
            if (detail::EqualsIgnoreCase(m, missileName)) return &s;
        }
    }
    return nullptr;
}

// Get all dangerous (CC) spells
inline std::vector<const SpellData*> GetDangerousSpells() {
    std::vector<const SpellData*> result;
    for (auto& s : GetSpellDatabase()) {
        if (s.isCC || s.dangerLevel >= 4)
            result.push_back(&s);
    }
    return result;
}

// Check if a spell name exists in the database
inline bool IsKnownSpell(const std::string& spellName) {
    return GetSpellByName(spellName) != nullptr;
}

} // namespace EzEvade
