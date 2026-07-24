#pragma once
#include <cmath>
#include <limits>
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

enum class MissileRouteMode {
    Straight,
    Steering
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
    MissileRouteMode missileRouteMode = MissileRouteMode::Straight;

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
    float coneAngleDegrees = 0.0f;
    float coneEdgePadding = 0.0f;
    int extraDelay = 0;

    // Arcs remain disabled until their authored circular path is explicitly
    // represented. These fields deliberately have no inferred/chord fallback.
    bool arcSupported = false;
    float arcCenterX = std::numeric_limits<float>::quiet_NaN();
    float arcCenterY = std::numeric_limits<float>::quiet_NaN();
    float arcRadius = std::numeric_limits<float>::quiet_NaN();
    float arcStartAngleDegrees = std::numeric_limits<float>::quiet_NaN();
    float arcSweepAngleDegrees = std::numeric_limits<float>::quiet_NaN();

    bool hasTrap = false;
    std::string trapBaseName;

    int collisionTargetLimit = 1;
    bool endExplosionRequiresCollision = false;
    bool endExplosionRequiresUnitCollision = false;
    float endExplosionMinimumTravelDistance = 0.0f;
    int endExplosionDelay = 0;
    int endExplosionDuration = 0;
    bool endExplosionAtUnitCenter = false;
    bool endExplosionFollowsUnit = false;
    bool endExplosionDetonatesOnUnitDeath = false;
    bool endExplosionOnProjectileWall = false;
    float endExplosionCenterOffset = 0.0f;

    bool HasValidArcGeometry() const {
        return arcSupported &&
            std::isfinite(arcCenterX) &&
            std::isfinite(arcCenterY) &&
            std::isfinite(arcRadius) &&
            arcRadius > 0.0f &&
            std::isfinite(arcStartAngleDegrees) &&
            std::isfinite(arcSweepAngleDegrees) &&
            std::fabs(arcSweepAngleDegrees) > 0.01f &&
            std::fabs(arcSweepAngleDegrees) <= 360.0f;
    }

    bool HasValidGeometryFields() const {
        if (!std::isfinite(radius) || radius < 0.0f ||
            !std::isfinite(range) || range < 0.0f)
            return false;
        if (hasEndExplosion &&
            (!std::isfinite(secondaryRadius) || secondaryRadius < 0.0f ||
             !std::isfinite(endExplosionMinimumTravelDistance) ||
             endExplosionMinimumTravelDistance < 0.0f ||
             !std::isfinite(endExplosionCenterOffset)))
            return false;
        switch (spellType) {
        case ZDSpellType::Ring:
            return std::isfinite(innerRadius) &&
                innerRadius >= 0.0f &&
                innerRadius <= radius;
        case ZDSpellType::Cone:
            return std::isfinite(coneAngleDegrees) &&
                coneAngleDegrees > 0.0f &&
                coneAngleDegrees <= 360.0f &&
                std::isfinite(coneEdgePadding) &&
                coneEdgePadding >= 0.0f;
        case ZDSpellType::Arc:
            return HasValidArcGeometry();
        case ZDSpellType::Line:
        case ZDSpellType::Circular:
            return true;
        }
        return false;
    }

    SpellData() = default;
};

} // namespace ZDEvade
