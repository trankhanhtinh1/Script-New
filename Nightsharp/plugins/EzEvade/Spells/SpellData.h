#pragma once
#include "../../../SDK/SDK.h"

#include <limits>
#include <string>
#include <vector>

namespace EzEvade {

using SDK::SpellSlot;
using SDK::SpellType;

// Match ZDEvade CCType set (C# ezEvade gốc không có trường này, thêm mới để phân loại spell).
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
    Sleep,
};

enum class CollisionObjectType {
    EnemyChampions,
    EnemyMinions,
    YasuoWall,
};

class SpellData {
public:
    std::string charName;
    SpellSlot spellKey = SpellSlot::Q;
    int dangerlevel = 1;
    std::string spellName;
    std::string name;
    float range = 0.0f;
    float radius = 0.0f;
    float secondaryRadius = 0.0f;
    float projectileSpeed = std::numeric_limits<float>::max();
    std::string missileName = "";
    SpellType spellType = SpellType::Line;
    float spellDelay = 250;
    bool fixedRange = false;
    bool useEndPosition = false;
    float angle = 0.0f;
    float sideRadius = 0.0f;
    //public int splits; no idea when this was added xd
    bool usePackets = false;
    bool invert = false;
    float extraDelay = 0.0f;
    float extraDistance = 0.0f;
    bool isThreeWay = false;
    bool defaultOff = false;
    bool noProcess = false;
    bool isWall = false;
    bool isPerpendicular = false;
    float extraEndTime = 0.0f;
    bool hasEndExplosion = false;
    bool hasTrap = false;
    bool isSpecial = false;
    bool updatePosition = true;
    float extraDrawHeight = 0.0f;
    std::vector<std::string> extraSpellNames{};
    std::vector<std::string> extraMissileNames{};
    std::vector<CollisionObjectType> collisionObjects{};
    std::string trapBaseName = "";
    std::string trapTroyName = "";

    CCType ccType = CCType::None;

    SpellData() = default;

    SpellData(
        const std::string& charName,
        const std::string& spellName,
        const std::string& name,
        int range,
        int radius,
        int dangerlevel,
        SpellType spellType)
    {
        this->charName = charName;
        this->spellName = spellName;
        this->name = name;
        this->range = static_cast<float>(range);
        this->radius = static_cast<float>(radius);
        this->dangerlevel = dangerlevel;
        this->spellType = spellType;
    }

    SpellData Clone() const {
        return *this;
    }
};

} // namespace EzEvade
