#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Heimerdinger = [] {
    ChampionProfile p{};
    p.ChampionId = SDK::ChampionId::Heimerdinger;
    p.DisplayName = "Heimerdinger";
    p.InternalId = "champion.kuroaio.ai.heimerdinger";
    p.PrimaryArchetype = Archetype::Specialist;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::Ammo | Mechanic::ObjectTracking | Mechanic::Pet |
                  Mechanic::Recast | Mechanic::Transform | Mechanic::WallInteraction;
    p.Ultimate = UltimatePolicy::RecastControl;
    p.PreferredCombatDistance = 800.0f;
    p.EngageHealthPercent = 58.0f;
    p.DefensiveHealthPercent = 34.0f;
    p.UltimateTargetHealthPercent = 48.0f;
    p.UltimateMinimumTargets = 2;
    p.MaximumCommitEnemies = 2;
    p.BaseHumanizerMs = 65;
    p.PreferSelectedTarget = true;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "HeimerdingerPassive";
    p.UltimateBuff = "HeimerdingerR";
    p.TrackedObjectToken = "HeimerTurret";
    p.TacticalSummary =
        "Build a safe turret zone before committing, reserve turret ammo for zone control, land the grenade center for stun, then choose upgraded turret, rockets or grenade from the actual fight state.";
    p.ResearchSummary =
        "Riot 26.15 / CommunityDragon 16.15 Summoner's Rift values with turret-object lifecycle, ammo reconciliation, prediction, projectile-wall and zone safety checks.";

    p.Spells[0] = Spell(SDK::SpellSlot::Q, "H-28G Evolution Turret", CastKind::Position,
        Intent::Damage | Intent::Setup | Intent::Waveclear | Intent::Jungle |
            Intent::LastHit | Intent::Objective,
        AllModes, 350.0f, 0.25f, 400.0f, 0.0f, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[0].Aim = AimPolicy::SafeCursor;
    p.Spells[0].Priority = 94;
    p.Spells[0].MinimumAmmo = 1;
    p.Spells[0].MaximumEnemiesAtDestination = 2;

    p.Spells[1] = Spell(SDK::SpellSlot::W, "Hextech Micro-Rockets", CastKind::Line,
        Intent::Damage | Intent::Waveclear | Intent::Jungle | Intent::LastHit |
            Intent::Finisher,
        CombatModes | FarmModes | Mode::Automatic, 1325.0f, 0.25f, 60.0f, 902.0f,
        true, SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[1].Aim = AimPolicy::Prediction;
    p.Spells[1].Priority = 88;
    p.Spells[1].MinimumAoeTargets = 1;

    p.Spells[2] = Spell(SDK::SpellSlot::E, "CH-2 Electron Storm Grenade",
        CastKind::Position,
        Intent::Damage | Intent::CrowdControl | Intent::Setup | Intent::Peel |
            Intent::Waveclear | Intent::Jungle | Intent::LastHit | Intent::Interrupt,
        AllModes, 925.0f, 0.25f, 200.0f, 2500.0f, true,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[2].Aim = AimPolicy::Prediction;
    p.Spells[2].Priority = 100;
    p.Spells[2].MinimumAoeTargets = 1;

    p.Spells[3] = Spell(SDK::SpellSlot::R, "UPGRADE!!!", CastKind::Self,
        Intent::Damage | Intent::CrowdControl | Intent::Setup | Intent::Recast |
            Intent::Engage | Intent::Disengage | Intent::Objective,
        Mode::Combo | Mode::Harass | Mode::Jungle | Mode::Flee | Mode::Automatic,
        450.0f, 0.0f, 400.0f, 0.0f, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[3].Aim = AimPolicy::BestAoe;
    p.Spells[3].Priority = 100;
    p.Spells[3].MinimumAoeTargets = 1;
    p.Spells[3].RecastSpellName = "HeimerdingerR";

    p.Trade = Plan("Turret-zone grenade trade",
        Step(SDK::SpellSlot::Q, StepRule::RequireSafePosition, 0, 850),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireCrowdControl, 110, 1000),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget, 250, 1150));
    p.AllIn = Plan("Grenade stun into upgrade",
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireCrowdControl, 0, 900),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget, 90, 1200),
        Step(SDK::SpellSlot::R, StepRule::RequireTarget | StepRule::RequireRecast, 180, 1200));
    p.Flee = Plan("Turret-zone peel",
        Step(SDK::SpellSlot::Q, StepRule::RequireSafePosition, 0, 850),
        Step(SDK::SpellSlot::E, StepRule::RequireSafePosition, 100, 950),
        Step(SDK::SpellSlot::R, StepRule::RequireSafePosition, 180, 1200));
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
