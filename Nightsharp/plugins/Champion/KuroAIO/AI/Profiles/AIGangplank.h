#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Gangplank = [] {
    ChampionProfile p{};
    p.ChampionId = SDK::ChampionId::Gangplank;
    p.DisplayName = "Gangplank";
    p.InternalId = "champion.kuroaio.ai.gangplank";
    p.PrimaryArchetype = Archetype::Specialist;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::Ammo | Mechanic::ObjectTracking | Mechanic::Cleanse |
                  Mechanic::AutoWeave | Mechanic::Global | Mechanic::MissingHealth;
    p.Ultimate = UltimatePolicy::GlobalExecute;
    p.PreferredCombatDistance = 625.0f;
    p.EngageHealthPercent = 48.0f;
    p.DefensiveHealthPercent = 32.0f;
    p.UltimateTargetHealthPercent = 58.0f;
    p.UltimateMinimumTargets = 2;
    p.MaximumCommitEnemies = 3;
    p.BaseHumanizerMs = 65;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "GangplankPassive";
    p.TrackedObjectToken = "GangplankBarrel";
    p.TacticalSummary =
        "Barrel-chain specialist: reconcile Powder Keg objects and health ticks, protect Q and auto timing, cleanse with W, and reserve Cannon Barrage for observable lethal, defensive or objective value.";
    p.ResearchSummary =
        "Riot 26.15 / CommunityDragon 16.15 values for Parrrley, Remove Scurvy, Powder Keg, Trial by Fire and Cannon Barrage; object and buff names remain telemetry-aliased at runtime.";

    p.Spells[0] = Spell(SDK::SpellSlot::Q, "Parrrley", CastKind::EnemyTarget,
        Intent::Damage | Intent::LastHit | Intent::Execute | Intent::AutoReset,
        AllModes, 625.0f, 0.25f, 0.0f, FLT_MAX, false,
        SDK::DamageType::Physical, SDK::SpellType::Targeted);
    p.Spells[0].Aim = AimPolicy::TargetPosition;
    p.Spells[0].Priority = 94;
    p.Spells[0].WeaveAfterAttack = true;
    p.Spells[0].PreserveAutoAttack = true;
    p.Spells[0].HarassManaPercent = 42.0f;
    p.Spells[0].ClearManaPercent = 28.0f;

    p.Spells[1] = Spell(SDK::SpellSlot::W, "Remove Scurvy", CastKind::Self,
        Intent::Heal | Intent::Cleanse | Intent::Peel,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::Automatic,
        0.0f, 0.0f, 0.0f, FLT_MAX, false,
        SDK::DamageType::True, SDK::SpellType::Targeted);
    p.Spells[1].Aim = AimPolicy::SelfPosition;
    p.Spells[1].Priority = 100;
    p.Spells[1].PlayerHealthPercent = 78.0f;

    p.Spells[2] = Spell(SDK::SpellSlot::E, "Powder Keg", CastKind::Position,
        Intent::Damage | Intent::Setup | Intent::Waveclear | Intent::Jungle |
            Intent::CrowdControl,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::Automatic,
        1000.0f, 0.25f, 325.0f, FLT_MAX, false,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotCircle);
    p.Spells[2].Aim = AimPolicy::BetweenPlayerAndTarget;
    p.Spells[2].Priority = 91;
    p.Spells[2].MinimumAmmo = 1;
    p.Spells[2].ClearManaPercent = 22.0f;

    p.Spells[3] = Spell(SDK::SpellSlot::R, "Cannon Barrage", CastKind::Circle,
        Intent::Damage | Intent::CrowdControl | Intent::Objective |
            Intent::Execute | Intent::Peel,
        Mode::Combo | Mode::Flee | Mode::Automatic,
        5000.0f, 0.25f, 525.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[3].Aim = AimPolicy::BestAoe;
    p.Spells[3].Priority = 100;
    p.Spells[3].MinimumAoeTargets = 2;
    p.Spells[3].TargetHealthPercent = 58.0f;

    p.Trade = Plan("Barrel and Parrrley trade",
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireNoMark, 0, 900),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::AllowDuringWindup, 120, 1000),
        Step(SDK::SpellSlot::W, StepRule::RequireNoCrowdControl, 180, 700));
    p.AllIn = Plan("Three-part keg chain",
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireNoMark, 0, 900),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::AllowDuringWindup, 90, 1000),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireMark, 180, 1100),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::AllowDuringWindup, 280, 1200),
        Step(SDK::SpellSlot::R, StepRule::RequireMultiTarget | StepRule::RequireTargetLow, 360, 1800));
    p.Flee = Plan("Scurvy and barrage peel",
        Step(SDK::SpellSlot::W, StepRule::RequireNoCrowdControl | StepRule::RequirePlayerLow, 0, 700),
        Step(SDK::SpellSlot::E, StepRule::RequireSafePosition, 80, 900),
        Step(SDK::SpellSlot::R, StepRule::RequireMultiTarget | StepRule::RequireSafePosition, 150, 1500));
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
