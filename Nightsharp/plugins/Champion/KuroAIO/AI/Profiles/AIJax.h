#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Jax = [] {
    ChampionProfile p{};
    p.ChampionName = "Jax";
    p.DisplayName = "Jax";
    p.InternalId = "champion.kuroaio.ai.jax";
    p.PrimaryArchetype = Archetype::Skirmisher;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::Dash | Mechanic::Recast | Mechanic::Stack |
                  Mechanic::AutoWeave | Mechanic::AutoReset;
    p.Ultimate = UltimatePolicy::Defensive;
    p.PreferredCombatDistance = 175.0f;
    p.EngageHealthPercent = 48.0f;
    p.DefensiveHealthPercent = 34.0f;
    p.UltimateTargetHealthPercent = 70.0f;
    p.MaximumCommitEnemies = 2;
    p.BaseHumanizerMs = 52;
    p.PreferSelectedTarget = true;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "JaxPassiveHaste";
    p.TrackedObjectToken = "Jax";
    p.ThemeFrom = 0xFF8E62D9u;
    p.ThemeTo = 0xFFE8B34Fu;
    p.TacticalSummary =
        "Passive-stack duelist: leap only to a safe endpoint, preserve AA-W-AA, "
        "time Counter Strike around incoming attacks, and gate R by real duel pressure.";
    p.ResearchSummary =
        "Riot 26.15 / CommunityDragon 16.15; Summoner's Rift values only, "
        "excluding alternate-mode modifiers.";

    p.Spells[0] = Spell(
        SDK::SpellSlot::Q, "Leap Strike", CastKind::AnyTarget,
        Intent::Damage | Intent::Mobility | Intent::Engage | Intent::Disengage |
            Intent::Jungle,
        Mode::Combo | Mode::Harass | Mode::Jungle | Mode::Flee,
        700.0f, 0.0f, 0.0f, FLT_MAX, false,
        SDK::DamageType::Physical, SDK::SpellType::Targeted);
    p.Spells[0].Aim = AimPolicy::TargetPosition;
    p.Spells[0].Priority = 86;
    p.Spells[0].DashDistance = 700.0f;
    p.Spells[0].MaximumEnemiesAtDestination = 2;
    p.Spells[0].AllowOnMinions = true;

    p.Spells[1] = Spell(
        SDK::SpellSlot::W, "Empower", CastKind::Self,
        Intent::Damage | Intent::Buff | Intent::AutoReset | Intent::LastHit |
            Intent::Jungle | Intent::Waveclear,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::LastHit | Mode::Automatic,
        0.0f, 0.0f, 0.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::Targeted);
    p.Spells[1].Aim = AimPolicy::SelfPosition;
    p.Spells[1].Priority = 92;
    p.Spells[1].WeaveAfterAttack = true;
    p.Spells[1].PreserveAutoAttack = true;

    p.Spells[2] = Spell(
        SDK::SpellSlot::E, "Counter Strike", CastKind::Self,
        Intent::Damage | Intent::CrowdControl | Intent::Disengage |
            Intent::Recast | Intent::Interrupt | Intent::AntiGapcloser,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::Flee | Mode::Automatic,
        375.0f, 0.0f, 375.0f, FLT_MAX, false,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotCircle);
    p.Spells[2].Aim = AimPolicy::SelfPosition;
    p.Spells[2].Priority = 96;
    p.Spells[2].RecastSpellName = "JaxCounterStrikeAttack";

    p.Spells[3] = Spell(
        SDK::SpellSlot::R, "Grandmaster-At-Arms", CastKind::Self,
        Intent::Damage | Intent::Buff | Intent::Shield | Intent::Setup,
        Mode::Combo | Mode::Flee | Mode::Automatic,
        0.0f, 0.0f, 0.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::Targeted);
    p.Spells[3].Aim = AimPolicy::SelfPosition;
    p.Spells[3].Priority = 88;
    p.Spells[3].TargetHealthPercent = 70.0f;
    p.Spells[3].PlayerHealthPercent = 48.0f;

    p.Variants[0] = { SDK::SpellSlot::E, "JaxCounterStrikeAttack", p.Spells[2] };
    p.VariantCount = 1;

    p.Trade = Plan("Counter Strike trade",
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireFirstCast, 0, 650),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequireSafePosition, 70, 850),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget | StepRule::RequireAfterAttack, 140, 1050),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireRecast, 1000, 2100));
    p.AllIn = Plan("Grandmaster duel",
        Step(SDK::SpellSlot::R, StepRule::RequireTarget | StepRule::RequirePlayerLow, 0, 700),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireFirstCast, 60, 750),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequireSafePosition, 120, 900),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget | StepRule::RequireAfterAttack, 190, 1100),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireRecast, 1000, 2100));
    p.Flee = Plan("Counter Strike retreat",
        Step(SDK::SpellSlot::E, StepRule::RequireNoCrowdControl, 0, 700),
        Step(SDK::SpellSlot::Q, StepRule::RequireSafePosition, 80, 900),
        Step(SDK::SpellSlot::E, StepRule::RequireRecast, 1000, 2100));
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
