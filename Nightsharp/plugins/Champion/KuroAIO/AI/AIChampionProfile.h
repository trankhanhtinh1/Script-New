#pragma once

// ============================================================================
// KuroAIO AI champion profile contract.
//
// Every unsupported champion owns one AI<Champion>.h file.  Those files only
// describe champion-specific spell semantics, combo orders and tactical rules;
// AIChampionEngine.h owns the shared state machine and event subscriptions.
// Keeping the two layers separate makes all 163 profiles independently
// auditable without duplicating unsafe cast/orbwalker code.
// ============================================================================

#include "../../../../SDK/SDK.h"

#include <array>
#include <cfloat>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace Plugins::KuroAIO::AI {

enum class Archetype : std::uint8_t {
    Assassin,
    Battlemage,
    BurstMage,
    Catcher,
    Diver,
    Enchanter,
    Juggernaut,
    Marksman,
    Skirmisher,
    Specialist,
    Tank,
    Vanguard,
};

enum class ResourceModel : std::uint8_t {
    Mana,
    Energy,
    Health,
    Fury,
    Ammo,
    Special,
    None,
};

enum class CastKind : std::uint8_t {
    None,
    Self,
    Toggle,
    EnemyTarget,
    AllyTarget,
    AnyTarget,
    Line,
    Circle,
    Cone,
    Direction,
    Position,
    Vector,
    ChargedLine,
    ChargedCircle,
};

enum class AimPolicy : std::uint8_t {
    Prediction,
    TargetPosition,
    BehindTarget,
    BetweenPlayerAndTarget,
    Cursor,
    SafeCursor,
    AwayFromThreat,
    BestAoe,
    SelfPosition,
};

enum class UltimatePolicy : std::uint8_t {
    DisabledByDefault,
    Execute,
    AllIn,
    SingleTarget,
    MultiTarget,
    Defensive,
    SaveAlly,
    GlobalExecute,
    RecastControl,
    ManualAssist,
};

enum class Intent : std::uint32_t {
    None          = 0,
    Damage        = 1u << 0,
    CrowdControl  = 1u << 1,
    Mobility      = 1u << 2,
    Engage        = 1u << 3,
    Disengage     = 1u << 4,
    Execute       = 1u << 5,
    Shield        = 1u << 6,
    Heal          = 1u << 7,
    Buff          = 1u << 8,
    Cleanse       = 1u << 9,
    Interrupt     = 1u << 10,
    AntiGapcloser = 1u << 11,
    Waveclear     = 1u << 12,
    Jungle        = 1u << 13,
    LastHit       = 1u << 14,
    Peel          = 1u << 15,
    Setup         = 1u << 16,
    Finisher      = 1u << 17,
    Vision        = 1u << 18,
    Objective     = 1u << 19,
    Recast        = 1u << 20,
    Channel       = 1u << 21,
    AutoReset     = 1u << 22,
    AllyUtility   = 1u << 23,
    Mark          = 1u << 24,
    AutoWeave     = 1u << 25,
    SpellShield   = 1u << 26,
    MissingHealth = 1u << 27,
    Global        = 1u << 28,
    Revive        = 1u << 29,
};

#ifdef Global
#undef Global
#endif

enum class Mechanic : std::uint64_t {
    None             = 0,
    Recast           = 1ull << 0,
    Charge           = 1ull << 1,
    Ammo             = 1ull << 2,
    Transform        = 1ull << 3,
    MultiForm        = 1ull << 4,
    Dash             = 1ull << 5,
    Blink            = 1ull << 6,
    Channel          = 1ull << 7,
    Execute          = 1ull << 8,
    Global           = 1ull << 9,
    ObjectTracking   = 1ull << 10,
    Mark             = 1ull << 11,
    Stack            = 1ull << 12,
    Pet              = 1ull << 13,
    Trap             = 1ull << 14,
    WallInteraction  = 1ull << 15,
    AllyTarget       = 1ull << 16,
    SpellShield      = 1ull << 17,
    Cleanse          = 1ull << 18,
    Revive           = 1ull << 19,
    Tether           = 1ull << 20,
    AutoWeave        = 1ull << 21,
    AutoReset        = 1ull << 22,
    DirectionalSweet = 1ull << 23,
    ReturnProjectile = 1ull << 24,
    MissingHealth    = 1ull << 25,
    Terrain          = 1ull << 26,
    Stance           = 1ull << 27,
    Possession       = 1ull << 28,
    Evolve           = 1ull << 29,
};

