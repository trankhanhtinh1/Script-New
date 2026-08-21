#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Skarner = [] {
    ChampionProfile p{};
    p.ChampionId = SDK::ChampionId::Skarner;
    p.DisplayName = "Skarner";
    p.InternalId = "champion.kuroaio.ai.skarner";
    p.PrimaryArchetype = Archetype::Vanguard;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::Dash | Mechanic::Recast | Mechanic::ObjectTracking |
                  Mechanic::Terrain | Mechanic::Stack | Mechanic::DirectionalSweet;
    p.Ultimate = UltimatePolicy::MultiTarget;
    p.PreferredCombatDistance = 375.0f;
    p.EngageHealthPercent = 58.0f;
    p.DefensiveHealthPercent = 34.0f;
    p.UltimateTargetHealthPercent = 62.0f;
    p.UltimateMinimumTargets = 2;
    p.MaximumCommitEnemies = 3;
    p.BaseHumanizerMs = 65;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "SkarnerPassive";
    p.ChannelBuff = "SkarnerEStun";
    p.UltimateBuff = "SkarnerImpale";
    p.TrackedObjectToken = "SkarnerRock";
    p.TacticalSummary =
        "Crystal-armored vanguard: preserve Q's three-hit state, use W shield for safe proximity, collide E with terrain or a reachable target, and reserve R for an observed multi-target impale or a defensive/kill-secure exception.";
    p.ResearchSummary =
        "Riot 26.15 / CommunityDragon 16.15 Summoner's Rift spell metadata, with stun, impale, terrain and mana state reconciled from events and polling.";

    p.Spells[0] = Spell(SDK::SpellSlot::Q, "Shattered Earth", CastKind::Self,
        Intent::Damage | Intent::AutoReset | Intent::Setup | Intent::Waveclear |
            Intent::Jungle | Intent::LastHit,
        AllModes, 350.0f, 0.25f, 110.0f, FLT_MAX, false,
        SDK::DamageType::Physical, SDK::SpellType::Targeted);
    p.Spells[0].Aim = AimPolicy::SelfPosition;
    p.Spells[0].Priority = 82;
    p.Spells[0].WeaveAfterAttack = true;
    p.Spells[0].PreserveAutoAttack = false;

    p.Spells[1] = Spell(SDK::SpellSlot::W, "Seismic Bastion", CastKind::Self,
        Intent::Damage | Intent::Shield | Intent::Peel | Intent::Engage,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::Automatic,
        350.0f, 0.25f, 350.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[1].Aim = AimPolicy::SelfPosition;
    p.Spells[1].Priority = 91;

    p.Spells[2] = Spell(SDK::SpellSlot::E, "Ixtal's Impact", CastKind::Position,
        Intent::Damage | Intent::Mobility | Intent::CrowdControl | Intent::Engage |
            Intent::Disengage | Intent::Interrupt | Intent::Peel,
        AllModes, 1000.0f, 0.20f, 120.0f, 1500.0f, true,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotLine);
    p.Spells[2].Aim = AimPolicy::Prediction;
    p.Spells[2].Priority = 96;
    p.Spells[2].MaximumEnemiesAtDestination = 2;

    p.Spells[3] = Spell(SDK::SpellSlot::R, "Impale", CastKind::Vector,
        Intent::Damage | Intent::CrowdControl | Intent::Engage | Intent::Peel |
            Intent::Interrupt | Intent::Recast,
        Mode::Combo | Mode::Flee | Mode::Automatic, 350.0f, 0.50f, 160.0f,
        1500.0f, false, SDK::DamageType::Physical, SDK::SpellType::SkillshotLine);
    p.Spells[3].Aim = AimPolicy::BestAoe;
    p.Spells[3].Priority = 100;
    p.Spells[3].MinimumAoeTargets = 2;

    p.Trade = Plan("Shielded crystal trade",
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::AllowDuringWindup, 0, 700),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget | StepRule::RequireSafePosition, 80, 750),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireSafePosition, 150, 950));
    p.AllIn = Plan("Terrain collision into Impale",
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::AllowDuringWindup, 0, 700),
        Step(SDK::SpellSlot::W, StepRule::RequireSafePosition, 80, 750),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireSafePosition, 150, 1000),
        Step(SDK::SpellSlot::R, StepRule::RequireMultiTarget | StepRule::RequireTarget, 280, 1300));
    p.Flee = Plan("Bastion peel",
        Step(SDK::SpellSlot::W, StepRule::RequireSafePosition, 0, 650),
        Step(SDK::SpellSlot::E, StepRule::RequireSafePosition, 80, 850),
        Step(SDK::SpellSlot::R, StepRule::RequireTarget | StepRule::RequireSafePosition, 120, 1100));
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
