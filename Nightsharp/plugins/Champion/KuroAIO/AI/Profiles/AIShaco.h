#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Shaco = [] {
    ChampionProfile p{};
    p.ChampionId = SDK::ChampionId::Shaco;
    p.DisplayName = "Shaco";
    p.InternalId = "champion.kuroaio.ai.shaco";
    p.PrimaryArchetype = Archetype::Assassin;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::Blink | Mechanic::ObjectTracking | Mechanic::Trap |
                  Mechanic::Pet | Mechanic::AutoWeave | Mechanic::Execute |
                  Mechanic::Terrain;
    p.Ultimate = UltimatePolicy::AllIn;
    p.PreferredCombatDistance = 325.0f;
    p.EngageHealthPercent = 42.0f;
    p.DefensiveHealthPercent = 30.0f;
    p.UltimateTargetHealthPercent = 65.0f;
    p.UltimateMinimumTargets = 1;
    p.MaximumCommitEnemies = 2;
    p.BaseHumanizerMs = 58;
    p.PassiveBuff = "ShacoPassive";
    p.UltimateBuff = "ShacoR";
    p.TrackedObjectToken = "JackInTheBox";
    p.TacticalSummary =
        "Deceive behind safe targets, arm boxes before contact, reserve Two-Shiv for poison execute checks, and detonate Hallucinate only when the clone has a real escape or explosion payoff.";
    p.ResearchSummary =
        "Riot 26.15 / CommunityDragon 16.15 Shaco Q stealth-blink, W arm/fear box, E poison execute and R clone identity/explosion behavior.";

    p.Spells[0] = Spell(SDK::SpellSlot::Q, "Deceive", CastKind::Position,
        Intent::Mobility | Intent::Engage | Intent::Disengage | Intent::Setup,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::Automatic,
        400.0f, 0.05f, 0.0f, FLT_MAX, false,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotLine);
    p.Spells[0].Aim = AimPolicy::BehindTarget;
    p.Spells[0].Priority = 88;
    p.Spells[0].DashDistance = 400.0f;
    p.Spells[0].PreserveAutoAttack = true;

    p.Spells[1] = Spell(SDK::SpellSlot::W, "Jack In The Box", CastKind::Position,
        Intent::CrowdControl | Intent::Setup | Intent::Damage | Intent::Waveclear |
            Intent::Jungle | Intent::Disengage,
        AllModes, 500.0f, 0.25f, 250.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[1].Aim = AimPolicy::BetweenPlayerAndTarget;
    p.Spells[1].Priority = 66;
    p.Spells[1].AllowOnMinions = true;
    p.Spells[1].ClearManaPercent = 35.0f;
    p.Spells[1].DesiredDistance = 180.0f;

    p.Spells[2] = Spell(SDK::SpellSlot::E, "Two-Shiv Poison", CastKind::EnemyTarget,
        Intent::Damage | Intent::Execute | Intent::Finisher | Intent::LastHit |
            Intent::CrowdControl | Intent::Jungle,
        AllModes, 625.0f, 0.25f, 80.0f, 1500.0f, true,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[2].Aim = AimPolicy::Prediction;
    p.Spells[2].Priority = 100;
    p.Spells[2].TargetHealthPercent = 30.0f;
    p.Spells[2].WeaveAfterAttack = true;
    p.Spells[2].HarassManaPercent = 42.0f;
    p.Spells[2].ClearManaPercent = 35.0f;

    p.Spells[3] = Spell(SDK::SpellSlot::R, "Hallucinate", CastKind::Self,
        Intent::Damage | Intent::Engage | Intent::Disengage | Intent::Setup,
        Mode::Combo | Mode::Automatic | Mode::Flee, 200.0f, 0.1f, 400.0f,
        FLT_MAX, false, SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[3].Aim = AimPolicy::SelfPosition;
    p.Spells[3].Priority = 96;
    p.Spells[3].MinimumAoeTargets = 1;
    p.Spells[3].MaximumEnemiesAtDestination = 2;
    p.Spells[3].PreserveAutoAttack = false;

    p.Trade = Plan("Box setup into poisoned shiv",
        Step(SDK::SpellSlot::W, StepRule::RequireTarget, 0, 700),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireAfterAttack, 180, 1000));
    p.AllIn = Plan("Deceive backstab and clone burst",
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequireSafePosition, 0, 800),
        Step(SDK::SpellSlot::R, StepRule::RequireTarget | StepRule::AllowDuringWindup, 80, 1000),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireTargetLow, 140, 1250),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget, 220, 1400));
    p.Flee = Plan("Deceive escape with fear box",
        Step(SDK::SpellSlot::Q, StepRule::RequireSafePosition | StepRule::AllowDuringWindup, 0, 850),
        Step(SDK::SpellSlot::W, StepRule::RequireSafePosition, 90, 900),
        Step(SDK::SpellSlot::R, StepRule::RequirePlayerLow | StepRule::AllowDuringWindup, 120, 1000));
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
