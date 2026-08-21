#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Zed = [] {
    ChampionProfile p{};
    p.ChampionId = SDK::ChampionId::Zed;
    p.DisplayName = "Zed";
    p.InternalId = "champion.kuroaio.ai.zed";
    p.PrimaryArchetype = Archetype::Assassin;
    p.Resource = ResourceModel::Energy;
    p.Mechanics = Mechanic::Recast | Mechanic::Dash | Mechanic::Blink |
                  Mechanic::ObjectTracking | Mechanic::Mark | Mechanic::Execute |
                  Mechanic::AutoWeave | Mechanic::DirectionalSweet;
    p.Ultimate = UltimatePolicy::RecastControl;
    p.PreferredCombatDistance = 525.0f;
    p.EngageHealthPercent = 42.0f;
    p.DefensiveHealthPercent = 32.0f;
    p.UltimateTargetHealthPercent = 58.0f;
    p.UltimateMinimumTargets = 1;
    p.MaximumCommitEnemies = 2;
    p.BaseHumanizerMs = 55;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "ZedPassive";
    p.MarkBuff = "ZedRDeathMark";
    p.ChannelBuff = "ZedR";
    p.UltimateBuff = "ZedR";
    p.TrackedObjectToken = "ZedShadow";
    p.TacticalSummary =
        "Energy assassin: model first-collision shurikens, W shadow pairs and swaps, "
        "shadow E marks, and death-mark return safety before committing.";
    p.ResearchSummary =
        "Riot 26.15 and CommunityDragon PC 16.15 Zed spell data, runtime names, "
        "shadow objects and death-mark buffs are reconciled from events and polling.";

    p.Spells[0] = Spell(SDK::SpellSlot::Q, "Razor Shuriken", CastKind::Position,
        Intent::Damage | Intent::Waveclear | Intent::Jungle | Intent::LastHit,
        AllModes, 925.0f, 0.25f, 90.0f, 1700.0f, false,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotLine);
    p.Spells[0].Aim = AimPolicy::Prediction;
    p.Spells[0].Priority = 88;
    p.Spells[0].PreserveAutoAttack = true;

    p.Spells[1] = Spell(SDK::SpellSlot::W, "Living Shadow", CastKind::Position,
        Intent::Mobility | Intent::Damage | Intent::Setup | Intent::Disengage,
        Mode::Combo | Mode::Harass | Mode::Automatic | Mode::Flee,
        650.0f, 0.25f, 80.0f, 1750.0f, false,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotLine);
    p.Spells[1].Aim = AimPolicy::BetweenPlayerAndTarget;
    p.Spells[1].Priority = 96;
    p.Spells[1].PreserveAutoAttack = true;

    p.Spells[2] = Spell(SDK::SpellSlot::E, "Shadow Slash", CastKind::Self,
        Intent::Damage | Intent::CrowdControl | Intent::Peel | Intent::Setup,
        AllModes, 290.0f, 0.25f, 290.0f, FLT_MAX, false,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotCircle);
    p.Spells[2].Aim = AimPolicy::SelfPosition;
    p.Spells[2].Priority = 92;
    p.Spells[2].PreserveAutoAttack = true;

    p.Spells[3] = Spell(SDK::SpellSlot::R, "Death Mark", CastKind::EnemyTarget,
        Intent::Damage | Intent::Engage | Intent::Execute | Intent::Recast |
            Intent::Mobility,
        Mode::Combo | Mode::Automatic,
        625.0f, 0.25f, 0.0f, FLT_MAX, false,
        SDK::DamageType::Physical, SDK::SpellType::Targeted);
    p.Spells[3].Aim = AimPolicy::TargetPosition;
    p.Spells[3].Priority = 100;
    p.Spells[3].PreserveAutoAttack = true;

    p.Trade = Plan("W poke and marked E/Q",
        Step(SDK::SpellSlot::W, StepRule::RequireTarget, 0, 900),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::AllowDuringWindup, 180, 1200),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget, 240, 1300));
    p.AllIn = Plan("Death mark shadow execution",
        Step(SDK::SpellSlot::R, StepRule::RequireTarget | StepRule::RequireSafePosition, 0, 850),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::AllowDuringWindup, 120, 1100),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget, 180, 1200),
        Step(SDK::SpellSlot::W, StepRule::RequireRecast | StepRule::RequireSafePosition, 500, 1800),
        Step(SDK::SpellSlot::R, StepRule::RequireRecast | StepRule::RequireSafePosition | StepRule::HoldForExecute, 900, 3600));
    p.Flee = Plan("Living Shadow return",
        Step(SDK::SpellSlot::W, StepRule::RequireRecast | StepRule::RequireSafePosition | StepRule::AllowDuringWindup, 0, 1200),
        Step(SDK::SpellSlot::W, StepRule::RequireFirstCast | StepRule::RequireSafePosition, 0, 900));
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
