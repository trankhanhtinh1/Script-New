#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Illaoi = [] {
    ChampionProfile p{};
    p.ChampionName = "Illaoi";
    p.DisplayName = "Illaoi";
    p.InternalId = "champion.kuroaio.ai.illaoi";
    p.PrimaryArchetype = Archetype::Juggernaut;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::ObjectTracking | Mechanic::Recast | Mechanic::Dash |
                  Mechanic::Mark | Mechanic::AutoReset | Mechanic::DirectionalSweet;
    p.Ultimate = UltimatePolicy::MultiTarget;
    p.PreferredCombatDistance = 350.0f;
    p.EngageHealthPercent = 58.0f;
    p.DefensiveHealthPercent = 30.0f;
    p.UltimateTargetHealthPercent = 100.0f;
    p.UltimateMinimumTargets = 2;
    p.MaximumCommitEnemies = 3;
    p.BaseHumanizerMs = 70;
    p.PreferSelectedTarget = true;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "IllaoiPassive";
    p.MarkBuff = "IllaoiEVessel";
    p.FormBuff = "IllaoiR";
    p.TacticalSummary =
        "Tentacle juggernaut: maintain nearby passive tentacles, slam Q through"
        " predicted lanes, reserve W leap/reset for reachable attacks, track E"
        " spirit vessels, and use R only when a safe multi-target fight can grow"
        " the tentacle field.";
    p.ResearchSummary =
        "Riot 26.15 / CommunityDragon 16.15: Q 825 range, 0.75-second slam and"
        " 105 width; W 350-range six-second empowered attack reset; E 950-range"
        " 1900-speed spirit projectile with 4-second vessel and 7-second spirit"
        " windows; R 450 range, 500 radius, eight-second field and enemy-scaled"
        " tentacle spawns.";

    p.Spells[0] = Spell(
        SDK::SpellSlot::Q, "Tentacle Smash", CastKind::Line,
        Intent::Damage | Intent::CrowdControl | Intent::Setup | Intent::Waveclear |
            Intent::Jungle | Intent::LastHit,
        AllModes, 825.0f, 0.75f, 105.0f, FLT_MAX, false,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotLine);
    p.Spells[0].Aim = AimPolicy::Prediction;
    p.Spells[0].Priority = 93;
    p.Spells[0].Hitchance = SDK::HitChance::High;
    p.Spells[0].AllowOnMinions = true;
    p.Spells[0].HarassManaPercent = 38.0f;
    p.Spells[0].ClearManaPercent = 28.0f;
    p.Spells[0].PreserveAutoAttack = true;

    p.Spells[1] = Spell(
        SDK::SpellSlot::W, "Harsh Lesson", CastKind::EnemyTarget,
        Intent::Damage | Intent::Engage | Intent::AutoReset | Intent::Recast,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::LastHit | Mode::Flee | Mode::Automatic,
        350.0f, 0.25f, 350.0f, FLT_MAX, false,
        SDK::DamageType::Physical, SDK::SpellType::Targeted);
    p.Spells[1].Aim = AimPolicy::TargetPosition;
    p.Spells[1].Priority = 96;
    p.Spells[1].RecastSpellName = "IllaoiWAttack";
    p.Spells[1].WeaveAfterAttack = true;
    p.Spells[1].PreserveAutoAttack = false;

    p.Spells[2] = Spell(
        SDK::SpellSlot::E, "Test of Spirit", CastKind::Line,
        Intent::Damage | Intent::CrowdControl | Intent::Setup | Intent::Recast,
        Mode::Combo | Mode::Harass | Mode::Automatic,
        950.0f, 0.25f, 50.0f, 1900.0f, true,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotLine);
    p.Spells[2].Aim = AimPolicy::Prediction;
    p.Spells[2].Priority = 98;
    p.Spells[2].Hitchance = SDK::HitChance::High;
    p.Spells[2].PreserveAutoAttack = true;

    p.Spells[3] = Spell(
        SDK::SpellSlot::R, "Leap of Faith", CastKind::Self,
        Intent::Damage | Intent::Engage | Intent::Buff | Intent::Setup,
        Mode::Combo | Mode::Flee | Mode::Automatic,
        450.0f, 0.5f, 500.0f, FLT_MAX, false,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotCircle);
    p.Spells[3].Aim = AimPolicy::SelfPosition;
    p.Spells[3].Priority = 100;
    p.Spells[3].MinimumAoeTargets = 2;
    p.Spells[3].MaximumEnemiesAtDestination = 3;

    p.Trade = Plan("Spirit pull into Q and Harsh Lesson reset",
        Step(SDK::SpellSlot::E, StepRule::RequireTarget, 0, 1100),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget, 180, 1000),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget | StepRule::AllowDuringWindup,
             300, 800));
    p.AllIn = Plan("Tentacle field all-in",
        Step(SDK::SpellSlot::E, StepRule::RequireTarget, 0, 1100),
        Step(SDK::SpellSlot::R, StepRule::RequireSafePosition | StepRule::RequireTarget,
             180, 700),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget, 320, 1000),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget | StepRule::AllowDuringWindup,
             420, 850));
    p.Flee = Plan("Peel with Q and spirit slow",
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequireSafePosition,
             0, 900),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireSafePosition,
             120, 1050));
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
