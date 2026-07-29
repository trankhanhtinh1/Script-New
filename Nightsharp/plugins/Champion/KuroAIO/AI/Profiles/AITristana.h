#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Tristana = [] {
    ChampionProfile p{};
    p.ChampionName = "Tristana";
    p.DisplayName = "Tristana";
    p.InternalId = "champion.kuroaio.ai.tristana";
    p.PrimaryArchetype = Archetype::Marksman;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::AutoWeave | Mechanic::Stack |
                  Mechanic::Mark | Mechanic::Dash;
    p.Ultimate = UltimatePolicy::Execute;

    p.Spells[0] = Spell(
        SDK::SpellSlot::Q, "Rapid Fire", CastKind::Self,
        Intent::Buff | Intent::Damage | Intent::AutoReset |
            Intent::Jungle | Intent::Objective,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle,
        0.0f, 0.0f, 0.0f, FLT_MAX, false,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotCircle);
    p.Spells[0].Priority = 97;
    p.Spells[0].PreserveAutoAttack = false;
    p.Spells[0].HarassManaPercent = 36.0f;

    p.Spells[1] = Spell(
        SDK::SpellSlot::W, "Rocket Jump", CastKind::Circle,
        Intent::Damage | Intent::Mobility | Intent::Engage |
            Intent::Disengage | Intent::Execute | Intent::Finisher,
        Mode::Combo | Mode::Flee | Mode::Automatic,
        900.0f, 0.50f, 270.0f, 1500.0f, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[1].Priority = 91;
    p.Spells[1].Hitchance = SDK::HitChance::High;
    p.Spells[1].DashDistance = 900.0f;
    p.Spells[1].MaximumEnemiesAtDestination = 1;
    p.Spells[1].PreserveAutoAttack = true;

    p.Spells[2] = Spell(
        SDK::SpellSlot::E, "Explosive Charge", CastKind::EnemyTarget,
        Intent::Damage | Intent::Setup | Intent::Execute |
            Intent::Waveclear | Intent::Jungle | Intent::Objective,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::Automatic,
        750.0f, 0.25f, 300.0f, 2400.0f, false,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotCircle);
    p.Spells[2].Priority = 99;
    p.Spells[2].PreserveAutoAttack = false;
    p.Spells[2].HarassManaPercent = 42.0f;
    p.Spells[2].AllowOnMinions = true;

    p.Spells[3] = Spell(
        SDK::SpellSlot::R, "Buster Shot", CastKind::EnemyTarget,
        Intent::Damage | Intent::Execute | Intent::Disengage |
            Intent::Peel | Intent::AntiGapcloser | Intent::Finisher,
        Mode::Combo | Mode::Flee | Mode::Automatic,
        750.0f, 0.25f, 200.0f, 2000.0f, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[3].Priority = 100;
    p.Spells[3].PreserveAutoAttack = true;

    p.Trade = Plan(
        "Attach E to the actual orbwalker target and enable Q before attacks",
        Step(SDK::SpellSlot::E,
             StepRule::RequireTarget | StepRule::AllowDuringWindup),
        Step(SDK::SpellSlot::Q,
             StepRule::RequireTarget | StepRule::AllowDuringWindup));
    p.AllIn = Plan(
        "Keep attacking the E target; reserve W and R for lethal or peel",
        Step(SDK::SpellSlot::E, StepRule::RequireTarget),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget),
        Step(SDK::SpellSlot::R,
             StepRule::RequireTarget | StepRule::RequireTargetLow));
    p.Flee = Plan(
        "Buster Shot the pursuer first, then Rocket Jump to a safe cursor point",
        Step(SDK::SpellSlot::R,
             StepRule::RequireTarget | StepRule::AllowDuringWindup),
        Step(SDK::SpellSlot::W,
             StepRule::RequireSafePosition | StepRule::AllowDuringWindup));

    p.PreferredCombatDistance = 610.0f;
    p.EngageHealthPercent = 66.0f;
    p.DefensiveHealthPercent = 32.0f;
    p.UltimateTargetHealthPercent = 25.0f;
    p.MaximumCommitEnemies = 1;
    p.BaseHumanizerMs = 20;
    p.PreferSelectedTarget = true;
    p.AllowTurretDiveIfKillable = false;
    p.MarkBuff = "TristanaECharge";
    p.ThemeFrom = 0xFF5EA8FFu;
    p.ThemeTo = 0xFFFF70D5u;
    p.ThemeSpeed = 1.0f;
    p.TacticalSummary =
        "Bind E and Q to the real orbwalker attack, force the reachable charged "
        "target until stacks finish, and never spend W/R unless the landing, "
        "detonation, execute or peel result is explicitly validated.";
    p.ResearchSummary =
        "Ported from TestOrbwalker Tristana.cs BeforeAttack/E focus logic and "
        "checked against current CommunityDragon 4-stack E and 900 W data.";
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