enum class StepRule : std::uint32_t {
    None                  = 0,
    RequireTarget         = 1u << 0,
    RequireCrowdControl   = 1u << 1,
    RequireNoCrowdControl = 1u << 2,
    RequireAfterAttack    = 1u << 3,
    RequireOutsideAaRange = 1u << 4,
    RequireInsideAaRange  = 1u << 5,
    RequireSafePosition   = 1u << 6,
    RequireTargetLow      = 1u << 7,
    RequirePlayerLow      = 1u << 8,
    RequireMultiTarget    = 1u << 9,
    RequireMark           = 1u << 10,
    RequireNoMark         = 1u << 11,
    RequireRecast         = 1u << 12,
    RequireFirstCast      = 1u << 13,
    SkipIfKillableWithout = 1u << 14,
    HoldForExecute        = 1u << 15,
    AllowDuringWindup     = 1u << 16,
    ManualAssistOnly      = 1u << 17,
};

enum class Mode : std::uint8_t {
    None      = 0,
    Combo     = 1u << 0,
    Harass    = 1u << 1,
    LaneClear = 1u << 2,
    Jungle    = 1u << 3,
    LastHit   = 1u << 4,
    Flee      = 1u << 5,
    Automatic = 1u << 6,
};

template <typename E>
constexpr E Or(E left, E right) {
    return static_cast<E>(
        static_cast<std::underlying_type_t<E>>(left) |
        static_cast<std::underlying_type_t<E>>(right));
}

template <typename E>
constexpr bool Has(E value, E flag) {
    return (static_cast<std::underlying_type_t<E>>(value) &
            static_cast<std::underlying_type_t<E>>(flag)) != 0;
}

constexpr Intent operator|(Intent left, Intent right) { return Or(left, right); }
constexpr Mechanic operator|(Mechanic left, Mechanic right) { return Or(left, right); }
constexpr StepRule operator|(StepRule left, StepRule right) { return Or(left, right); }
constexpr Mode operator|(Mode left, Mode right) { return Or(left, right); }

struct SpellSpec {
    SDK::SpellSlot Slot = SDK::SpellSlot::Unknown;
    const char* Name = "";
    CastKind Kind = CastKind::None;
    AimPolicy Aim = AimPolicy::Prediction;
    Intent Intents = Intent::None;
    Mode Modes = Mode::None;
    SDK::DamageType Damage = SDK::DamageType::True;
    SDK::HitChance Hitchance = SDK::HitChance::High;
    SDK::SpellType Shape = SDK::SpellType::SkillshotLine;

    float Range = 0.0f;
    float Delay = 0.25f;
    float Width = 80.0f;
    float Speed = FLT_MAX;
    float TriggerRange = 0.0f;
    float DesiredDistance = 0.0f;
    float DashDistance = 0.0f;
    bool Collision = false;
    bool WeaveAfterAttack = false;
    bool PreserveAutoAttack = true;
    bool AllowOnMinions = false;

    int Priority = 50;
    int MinimumAoeTargets = 1;
    int MaximumEnemiesAtDestination = 2;
    int MinimumAmmo = 1;
    int HumanizerExtraMs = 0;

    float ComboManaPercent = 0.0f;
    float HarassManaPercent = 35.0f;
    float ClearManaPercent = 45.0f;
    float TargetHealthPercent = 100.0f;
    float PlayerHealthPercent = 100.0f;

    const char* RequiredPlayerBuff = "";
    const char* ForbiddenPlayerBuff = "";
    const char* RequiredTargetBuff = "";
    const char* RecastSpellName = "";
    const char* ChargeBuffName = "";
    int ChargeMinRange = 0;
    int ChargeMaxRange = 0;
    float ChargeDurationSeconds = 0.0f;
};

