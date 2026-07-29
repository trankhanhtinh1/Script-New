#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Twitch = [] {
    ChampionProfile p{};
    p.ChampionName = "Twitch";
    p.DisplayName = "Twitch";
    p.InternalId = "champion.kuroaio.ai.twitch";
    p.PrimaryArchetype = Archetype::Marksman;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::AutoWeave | Mechanic::Stack |
                  Mechanic::Mark | Mechanic::Stance;
    p.Ultimate = UltimatePolicy::MultiTarget;

    p.Spells[0] = Spell(
        SDK::SpellSlot::Q, "Ambush", CastKind::Self,
        Intent::Buff | Intent::Mobility | Intent::Engage |
            Intent::Disengage | Intent::Setup,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::Automatic,
        0.0f, 0.0f, 0.0f, FLT_MAX, false,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotCircle);
    p.Spells[0].Priority = 88;
    p.Spells[0].PreserveAutoAttack = true;
    p.Spells[0].HarassManaPercent = 44.0f;

    p.Spells[1] = Spell(
        SDK::SpellSlot::W, "Venom Cask", CastKind::Circle,
        Intent::CrowdControl | Intent::Setup | Intent::Peel |
            Intent::Waveclear | Intent::Jungle,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::Flee,
        950.0f, 0.25f, 275.0f, 1750.0f, false,
        SDK::DamageType::True, SDK::SpellType::SkillshotCircle);
    p.Spells[1].Priority = 91;
    p.Spells[1].Hitchance = SDK::HitChance::High;
    p.Spells[1].PreserveAutoAttack = true;
    p.Spells[1].HarassManaPercent = 47.0f;
    p.Spells[1].ClearManaPercent = 52.0f;
    p.Spells[1].AllowOnMinions = true;

    p.Spells[2] = Spell(
        SDK::SpellSlot::E, "Contaminate", CastKind::Self,
        Intent::Damage | Intent::Execute | Intent::Finisher |
            Intent::Waveclear | Intent::Jungle,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::Automatic,
        1200.0f, 0.0f, 1200.0f, FLT_MAX, false,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotCircle);
    p.Spells[2].Priority = 99;
    p.Spells[2].PreserveAutoAttack = true;
    p.Spells[2].HarassManaPercent = 34.0f;

    p.Spells[3] = Spell(
        SDK::SpellSlot::R, "Spray and Pray", CastKind::Self,
        Intent::Buff | Intent::Damage | Intent::Engage |
            Intent::Execute | Intent::Objective,
        Mode::Combo | Mode::Automatic,
        0.0f, 0.0f, 0.0f, FLT_MAX, false,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotLine);
    p.Spells[3].Priority = 98;
    p.Spells[3].MinimumAoeTargets = 2;
    p.Spells[3].PreserveAutoAttack = false;

    p.Trade = Plan(
        "Slow an escaping target, stack venom with attacks, then E before range loss",
        Step(SDK::SpellSlot::W, StepRule::RequireTarget),
        Step(SDK::SpellSlot::E,
             StepRule::RequireTarget | StepRule::HoldForExecute));
    p.AllIn = Plan(
        "Open R before a valuable attack window and preserve six-stack E",
        Step(SDK::SpellSlot::R,
             StepRule::RequireTarget | StepRule::AllowDuringWindup),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget),
        Step(SDK::SpellSlot::E,
             StepRule::RequireTarget | StepRule::HoldForExecute));
    p.Flee = Plan(
        "Cask the pursuer and Ambush only when incoming damage will not cancel it",
        Step(SDK::SpellSlot::W,
             StepRule::RequireTarget | StepRule::AllowDuringWindup),
        Step(SDK::SpellSlot::Q,
             StepRule::RequireSafePosition | StepRule::AllowDuringWindup));

    p.PreferredCombatDistance = 690.0f;
    p.EngageHealthPercent = 60.0f;
    p.DefensiveHealthPercent = 29.0f;
    p.UltimateTargetHealthPercent = 58.0f;
    p.UltimateMinimumTargets = 2;
    p.MaximumCommitEnemies = 2;
    p.BaseHumanizerMs = 21;
    p.PreferSelectedTarget = true;
    p.AllowTurretDiveIfKillable = false;
    p.MarkBuff = "TwitchDeadlyVenom";
    p.FormBuff = "TwitchHideInShadows";
    p.UltimateBuff = "TwitchUlt";
    p.ThemeFrom = 0xFF7BE56Fu;
    p.ThemeTo = 0xFF8C49CEu;
    p.ThemeSpeed = 0.96f;
    p.TacticalSummary =
        "Maintain a reachable venom target through the orbwalker, delay E until "
        "lethal/six stacks/range loss, use W only when attacks cannot do the job, "
        "and activate R before the attack whose extra range or piercing matters.";
    p.ResearchSummary =
        "Ported from TestOrbwalker Twitch.cs and checked against current "
        "CommunityDragon E stack formula, 1200 E range and +300 R range.";
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
