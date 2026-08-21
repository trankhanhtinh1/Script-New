#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Volibear = [] {
    ChampionProfile p{};
    p.ChampionId = SDK::ChampionId::Volibear;
    p.DisplayName = "Volibear";
    p.InternalId = "champion.kuroaio.ai.volibear";
    p.PrimaryArchetype = Archetype::Diver;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::Stack | Mechanic::Mark | Mechanic::Recast | Mechanic::Dash |
                  Mechanic::AutoWeave | Mechanic::AutoReset | Mechanic::ObjectTracking |
                  Mechanic::Terrain;
    p.Ultimate = UltimatePolicy::AllIn;
    p.PreferredCombatDistance = 300.0f;
    p.EngageHealthPercent = 48.0f;
    p.DefensiveHealthPercent = 32.0f;
    p.UltimateTargetHealthPercent = 60.0f;
    p.UltimateMinimumTargets = 2;
    p.MaximumCommitEnemies = 2;
    p.BaseHumanizerMs = 55;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "VolibearP";
    p.FormBuff = "VolibearQ/VolibearW/VolibearE/VolibearR";
    p.TrackedObjectToken = "VolibearE/VolibearR";
    p.TacticalSummary =
        "Storm diver: Thundering Smash chases and stuns, Frenzied Maul marks for a stronger recast, "
        "Sky Splitter calls a lightning shield, and Stormbringer leaps through a safe disabling endpoint.";
    p.ResearchSummary =
        "Riot live 26.15 / CommunityDragon 16.15 models The Relentless Storm stacks, Q movement/stun, "
        "W wounded recast, E lightning/shield and R leap, turret disable and endpoint safety.";

    p.Spells[0] = Spell(SDK::SpellSlot::Q, "Thundering Smash", CastKind::Self,
        Intent::Damage | Intent::Mobility | Intent::CrowdControl | Intent::Engage |
            Intent::AutoReset,
        AllModes, 350.0f, 0.0f, 90.0f, FLT_MAX, false,
        SDK::DamageType::Physical, SDK::SpellType::Targeted);
    p.Spells[0].Aim = AimPolicy::Prediction;
    p.Spells[0].Priority = 94;
    p.Spells[0].WeaveAfterAttack = true;
    p.Spells[0].PreserveAutoAttack = true;
    p.Spells[0].ComboManaPercent = 0.0f;
    p.Spells[0].HarassManaPercent = 45.0f;
    p.Spells[0].ClearManaPercent = 30.0f;

    p.Spells[1] = Spell(SDK::SpellSlot::W, "Frenzied Maul", CastKind::EnemyTarget,
        Intent::Damage | Intent::Heal | Intent::Mark | Intent::Recast | Intent::AutoReset |
            Intent::Jungle | Intent::LastHit,
        AllModes, 350.0f, 0.25f, 0.0f, FLT_MAX, false,
        SDK::DamageType::Physical, SDK::SpellType::Targeted);
    p.Spells[1].Aim = AimPolicy::Prediction;
    p.Spells[1].Priority = 96;
    p.Spells[1].WeaveAfterAttack = true;
    p.Spells[1].PreserveAutoAttack = true;
    p.Spells[1].RecastSpellName = "VolibearW2";
    p.Spells[1].ComboManaPercent = 0.0f;
    p.Spells[1].HarassManaPercent = 45.0f;
    p.Spells[1].ClearManaPercent = 30.0f;

    p.Spells[2] = Spell(SDK::SpellSlot::E, "Sky Splitter", CastKind::Position,
        Intent::Damage | Intent::Shield | Intent::Setup | Intent::Jungle | Intent::Waveclear,
        AllModes, 750.0f, 1.0f, 650.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[2].Aim = AimPolicy::Prediction;
    p.Spells[2].Priority = 88;
    p.Spells[2].MinimumAoeTargets = 1;
    p.Spells[2].ComboManaPercent = 0.0f;
    p.Spells[2].HarassManaPercent = 55.0f;
    p.Spells[2].ClearManaPercent = 35.0f;

    p.Spells[3] = Spell(SDK::SpellSlot::R, "Stormbringer", CastKind::Position,
        Intent::Damage | Intent::Mobility | Intent::Engage | Intent::Disengage |
            Intent::CrowdControl | Intent::Objective | Intent::Setup,
        AllModes, 700.0f, 0.65f, 600.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[3].Aim = AimPolicy::BestAoe;
    p.Spells[3].Priority = 99;
    p.Spells[3].MinimumAoeTargets = 2;
    p.Spells[3].MaximumEnemiesAtDestination = 2;
    p.Spells[3].ComboManaPercent = 0.0f;
    p.Spells[3].HarassManaPercent = 0.0f;
    p.Spells[3].ClearManaPercent = 0.0f;

    p.Trade = Plan("stun, mark and wounded bite weave",
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequireSafePosition, 0, 800),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget | StepRule::RequireAfterAttack, 100, 1100),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireSafePosition, 180, 1300),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget | StepRule::RequireMark, 300, 1500));
    p.AllIn = Plan("safe storm leap into stun and wounded recast",
        Step(SDK::SpellSlot::R, StepRule::RequireTarget | StepRule::RequireMultiTarget |
            StepRule::RequireSafePosition, 0, 1200),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequireInsideAaRange, 100, 1400),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget | StepRule::RequireAfterAttack, 220, 1600),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget | StepRule::RequireMark, 420, 1900),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireSafePosition, 500, 2000));
    p.Flee = Plan("storm leap retreat and defensive lightning shield",
        Step(SDK::SpellSlot::R, StepRule::RequireTarget | StepRule::RequireSafePosition |
            StepRule::AllowDuringWindup, 0, 1100),
        Step(SDK::SpellSlot::E, StepRule::RequireSafePosition | StepRule::AllowDuringWindup, 120, 1300),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::AllowDuringWindup, 220, 1500));
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
