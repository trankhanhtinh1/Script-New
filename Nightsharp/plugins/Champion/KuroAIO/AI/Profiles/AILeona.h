#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Leona = [] {
    ChampionProfile p{};
    p.ChampionId = SDK::ChampionId::Leona;
    p.DisplayName = "Leona";
    p.InternalId = "champion.kuroaio.ai.leona";
    p.PrimaryArchetype = Archetype::Vanguard;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::Dash | Mechanic::Stance |
                  Mechanic::AutoReset | Mechanic::DirectionalSweet;
    p.Ultimate = UltimatePolicy::MultiTarget;

    p.Spells[0] = Spell(
        SDK::SpellSlot::Q, "Shield of Daybreak", CastKind::Self,
        Intent::Damage | Intent::CrowdControl | Intent::AutoReset |
            Intent::Setup | Intent::Peel | Intent::Interrupt,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::Automatic,
        125.0f, 0.0f, 125.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::Targeted);
    p.Spells[0].Priority = 98;
    p.Spells[0].TriggerRange = 125.0f;
    p.Spells[0].PreserveAutoAttack = true;
    p.Spells[0].WeaveAfterAttack = true;

    p.Spells[1] = Spell(
        SDK::SpellSlot::W, "Eclipse", CastKind::Self,
        Intent::Damage | Intent::Buff | Intent::Engage |
            Intent::Disengage | Intent::Peel | Intent::Setup,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::Automatic,
        275.0f, 0.0f, 275.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[1].Priority = 86;
    p.Spells[1].TriggerRange = 275.0f;
    p.Spells[1].HarassManaPercent = 58.0f;
    p.Spells[1].PreserveAutoAttack = true;

    p.Spells[2] = Spell(
        SDK::SpellSlot::E, "Zenith Blade", CastKind::Line,
        Intent::Damage | Intent::CrowdControl | Intent::Mobility |
            Intent::Engage | Intent::Disengage | Intent::Interrupt |
            Intent::AntiGapcloser | Intent::Peel | Intent::Setup,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::Automatic,
        875.0f, 0.25f, 70.0f, 1200.0f, true,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[2].Priority = 96;
    p.Spells[2].TriggerRange = 875.0f;
    p.Spells[2].Aim = AimPolicy::Prediction;
    p.Spells[2].Hitchance = SDK::HitChance::High;
    p.Spells[2].DashDistance = 875.0f;
    p.Spells[2].MaximumEnemiesAtDestination = 3;
    p.Spells[2].PreserveAutoAttack = true;

    p.Spells[3] = Spell(
        SDK::SpellSlot::R, "Solar Flare", CastKind::Position,
        Intent::Damage | Intent::CrowdControl | Intent::Engage |
            Intent::Disengage | Intent::Peel | Intent::Interrupt | Intent::Setup |
            Intent::Execute,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::Automatic,
        1200.0f, 0.625f, 300.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[3].Priority = 99;
    p.Spells[3].TriggerRange = 1200.0f;
    p.Spells[3].Aim = AimPolicy::Prediction;
    p.Spells[3].Hitchance = SDK::HitChance::High;
    p.Spells[3].MinimumAoeTargets = 2;
    p.Spells[3].MaximumEnemiesAtDestination = 4;
    p.Spells[3].PreserveAutoAttack = true;

    p.Trade = Plan(
        "E first-hit entry, W stance, Q reset stun and preserve R for conversion",
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireSafePosition),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget | StepRule::RequireSafePosition),
        Step(SDK::SpellSlot::Q,
             StepRule::RequireTarget | StepRule::RequireInsideAaRange |
                 StepRule::AllowDuringWindup));

    p.AllIn = Plan(
        "predict Solar Flare, Zenith Blade first hit, Eclipse stance and Q reset",
        Step(SDK::SpellSlot::R, StepRule::RequireTarget | StepRule::RequireSafePosition),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireSafePosition),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget),
        Step(SDK::SpellSlot::Q,
             StepRule::RequireTarget | StepRule::RequireInsideAaRange |
                 StepRule::AllowDuringWindup));

    p.Flee = Plan(
        "E aligned pursuer, W stance, Q stun reset and Solar Flare peel",
        Step(SDK::SpellSlot::W, StepRule::RequireSafePosition),
        Step(SDK::SpellSlot::Q,
             StepRule::RequireTarget | StepRule::RequireInsideAaRange |
                 StepRule::AllowDuringWindup),
        Step(SDK::SpellSlot::R, StepRule::RequireTarget | StepRule::RequireSafePosition));

    p.PreferredCombatDistance = 260.0f;
    p.EngageHealthPercent = 52.0f;
    p.DefensiveHealthPercent = 34.0f;
    p.UltimateTargetHealthPercent = 100.0f;
    p.UltimateMinimumTargets = 2;
    p.MaximumCommitEnemies = 3;
    p.BaseHumanizerMs = 42;
    p.PreferSelectedTarget = true;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "LeonaSunlight";
    p.FormBuff = "LeonaW";
    p.MarkBuff = "LeonaSunlight";
    p.UltimateBuff = "LeonaSolarFlare";
    p.ThemeFrom = 0xFFFFD35Au;
    p.ThemeTo = 0xFFEA7B26u;
    p.ThemeSpeed = 0.92f;
    p.TacticalSummary =
        "Peel-first vanguard route: use Zenith Blade's first predicted champion hit, "
        "arm Eclipse before entering danger, reset Shield of Daybreak into the next "
        "attack for a stun, and reserve Solar Flare for predicted multi-target engage "
        "or an ally-safe peel window.";
    p.ResearchSummary =
        "Riot 26.15 and CommunityDragon PC 16.15 spell data, current Season 16 "
        "support build and Challenger engage/peel patterns, plus deterministic "
        "Q/W/E/R geometry and safety regressions.";
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
