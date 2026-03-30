#pragma once

#include <cmath>
#include <string>
#include <vector>

namespace SDK {
namespace Data {

enum class SpellType {
    Line,
    Circular,
    Cone,
    Arc,
    None
};

enum class SkillshotType {
    Line,
    MissileLine,
    Circle,
    Cone,
    Ring,
    Arc,
    MissileArc,
    None
};

enum class SpellSlot {
    Q, W, E, R, Unknown
};

enum CollisionObjectType {
    EnemyChampions,
    EnemyMinions,
    YasuoWall
};

enum CollisionFlags {
    CollisionNone = 0,
    CollisionMinions = 1,
    CollisionChampions = 2,
    CollisionYasuoWall = 4
};

struct SpellData {
    std::string charName;
    SpellSlot spellKey = SpellSlot::Q;
    int dangerlevel = 1;
    std::string spellName;
    std::string name;

    float range = 0.0f;
    float extraRange = 0.0f;
    float radius = 0.0f;
    float secondaryRadius = 0.0f;
    float projectileSpeed = 3.402823466e+38F;
    float speed = 0.0f;
    float angle = 0.0f;
    float sideRadius = 0.0f;

    std::string missileName = "";
    std::string missileSpellName = "";
    std::string displayName = "";

    SpellType spellType = SpellType::None;
    SkillshotType type = SkillshotType::Line;

    float spellDelay = 250.0f;
    float castDelay = 0.25f;
    float extraDelay = 0.0f;
    float extraEndTime = 0.0f;

    bool fixedRange = false;
    bool useEndPosition = false;
    bool usePackets = false;
    bool invert = false;
    float extraDistance = 0.0f;
    bool isThreeWay = false;
    bool defaultOff = false;
    bool noProcess = false;
    bool isWall = false;
    bool isPerpendicular = false;
    bool hasEndExplosion = false;
    bool hasTrap = false;
    bool isSpecial = false;
    bool updatePosition = true;
    float extraDrawHeight = 0.0f;

    bool isMissile = false;
    bool isCC = false;
    int dangerLevel = 1;
    int defaultEvadePct = 0;
    bool defaultEnabled = true;
    bool canBeRemoved = false;
    int collisionObjectsMask = CollisionNone;

    std::vector<std::string> extraSpellNames;
    std::vector<std::string> extraMissileNames;
    std::vector<CollisionObjectType> collisionObjects;
    std::string trapBaseName = "";
    std::string trapTroyName = "";

    bool IsSkillshot() const {
        return spellType != SpellType::None || type != SkillshotType::None;
    }

    bool CollidesWithChampions() const {
        if (collisionObjectsMask & CollisionChampions) return true;
        for (auto obj : collisionObjects) {
            if (obj == EnemyChampions) return true;
        }
        return false;
    }

    bool CollidesWithMinions() const {
        if (collisionObjectsMask & CollisionMinions) return true;
        for (auto obj : collisionObjects) {
            if (obj == EnemyMinions) return true;
        }
        return false;
    }

    bool CanBeWindWalled() const {
        if (collisionObjectsMask & CollisionYasuoWall) return true;
        for (auto obj : collisionObjects) {
            if (obj == YasuoWall) return true;
        }
        return false;
    }
};

} // namespace Data
} // namespace SDK
