#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Gnar = [] {
    ChampionProfile profile{};
    profile.ChampionName = "Gnar";
    profile.DisplayName = "Gnar";
    profile.InternalId = "champion.kuroaio.ai.gnar";
    profile.PrimaryArchetype = Archetype::Specialist;
    profile.Resource = ResourceModel::Fury;
    profile.Mechanics = Mechanic::Transform | Mechanic::MultiForm |
                        Mechanic::Dash | Mechanic::ReturnProjectile |
                        Mechanic::Stack | Mechanic::WallInteraction |
                        Mechanic::Terrain;
    profile.Ultimate = UltimatePolicy::MultiTarget;
    profile.PreferredCombatDistance = 575.0f;
    profile.EngageHealthPercent = 58.0f;
    profile.DefensiveHealthPercent = 32.0f;
    profile.UltimateTargetHealthPercent = 45.0f;
    profile.UltimateMinimumTargets = 2;
    profile.MaximumCommitEnemies = 2;
    profile.BaseHumanizerMs = 55;
    profile.PreferSelectedTarget = true;
    profile.AllowTurretDiveIfKillable = false;
    profile.ProtectManualChannels = true;
    profile.TrackedObjectToken = "GnarQMissile";
    profile.TacticalSummary =
        "Rage-managed Mini/Mega specialist: kite with boomerang and Hyper, "
        "transform around prepared fights, commit Hop/Crunch only to verified "
        "safe endpoints, and reserve GNAR! for wall, multi-target, lethal or peel value.";
    profile.ResearchSummary =
        "Riot 26.15 / CommunityDragon 16.15; Summoner's Rift baseline.";

    profile.Spells[0] = Spell(
        SDK::SpellSlot::Q, "Boomerang Throw / Boulder Toss",
        CastKind::Position,
        Intent::Damage | Intent::CrowdControl | Intent::Waveclear |
            Intent::Jungle | Intent::LastHit,
        AllModes, 1125.0f, 0.25f, 90.0f, 2100.0f, true,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotLine);
    profile.Spells[0].Aim = AimPolicy::Prediction;
    profile.Spells[0].Priority = 90;
    profile.Spells[0].PreserveAutoAttack = true;
    profile.Spells[0].AllowOnMinions = true;

    profile.Spells[1] = Spell(
        SDK::SpellSlot::W, "Hyper / Wallop",
        CastKind::Position,
        Intent::Damage | Intent::CrowdControl | Intent::Interrupt |
            Intent::AntiGapcloser | Intent::Waveclear | Intent::Jungle,
        AllModes, 550.0f, 0.60f, 200.0f, FLT_MAX, false,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotLine);
    profile.Spells[1].Aim = AimPolicy::Prediction;
    profile.Spells[1].Priority = 84;
    profile.Spells[1].PreserveAutoAttack = true;

    profile.Spells[2] = Spell(
        SDK::SpellSlot::E, "Hop / Crunch",
        CastKind::Position,
        Intent::Damage | Intent::Mobility | Intent::Engage |
            Intent::Disengage | Intent::Waveclear | Intent::Jungle,
        AllModes, 675.0f, 0.25f, 150.0f, FLT_MAX, false,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotCircle);
    profile.Spells[2].Aim = AimPolicy::SafeCursor;
    profile.Spells[2].DashDistance = 675.0f;
    profile.Spells[2].MaximumEnemiesAtDestination = 2;
    profile.Spells[2].Priority = 78;

    profile.Spells[3] = Spell(
        SDK::SpellSlot::R, "GNAR!",
        CastKind::Self,
        Intent::Damage | Intent::CrowdControl | Intent::Disengage |
            Intent::Peel | Intent::Interrupt | Intent::Finisher,
        Mode::Combo | Mode::Flee | Mode::Automatic,
        475.0f, 0.25f, 475.0f, FLT_MAX, false,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotCircle);
    profile.Spells[3].Aim = AimPolicy::SelfPosition;
    profile.Spells[3].Priority = 96;
    profile.Spells[3].MinimumAoeTargets = 2;

    profile.Trade = Plan(
        "Mini spacing trade",
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget, 0, 800),
        Step(SDK::SpellSlot::W, StepRule::RequireAfterAttack, 80, 900));
    profile.AllIn = Plan(
        "Mega wall conversion",
        Step(SDK::SpellSlot::E,
             StepRule::RequireTarget | StepRule::RequireSafePosition,
             0, 850),
        Step(SDK::SpellSlot::R,
             StepRule::RequireTarget | StepRule::RequireSafePosition,
             80, 850),
        Step(SDK::SpellSlot::W,
             StepRule::RequireCrowdControl,
             120, 950),
        Step(SDK::SpellSlot::Q,
             StepRule::RequireTarget,
             100, 1000));
    profile.Flee = Plan(
        "Transform-aware disengage",
        Step(SDK::SpellSlot::R,
             StepRule::RequireMultiTarget | StepRule::RequireSafePosition,
             0, 650),
        Step(SDK::SpellSlot::W,
             StepRule::RequireTarget,
             40, 700),
        Step(SDK::SpellSlot::E,
             StepRule::RequireSafePosition,
             70, 900));
    return profile;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
