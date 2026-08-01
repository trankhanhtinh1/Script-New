#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

// Ambessa is an input-buffered energy skirmisher.  Her controller owns the
// distinction between an intentional passive dash and a deliberate no-dash,
// the Q1 blade edge, Q2 first-target ordering, W's damage-intercept window,
// E's conditional second strike and R's farthest-champion selection.
inline constexpr ChampionProfile Ambessa = [] {
    ChampionProfile p{};
    p.ChampionId = SDK::ChampionId::Ambessa;
    p.DisplayName = "Ambessa";
    p.InternalId = "champion.kuroaio.ai.ambessa";
    p.PrimaryArchetype = Archetype::Diver;
    p.Resource = ResourceModel::Energy;
    p.Mechanics = Mechanic::Recast | Mechanic::Dash | Mechanic::Stack |
                  Mechanic::AutoWeave | Mechanic::DirectionalSweet |
                  Mechanic::Terrain | Mechanic::MissingHealth;
    p.Ultimate = UltimatePolicy::AllIn;

    p.Spells[0] = Spell(
        SDK::SpellSlot::Q, "Cunning Sweep / Sundering Slam",
        CastKind::Direction,
        Intent::Damage | Intent::Setup | Intent::Finisher |
            Intent::Waveclear | Intent::Jungle | Intent::Recast,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::LastHit | Mode::Flee | Mode::Automatic,
        650.0f, 0.225f, 400.0f, FLT_MAX, false,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotCone);
    p.Spells[0].Priority = 94;
    p.Spells[0].TriggerRange = 650.0f;
    p.Spells[0].DesiredDistance = 350.0f;
    p.Spells[0].Hitchance = SDK::HitChance::High;
    p.Spells[0].HarassManaPercent = 42.0f;
    p.Spells[0].ClearManaPercent = 55.0f;
    p.Spells[0].AllowOnMinions = true;
    p.Spells[0].PreserveAutoAttack = true;
    p.Spells[0].RecastSpellName = "AmbessaQ2";

    p.Spells[1] = Spell(
        SDK::SpellSlot::W, "Repudiation", CastKind::Self,
        Intent::Damage | Intent::Shield | Intent::Setup |
            Intent::Disengage | Intent::Jungle,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::Flee | Mode::Automatic,
        325.0f, 0.50f, 650.0f, FLT_MAX, false,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotCircle);
    p.Spells[1].Priority = 96;
    p.Spells[1].TriggerRange = 325.0f;
    p.Spells[1].DashDistance = 350.0f;
    p.Spells[1].HarassManaPercent = 50.0f;
    p.Spells[1].PreserveAutoAttack = true;

    p.Spells[2] = Spell(
        SDK::SpellSlot::E, "Lacerate", CastKind::Self,
        Intent::Damage | Intent::CrowdControl | Intent::Mobility |
            Intent::Setup | Intent::Disengage | Intent::Waveclear |
            Intent::Jungle,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::Flee | Mode::Automatic,
        325.0f, 0.225f, 650.0f, FLT_MAX, false,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotCircle);
    p.Spells[2].Priority = 91;
    p.Spells[2].TriggerRange = 325.0f;
    p.Spells[2].DashDistance = 350.0f;
    p.Spells[2].HarassManaPercent = 50.0f;
    p.Spells[2].ClearManaPercent = 60.0f;
    p.Spells[2].AllowOnMinions = true;
    p.Spells[2].PreserveAutoAttack = true;

    p.Spells[3] = Spell(
        SDK::SpellSlot::R, "Public Execution", CastKind::Line,
        Intent::Damage | Intent::CrowdControl | Intent::Mobility |
            Intent::Engage | Intent::Execute | Intent::Interrupt |
            Intent::Setup,
        Mode::Combo | Mode::Flee | Mode::Automatic,
        1250.0f, 0.70f, 130.0f, FLT_MAX, false,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotLine);
    p.Spells[3].Priority = 98;
    p.Spells[3].TriggerRange = 1250.0f;
    p.Spells[3].Hitchance = SDK::HitChance::VeryHigh;
    p.Spells[3].DashDistance = 1250.0f;
    p.Spells[3].MaximumEnemiesAtDestination = 2;
    p.Spells[3].PreserveAutoAttack = true;

    p.Variants[0] = SpellVariant{
        SDK::SpellSlot::Q,
        "AmbessaQ1",
        p.Spells[0],
    };
    p.Variants[0].Spec.Name = "Cunning Sweep";
    p.Variants[0].Spec.Range = 400.0f;
    p.Variants[0].Spec.TriggerRange = 435.0f;
    p.Variants[0].Spec.Width = 400.0f;
    p.Variants[0].Spec.Shape = SDK::SpellType::SkillshotCone;

    p.Variants[1] = SpellVariant{
        SDK::SpellSlot::Q,
        "AmbessaQ2",
        p.Spells[0],
    };
    p.Variants[1].Spec.Name = "Sundering Slam";
    p.Variants[1].Spec.Range = 650.0f;
    p.Variants[1].Spec.TriggerRange = 650.0f;
    p.Variants[1].Spec.Width = 80.0f;
    p.Variants[1].Spec.Shape = SDK::SpellType::SkillshotLine;
    p.VariantCount = 2;

    p.Trade = Plan(
        "Q1 edge, player-owned spacing, passive AA, E slow, Q2 exit",
        Step(SDK::SpellSlot::Q,
             StepRule::RequireTarget | StepRule::RequireFirstCast),
        Step(SDK::SpellSlot::E,
             StepRule::RequireTarget),
        Step(SDK::SpellSlot::Q,
             StepRule::RequireTarget | StepRule::RequireRecast));

    p.AllIn = Plan(
        "select safe entry, weave energy, intercept with W, isolate with R",
        Step(SDK::SpellSlot::Q,
             StepRule::RequireTarget | StepRule::RequireFirstCast),
        Step(SDK::SpellSlot::W,
             StepRule::RequireTarget | StepRule::RequireSafePosition),
        Step(SDK::SpellSlot::E,
             StepRule::RequireTarget),
        Step(SDK::SpellSlot::Q,
             StepRule::RequireTarget | StepRule::RequireRecast),
        Step(SDK::SpellSlot::R,
             StepRule::RequireTarget | StepRule::SkipIfKillableWithout));

    p.Flee = Plan(
        "slow contact with E, keep W for real damage, Q only for a safe step",
        Step(SDK::SpellSlot::E),
        Step(SDK::SpellSlot::W,
             StepRule::RequirePlayerLow),
        Step(SDK::SpellSlot::Q));

    p.PreferredCombatDistance = 350.0f;
    p.EngageHealthPercent = 48.0f;
    p.DefensiveHealthPercent = 30.0f;
    p.UltimateTargetHealthPercent = 52.0f;
    p.UltimateMinimumTargets = 1;
    p.MaximumCommitEnemies = 2;
    p.BaseHumanizerMs = 38;
    p.AllowTurretDiveIfKillable = true;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "AmbessaPassiveAttackEmpower";
    p.MarkBuff = "AmbessaQEmpowerReady";
    p.FormBuff = "AmbessaPassiveDash";
    p.UltimateBuff = "AmbessaRBuffSuppressing";
    p.ThemeFrom = 0xFFE05252u;
    p.ThemeTo = 0xFF8B1E3Fu;
    p.ThemeSpeed = 1.08f;
    p.TacticalSummary =
        "Choose dash versus no-dash after every lockout, land Q1 with the "
        "blade edge and Q2 on the intended first unit, weave empowered autos "
        "for energy without delaying burst, time W into real incoming damage, "
        "double E only after a real Step, and aim R around its farthest-target "
        "rule and behind-target landing danger.";
    p.ResearchSummary =
        "CommunityDragon PC 16.14 bin/game data, Riot 14.23-14.24, 25.24, "
        "26.9-26.10 and 26.14 notes, current League mechanics, Season 16 "
        "high-elo/OTP material, Heywil's 2026 guide plus advanced combo and "
        "animation-cancel videos, Challenger gameplay, jungle routing, and "
        "deterministic geometry/energy regressions.";
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
