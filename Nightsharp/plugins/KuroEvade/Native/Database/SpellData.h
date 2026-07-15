#pragma once

#include "ProjectileWallDatabase.h"

#include "../../../../SDK/Wrappers/Spells/Database/SpellDatabaseEntry.h"

#include <algorithm>
#include <cmath>
#include <climits>
#include <string>
#include <vector>

namespace Plugins::KuroEvade::Database {

enum class SpellSlot {
    Q,
    W,
    E,
    R,
    Unknown,
};

// Names intentionally follow the supplied Evade SpellData model. The
// database owns these values; SDK::SpellDatabaseEntry is built only at the
// NightSharp integration boundary.
enum class SkillShotType {
    SkillshotLine,
    SkillshotCircle,
    SkillshotCone,
    SkillshotArc,
};

enum class CollisionObjectType {
    EnemyChampions,
    EnemyMinions,
    EnemyLargeMonsters,
    EnemyYasuoWall,
    Terrain,
};

enum class CrowdControlType {
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

struct SpellData {
    std::string CharacterName;
    int DangerValue = 1;
    std::string DisplayName;
    std::string MissileSpellName;
    float MissileSpeed = 0.0f;
    float Radius = 0.0f;
    float Range = 0.0f;
    int Delay = 250;
    SpellSlot Slot = SpellSlot::Unknown;
    std::string SpellName;
    SkillShotType Type = SkillShotType::SkillshotLine;
    CrowdControlType CrowdControl = CrowdControlType::None;

    std::vector<std::string> ExtraSpellNames;
    std::vector<std::string> ExtraMissileNames;
    std::vector<CollisionObjectType> CollisionObjects;

    // Number of unit collisions the projectile can consume before it stops.
    // Most projectiles stop on the first target; Lux Q is the canonical
    // two-target projectile. Projectile walls and terrain are always terminal.
    int CollisionTargetLimit = 1;

    // Process-spell and missile detections can expose different spell names
    // for the same cast (for example normal/accelerated Jayce Q). Variants in
    // the same group are merged when the real missile becomes available.
    std::string DetectionGroup;

    // Some projectiles transform instead of disappearing on their first unit
    // collision. They then travel either a relative continuation distance
    // (Bard Q) or to an absolute range (Lissandra Q).
    float CollisionInitialRange = 0.0f;
    float CollisionContinuationDistance = 0.0f;
    float CollisionContinuationRange = 0.0f;
    float CollisionContinuationRadius = 0.0f;
    bool CollisionContinuationStopsOnSecondTarget = false;
    bool CollisionContinuationStopsOnTerrain = false;

    // Some projectiles stop on a unit but their damaging payload lands farther
    // along the cast direction (Milio Q). Champion and non-champion targets can
    // use different bounce distance, explosion radius and delay.
    float CollisionBounceDistance = 0.0f;
    float CollisionBounceDistanceNonChampion = 0.0f;
    float EndExplosionRadiusNonChampion = 0.0f;
    int EndExplosionDelayNonChampion = -1;

    bool FixedRange = false;
    bool IsDangerous = false;
    bool IsSpecial = false;
    bool DisabledByDefault = false;
    bool DontProcess = false;
    bool HasEndExplosion = false;
    // Collision-only payloads such as Lillia's rolling Swirlseed can detonate
    // on units or terrain but have no natural maximum-range explosion.
    bool EndExplosionRequiresCollision = false;
    // Some projectiles only create their area damage after hitting a unit
    // (Corki R, Jinx R, Karma Q).  They must not expose a false danger circle
    // at maximum range while no valid collision has been predicted.
    bool EndExplosionRequiresUnitCollision = false;
    // Conditional payloads such as empowered Sejuani R only exist after the
    // projectile has travelled far enough.
    float EndExplosionMinimumTravelDistance = 0.0f;
    // Delay from projectile impact to the secondary area becoming dangerous.
    int EndExplosionDelay = 0;
    // Time the secondary area remains hazardous after becoming active.
    int EndExplosionDuration = 0;
    // Distance-scaled endpoint payloads (Fizz R) use SecondaryRadius below
    // the first threshold and promote through these two tiers.
    float EndExplosionMediumTravelDistance = 0.0f;
    float EndExplosionFarTravelDistance = 0.0f;
    float EndExplosionRadiusMedium = 0.0f;
    float EndExplosionRadiusFar = 0.0f;
    // Center the payload on the struck unit rather than the missile's surface
    // contact point. This does not imply that the area follows a moving unit.
    bool EndExplosionAtUnitCenter = false;
    // Delayed attachments such as Senna W keep following their struck unit
    // until detonation; impact fields such as Sejuani R remain fixed.
    bool EndExplosionFollowsUnit = false;
    bool EndExplosionDetonatesOnUnitDeath = false;
    // Most projectile-destroying walls erase the payload. A small number of
    // spells explicitly detonate when the rolling projectile is destroyed.
    bool EndExplosionOnProjectileWall = false;
    // Collision payloads such as Kayle Q expand into a center disc plus four
    // directional capsules rather than a single circle.
    bool EndExplosionCross = false;
    float EndExplosionCenterOffset = 0.0f;
    float EndExplosionForwardLength = 0.0f;
    float EndExplosionBackwardLength = 0.0f;
    float EndExplosionSideLength = 0.0f;
    float EndExplosionLongitudinalRadius = 0.0f;
    float EndExplosionSideRadius = 0.0f;
    bool UseEndPosition = false;
    float SecondaryRadius = 0.0f;
    int ExtraEndTime = 0;
    float MultipleAngle = 0.0f;
    int ExtraDelay = 0;
    bool HasTrap = false;
    std::string TrapBaseName;

