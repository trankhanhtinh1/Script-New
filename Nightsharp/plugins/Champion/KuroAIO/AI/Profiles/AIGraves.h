#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Graves = [] {
    ChampionProfile p{};
    p.ChampionName = "Graves";
    p.DisplayName = "Graves";
    p.InternalId = "champion.kuroaio.ai.graves";
    p.PrimaryArchetype = Archetype::Marksman;
    p.Resource = ResourceModel::Ammo;
    p.Mechanics = Mechanic::Ammo | Mechanic::Dash | Mechanic::Terrain |
                  Mechanic::AutoWeave | Mechanic::DirectionalSweet;
    p.Ultimate = UltimatePolicy::Execute;
    p.PreferredCombatDistance = 425.0f;
    p.EngageHealthPercent = 58.0f;
    p.DefensiveHealthPercent = 30.0f;
    p.UltimateTargetHealthPercent = 72.0f;
    p.UltimateMinimumTargets = 2;
    p.MaximumCommitEnemies = 2;
    p.BaseHumanizerMs = 55;
    p.PreferSelectedTarget = true;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "GravesBasicAttackAmmo";
    p.MarkBuff = "GravesEGrit";
    p.TrackedObjectToken = "GravesSmokeCloud";
    p.TacticalSummary =
        "Shell-aware shotgun marksman: hold Q for the end-wall explosion, place W smoke "
        "only through a clear vision-safe route, use E after a shot for True Grit armor "
        "and reload tempo, and reserve R for a first-collision-safe recoil execute.";
    p.ResearchSummary =
        "Pinned to Riot 26.15 and CommunityDragon 16.15: Q 800 cast/925 display range "
        "with 0.25 second terrain detonation, W 950/225 smoke at 1650 speed, E 425 "
        "Quickdraw with 4 second stacking armor/MR, and R 1000 display range with 400 "
        "recoil and 100 width physical line.";

    p.Spells[0] = Spell(
        SDK::SpellSlot::Q, "End of the Line", CastKind::Line,
        Intent::Damage | Intent::Setup | Intent::Finisher | Intent::Waveclear |
            Intent::Jungle | Intent::LastHit,
        AllModes, 925.0f, 0.25f, 100.0f, 902.0f, false,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotLine);
    p.Spells[0].Aim = AimPolicy::Prediction;
    p.Spells[0].Priority = 90;
    p.Spells[0].Hitchance = SDK::HitChance::High;
    p.Spells[0].Collision = false;
    p.Spells[0].MinimumAmmo = 1;
    p.Spells[0].PreserveAutoAttack = true;

    p.Spells[1] = Spell(
        SDK::SpellSlot::W, "Smoke Screen", CastKind::Circle,
        Intent::Damage | Intent::CrowdControl | Intent::Vision | Intent::Peel |
            Intent::Setup | Intent::Waveclear | Intent::Jungle,
        Mode::Combo | Mode::Harass | Mode::Jungle | Mode::Automatic | Mode::Flee,
        950.0f, 0.25f, 225.0f, 1650.0f, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[1].Aim = AimPolicy::Prediction;
    p.Spells[1].Priority = 76;
    p.Spells[1].Hitchance = SDK::HitChance::High;
    p.Spells[1].Collision = true;
    p.Spells[1].PreserveAutoAttack = true;

    p.Spells[2] = Spell(
        SDK::SpellSlot::E, "Quickdraw", CastKind::Position,
        Intent::Mobility | Intent::Disengage | Intent::Engage | Intent::Buff |
            Intent::AntiGapcloser | Intent::AutoReset | Intent::Jungle,
        AllModes, 425.0f, 0.05f, 20.0f, FLT_MAX, false,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotLine);
    p.Spells[2].Aim = AimPolicy::SafeCursor;
    p.Spells[2].Priority = 83;
    p.Spells[2].WeaveAfterAttack = true;
    p.Spells[2].PreserveAutoAttack = false;
    p.Spells[2].DashDistance = 425.0f;

    p.Spells[3] = Spell(
        SDK::SpellSlot::R, "Collateral Damage", CastKind::Line,
        Intent::Damage | Intent::Execute | Intent::Finisher | Intent::Disengage |
            Intent::Objective,
        Mode::Combo | Mode::Automatic | Mode::Flee,
        1000.0f, 0.25f, 100.0f, 1400.0f, false,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotLine);
    p.Spells[3].Aim = AimPolicy::Prediction;
    p.Spells[3].Priority = 100;
    p.Spells[3].Hitchance = SDK::HitChance::High;
    p.Spells[3].Collision = true;
    p.Spells[3].TargetHealthPercent = 72.0f;
    p.Spells[3].MaximumEnemiesAtDestination = 2;
    p.Spells[3].PreserveAutoAttack = true;

    p.Trade = Plan("Smoke, shell and safe Quickdraw",
        Step(SDK::SpellSlot::W, StepRule::RequireTarget | StepRule::RequireNoCrowdControl, 0, 1000),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequireFirstCast, 80, 1100),
        Step(SDK::SpellSlot::E, StepRule::RequireAfterAttack | StepRule::RequireSafePosition, 180, 1250));
    p.AllIn = Plan("End-wall buckshot and Collateral recoil",
        Step(SDK::SpellSlot::W, StepRule::RequireTarget, 0, 1000),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequireFirstCast, 80, 1200),
        Step(SDK::SpellSlot::E, StepRule::RequireAfterAttack | StepRule::RequireSafePosition, 180, 1300),
        Step(SDK::SpellSlot::R, StepRule::RequireTarget | StepRule::RequireTargetLow | StepRule::RequireSafePosition, 260, 1700));
    p.Flee = Plan("Smoke peel and recoil escape",
        Step(SDK::SpellSlot::W, StepRule::RequireTarget | StepRule::RequireSafePosition, 0, 900),
        Step(SDK::SpellSlot::E, StepRule::RequireSafePosition | StepRule::AllowDuringWindup, 70, 900),
        Step(SDK::SpellSlot::R, StepRule::RequireTarget | StepRule::RequireSafePosition | StepRule::ManualAssistOnly, 150, 1600));
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
