#pragma once
#include <string>
#include <vector>

namespace Plugins::KuroEvade {

enum class KuroSpellSlot {
    Q = 0,
    W,
    E,
    R,
    Unknown
};

enum class KuroSpellType {
    Line,
    Circular,
    Cone,
    Arc
};

enum class KuroCollisionObjectType {
    EnemyChampions,
    EnemyMinions,
    EnemyYasuoWall,
    Terrain
};

enum class KuroCCType {
    None,
    Slow,
    Stun,
    Snare,
    KnockUp,
    KnockBack,
    Charm,
    Suppression,
    Silence,
    Terrain,
    Blind,
    Fear,
    Taunt,
    Sleep
};

namespace InternalDatabase {

using ZDSpellSlot = KuroSpellSlot;
using ZDSpellType = KuroSpellType;
using ZDCollisionObjectType = KuroCollisionObjectType;
using CCType = KuroCCType;

struct SpellData {
    std::string charName;
    int dangerlevel = 1;
    std::string name;
    std::string missileName;
    float projectileSpeed = 0.0f;
    float radius = 0.0f;
    float range = 0.0f;
    int spellDelay = 250;
    ZDSpellSlot spellKey = ZDSpellSlot::Unknown;
    std::string spellName;
    ZDSpellType spellType = ZDSpellType::Line;

    CCType ccType = CCType::None;

    std::vector<std::string> extraSpellNames;
    std::vector<std::string> extraMissileNames;
    std::vector<ZDCollisionObjectType> collisionObjects;

    bool fixedRange = false;
    bool isSpecial = false;
    bool defaultOff = false;
    bool noProcess = false;
    bool hasEndExplosion = false;
    bool useEndPosition = false;
    float secondaryRadius = 0.0f;
    int extraEndTime = 0;
    float angle = 0.0f;
    int extraDelay = 0;

    bool hasTrap = false;
    std::string trapBaseName;
};

} // namespace InternalDatabase

// ============================================================================
// KuroEvade Compatibility Wrapper
// ============================================================================
#include "../../../SDK/SDK.h"

struct SpellDataEntry {
    SDK::SpellDatabaseEntry sdk;
    bool Centered = false;
    bool CollisionExceptMini = false;
    std::string DisplayName;
    bool FollowCaster = false;
    bool Invert = false;
    bool IsPerpendicular = false;
    bool IsHorizontal = false;
    bool IsSpecialIgnore = false;
    bool NoTarget = false;
    bool UsePacket = false;
    bool UseEndPosition = false;
    bool HasTrap = false;
    std::string TrapBaseName;
    std::string TrapTroyName;
    bool DisabledByDefault = false;
    int SecondaryRadius = 0;
    int MultipleNumber = -1;
    float MultipleAngle = 0.0f;
    float ExtraEndTime = 0.0f;
    float ExtraDrawHeight = 0.0f;
};

using SpellData = SpellDataEntry;

namespace Generated {
    using SpellDataEntry = Plugins::KuroEvade::SpellDataEntry;
}

} // namespace Plugins::KuroEvade