struct ComboStep {
    SDK::SpellSlot Slot = SDK::SpellSlot::Unknown;
    StepRule Rules = StepRule::RequireTarget;
    int EarliestMs = 0;
    int ExpireMs = 900;
    float TargetHealthPercent = 100.0f;
    float PlayerHealthPercent = 100.0f;
    int MinimumNearbyEnemies = 1;
};

struct ComboPlan {
    const char* Name = "";
    std::array<ComboStep, 12> Steps = {};
    std::size_t Count = 0;
    int ResetAfterMs = 1800;
};

struct SpellVariant {
    SDK::SpellSlot Slot = SDK::SpellSlot::Unknown;
    const char* RuntimeNameToken = "";
    SpellSpec Spec = {};
};

struct ChampionProfile {
    const char* ChampionName = "";
    const char* DisplayName = "";
    const char* InternalId = "";
    Archetype PrimaryArchetype = Archetype::Specialist;
    ResourceModel Resource = ResourceModel::Mana;
    Mechanic Mechanics = Mechanic::None;
    UltimatePolicy Ultimate = UltimatePolicy::AllIn;

    std::array<SpellSpec, 4> Spells = {};
    std::array<SpellVariant, 20> Variants = {};
    std::size_t VariantCount = 0;
    ComboPlan Trade = {};
    ComboPlan AllIn = {};
    ComboPlan Flee = {};

    float PreferredCombatDistance = 450.0f;
    float EngageHealthPercent = 32.0f;
    float DefensiveHealthPercent = 28.0f;
    float UltimateTargetHealthPercent = 45.0f;
    int UltimateMinimumTargets = 2;
    int MaximumCommitEnemies = 2;
    int BaseHumanizerMs = 65;
    bool PreferSelectedTarget = true;
    bool AllowTurretDiveIfKillable = false;
    bool ProtectManualChannels = true;

    const char* PassiveBuff = "";
    const char* MarkBuff = "";
    const char* ChannelBuff = "";
    const char* FormBuff = "";
    const char* UltimateBuff = "";
    const char* TrackedObjectToken = "";

    std::uint32_t ThemeFrom = 0xFFFFAA40u;
    std::uint32_t ThemeTo = 0xFF9C40FFu;
    float ThemeSpeed = 1.0f;

    const char* TacticalSummary = "";
    const char* ResearchSummary = "";
};

constexpr SpellSpec Spell(
    SDK::SpellSlot slot,
    const char* name,
    CastKind kind,
    Intent intents,
    Mode modes,
    float range,
    float delay = 0.25f,
    float width = 80.0f,
    float speed = FLT_MAX,
    bool collision = false,
    SDK::DamageType damage = SDK::DamageType::True,
    SDK::SpellType shape = SDK::SpellType::SkillshotLine) {
    SpellSpec value{};
    value.Slot = slot;
    value.Name = name;
    value.Kind = kind;
    value.Intents = intents;
    value.Modes = modes;
    value.Range = range;
    value.TriggerRange = range;
    value.Delay = delay;
    value.Width = width;
    value.Speed = speed;
    value.Collision = collision;
    value.Damage = damage;
    value.Shape = shape;
    return value;
}

constexpr ComboStep Step(
    SDK::SpellSlot slot,
    StepRule rules = StepRule::RequireTarget,
    int earliestMs = 0,
    int expireMs = 900) {
    ComboStep value{};
    value.Slot = slot;
    value.Rules = rules;
    value.EarliestMs = earliestMs;
    value.ExpireMs = expireMs;
    return value;
}

template <typename... TSteps>
constexpr ComboPlan Plan(const char* name, TSteps... steps) {
    static_assert(sizeof...(steps) <= 12, "AI combo plan exceeds 12 steps");
    ComboPlan plan{};
    plan.Name = name;
    const ComboStep values[] = { steps... };
    plan.Count = sizeof...(steps);
    for (std::size_t i = 0; i < plan.Count; ++i) {
        plan.Steps[i] = values[i];
    }
    return plan;
}

inline constexpr Mode CombatModes = Mode::Combo | Mode::Harass;
inline constexpr Mode FarmModes = Mode::LaneClear | Mode::Jungle | Mode::LastHit;
inline constexpr Mode AllModes = CombatModes | FarmModes | Mode::Flee | Mode::Automatic;

} // namespace Plugins::KuroAIO::AI
