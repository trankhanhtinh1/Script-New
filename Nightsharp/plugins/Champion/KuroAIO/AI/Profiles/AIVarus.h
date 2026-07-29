#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Varus = [] {
    ChampionProfile p{};
    p.ChampionName = "Varus";
    p.DisplayName = "Varus";
    p.InternalId = "champion.kuroaio.ai.varus";
    p.PrimaryArchetype = Archetype::Marksman;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::Charge | Mechanic::Stack |
                  Mechanic::Mark | Mechanic::AutoWeave |
                  Mechanic::MissingHealth;
    p.Ultimate = UltimatePolicy::ManualAssist;

    p.Spells[0] = Spell(
        SDK::SpellSlot::Q, "Piercing Arrow", CastKind::ChargedLine,
        Intent::Damage | Intent::Execute | Intent::Waveclear |
            Intent::LastHit | Intent::Jungle | Intent::Objective |
            Intent::Finisher,
        Mode::Combo | Mode::Harass | Mode::LaneClear |
            Mode::Jungle | Mode::LastHit | Mode::Automatic,
        1625.0f, 0.25f, 70.0f, 1500.0f, false,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotLine);
    p.Spells[0].Priority = 99;
    p.Spells[0].Hitchance = SDK::HitChance::High;
    p.Spells[0].ChargeBuffName = "VarusQLaunch";
    p.Spells[0].ChargeMinRange = 925;
    p.Spells[0].ChargeMaxRange = 1625;
    p.Spells[0].ChargeDurationSeconds = 1.25f;
    p.Spells[0].PreserveAutoAttack = true;
    p.Spells[0].HarassManaPercent = 42.0f;
    p.Spells[0].ClearManaPercent = 48.0f;
    p.Spells[0].AllowOnMinions = true;

    p.Spells[1] = Spell(
        SDK::SpellSlot::W, "Blighted Quiver", CastKind::Self,
        Intent::Buff | Intent::Damage | Intent::Execute | Intent::Setup,
        Mode::Combo | Mode::Harass | Mode::Automatic,
        0.0f, 0.0f, 0.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[1].Priority = 97;
    p.Spells[1].PreserveAutoAttack = true;

    p.Spells[2] = Spell(
        SDK::SpellSlot::E, "Hail of Arrows", CastKind::Circle,
        Intent::Damage | Intent::CrowdControl | Intent::Setup |
            Intent::Execute | Intent::Waveclear | Intent::Jungle |
            Intent::Objective,
        Mode::Combo | Mode::Harass | Mode::LaneClear |
            Mode::Jungle | Mode::Automatic,
        925.0f, 1.0f, 250.0f, 1750.0f, false,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotCircle);
    p.Spells[2].Priority = 94;
    p.Spells[2].Hitchance = SDK::HitChance::High;
    p.Spells[2].PreserveAutoAttack = true;
    p.Spells[2].HarassManaPercent = 46.0f;
    p.Spells[2].ClearManaPercent = 50.0f;

    p.Spells[3] = Spell(
        SDK::SpellSlot::R, "Chain of Corruption", CastKind::Line,
        Intent::Damage | Intent::CrowdControl | Intent::Engage |
            Intent::Peel | Intent::Interrupt | Intent::AntiGapcloser,
        Mode::Combo | Mode::Flee | Mode::Automatic,
        1200.0f, 0.25f, 120.0f, 1200.0f, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[3].Priority = 100;
    p.Spells[3].Hitchance = SDK::HitChance::VeryHigh;

    p.Trade = Plan(
        "Build three Blight stacks with autos, then detonate using E or charged Q",
        Step(SDK::SpellSlot::E,
             StepRule::RequireTarget | StepRule::RequireMark),
        Step(SDK::SpellSlot::Q,
             StepRule::RequireTarget | StepRule::RequireMark));
    p.AllIn = Plan(
        "R catch, autos for Blight, E detonation, W-charged Q execute",
        Step(SDK::SpellSlot::R, StepRule::RequireTarget),
        Step(SDK::SpellSlot::E,
             StepRule::RequireTarget | StepRule::RequireMark),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget),
        Step(SDK::SpellSlot::Q,
             StepRule::RequireTarget | StepRule::RequireMark));
    p.Flee = Plan(
        "R the committed pursuer; otherwise E slow without charging Q",
        Step(SDK::SpellSlot::R,
             StepRule::RequireTarget | StepRule::AllowDuringWindup),
        Step(SDK::SpellSlot::E,
             StepRule::RequireTarget | StepRule::AllowDuringWindup));

    p.PreferredCombatDistance = 575.0f;
    p.EngageHealthPercent = 60.0f;
    p.DefensiveHealthPercent = 32.0f;
    p.UltimateTargetHealthPercent = 52.0f;
    p.MaximumCommitEnemies = 2;
    p.BaseHumanizerMs = 21;
    p.PreferSelectedTarget = true;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.MarkBuff = "varuswdebuff";
    p.ChannelBuff = "VarusQLaunch";
    p.ThemeFrom = 0xFFB765FFu;
    p.ThemeTo = 0xFFEA446Du;
    p.ThemeSpeed = 0.94f;
    p.TacticalSummary =
        "Do not detonate Blight early when another auto is safe; charge Q only "
        "for a reachable predicted target, release as soon as required range and "
        "hit confidence are met, empower only a committed Q, and reserve R for "
        "manual catch, interrupt or immediate peel.";
    p.ResearchSummary =
        "Ported from TestOrbwalker AllChampions/Varus.cs: two/three-stack wait, "
        "charged-Q range logic, W-before-Q execute, E fallback and semi-manual R; "
        "upgraded with charge-state reconciliation and reachable-target scoring.";
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
