#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Kled = [] {
    ChampionProfile p{};
    p.ChampionId = SDK::ChampionId::Kled;
    p.DisplayName = "Kled";
    p.InternalId = "champion.kuroaio.ai.kled";
    p.PrimaryArchetype = Archetype::Skirmisher;
    p.Resource = ResourceModel::Special;
    p.Mechanics = Mechanic::Recast | Mechanic::Dash | Mechanic::Tether |
                  Mechanic::Stack | Mechanic::AutoWeave | Mechanic::Terrain;
    p.Ultimate = UltimatePolicy::AllIn;
    p.PreferredCombatDistance = 325.0f;
    p.EngageHealthPercent = 52.0f;
    p.DefensiveHealthPercent = 30.0f;
    p.UltimateTargetHealthPercent = 58.0f;
    p.UltimateMinimumTargets = 1;
    p.MaximumCommitEnemies = 2;
    p.BaseHumanizerMs = 50;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "KledPassive";
    p.MarkBuff = "KledQMark";
    p.ChannelBuff = "KledR";
    p.UltimateBuff = "KledRShield";
    p.TacticalSummary =
        "Courage-aware Kled: preserve the four-hit Violent Tendencies cadence, "
        "hold mounted Bear Trap on a Rope tethers, recast mounted Joust safely, "
        "and charge Chaaaaaaaarge!!! only to a viable endpoint.";
    p.ResearchSummary =
        "Riot 26.15 / CommunityDragon 16.15 Kled data; Courage, Skaarl mount "
        "state, Q tether, W cadence, E recast and R endpoint are event/poll reconciled.";

    p.Spells[0] = Spell(
        SDK::SpellSlot::Q, "Bear Trap on a Rope", CastKind::Position,
        Intent::Damage | Intent::CrowdControl | Intent::Setup | Intent::Waveclear |
            Intent::Jungle | Intent::LastHit,
        AllModes, 800.0f, 0.25f, 45.0f, 1600.0f, true,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotLine);
    p.Spells[0].Aim = AimPolicy::Prediction;
    p.Spells[0].Priority = 91;
    p.Spells[0].AllowOnMinions = true;

    p.Spells[1] = Spell(
        SDK::SpellSlot::W, "Violent Tendencies", CastKind::Self,
        Intent::Damage | Intent::AutoReset | Intent::Waveclear | Intent::Jungle,
        AllModes, 0.0f, 0.0f, 0.0f, FLT_MAX, false,
        SDK::DamageType::Physical, SDK::SpellType::Targeted);
    p.Spells[1].Priority = 88;
    p.Spells[1].PreserveAutoAttack = false;

    p.Spells[2] = Spell(
        SDK::SpellSlot::E, "Joust", CastKind::Position,
        Intent::Damage | Intent::Mobility | Intent::Engage | Intent::Disengage,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::Automatic,
        550.0f, 0.25f, 100.0f, 600.0f, false,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotLine);
    p.Spells[2].Aim = AimPolicy::Prediction;
    p.Spells[2].DashDistance = 550.0f;
    p.Spells[2].Priority = 84;
    p.Spells[2].MaximumEnemiesAtDestination = 2;

    p.Spells[3] = Spell(
        SDK::SpellSlot::R, "Chaaaaaaaarge!!!", CastKind::Position,
        Intent::Damage | Intent::CrowdControl | Intent::Mobility | Intent::Engage |
            Intent::Disengage | Intent::Shield | Intent::Finisher,
        Mode::Combo | Mode::Flee | Mode::Automatic,
        6000.0f, 0.25f, 500.0f, FLT_MAX, false,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotLine);
    p.Spells[3].Aim = AimPolicy::TargetPosition;
    p.Spells[3].Priority = 100;
    p.Spells[3].MaximumEnemiesAtDestination = 2;

    p.Trade = Plan(
        "Mounted chain and four-hit trade",
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget, 0, 850),
        Step(SDK::SpellSlot::W, StepRule::RequireAfterAttack, 80, 1100),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireSafePosition, 180, 1000));
    p.AllIn = Plan(
        "Safe charge into tethered target",
        Step(SDK::SpellSlot::R, StepRule::RequireTarget | StepRule::RequireSafePosition, 0, 1200),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget, 100, 1100),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireSafePosition, 180, 1200),
        Step(SDK::SpellSlot::W, StepRule::RequireAfterAttack, 240, 1400));
    p.Flee = Plan(
        "Charge disengage and Joust recast",
        Step(SDK::SpellSlot::R, StepRule::RequireSafePosition | StepRule::AllowDuringWindup, 0, 1000),
        Step(SDK::SpellSlot::E, StepRule::RequireSafePosition | StepRule::AllowDuringWindup, 80, 900));
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
