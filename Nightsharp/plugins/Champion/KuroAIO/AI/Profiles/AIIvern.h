#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Ivern = [] {
    ChampionProfile p{};
    p.ChampionId = SDK::ChampionId::Ivern;
    p.DisplayName = "Ivern";
    p.InternalId = "champion.kuroaio.ai.ivern";
    p.PrimaryArchetype = Archetype::Enchanter;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::Mark | Mechanic::Pet | Mechanic::ObjectTracking |
                  Mechanic::AllyTarget | Mechanic::Dash | Mechanic::Terrain;
    p.Ultimate = UltimatePolicy::Defensive;
    p.PreferredCombatDistance = 700.0f;
    p.EngageHealthPercent = 55.0f;
    p.DefensiveHealthPercent = 42.0f;
    p.UltimateTargetHealthPercent = 100.0f;
    p.UltimateMinimumTargets = 1;
    p.MaximumCommitEnemies = 2;
    p.BaseHumanizerMs = 65;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "IvernPassive";
    p.MarkBuff = "IvernPassive";
    p.TrackedObjectToken = "Daisy";
    p.ThemeFrom = 0xFF7DE2A8u;
    p.ThemeTo = 0xFFB8F6D5u;
    p.TacticalSummary =
        "Grove-support controller: mark camps safely, root and dash on Q, "
        "place empowering brush, shield allies with E, and maintain Daisy only "
        "for a safe fight or epic-objective contest.";
    p.ResearchSummary =
        "Riot 26.15 / CommunityDragon 16.15; Summoner's Rift values only, "
        "with live grove, brush, Triggerseed and Daisy state reconciliation.";

    p.Spells[0] = Spell(
        SDK::SpellSlot::Q, "Rootcaller", CastKind::Line,
        Intent::Damage | Intent::CrowdControl | Intent::Mobility | Intent::Setup |
            Intent::Jungle | Intent::LastHit | Intent::AutoWeave,
        Mode::Combo | Mode::Harass | Mode::Jungle | Mode::LastHit | Mode::Flee |
            Mode::Automatic,
        1100.0f, 0.25f, 80.0f, 1300.0f, true,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[0].Aim = AimPolicy::Prediction;
    p.Spells[0].Priority = 92;
    p.Spells[0].Hitchance = SDK::HitChance::High;
    p.Spells[0].WeaveAfterAttack = true;
    p.Spells[0].DashDistance = 1100.0f;

    p.Spells[1] = Spell(
        SDK::SpellSlot::W, "Brushmaker", CastKind::Position,
        Intent::Damage | Intent::Buff | Intent::Vision | Intent::Setup | Intent::Waveclear |
            Intent::Jungle | Intent::AutoReset,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle | Mode::Automatic,
        1000.0f, 0.25f, 160.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[1].Aim = AimPolicy::BetweenPlayerAndTarget;
    p.Spells[1].Priority = 72;
    p.Spells[1].MinimumAoeTargets = 1;
    p.Spells[1].WeaveAfterAttack = true;

    p.Spells[2] = Spell(
        SDK::SpellSlot::E, "Triggerseed", CastKind::AllyTarget,
        Intent::Shield | Intent::Damage | Intent::CrowdControl | Intent::AllyUtility |
            Intent::Peel | Intent::AntiGapcloser | Intent::Jungle | Intent::Objective,
        Mode::Combo | Mode::Harass | Mode::Jungle | Mode::Automatic | Mode::Flee,
        700.0f, 0.0f, 325.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[2].Aim = AimPolicy::SelfPosition;
    p.Spells[2].Priority = 98;
    p.Spells[2].MaximumEnemiesAtDestination = 3;
    p.Spells[2].AllowOnMinions = false;

    p.Spells[3] = Spell(
        SDK::SpellSlot::R, "Daisy!", CastKind::Self,
        Intent::Damage | Intent::CrowdControl | Intent::Engage | Intent::Disengage |
            Intent::Objective | Intent::AllyUtility,
        Mode::Combo | Mode::Jungle | Mode::Automatic | Mode::Flee,
        0.0f, 0.25f, 325.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[3].Aim = AimPolicy::SelfPosition;
    p.Spells[3].Priority = 96;
    p.Spells[3].MinimumAoeTargets = 1;
    p.Spells[3].TargetHealthPercent = 100.0f;

    p.Variants[0] = {SDK::SpellSlot::Q, "IvernQ", p.Spells[0]};
    p.Variants[1] = {SDK::SpellSlot::W, "IvernW", p.Spells[1]};
    p.Variants[2] = {SDK::SpellSlot::E, "IvernE", p.Spells[2]};
    p.Variants[3] = {SDK::SpellSlot::R, "IvernR", p.Spells[3]};
    p.VariantCount = 4;

    p.Trade = Plan("Rootcaller brush trade",
        Step(SDK::SpellSlot::W, StepRule::RequireSafePosition, 0, 650),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequireAfterAttack, 80, 900),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::AllowDuringWindup, 150, 1000));
    p.AllIn = Plan("Daisy root and shield engage",
        Step(SDK::SpellSlot::W, StepRule::RequireSafePosition, 0, 600),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequireAfterAttack, 80, 900),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::AllowDuringWindup, 160, 1100),
        Step(SDK::SpellSlot::R, StepRule::RequireSafePosition | StepRule::RequireMultiTarget, 240, 1300));
    p.Flee = Plan("Rootcaller and ally peel",
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequirePlayerLow | StepRule::AllowDuringWindup, 0, 700),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequireNoCrowdControl, 90, 900));
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
