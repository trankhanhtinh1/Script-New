#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Smolder = [] {
    ChampionProfile p{};
    p.ChampionId = SDK::ChampionId::Smolder;
    p.DisplayName = "Smolder";
    p.InternalId = "champion.kuroaio.ai.smolder";
    p.PrimaryArchetype = Archetype::Marksman;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::Stack | Mechanic::Evolve | Mechanic::Execute |
                  Mechanic::Dash | Mechanic::AutoWeave | Mechanic::MissingHealth;
    p.Ultimate = UltimatePolicy::AllIn;
    p.PreferredCombatDistance = 700.0f;
    p.EngageHealthPercent = 62.0f;
    p.DefensiveHealthPercent = 34.0f;
    p.UltimateTargetHealthPercent = 48.0f;
    p.UltimateMinimumTargets = 2;
    p.MaximumCommitEnemies = 2;
    p.BaseHumanizerMs = 28;
    p.PreferSelectedTarget = true;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "SmolderDragonPractice";
    p.TrackedObjectToken = "SmolderQExplosion";

    p.Spells[0] = Spell(
        SDK::SpellSlot::Q, "Super Scorcher Breath", CastKind::EnemyTarget,
        Intent::Damage | Intent::Execute | Intent::AutoReset |
            Intent::Waveclear | Intent::Jungle | Intent::LastHit,
        AllModes, 925.0f, 0.25f, 100.0f, 1300.0f, false,
        SDK::DamageType::Physical, SDK::SpellType::Targeted);
    p.Spells[0].Aim = AimPolicy::TargetPosition;
    p.Spells[0].Priority = 98;
    p.Spells[0].Hitchance = SDK::HitChance::High;
    p.Spells[0].WeaveAfterAttack = true;
    p.Spells[0].AllowOnMinions = true;

    p.Spells[1] = Spell(
        SDK::SpellSlot::W, "Achoo!", CastKind::Line,
        Intent::Damage | Intent::CrowdControl | Intent::Setup |
            Intent::Waveclear | Intent::Jungle,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::Automatic,
        1100.0f, 0.25f, 100.0f, 1200.0f, true,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotLine);
    p.Spells[1].Aim = AimPolicy::Prediction;
    p.Spells[1].Priority = 91;
    p.Spells[1].Hitchance = SDK::HitChance::High;
    p.Spells[1].PreserveAutoAttack = true;

    p.Spells[2] = Spell(
        SDK::SpellSlot::E, "Flap, Flap!", CastKind::Position,
        Intent::Mobility | Intent::Disengage | Intent::Engage | Intent::Damage,
        Mode::Combo | Mode::Flee | Mode::Automatic,
        1200.0f, 0.10f, 0.0f, FLT_MAX, false,
        SDK::DamageType::Physical, SDK::SpellType::Targeted);
    p.Spells[2].Aim = AimPolicy::SafeCursor;
    p.Spells[2].Priority = 96;
    p.Spells[2].MaximumEnemiesAtDestination = 1;

    p.Spells[3] = Spell(
        SDK::SpellSlot::R, "MMOOOMMMM!", CastKind::Circle,
        Intent::Damage | Intent::Heal | Intent::Execute | Intent::Finisher |
            Intent::CrowdControl | Intent::Peel,
        Mode::Combo | Mode::Flee | Mode::Automatic,
        1200.0f, 1.0f, 350.0f, FLT_MAX, false,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotCircle);
    p.Spells[3].Aim = AimPolicy::BestAoe;
    p.Spells[3].Priority = 100;
    p.Spells[3].MinimumAoeTargets = 2;

    p.Trade = Plan(
        "Stack-safe poke: weave Q after attacks, then W only on a clear line",
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::AllowDuringWindup, 0, 650),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget, 90, 800));
    p.AllIn = Plan(
        "W slow into Q evolution damage, reserve E for a safe angle and R for area execute/heal",
        Step(SDK::SpellSlot::W, StepRule::RequireTarget, 0, 900),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::AllowDuringWindup, 90, 900),
        Step(SDK::SpellSlot::E, StepRule::RequireSafePosition, 150, 1000),
        Step(SDK::SpellSlot::R, StepRule::RequireTarget | StepRule::RequireMultiTarget, 220, 1400));
    p.Flee = Plan(
        "Break pursuit with an away-from-threat flight and defensive healing R",
        Step(SDK::SpellSlot::E, StepRule::RequireSafePosition | StepRule::AllowDuringWindup, 0, 850),
        Step(SDK::SpellSlot::R, StepRule::RequirePlayerLow | StepRule::AllowDuringWindup, 100, 1200));

    p.ThemeFrom = 0xFF73D7FFu;
    p.ThemeTo = 0xFFFFA14Bu;
    p.ThemeSpeed = 0.85f;
    p.TacticalSummary =
        "Build Dragon Practice through observed champion and large-unit contacts, evolve Q at 25/125/225 stacks, use W for line slow and setup, protect E's flight endpoint from walls/turrets/threats, and reserve R for area value, execute or emergency heal.";
    p.ResearchSummary =
        "Riot 26.15 / CommunityDragon PC 16.15 Summoner's Rift baseline; passive stack thresholds and Q evolution are reconciled from buff polling/events while damage and reach stay in the owned geometry dossier.";
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
