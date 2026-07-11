#pragma once

// ============================================================================
// EvadeSpellData.h - evade spell data schema.
// ============================================================================
// Contains the data struct + enums for player evade spells.
// ============================================================================

#include <cstdint>
#include <string>
#include <vector>
#include <functional>

// ── SpellSlot enum (matches LeagueSharp/EnsoulSharp) ────────────────────────
enum class EvadeSpellSlot {
    Q = 0,
    W = 1,
    E = 2,
    R = 3,
    Recall = 4,
};

// ── Cast type: how the evade spell is targeted ───────────────────────────────
enum class EvadeCastType {
    Position,
    Target,
    Self,
};

// ── Valid target types for CastType::Target spells ───────────────────────────
enum class EvadeSpellTargets {
    AllyMinions,
    EnemyMinions,
    AllyChampions,
    EnemyChampions,
    Targetables,
};

// ── Evade category ───────────────────────────────────────────────────────────
enum class EvadeType {
    Blink,
    Dash,
    Invulnerability,
    MovementSpeedBuff,
    Shield,
    SpellShield,
    WindWall,
};

// ── EvadeSpellData struct (1-1 mapping from C#) ──────────────────────────────
struct EvadeSpellData {
    std::string charName;
    EvadeSpellSlot spellKey = EvadeSpellSlot::Q;
    int dangerlevel = 1;
    std::string spellName;
    std::string name;
    bool checkSpellName = false;
    float spellDelay = 250.0f;
    float range = 0.0f;
    float speed = 0.0f;
    std::vector<float> speedArray;
    bool fixedRange = false;
    EvadeType evadeType = EvadeType::Blink;
    bool isReversed = false;
    bool behindTarget = false;
    bool infrontTarget = false;
    bool isSummonerSpell = false;
    bool isItem = false;
    int itemID = 0;
    EvadeCastType castType = EvadeCastType::Position;
    std::vector<EvadeSpellTargets> spellTargets;
    bool isSpecial = false;
    bool untargetable = false;

    EvadeSpellData() = default;

    EvadeSpellData(const std::string& charName, const std::string& name,
                   EvadeSpellSlot spellKey, EvadeType evadeType, int dangerlevel)
        : charName(charName), name(name), spellKey(spellKey),
          evadeType(evadeType), dangerlevel(dangerlevel) {}
};
