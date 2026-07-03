#pragma once
// ============================================================================
// SpellData.h  —  C++ port of EnsoulSharp.SDK's `SpellDatabaseEntry` schema.
// ============================================================================
// Maps the full 44-property C# record at
//   `Core/Wrappers/Spells/Database/SpellDatabaseEntry.cs`
// into a single POD-ish struct the skillshot/evade/prediction layers can read
// without any JSON runtime. Every entry in `Database.h` is initialised via a
// lambda that stamps fields onto this struct in the same style Nightsharp
// has been using since the first port (`Spells.push_back([](){ SpellData d; ...; return d; }())`).
//
// Design rules
// ------------
//  * Backward compatible — all legacy field names from the original 317-entry
//    `Database.h` (`charName`, `spellKey`, `dangerlevel`, `radius`, `spellDelay`
//    …) are preserved exactly as-is.  New EnsoulSharp-sourced properties live
//    alongside them with the C# naming kept (camelCase first-letter-lower) so
//    the CommunityDragon-driven generator that will repopulate `Database.h`
//    has a 1:1 mapping from the C# source to the C++ field.
//
//  * No STL surprises in hot code — everything is either a scalar, a
//    `std::string`, a `std::vector<std::string>`, or one of the bitmask enums
//    defined just below.  The struct stays copyable/moveable; the entire
//    database is held in a `std::vector<SpellData>` built once at first use.
//
//  * Enum alias strategy —
//      - `SpellType` keeps its legacy values (`Line/Circular/Cone/Arc/None`)
//        so every current entry still compiles.  A richer `SkillshotType`
//        enum layered on top (`MissileLine`, `MissileArc`, `Ring`, …) captures
//        the extra variants EnsoulSharp expresses through
//        `SpellDatabaseEntry.SpellType`.
//      - `CastType` and `SpellTags` are ported as *flags* (`uint32_t`
//        bitmasks) so a single integer per entry replaces the C# arrays.
//      - `CollisionableObjects` is ported as flags too (matching the
//        `[Flags]` attribute EnsoulSharp already puts on the C# enum).
// ============================================================================

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "../Enumerations/SpellSlot.h"

