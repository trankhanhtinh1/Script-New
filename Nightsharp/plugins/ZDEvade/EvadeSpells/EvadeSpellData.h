#pragma once

// ============================================================================
// EvadeSpellData.h  —  C++ port of EzEvade's EvadeSpellData schema.
// ============================================================================
// Ported 1-1 from `EzEvade/EvadeSpells/EvadeSpellData.cs`.
// Contains the data struct + enums for spells the PLAYER can use to EVADE
// enemy skillshots (dash, blink, spell shield, movement speed buff, windwall).
// ============================================================================

#include <cstdint>
#include <string>
#include <vector>
#include <functional>
#include <unordered_map>

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

enum class HoldProtectionKind {
    None,
    SpeedBuff,
    Shield,
    Displacement,
    Untargetable,
    Invulnerable,
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

inline bool EvadeSpellNameEqualsNoCase(const std::string& left,
                                       const std::string& right) {
    if (left.empty() || right.empty() || left.size() != right.size())
        return false;
    for (std::size_t index = 0; index < left.size(); ++index) {
        unsigned char leftCharacter =
            static_cast<unsigned char>(left[index]);
        unsigned char rightCharacter =
            static_cast<unsigned char>(right[index]);
        if (leftCharacter >= 'A' && leftCharacter <= 'Z')
            leftCharacter = static_cast<unsigned char>(
                leftCharacter - 'A' + 'a');
        if (rightCharacter >= 'A' && rightCharacter <= 'Z')
            rightCharacter = static_cast<unsigned char>(
                rightCharacter - 'A' + 'a');
        if (leftCharacter != rightCharacter) return false;
    }
    return true;
}

inline bool StrictEvadeSpellNameMatches(
        const EvadeSpellData& data,
        const std::string& runtimeName,
        const std::string& runtimeScriptName,
        const std::string& runtimeIconName) {
    const std::string* runtimeNames[] = {
        &runtimeName,
        &runtimeScriptName,
        &runtimeIconName,
    };
    for (const std::string* runtime : runtimeNames) {
        if (EvadeSpellNameEqualsNoCase(*runtime, data.spellName) ||
            EvadeSpellNameEqualsNoCase(*runtime, data.name))
            return true;
    }
    return false;
}

inline HoldProtectionKind HoldProtectionFor(const EvadeSpellData& data) {
    if (data.untargetable) return HoldProtectionKind::Untargetable;
    switch (data.evadeType) {
    case EvadeType::Invulnerability:
        return HoldProtectionKind::Invulnerable;
    case EvadeType::Dash:
    case EvadeType::Blink:
        return HoldProtectionKind::Displacement;
    case EvadeType::Shield:
    case EvadeType::SpellShield:
    case EvadeType::WindWall:
        return HoldProtectionKind::Shield;
    case EvadeType::MovementSpeedBuff:
        return HoldProtectionKind::SpeedBuff;
    default:
        return HoldProtectionKind::None;
    }
}

struct EvadeSpellRule {
    bool enabled = true;
    int danger = 1;
    bool wardJump = true;
};

using EvadeSpellRuleMap = std::unordered_map<std::string, EvadeSpellRule>;
