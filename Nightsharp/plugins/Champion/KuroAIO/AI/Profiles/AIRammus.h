#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Rammus = [] {
    ChampionProfile p{};
    p.ChampionId = SDK::ChampionId::Rammus;
    p.DisplayName = "Rammus";
    p.InternalId = "champion.kuroaio.ai.rammus";
    p.PrimaryArchetype = Archetype::Vanguard;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::Charge | Mechanic::Recast | Mechanic::Dash |
                  Mechanic::ObjectTracking | Mechanic::Terrain | Mechanic::Stance |
                  Mechanic::DirectionalSweet;
    p.Ultimate = UltimatePolicy::MultiTarget;
    p.PreferredCombatDistance = 325.0f;
    p.EngageHealthPercent = 58.0f;
    p.DefensiveHealthPercent = 42.0f;
    p.UltimateTargetHealthPercent = 48.0f;
    p.UltimateMinimumTargets = 2;
    p.MaximumCommitEnemies = 3;
    p.BaseHumanizerMs = 65;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "RammusPassive";
    p.ChannelBuff = "PowerBall";
    p.FormBuff = "DefensiveBallCurl";
    p.UltimateBuff = "Tremors2";
    p.TrackedObjectToken = "RammusPowerBall";
    p.TacticalSummary =
        "Armor posture vanguard: charge Powerball into a verified impact, taunt the priority target, curl through its attacks, and land Soaring Slam only when the destination is safe.";
    p.ResearchSummary =
        "Riot 26.15 / CommunityDragon 16.15 values with Powerball charge and impact state, Defensive Ball Curl armor posture, taunt peel, and Soaring Slam landing/aftershock reconciliation.";

    p.Spells[0] = Spell(SDK::SpellSlot::Q, "Powerball", CastKind::ChargedLine,
        Intent::Damage | Intent::CrowdControl | Intent::Mobility | Intent::Engage |
            Intent::Peel | Intent::Waveclear | Intent::Jungle | Intent::LastHit,
        AllModes, 300.0f, 0.25f, 200.0f, 1000.0f, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[0].Aim = AimPolicy::Prediction;
    p.Spells[0].Priority = 96;
    p.Spells[0].ChargeBuffName = "PowerBall";
    p.Spells[0].ChargeMinRange = 175;
    p.Spells[0].ChargeMaxRange = 300;
    p.Spells[0].ChargeDurationSeconds = 6.0f;
    p.Spells[0].WeaveAfterAttack = false;

    p.Spells[1] = Spell(SDK::SpellSlot::W, "Defensive Ball Curl", CastKind::Toggle,
        Intent::Buff | Intent::Shield | Intent::Damage | Intent::Peel,
        Mode::Combo | Mode::Harass | Mode::Automatic | Mode::Flee,
        300.0f, 0.0f, 300.0f, 0.0f, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[1].Aim = AimPolicy::SelfPosition;
    p.Spells[1].Priority = 100;
    p.Spells[1].RequiredPlayerBuff = "";
    p.Spells[1].ChargeDurationSeconds = 7.0f;

    p.Spells[2] = Spell(SDK::SpellSlot::E, "Frenzying Taunt", CastKind::EnemyTarget,
        Intent::CrowdControl | Intent::Engage | Intent::Peel | Intent::Interrupt |
            Intent::Jungle | Intent::LastHit,
        AllModes, 325.0f, 0.25f, 80.0f, 0.0f, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[2].Aim = AimPolicy::TargetPosition;
    p.Spells[2].Priority = 98;
    p.Spells[2].TargetHealthPercent = 100.0f;

    p.Spells[3] = Spell(SDK::SpellSlot::R, "Soaring Slam", CastKind::Position,
        Intent::Damage | Intent::CrowdControl | Intent::Mobility | Intent::Engage |
            Intent::Peel | Intent::Interrupt | Intent::Objective,
        Mode::Combo | Mode::Automatic | Mode::Flee, 800.0f, 0.25f, 400.0f, 2000.0f,
        false, SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[3].Aim = AimPolicy::BestAoe;
    p.Spells[3].Priority = 100;
    p.Spells[3].MinimumAoeTargets = 2;
    p.Spells[3].MaximumEnemiesAtDestination = 3;
    p.Spells[3].ChargeBuffName = "Tremors2";
    p.Spells[3].ChargeMinRange = 250;
    p.Spells[3].ChargeMaxRange = 1700;
    p.Spells[3].ChargeDurationSeconds = 1.0f;

    p.Trade = Plan("Powerball taunt trade",
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequireNoCrowdControl, 0, 850),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireCrowdControl, 80, 900),
        Step(SDK::SpellSlot::W, StepRule::AllowDuringWindup, 140, 1100));
    p.AllIn = Plan("Impact taunt and tremors",
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget, 0, 900),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireCrowdControl, 90, 1050),
        Step(SDK::SpellSlot::W, StepRule::RequireSafePosition | StepRule::AllowDuringWindup, 150, 1200),
        Step(SDK::SpellSlot::R, StepRule::RequireMultiTarget, 250, 1800));
    p.Flee = Plan("Curl and peel landing",
        Step(SDK::SpellSlot::W, StepRule::RequireSafePosition | StepRule::AllowDuringWindup, 0, 700),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::AllowDuringWindup, 60, 950),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireCrowdControl, 100, 1100),
        Step(SDK::SpellSlot::R, StepRule::RequireTarget | StepRule::RequireSafePosition, 120, 1500));
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
