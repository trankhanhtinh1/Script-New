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

namespace Plugins::KuroEvade {

enum class KuroEvadeSpellSlot {
    Q = 0,
    W = 1,
    E = 2,
    R = 3,
    Recall = 4,
};

enum class KuroEvadeCastType {
    Position,
    Target,
    Self,
};

enum class KuroEvadeSpellTargets {
    AllyMinions,
    EnemyMinions,
    AllyChampions,
    EnemyChampions,
    Targetables,
};

enum class KuroEvadeType {
    Blink,
    Dash,
    Invulnerability,
    MovementSpeedBuff,
    Shield,
    SpellShield,
    WindWall,
};

namespace InternalDatabase {

using EvadeSpellSlot = KuroEvadeSpellSlot;
using EvadeCastType = KuroEvadeCastType;
using EvadeSpellTargets = KuroEvadeSpellTargets;
using EvadeType = KuroEvadeType;

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

} // namespace InternalDatabase
} // namespace Plugins::KuroEvade

// ============================================================================
// KuroEvade Compatibility Wrapper
// ============================================================================
#include "CastType.h"
#include "EvadeType.h"
#include "MoveSpeedAmount.h"
#include "SpellTargets.h"
#include "UseSpellFunc.h"

#include "../../../SDK/SDK.h"

namespace Plugins::KuroEvade {

struct EvadeSpellData {
    CastType CastTypeValue = CastType::Position;
    std::string ChampionName;
    std::string CheckSpellName;
    int DangerLevel = 0;
    int Delay = 0;
    EvadeType EvadeTypeValue = EvadeType::Blink;
    bool FixedRange = false;
    bool IsBehindTarget = false;
    bool IsInfrontTarget = false;
    bool IsItem = false;
    bool IsReversed = false;
    bool IsSpecial = false;
    bool IsSummonerSpell = false;
    int ItemId = 0;
    int MaxRange = 0;
    int MinRange = 0;
    bool HasMoveSpeedAmount = false;
    MoveSpeedAmount MoveSpeedTotalAmount;
    std::string Name;
    SDK::SpellSlot Slot = SDK::SpellSlot::Unknown;
    int Speed = 0;
    UseSpellFunc UseSpell;
    std::vector<SpellTargets> ValidTargets;
    bool IsEnabled = true;
    int IndexUseWhen = 2;
};

} // namespace Plugins::KuroEvade
