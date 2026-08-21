#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Sett = [] {
    ChampionProfile p{};
    p.ChampionId = SDK::ChampionId::Sett;
    p.DisplayName = "The Boss";
    p.InternalId = "champion.kuroaio.ai.sett";
    p.PrimaryArchetype = Archetype::Juggernaut;
    p.Resource = ResourceModel::Special;
    p.Mechanics = Mechanic::Stack | Mechanic::AutoReset | Mechanic::DirectionalSweet |
                  Mechanic::MissingHealth | Mechanic::Dash;
    p.Ultimate = UltimatePolicy::AllIn;
    p.PreferredCombatDistance = 175.0f;
    p.EngageHealthPercent = 46.0f;
    p.DefensiveHealthPercent = 40.0f;
    p.UltimateTargetHealthPercent = 65.0f;
    p.UltimateMinimumTargets = 1;
    p.MaximumCommitEnemies = 3;
    p.BaseHumanizerMs = 70;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "SettPassive";
    p.MarkBuff = "SettQ";
    p.ChannelBuff = "SettW";
    p.UltimateBuff = "SettR";
    p.TacticalSummary =
        "Grit-aware juggernaut: preserve a real Q two-hit reset, aim Haymaker's true center, "
        "pull opposite-side targets with Facebreaker, and only slam Showstopper into a safe endpoint.";
    p.ResearchSummary =
        "Riot 26.15 / CommunityDragon 16.15: Pit Grit stores incoming damage, Q empowers two attacks, "
        "W converts Grit into a shield and center true damage, E displaces enemies from both sides, "
        "and R carries a target to a target-directed landing crater.";

    p.Spells[0] = Spell(
        SDK::SpellSlot::Q, "Knuckle Down", CastKind::Self,
        Intent::Damage | Intent::Buff | Intent::Mobility | Intent::AutoReset,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::LastHit | Mode::Flee | Mode::Automatic,
        0.0f, 0.33f, 0.0f, FLT_MAX, false,
        SDK::DamageType::Physical, SDK::SpellType::Targeted);
    p.Spells[0].Aim = AimPolicy::SelfPosition;
    p.Spells[0].Priority = 94;
    p.Spells[0].WeaveAfterAttack = true;
    p.Spells[0].PreserveAutoAttack = false;
    p.Spells[0].ComboManaPercent = 0.0f;
    p.Spells[0].HarassManaPercent = 0.0f;
    p.Spells[0].ClearManaPercent = 0.0f;

    p.Spells[1] = Spell(
        SDK::SpellSlot::W, "Haymaker", CastKind::Position,
        Intent::Damage | Intent::Shield | Intent::CrowdControl | Intent::Finisher,
        Mode::Combo | Mode::Harass | Mode::Jungle | Mode::Flee | Mode::Automatic,
        805.0f, 0.75f, 210.0f, FLT_MAX, false,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotCone);
    p.Spells[1].Aim = AimPolicy::Prediction;
    p.Spells[1].Priority = 98;
    p.Spells[1].MinimumAoeTargets = 1;
    p.Spells[1].PreserveAutoAttack = false;
    p.Spells[1].ComboManaPercent = 0.0f;
    p.Spells[1].HarassManaPercent = 0.0f;

    p.Spells[2] = Spell(
        SDK::SpellSlot::E, "Facebreaker", CastKind::Position,
        Intent::Damage | Intent::CrowdControl | Intent::Engage | Intent::Peel,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::Automatic | Mode::Flee,
        490.0f, 0.25f, 350.0f, FLT_MAX, false,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotCircle);
    p.Spells[2].Aim = AimPolicy::BetweenPlayerAndTarget;
    p.Spells[2].Priority = 91;
    p.Spells[2].MinimumAoeTargets = 1;
    p.Spells[2].PreserveAutoAttack = false;
    p.Spells[2].AllowOnMinions = true;
    p.Spells[2].ComboManaPercent = 0.0f;
    p.Spells[2].HarassManaPercent = 0.0f;
    p.Spells[2].ClearManaPercent = 0.0f;

    p.Spells[3] = Spell(
        SDK::SpellSlot::R, "The Show Stopper", CastKind::EnemyTarget,
        Intent::Damage | Intent::CrowdControl | Intent::Engage | Intent::Mobility | Intent::Finisher,
        Mode::Combo | Mode::Automatic | Mode::Flee,
        400.0f, 0.25f, 350.0f, 780.0f, false,
        SDK::DamageType::Physical, SDK::SpellType::Targeted);
    p.Spells[3].Aim = AimPolicy::TargetPosition;
    p.Spells[3].Priority = 100;
    p.Spells[3].MinimumAoeTargets = 1;
    p.Spells[3].TargetHealthPercent = 80.0f;
    p.Spells[3].PreserveAutoAttack = false;
    p.Spells[3].ComboManaPercent = 0.0f;

    p.Trade = Plan(
        "Q reset into Facebreaker and center Haymaker",
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequireInsideAaRange, 0, 900),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireSafePosition, 80, 1000),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget | StepRule::RequireSafePosition, 260, 1300));
    p.AllIn = Plan(
        "Safe Showstopper slam with Grit finisher",
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireSafePosition, 0, 900),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequireInsideAaRange, 60, 1100),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget | StepRule::RequireSafePosition, 220, 1400),
        Step(SDK::SpellSlot::R, StepRule::RequireTarget | StepRule::RequireSafePosition, 300, 1700));
    p.Flee = Plan(
        "Facebreaker peel and Grit shield",
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireSafePosition, 0, 700),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget | StepRule::RequireSafePosition, 80, 900),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::AllowDuringWindup, 120, 1000));
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
