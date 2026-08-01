#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Camille = [] {
    ChampionProfile p{};
    p.ChampionId = SDK::ChampionId::Camille;
    p.DisplayName = "Camille";
    p.InternalId = "champion.kuroaio.ai.camille";
    p.PrimaryArchetype = Archetype::Diver;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::Recast | Mechanic::Dash | Mechanic::WallInteraction |
                  Mechanic::Terrain | Mechanic::AutoWeave | Mechanic::AutoReset |
                  Mechanic::DirectionalSweet;
    p.Ultimate = UltimatePolicy::AllIn;
    p.PreferredCombatDistance = 325.0f;
    p.EngageHealthPercent = 62.0f;
    p.DefensiveHealthPercent = 30.0f;
    p.UltimateTargetHealthPercent = 48.0f;
    p.UltimateMinimumTargets = 1;
    p.MaximumCommitEnemies = 2;
    p.BaseHumanizerMs = 55;
    p.PreferSelectedTarget = true;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.UltimateBuff = "CamilleR";
    p.TacticalSummary =
        "Precision diver: trade around Adaptive Defenses, weave both Precision Protocol resets, "
        "land Tactical Sweep's outer cone, hook real wall endpoints and enter Hextech Ultimatum only with a safe arena plan.";
    p.ResearchSummary =
        "Riot 26.15 / CommunityDragon 16.15; Summoner's Rift baseline.";

    p.Spells[0] = Spell(
        SDK::SpellSlot::Q, "Precision Protocol", CastKind::Self,
        Intent::Damage | Intent::Buff | Intent::AutoReset | Intent::Recast,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle | Mode::LastHit,
        325.0f, 0.0f, 0.0f, FLT_MAX, false,
        SDK::DamageType::Physical, SDK::SpellType::Targeted);
    p.Spells[0].Aim = AimPolicy::SelfPosition;
    p.Spells[0].WeaveAfterAttack = true;
    p.Spells[0].PreserveAutoAttack = true;
    p.Spells[0].RecastSpellName = "CamilleQ2";
    p.Spells[0].Priority = 96;

    p.Spells[1] = Spell(
        SDK::SpellSlot::W, "Tactical Sweep", CastKind::Cone,
        Intent::Damage | Intent::CrowdControl | Intent::Heal | Intent::Waveclear,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle | Mode::Flee | Mode::Automatic,
        650.0f, 0.75f, 35.0f, FLT_MAX, false,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotCone);
    p.Spells[1].Aim = AimPolicy::Prediction;
    p.Spells[1].TriggerRange = 325.0f;
    p.Spells[1].Priority = 82;

    p.Spells[2] = Spell(
        SDK::SpellSlot::E, "Hookshot / Wall Dive", CastKind::Direction,
        Intent::Damage | Intent::CrowdControl | Intent::Mobility | Intent::Engage |
            Intent::Disengage | Intent::Recast,
        Mode::Combo | Mode::Harass | Mode::Jungle | Mode::Flee,
        800.0f, 0.25f, 130.0f, 1050.0f, false,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotLine);
    p.Spells[2].Aim = AimPolicy::SafeCursor;
    p.Spells[2].DashDistance = 800.0f;
    p.Spells[2].MaximumEnemiesAtDestination = 2;
    p.Spells[2].RecastSpellName = "CamilleEDash2";
    p.Spells[2].Priority = 91;

    p.Spells[3] = Spell(
        SDK::SpellSlot::R, "The Hextech Ultimatum", CastKind::EnemyTarget,
        Intent::Engage | Intent::CrowdControl | Intent::Buff | Intent::Finisher,
        Mode::Combo | Mode::Automatic,
        475.0f, 0.25f, 425.0f, 1200.0f, false,
        SDK::DamageType::Magical, SDK::SpellType::Targeted);
    p.Spells[3].Aim = AimPolicy::TargetPosition;
    p.Spells[3].Priority = 88;

    p.Trade = Plan(
        "Adaptive reset trade",
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequireAfterAttack, 0, 650),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget | StepRule::RequireOutsideAaRange, 80, 1100),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequireRecast |
                                      StepRule::RequireAfterAttack, 1500, 3500));
    p.AllIn = Plan(
        "Hookshot arena all-in",
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireSafePosition, 0, 900),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequireAfterAttack, 120, 1100),
        Step(SDK::SpellSlot::R, StepRule::RequireTarget | StepRule::RequireSafePosition |
                                      StepRule::SkipIfKillableWithout, 250, 1500),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequireRecast |
                                      StepRule::RequireAfterAttack, 1500, 3500));
    p.Flee = Plan(
        "Wall-hook escape",
        Step(SDK::SpellSlot::E, StepRule::RequireSafePosition, 0, 850),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget, 100, 900));
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
