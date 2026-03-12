#pragma once
// ============================================================================
// Game Enumerations
// Ported from EnsoulSharp.SDK/Core/Enumerations/
// ============================================================================

namespace SDK {

// Team IDs (byte at offset 0x259: 1=Blue, 2=Red, 3=Neutral)
enum class GameObjectTeam : int {
    Unknown  = 0,
    Blue     = 1,
    Red      = 2,
    Neutral  = 3
};

// Orbwalker modes (EnsoulSharp OrbwalkingMode.cs)
enum class OrbwalkingMode {
    None = 0,
    Combo,
    Harass,
    Hybrid = Harass, // Backward compatibility alias
    LastHit,
    LaneClear,
    Flee
};

// Spell slot indices
enum class SpellSlotId : int {
    Q = 0,
    W = 1,
    E = 2,
    R = 3,
    Summoner1 = 4,
    Summoner2 = 5,
    Item1 = 6,
    Item2 = 7,
    Item3 = 8,
    Item4 = 9,
    Item5 = 10,
    Item6 = 11,
    Trinket = 12,
    Recall = 13
};

// IssueOrder types
enum class OrderType : int {
    None       = 0,
    Hold       = 1,
    MoveTo     = 2,
    AttackUnit = 3,
    Pet        = 4,
    Stop       = 10
};

// Skillshot types (SkillshotType.cs)
enum class SkillshotType {
    Line,
    Circle,
    Cone
};

// HitChance for prediction (HitChance.cs)
enum class HitChance : int {
    None       = -1,
    Collision  = 0,
    OutOfRange = 1,
    Impossible = 2,
    Low        = 3,
    Medium     = 4,
    High       = 5,
    VeryHigh   = 6,
    Dashing    = 7,
    Immobile   = 8
};

// Damage types
enum class DamageType {
    Physical,
    Magical,
    True,
    Mixed
};

// Minion types — matches game-internal values from sub_BBB10 (IDA)
// Read from Offset::Minion::LaneType (obj+0x4CC9)
enum class MinionType : int {
    Unknown = 0,    // Unset / unclassified
    Pet     = 1,    // Champion pet (Annie Tibbers, Yorick ghouls, Zyra plants, Illaoi tentacles...)
    Jungle  = 2,    // Jungle monster
    Team    = 3,    // Team minion (special spawn)
    Melee   = 4,    // Melee lane minion
    Ranged  = 5,    // Ranged (caster) lane minion
    Cannon  = 6,    // Siege/Cannon lane minion
    Super   = 7     // Super lane minion
};

// Buff types
enum class BuffType : int {
    Internal         = 0,
    Aura             = 1,
    CombatEnchancer  = 2,
    CombatDehancer   = 3,
    SpellShield      = 4,
    Stun             = 5,
    Invisibility     = 6,
    Silence          = 7,
    Taunt            = 8,
    Berserk          = 9,
    Polymorph        = 10,
    Slow             = 11,
    Snare            = 12,
    Damage           = 13,
    Heal             = 14,
    Haste            = 15,
    Fear             = 16,
    Flee             = 17,
    NearSight        = 18,
    Blind            = 19,
    Suppression      = 20,
    Asleep           = 21,
    Grounded         = 22,
    Drowsy           = 23,
    Charm            = 24,
    Poison           = 25,
    Knockup          = 26,
    Knockback        = 27
};

// Action states (bitflags)
namespace ActionState {
    constexpr int CanAttack    = 1;
    constexpr int CanMove      = 2;
    constexpr int CanCast      = 4;
    constexpr int Immovable    = 8;
    constexpr int IsStealth    = 16;
    constexpr int Taunted      = 32;
    constexpr int Feared       = 64;
    constexpr int Fleeing      = 128;
    constexpr int Charmed      = 256;
    constexpr int Asleep       = 512;
    constexpr int NearSight    = 1024;
    constexpr int Ghosted      = 2048;
    constexpr int Suppressed   = 8192;
}

// Orbwalker action types (OrbwalkingType.cs)
// Fired via Orbwalker::OnAction events
enum class OrbwalkingType {
    None = 0,
    Movement,           // Move command issued
    StopMovement,       // Move suppressed (hold position / menu disabled)
    BeforeAttack,       // About to issue attack (Process flag can cancel)
    AfterAttack,        // Missile launched / ranged: after windup
    OnAttack,           // Attack command confirmed (DoCast)
    NonKillableMinion,  // Minion about to die before our AA arrives
    TargetSwitch,       // Orbwalker switched target
    OnAttackReady,      // Attack cooldown ended, ready to attack again
    OnMoveReady         // Move delay elapsed, ready to move again
};

// Gapcloser types (GapcloserType.cs)
enum class GapcloserType {
    Skillshot,
    Targeted
};

// Game object types (EnsoulSharp GameObjectType)
enum class GameObjectType {
    Unknown           = 0,
    AIHeroClient      = 1,
    AIMinionClient    = 2,
    AITurretClient    = 3,
    MissileClient     = 4,
    NeutralMinionCamp = 5,
    BarracksDampener  = 6,
    HQ               = 7
};

// Resource types (mana, energy, etc.)
enum class ResourceType : int {
    Mana      = 0,
    Energy    = 1,
    None      = 2,
    Shield    = 3,
    BattleFury = 4,
    DragonFury = 5,
    Rage      = 6,
    Heat      = 7,
    Gnarfury  = 8,
    Ferocity  = 9,
    BloodWell = 10,
    Wind      = 11,
    Ammo      = 12,
    Other     = 13
};

// Summoner spell names (for identification)
namespace SummonerSpells {
    constexpr const char* Flash     = "SummonerFlash";
    constexpr const char* Ignite    = "SummonerDot";
    constexpr const char* Heal      = "SummonerHeal";
    constexpr const char* Exhaust   = "SummonerExhaust";
    constexpr const char* Barrier   = "SummonerBarrier";
    constexpr const char* Teleport  = "SummonerTeleport";
    constexpr const char* Smite     = "SummonerSmite";
    constexpr const char* Cleanse   = "SummonerBoost";
    constexpr const char* Ghost     = "SummonerHaste";
    constexpr const char* Mark      = "SummonerSnowball";  // ARAM
}

// ============================================================================
// SpellType — Comprehensive spell type (from SpellType.cs)
// Used by SpellDatabaseEntry for full spell classification
// ============================================================================
enum class SpellType {
    SkillshotCircle,           // Circle skillshot (Ziggs Q)
    SkillshotMissileCircle,    // Circle skillshot with missile (Lulu E)
    SkillshotLine,             // Line skillshot (Ezreal Q)
    SkillshotMissileLine,      // Line skillshot with missile (Morgana Q)
    SkillshotCone,             // Cone skillshot (Annie W)
    SkillshotMissileCone,      // Cone skillshot with missile (Ashe W)
    SkillshotMissileArc,       // Arc missile skillshot (Diana Q)
    SkillshotRing,             // Ring skillshot (Veigar E)
    SkillshotArc,              // Arc skillshot
    Targeted,                  // Targeted spell (Annie Q)
    TargetedMissile,           // Targeted with missile (Caitlyn R)
    Toggled,                   // Toggle spell (Singed Q)
    Activated,                 // Activated spell (Vayne R, Olaf R)
    Passive,                   // Passive only (Vayne W)
    Position,                  // Position-based but undodgeable (Ezreal E)
    Vector                     // Start+End point (Viktor E, Rumble R)
};

// ============================================================================
// CastType — How a spell can be cast (from CastTypes.cs)
// ============================================================================
enum class CastType {
    EnemyChampions,
    EnemyMinions,
    EnemyTurrets,
    AllyChampions,
    AllyMinions,
    AllyTurrets,
    HeroPets,
    Position,
    Direction,
    Self,
    Charging,
    Toggle,
    Channel,
    Activate,
    ImpossibleToCast
};

// ============================================================================
// CollisionableObjects — What a spell missile can collide with (Flags)
// From CollisionableObjects.cs
// ============================================================================
enum CollisionableObjects : int {
    CollisionNone       = 0,
    CollisionMinions    = 1 << 0,
    CollisionHeroes     = 1 << 1,
    CollisionYasuoWall  = 1 << 2,
    CollisionBraumShield = 1 << 3,
    CollisionWalls      = 1 << 4
};

// ============================================================================
// SpellTags — Properties/tags a spell can have (from SpellTags.cs)
// ============================================================================
enum class SpellTags {
    Damage,
    AoE,
    AppliesOnHitEffects,
    CrowdControl,
    Shield,
    Heal,
    Stasis,
    LeavesMark,
    CanDetonateMark,
    Transformation,
    Dash,
    Blink,
    Teleport,
    DamageAmplifier,
    DefensiveBuff,
    MovementSpeedAmplifier,
    AttackSpeedAmplifier,
    AttackRangeModifier,
    SpellShield,
    RemoveCrowdControl,
    GrantsVision,
    Interruptable
};

} // namespace SDK
