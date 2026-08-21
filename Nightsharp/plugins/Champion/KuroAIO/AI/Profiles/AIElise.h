#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Elise = [] {
    ChampionProfile p{};
    p.ChampionId = SDK::ChampionId::Elise;
    p.DisplayName = "Elise";
    p.InternalId = "champion.kuroaio.ai.elise";
    p.PrimaryArchetype = Archetype::Specialist;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::Transform | Mechanic::MultiForm | Mechanic::Recast |
                  Mechanic::Execute | Mechanic::Dash | Mechanic::Pet |
                  Mechanic::ObjectTracking | Mechanic::Terrain;
    p.Ultimate = UltimatePolicy::RecastControl;
    p.PreferredCombatDistance = 550.0f;
    p.EngageHealthPercent = 58.0f;
    p.DefensiveHealthPercent = 34.0f;
    p.UltimateTargetHealthPercent = 42.0f;
    p.UltimateMinimumTargets = 1;
    p.MaximumCommitEnemies = 2;
    p.BaseHumanizerMs = 65;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "ElisePassive";
    p.FormBuff = "EliseSpiderForm";
    p.ChannelBuff = "EliseSpiderE";
    p.TrackedObjectToken = "EliseSpiderling";
    p.TacticalSummary =
        "Human Cocoon and Neurotoxin establish a pick; transform only for a safe "
        "spider Q execute, Skittering Frenzy farm, or a validated Rappel landing. "
        "Human and spider state is reconciled from runtime spell names, buffs and polling.";
    p.ResearchSummary =
        "Riot 26.15 / CommunityDragon 16.15 Summoner's Rift values, including "
        "current-health Neurotoxin, missing-health Venomous Bite and 700-range Rappel.";

    p.Spells[0] = Spell(SDK::SpellSlot::Q, "Neurotoxin",
        CastKind::EnemyTarget, Intent::Damage | Intent::Setup | Intent::Execute,
        AllModes, 625.0f, 0.0f, 0.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::Targeted);
    p.Spells[0].Priority = 92;
    p.Spells[0].TargetHealthPercent = 100.0f;
    p.Spells[0].WeaveAfterAttack = true;

    p.Spells[1] = Spell(SDK::SpellSlot::W, "Volatile Spiderling",
        CastKind::Position, Intent::Damage | Intent::Setup | Intent::Waveclear |
            Intent::Jungle, AllModes, 950.0f, 0.25f, 90.0f, 950.0f, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[1].Aim = AimPolicy::Prediction;
    p.Spells[1].Priority = 74;
    p.Spells[1].AllowOnMinions = true;

    p.Spells[2] = Spell(SDK::SpellSlot::E, "Cocoon",
        CastKind::Position, Intent::CrowdControl | Intent::Setup | Intent::Damage,
        CombatModes | Mode::Automatic, 1075.0f, 0.25f, 55.0f, 1300.0f, true,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[2].Aim = AimPolicy::Prediction;
    p.Spells[2].Priority = 98;
    p.Spells[2].Hitchance = SDK::HitChance::High;

    p.Spells[3] = Spell(SDK::SpellSlot::R, "Spider Form / Human Form",
        CastKind::Self, Intent::Recast | Intent::Mobility | Intent::Engage |
            Intent::Disengage, AllModes, 0.0f, 0.25f, 0.0f, FLT_MAX, false,
        SDK::DamageType::True, SDK::SpellType::Targeted);
    p.Spells[3].Aim = AimPolicy::SelfPosition;
    p.Spells[3].Priority = 90;

    p.Variants[0] = {SDK::SpellSlot::Q, "VenomousBite", Spell(
        SDK::SpellSlot::Q, "Venomous Bite", CastKind::EnemyTarget,
        Intent::Damage | Intent::Execute | Intent::AutoReset,
        AllModes, 400.0f, 0.0f, 0.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::Targeted)};
    p.Variants[1] = {SDK::SpellSlot::W, "SkitteringFrenzy", Spell(
        SDK::SpellSlot::W, "Skittering Frenzy", CastKind::Self,
        Intent::Damage | Intent::AutoWeave | Intent::Jungle | Intent::Waveclear,
        AllModes, 0.0f, 0.0f, 0.0f, FLT_MAX, false,
        SDK::DamageType::Physical, SDK::SpellType::Targeted)};
    p.Variants[2] = {SDK::SpellSlot::E, "Rappel", Spell(
        SDK::SpellSlot::E, "Rappel", CastKind::Position,
        Intent::Mobility | Intent::Disengage | Intent::Execute,
        CombatModes | Mode::Flee | Mode::Automatic, 700.0f, 0.0f, 180.0f,
        FLT_MAX, false, SDK::DamageType::True, SDK::SpellType::Targeted)};
    p.VariantCount = 3;

    p.Trade = Plan("Cocoon pick into spider conversion",
        Step(SDK::SpellSlot::E, StepRule::RequireTarget, 0, 1000),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::AllowDuringWindup,
             80, 850),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget, 140, 950),
        Step(SDK::SpellSlot::R, StepRule::RequireFirstCast | StepRule::RequireSafePosition,
             220, 900),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequireRecast |
             StepRule::HoldForExecute, 300, 1100));
    p.AllIn = Plan("Spider bite and rappel execute",
        Step(SDK::SpellSlot::E, StepRule::RequireTarget, 0, 900),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequireRecast |
             StepRule::HoldForExecute, 80, 850),
        Step(SDK::SpellSlot::W, StepRule::RequireRecast | StepRule::AllowDuringWindup,
             140, 850),
        Step(SDK::SpellSlot::E, StepRule::RequireRecast | StepRule::RequireSafePosition,
             220, 1200));
    p.Flee = Plan("Rappel disengage and human reset",
        Step(SDK::SpellSlot::E, StepRule::RequireRecast | StepRule::RequireSafePosition,
             0, 900),
        Step(SDK::SpellSlot::R, StepRule::RequireRecast | StepRule::RequireSafePosition,
             280, 900));
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
