#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Malzahar = [] {
    ChampionProfile p{};
    p.ChampionId = SDK::ChampionId::Malzahar;
    p.DisplayName = "Malzahar";
    p.InternalId = "champion.kuroaio.ai.malzahar";
    p.PrimaryArchetype = Archetype::Battlemage;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::SpellShield | Mechanic::Pet | Mechanic::Mark |
                  Mechanic::Channel | Mechanic::ObjectTracking | Mechanic::AutoWeave;
    p.Ultimate = UltimatePolicy::AllIn;
    p.PreferredCombatDistance = 650.0f;
    p.EngageHealthPercent = 55.0f;
    p.DefensiveHealthPercent = 35.0f;
    p.UltimateTargetHealthPercent = 58.0f;
    p.UltimateMinimumTargets = 1;
    p.MaximumCommitEnemies = 3;
    p.BaseHumanizerMs = 60;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "MalzaharPassive";
    p.MarkBuff = "MalzaharE";
    p.ChannelBuff = "MalzaharR";
    p.UltimateBuff = "MalzaharR";
    p.TacticalSummary =
        "Void Shift blocks the next crowd-control or damage event, Call of the Void silences, "
        "Void Swarm feeds Malefic Visions, and Nether Grasp is a protected suppression channel.";
    p.ResearchSummary =
        "Pinned to Riot 26.15 and CommunityDragon PC 16.15: 30-second passive shield cooldown, "
        "900-range Q portals, two-charge W voidling reservoir, four-second E spread and a 2.5-second R channel.";

    p.Spells[0] = Spell(
        SDK::SpellSlot::Q, "Call of the Void", CastKind::Position,
        Intent::Damage | Intent::CrowdControl | Intent::Waveclear |
            Intent::Jungle | Intent::LastHit | Intent::Interrupt,
        AllModes, 900.0f, 0.75f, 170.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[0].Aim = AimPolicy::Prediction;
    p.Spells[0].Priority = 90;
    p.Spells[0].Hitchance = SDK::HitChance::High;
    p.Spells[0].PreserveAutoAttack = true;
    p.Spells[0].AllowOnMinions = true;
    p.Spells[0].HarassManaPercent = 42.0f;
    p.Spells[0].ClearManaPercent = 35.0f;

    p.Spells[1] = Spell(
        SDK::SpellSlot::W, "Void Swarm", CastKind::Position,
        Intent::Damage | Intent::Setup | Intent::Waveclear | Intent::Jungle |
            Intent::LastHit,
        AllModes, 450.0f, 0.25f, 100.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[1].Aim = AimPolicy::TargetPosition;
    p.Spells[1].Priority = 84;
    p.Spells[1].PreserveAutoAttack = true;
    p.Spells[1].MinimumAmmo = 1;
    p.Spells[1].HarassManaPercent = 48.0f;
    p.Spells[1].ClearManaPercent = 38.0f;

    p.Spells[2] = Spell(
        SDK::SpellSlot::E, "Malefic Visions", CastKind::EnemyTarget,
        Intent::Damage | Intent::Mark | Intent::Waveclear | Intent::Jungle |
            Intent::LastHit | Intent::Finisher,
        AllModes, 650.0f, 0.25f, 70.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::Targeted);
    p.Spells[2].Aim = AimPolicy::TargetPosition;
    p.Spells[2].Priority = 94;
    p.Spells[2].PreserveAutoAttack = true;
    p.Spells[2].HarassManaPercent = 52.0f;
    p.Spells[2].ClearManaPercent = 42.0f;
    p.Spells[2].RequiredTargetBuff = "";

    p.Spells[3] = Spell(
        SDK::SpellSlot::R, "Nether Grasp", CastKind::EnemyTarget,
        Intent::Damage | Intent::CrowdControl | Intent::Channel | Intent::Finisher |
            Intent::Interrupt | Intent::Setup,
        Mode::Combo | Mode::Flee | Mode::Automatic, 700.0f, 2.5f, 95.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::Targeted);
    p.Spells[3].Aim = AimPolicy::TargetPosition;
    p.Spells[3].Priority = 100;
    p.Spells[3].Hitchance = SDK::HitChance::VeryHigh;
    p.Spells[3].MinimumAoeTargets = 1;
    p.Spells[3].TargetHealthPercent = 58.0f;
    p.Spells[3].PreserveAutoAttack = true;
    p.Spells[3].RecastSpellName = "MalzaharR";

    p.Trade = Plan(
        "Malefic Visions into silence and a marked voidling wave",
        Step(SDK::SpellSlot::E, StepRule::RequireTarget, 0, 900),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequireNoCrowdControl, 150, 1000),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget | StepRule::AllowDuringWindup, 260, 1100));
    p.AllIn = Plan(
        "Mark, spawn voidlings, silence, then hold the suppression channel",
        Step(SDK::SpellSlot::E, StepRule::RequireTarget, 0, 900),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget, 120, 950),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget, 240, 1100),
        Step(SDK::SpellSlot::R, StepRule::RequireTarget | StepRule::RequireSafePosition |
             StepRule::HoldForExecute, 360, 2800));
    p.Flee = Plan(
        "Silence and suppression only as reactive peel; do not turret dive",
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::AllowDuringWindup, 0, 900),
        Step(SDK::SpellSlot::R, StepRule::RequireTarget | StepRule::RequireSafePosition |
             StepRule::HoldForExecute, 160, 2800));
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
