#pragma once
#include <string>
#include <vector>

namespace ZDEvade {

enum class ZDSpellSlot {
    Q = 0,
    W,
    E,
    R,
    Unknown
};

enum class ZDSpellType {
    Line,
    Circular,
    Cone,
    Arc,
    Ring
};

enum class ZDCollisionObjectType {
    EnemyChampions,
    EnemyMinions,
    EnemyYasuoWall,
    Terrain
};

enum class CCType {
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

struct SpellData {
    std::string charName;
    int dangerlevel = 1;
    std::string name;
    std::string missileName;
    float projectileSpeed = 0.0f;
    float radius = 0.0f;
    float innerRadius = 0.0f;
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
    int multipleNumber = 1;
    float multipleAngle = 0.0f;
    int extraDelay = 0;

    bool hasTrap = false;
    std::string trapBaseName;
    std::vector<std::string> extraTrapNames;
    float trapRadius = 0.0f;
    int trapActivationDelay = 0;

    SpellData() = default;
};

} // namespace ZDEvade