    // KuroEvade-only adapters needed by special spells and the ImGui drawer.
    bool Centered = false;
    bool CollisionExceptMini = false;
    bool FollowCaster = false;
    bool Invert = false;
    bool IsPerpendicular = false;
    bool IsHorizontal = false;
    bool NoTarget = false;
    bool UsePacket = false;
    std::string TrapTroyName;
    int MultipleNumber = -1;
    float ExtraDrawHeight = 0.0f;

    SDK::SpellDatabaseEntry Runtime;

    void Finalize() {
        Runtime = {};
        Runtime.ChampionName = CharacterName;
        Runtime.SpellName = SpellName;
        Runtime.MissileSpellName = MissileSpellName;
        Runtime.DangerValue = DangerValue;
        Runtime.Delay = Delay;
        Runtime.Radius = static_cast<int>(std::lround(Radius));
        Runtime.Range = static_cast<int>(std::lround(Range));
        Runtime.Width = Runtime.Radius;
        Runtime.MissileSpeed = MissileSpeed > 0.0f
            ? static_cast<int>(std::lround(MissileSpeed))
            : INT_MAX;
        Runtime.FixedRange = FixedRange;
        // Entries backed by a real missile must react to its authoritative
        // delete event. Endpoint-area profiles can then retain only their
        // secondary geometry instead of keeping a stale SDK missile handle.
        Runtime.CanBeRemoved = !MissileSpellName.empty() ||
            !ExtraMissileNames.empty();
        Runtime.ExtraSpellNames = ExtraSpellNames;
        Runtime.ExtraMissileNames = ExtraMissileNames;
        Runtime.Angle = MultipleAngle > 0.0f
            ? static_cast<int>(std::lround(MultipleAngle))
            : 0;
        Runtime.ArcAngle = Runtime.Angle;

        switch (Slot) {
        case SpellSlot::Q: Runtime.Slot = SDK::SpellSlot::Q; break;
        case SpellSlot::W: Runtime.Slot = SDK::SpellSlot::W; break;
        case SpellSlot::E: Runtime.Slot = SDK::SpellSlot::E; break;
        case SpellSlot::R: Runtime.Slot = SDK::SpellSlot::R; break;
        default: Runtime.Slot = SDK::SpellSlot::Unknown; break;
        }

        switch (Type) {
        case SkillShotType::SkillshotCircle:
            Runtime.SpellType = SDK::SpellType::SkillshotCircle;
            break;
        case SkillShotType::SkillshotCone:
            Runtime.SpellType = SDK::SpellType::SkillshotCone;
            break;
        case SkillShotType::SkillshotArc:
            Runtime.SpellType = SDK::SpellType::SkillshotMissileLine;
            break;
        default:
            Runtime.SpellType = SDK::SpellType::SkillshotLine;
            break;
        }

        const auto addCollisionObject = [&](SDK::CollisionableObjects object) {
            if (std::find(Runtime.CollisionObjects.begin(),
                          Runtime.CollisionObjects.end(), object) ==
                Runtime.CollisionObjects.end()) {
                Runtime.CollisionObjects.push_back(object);
            }
        };

        for (const CollisionObjectType object : CollisionObjects) {
            switch (object) {
            case CollisionObjectType::EnemyChampions:
                addCollisionObject(SDK::CollisionableObjects::Heroes);
                break;
            case CollisionObjectType::EnemyMinions:
                addCollisionObject(SDK::CollisionableObjects::Minions);
                break;
            case CollisionObjectType::EnemyLargeMonsters:
                // NightSharp exposes monsters through the Minions runtime
                // bucket; KuroEvade keeps the authored distinction and
                // filters JungleType inside SourceCollision.
                addCollisionObject(SDK::CollisionableObjects::Minions);
                break;
            case CollisionObjectType::EnemyYasuoWall:
                // Keep the legacy authored name for database compatibility,
                // but all three effects intercept hostile projectiles. Mel's
                // barrier reflects instead of destroying; ownership of the
                // recreated missile is handled by SkillshotDetector.
                addCollisionObject(SDK::CollisionableObjects::YasuoWall);
                addCollisionObject(SDK::CollisionableObjects::SamiraWall);
                addCollisionObject(SDK::CollisionableObjects::MelWall);
                break;
            case CollisionObjectType::Terrain:
                addCollisionObject(SDK::CollisionableObjects::Walls);
                break;
            }
        }
        const bool projectileWallBlocked =
            IsProjectileWallBlocked(MissileSpellName) ||
            std::any_of(ExtraMissileNames.begin(), ExtraMissileNames.end(),
                [](const std::string& name) {
                    return IsProjectileWallBlocked(name);
                });
        if (projectileWallBlocked) {
            addCollisionObject(SDK::CollisionableObjects::YasuoWall);
            addCollisionObject(SDK::CollisionableObjects::SamiraWall);
            addCollisionObject(SDK::CollisionableObjects::MelWall);
        }
    }

    bool MatchesChampion(const char* name) const {
        return name && _stricmp(CharacterName.c_str(), name) == 0;
    }
};

} // namespace Plugins::KuroEvade::Database
