#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Zeri = [] {
    ChampionProfile p{};
    p.ChampionName = "Zeri";
    p.DisplayName = "Zeri";
    p.InternalId = "champion.kuroaio.ai.zeri";
    p.PrimaryArchetype = Archetype::Marksman;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::Charge | Mechanic::Dash | Mechanic::Terrain |
                  Mechanic::Stack | Mechanic::AutoWeave | Mechanic::WallInteraction;
    p.Ultimate = UltimatePolicy::MultiTarget;

    p.Spells[0] = Spell(
        SDK::SpellSlot::Q, "Burst Fire", CastKind::Line,
        Intent::Damage | Intent::AutoReset | Intent::Waveclear |
            Intent::Jungle | Intent::LastHit | Intent::Objective,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::LastHit | Mode::Automatic,
        825.0f, 0.05f, 20.0f, 2600.0f, true,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotLine);
    p.Spells[0].Priority = 98;
    p.Spells[0].Hitchance = SDK::HitChance::High;
    p.Spells[0].PreserveAutoAttack = true;
    p.Spells[0].WeaveAfterAttack = true;
    p.Spells[0].ChargeBuffName = "ZeriQPassiveReady";
    p.Spells[0].ChargeMinRange = 825;
    p.Spells[0].ChargeMaxRange = 825;
    p.Spells[0].HarassManaPercent = 34.0f;
    p.Spells[0].ClearManaPercent = 42.0f;
    p.Spells[0].AllowOnMinions = true;

    p.Spells[1] = Spell(
        SDK::SpellSlot::W, "Ultrashock Laser", CastKind::Line,
        Intent::Damage | Intent::CrowdControl | Intent::Setup |
            Intent::Waveclear | Intent::Jungle | Intent::LastHit,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::LastHit | Mode::Flee | Mode::Automatic,
        1200.0f, 0.55f, 80.0f, 2500.0f, true,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotLine);
    p.Spells[1].Priority = 92;
    p.Spells[1].Hitchance = SDK::HitChance::High;
    p.Spells[1].PreserveAutoAttack = true;
    p.Spells[1].HarassManaPercent = 44.0f;
    p.Spells[1].ClearManaPercent = 54.0f;
    p.Spells[1].AllowOnMinions = true;

    p.Spells[2] = Spell(
        SDK::SpellSlot::E, "Spark Surge", CastKind::Position,
        Intent::Mobility | Intent::Engage | Intent::Disengage |
            Intent::Peel | Intent::AntiGapcloser,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::Automatic,
        300.0f, 0.25f, 0.0f, FLT_MAX, false,
        SDK::DamageType::True, SDK::SpellType::SkillshotCircle);
    p.Spells[2].Priority = 90;
    p.Spells[2].DashDistance = 300.0f;
    p.Spells[2].MaximumEnemiesAtDestination = 1;
    p.Spells[2].PreserveAutoAttack = true;
    p.Spells[2].HarassManaPercent = 48.0f;

    p.Spells[3] = Spell(
        SDK::SpellSlot::R, "Lightning Crash", CastKind::Self,
        Intent::Buff | Intent::Damage | Intent::Engage | Intent::Execute |
            Intent::Objective | Intent::Peel,
        Mode::Combo | Mode::Automatic,
        825.0f, 0.25f, 0.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[3].Priority = 100;
    p.Spells[3].MinimumAoeTargets = 2;
    p.Spells[3].PreserveAutoAttack = false;

    p.Trade = Plan(
        "Preserve the charged basic shot, weave Q into the orbwalker's attack, then use a clear wall-aware W",
        Step(SDK::SpellSlot::Q,
             StepRule::RequireTarget | StepRule::AllowDuringWindup),
        Step(SDK::SpellSlot::W,
             StepRule::RequireTarget | StepRule::RequireAfterAttack));
    p.AllIn = Plan(
        "Activate Lightning Crash for a committed multi-target fight, keep firing Q and spend E only on a safe route",
        Step(SDK::SpellSlot::R, StepRule::RequireTarget),
        Step(SDK::SpellSlot::Q,
             StepRule::RequireTarget | StepRule::AllowDuringWindup),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget),
        Step(SDK::SpellSlot::E, StepRule::RequireSafePosition));
    p.Flee = Plan(
        "Use a walkable Spark Surge endpoint away from the threat, then fire W for peel if the laser is clear",
        Step(SDK::SpellSlot::E,
             StepRule::RequireSafePosition | StepRule::AllowDuringWindup),
        Step(SDK::SpellSlot::W,
             StepRule::RequireTarget | StepRule::AllowDuringWindup));

    p.PreferredCombatDistance = 675.0f;
    p.EngageHealthPercent = 62.0f;
    p.DefensiveHealthPercent = 31.0f;
    p.UltimateTargetHealthPercent = 78.0f;
    p.UltimateMinimumTargets = 2;
    p.MaximumCommitEnemies = 2;
    p.BaseHumanizerMs = 22;
    p.PreferSelectedTarget = true;
    p.AllowTurretDiveIfKillable = false;
    p.PassiveBuff = "ZeriQPassiveReady";
    p.UltimateBuff = "ZeriR";
    p.ThemeFrom = 0xFF36D7FFu;
    p.ThemeTo = 0xFF4C6BFFu;
    p.ThemeSpeed = 0.95f;
    p.TacticalSummary =
        "Treat Q as Zeri's basic-shot route: preserve a charged passive shot, accept only first-target predictions, and let the orbwalker keep AA windup. W is wall-aware and must not fire through a collision body; E is a terrain dash that never trades a safe exit for a speculative engage; R starts only for a safe committed fight or clear lethal window and its overcharge stacks are reconciled from buffs and attacks.";
    p.ResearchSummary =
        "Pinned to Riot 26.15 / CommunityDragon 16.15 semantics: Burst Fire Q is the basic-attack replacement with 825 range, Ultrashock Laser W reaches 1200 and changes shape through terrain, Spark Surge E is a 300-unit terrain dash, and Lightning Crash R supplies a timed overcharge stack state. Runtime spell names and passive buff aliases are observed defensively because live SDK telemetry may omit transient Q/R buff names.";
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
