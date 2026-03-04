#pragma once
// ============================================================================
// Game Enumerations
// Ported from EnsoulSharp.SDK/Core/Enumerations/
// ============================================================================

namespace SDK {

// Team IDs
enum class GameObjectTeam : int {
    Unknown  = 0,
    Blue     = 100,
    Red      = 200,
    Neutral  = 300
};

// Orbwalker modes (EnsoulSharp OrbwalkingMode.cs)
enum class OrbwalkingMode {
    None = 0,
    Combo,
    Hybrid,    // Harass
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

// Minion types (MinionTypes.cs)
enum class MinionType : int {
    Unknown = 0,
    Melee   = 4,
    Ranged  = 5,
    Cannon  = 6,
    Super   = 7
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
    Charm            = 24
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

// Gapcloser types (GapcloserType.cs)
enum class GapcloserType {
    Skillshot,
    Targeted
};

} // namespace SDK