namespace SDK {
namespace Data {

// ── Geometric skillshot shape (legacy Nightsharp enum) ──────────────────────
enum class SpellType {
    Line,
    Circular,
    Cone,
    Arc,
    None
};

// ── Richer shape classifier (missile vs. non-missile, ring/arc variants) ────
// Set independently from `SpellType` so the legacy 317-entry dataset keeps
// working while new EnsoulSharp-sourced entries can describe missile-based
// vs. instant skillshots precisely.
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

// ── Spell slot letter ───────────────────────────────────────────────────────
using SpellSlot = SDK::SpellSlot;

// ── `CollisionableObjects` — EnsoulSharp port ───────────────────────────────
// Ported from `Core/Enumerations/CollisionableObjects.cs`.  C# marks the enum
// `[Flags]`; we do the same here.  The legacy `EnemyChampions / EnemyMinions /
// YasuoWall` values are ALIASED below so every existing `d.collisionObjects =
// { EnemyChampions, EnemyMinions }` literal in `Database.h` still compiles.
enum CollisionableObjects : uint32_t {
    Collision_None             = 0,
    Collision_Minions          = 1u << 0,  // `Minions`
    Collision_Heroes           = 1u << 1,  // `Heroes` (enemy champions)
    Collision_YasuoWall        = 1u << 2,  // `YasuoWall`
    Collision_BraumShield      = 1u << 3,  // `BraumShield` (partial block)
    Collision_Walls            = 1u << 4,  // Terrain walls (pathing)
    Collision_Turrets          = 1u << 5,
    Collision_AllyMinions      = 1u << 6,
    Collision_AllyChampions    = 1u << 7,
    Collision_SamiraWall       = 1u << 8,
    Collision_MelWall          = 1u << 9
};

// Legacy aliases — keep old enum-name literals that Database.h uses.
// (`CollisionObjectType` historically carried UNPREFIXED names.)
enum CollisionObjectType : uint32_t {
    EnemyChampions = Collision_Heroes,
    EnemyMinions   = Collision_Minions,
    YasuoWall      = Collision_YasuoWall,
    BraumShield    = Collision_BraumShield,
    Walls          = Collision_Walls,
    AllyMinions    = Collision_AllyMinions,
    AllyChampions  = Collision_AllyChampions,
    SamiraWall     = Collision_SamiraWall,
    MelWall        = Collision_MelWall
};

// Legacy bit-flag enum (Nightsharp pre-port). Kept so existing
// `d.collisionObjectsMask = CollisionMinions | CollisionChampions` lines work.
enum SpellCollisionFlags : uint32_t {
    CollisionNone       = 0,
    CollisionMinions    = Collision_Minions,
    CollisionChampions  = Collision_Heroes,
    CollisionYasuoWall  = Collision_YasuoWall,
    CollisionBraumShield= Collision_BraumShield,
    CollisionWalls      = Collision_Walls,
    CollisionTurrets    = Collision_Turrets,
    CollisionSamiraWall = Collision_SamiraWall,
    CollisionMelWall    = Collision_MelWall
};

// ── `CastType` — EnsoulSharp port (bitmask) ─────────────────────────────────
// Source: `Core/Enumerations/CastTypes.cs`.  C# uses a `CastType[]`, we pack
// the same information into a single `uint32_t` bitmask.
enum CastTypeFlags : uint32_t {
    CastType_None            = 0,
    CastType_EnemyChampions  = 1u << 0,
    CastType_EnemyMinions    = 1u << 1,
    CastType_EnemyTurrets    = 1u << 2,
    CastType_AllyChampions   = 1u << 3,
    CastType_AllyMinions     = 1u << 4,
    CastType_AllyTurrets     = 1u << 5,
    CastType_Self            = 1u << 6,
    CastType_Position        = 1u << 7,
    CastType_Direction       = 1u << 8
};

// ── `SpellTags` — EnsoulSharp port (bitmask) ────────────────────────────────
// Source: `Core/Enumerations/SpellTags.cs`.  The C# enum is a plain enum
// (not `[Flags]`) but EnsoulSharp stores it as an array of tags; the bitmask
// representation is semantically equivalent and cheaper to test.
enum SpellTagsFlags : uint32_t {
    Tag_None                 = 0,
    Tag_Damage               = 1u << 0,
    Tag_AoE                  = 1u << 1,
    Tag_AppliesOnHitEffects  = 1u << 2,
    Tag_CrowdControl         = 1u << 3,
    Tag_Stun                 = 1u << 4,
    Tag_Slow                 = 1u << 5,
    Tag_Snare                = 1u << 6,
    Tag_Root                 = 1u << 7,
    Tag_Charm                = 1u << 8,
    Tag_Taunt                = 1u << 9,
    Tag_Fear                 = 1u << 10,
    Tag_Silence              = 1u << 11,
    Tag_Suppression          = 1u << 12,
    Tag_Airborne             = 1u << 13,
    Tag_Knockup              = 1u << 14,
    Tag_Knockback            = 1u << 15,
    Tag_Blind                = 1u << 16,
    Tag_Heal                 = 1u << 17,
    Tag_Shield               = 1u << 18,
    Tag_Buff                 = 1u << 19,
    Tag_Debuff               = 1u << 20,
    Tag_Dash                 = 1u << 21,
    Tag_Blink                = 1u << 22,
    Tag_Channeled            = 1u << 23,
    Tag_Targeted             = 1u << 24,
    Tag_Skillshot            = 1u << 25,
    Tag_Ultimate             = 1u << 26,
    Tag_Passive              = 1u << 27,
    Tag_Toggle               = 1u << 28,
    Tag_Charged              = 1u << 29,
    Tag_Trap                 = 1u << 30,
    Tag_Wall                 = 1u << 31
};

// ============================================================================
// SpellData — full record
// ============================================================================
struct SpellData {
    // ── Identity (legacy + EnsoulSharp mapping) ─────────────────────────────
    //   charName       ↔  ChampionName
    //   spellKey       ↔  Slot
    //   spellName      ↔  SpellName
    //   name           (display-friendly label — NS-only)
    //   missileName    ↔  MissileSpellName (NS's `missileSpellName` retained
    //                      for backward compat)
    //   displayName    ↔  (NS-only legacy)
    std::string charName;
    SpellSlot   spellKey = SpellSlot::Q;
    int         dangerlevel = 1;     // legacy alias, mirrors `dangerValue`
    std::string spellName;
    std::string name;

    // ── Geometry ────────────────────────────────────────────────────────────
    //   range              ↔  Range
    //   extraRange         ↔  ExtraRange
    //   radius             ↔  Radius
    //   secondaryRadius    (NS-only)
    //   projectileSpeed    ↔  MissileSpeed  (legacy float name preserved)
    //   speed              (NS-only, non-missile movement speed)
    //   angle              ↔  Angle
    //   sideRadius         (NS-only)
    //   width              ↔  Width
    //   arcAngle           ↔  ArcAngle
    //   ringRadius         ↔  RingRadius
    float range              = 0.0f;
    float extraRange         = 0.0f;
    float radius             = 0.0f;
    float secondaryRadius    = 0.0f;
    float projectileSpeed    = 3.402823466e+38F;
    float speed              = 0.0f;
    float angle              = 0.0f;
    float sideRadius         = 0.0f;
    float width              = 50.0f;
    float arcAngle           = 0.0f;
    float ringRadius         = 0.0f;

    // ── Missile behaviour (EnsoulSharp) ─────────────────────────────────────
    float missileAccel       = 0.0f;  // ↔ MissileAccel
    float missileMaxSpeed    = 0.0f;  // ↔ MissileMaxSpeed
    float missileMinSpeed    = 0.0f;  // ↔ MissileMinSpeed
    bool  missileDelayed     = false; // ↔ MissileDelayed
    bool  missileFollowsCaster = false; // ↔ MissileFollowsCaster

    // ── Name fields ─────────────────────────────────────────────────────────
    std::string missileName       = "";  // legacy
    std::string missileSpellName  = "";  // ↔ MissileSpellName
    std::string displayName       = "";
    std::string sourceObjectName  = "";  // ↔ SourceObjectName
    std::string toggleParticleName= "";  // ↔ ToggleParticleName
    std::string fromObject        = "";  // ↔ FromObject
    std::vector<std::string> fromObjects;       // ↔ FromObjects
    std::vector<std::string> extraSpellNames;   // ↔ ExtraSpellNames
    std::vector<std::string> extraMissileNames; // ↔ ExtraMissileNames

    // ── Applied-buff descriptors ────────────────────────────────────────────
    // EnsoulSharp stores both named buffs (`AppliedBuffOnXxxName`) and typed
    // buff arrays (`AppliedBuffsOnXxx`). We keep both — name fields stay
    // strings, the typed arrays become string lists (BuffType enum identifier
    // kept as free-form string for portability; the concrete BuffType enum in
    // EnsoulSharp isn't critical on the C++ side).
    std::string appliedBuffName        = "";  // ↔ AppliedBuffName
    std::string appliedBuffOnSelfName  = "";  // ↔ AppliedBuffOnSelfName
    std::string appliedBuffOnAllyName  = "";  // ↔ AppliedBuffOnAllyName
    std::string appliedBuffOnEnemyName = "";  // ↔ AppliedBuffOnEnemyName
    std::vector<std::string> appliedBuffsOnAllies;  // ↔ AppliedBuffsOnAllies
    std::vector<std::string> appliedBuffsOnEnemies; // ↔ AppliedBuffsOnEnemies
    std::vector<std::string> appliedBuffsOnSelf;    // ↔ AppliedBuffsOnSelf

    // ── Shape classifiers ───────────────────────────────────────────────────
    SpellType     spellType = SpellType::None;     // ↔ SpellType
    SkillshotType type      = SkillshotType::Line; // NS enrichment

    // ── Timing ──────────────────────────────────────────────────────────────
    //   spellDelay        ↔  Delay            (ms)
    //   minChannelDuration↔  MinChannelDuration (ms)
    //   maxChannelDuration↔  MaxChannelDuration (ms)
    float spellDelay         = 250.0f;
    float castDelay          = 0.25f;   // NS-legacy (seconds)
    float extraDelay         = 0.0f;
    float extraEndTime       = 0.0f;
    float minChannelDuration = 0.0f;
    float maxChannelDuration = 0.0f;

    // ── Range / behaviour flags ─────────────────────────────────────────────
    bool  fixedRange             = false; // ↔ FixedRange
    bool  avoidMaxRangeReduction = false; // ↔ AvoidMaxRangeReduction
    bool  useEndPosition         = false;
    bool  usePackets             = false;
    bool  invert                 = false;
    float extraDistance          = 0.0f;
    bool  isThreeWay             = false;
    bool  defaultOff             = false;
    bool  noProcess              = false;
    bool  isWall                 = false;
    bool  isPerpendicular        = false;
    bool  hasEndExplosion        = false;
    bool  hasTrap                = false;
    bool  isSpecial              = false;
    bool  updatePosition         = true;
    float extraDrawHeight        = 0.0f;

    // ── Combat classification ───────────────────────────────────────────────
    bool  isMissile              = false;
    bool  isCC                   = false;
    bool  isDangerous            = false; // ↔ IsDangerous
    bool  resetsAutoAttackTimer  = false; // ↔ ResetsAutoAttackTimer
    int   dangerLevel            = 1;     // NS-legacy duplicate of dangerlevel
    int   dangerValue            = 1;     // ↔ DangerValue
    int   defaultEvadePct        = 0;
    bool  defaultEnabled         = true;
    bool  canBeRemoved           = false; // ↔ CanBeRemoved
    bool  forceRemove            = false; // ↔ ForceRemove

    // ── Bitmasks (EnsoulSharp arrays → packed uint32) ───────────────────────
    uint32_t castTypeMask        = CastType_None;   // ↔ CastType[]
    uint32_t spellTagsMask       = Tag_None;        // ↔ SpellTags[]
    uint32_t collisionObjectsMask= CollisionNone;

    // Legacy list form — still populated by existing Database.h entries.
    std::vector<CollisionObjectType> collisionObjects;

    // Trap descriptors (NS-legacy).
    std::string trapBaseName = "";
    std::string trapTroyName = "";

    // ── Helpers ─────────────────────────────────────────────────────────────
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

    bool CanBeSamiraWalled() const {
        if (collisionObjectsMask & CollisionSamiraWall) return true;
        for (auto obj : collisionObjects) {
            if (obj == SamiraWall) return true;
        }
        return false;
    }

    bool CanBeMelWalled() const {
        if (collisionObjectsMask & CollisionMelWall) return true;
        for (auto obj : collisionObjects) {
            if (obj == MelWall) return true;
        }
        return false;
    }

    bool HasTag(SpellTagsFlags tag) const { return (spellTagsMask & tag) != 0; }
    bool CanCastOn(CastTypeFlags t) const { return (castTypeMask & t) != 0; }
};

} // namespace Data
} // namespace SDK
