#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Nami = [] {
    ChampionProfile p{};
    p.ChampionName = "Nami";
    p.DisplayName = "Nami";
    p.InternalId = "champion.kuroaio.ai.nami";
    p.PrimaryArchetype = Archetype::Enchanter;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::AllyTarget | Mechanic::AutoWeave |
                  Mechanic::MissingHealth | Mechanic::Mark;
    p.Ultimate = UltimatePolicy::MultiTarget;
    p.PreferredCombatDistance = 725.0f;
    p.EngageHealthPercent = 52.0f;
    p.DefensiveHealthPercent = 35.0f;
    p.UltimateTargetHealthPercent = 68.0f;
    p.UltimateMinimumTargets = 2;
    p.MaximumCommitEnemies = 2;
    p.BaseHumanizerMs = 42;
    p.PreferSelectedTarget = true;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "NamiPassive";
    p.MarkBuff = "NamiE";
    p.TrackedObjectToken = "NamiRMissile";
    p.ThemeFrom = 0xFF35C8FFu;
    p.ThemeTo = 0xFF4169E1u;
    p.TacticalSummary =
        "Tidecaller support route: predict Aqua Prison, alternate Ebb and Flow "
        "damage/heal bounces, empower the correct ally, and reserve Tidal Wave "
        "for a safe engage or peel line.";
    p.ResearchSummary =
        "Riot live patch 26.15 and CommunityDragon 16.15 Summoner's Rift values; "
        "W bounce scaling, E three-hit empower and R line safety are modeled from "
        "the pinned source artifact.";

    p.Spells[0] = Spell(
        SDK::SpellSlot::Q, "Aqua Prison", CastKind::Circle,
        Intent::Damage | Intent::CrowdControl | Intent::Setup |
            Intent::Interrupt | Intent::AntiGapcloser | Intent::Peel,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::Automatic,
        875.0f, 0.95f, 200.0f, 1750.0f, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[0].Aim = AimPolicy::Prediction;
    p.Spells[0].Priority = 94;
    p.Spells[0].Hitchance = SDK::HitChance::High;
    p.Spells[0].PreserveAutoAttack = true;

    p.Spells[1] = Spell(
        SDK::SpellSlot::W, "Ebb and Flow", CastKind::AnyTarget,
        Intent::Damage | Intent::Heal | Intent::AllyUtility | Intent::Setup |
            Intent::Peel | Intent::Jungle,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::LastHit | Mode::Automatic,
        725.0f, 0.25f, 210.0f, 1750.0f, false,
        SDK::DamageType::Magical, SDK::SpellType::Targeted);
    p.Spells[1].Aim = AimPolicy::Prediction;
    p.Spells[1].Priority = 88;
    p.Spells[1].HarassManaPercent = 48.0f;
    p.Spells[1].ClearManaPercent = 42.0f;
    p.Spells[1].PreserveAutoAttack = true;

    p.Spells[2] = Spell(
        SDK::SpellSlot::E, "Tidecaller's Blessing", CastKind::AllyTarget,
        Intent::Damage | Intent::Buff | Intent::AllyUtility | Intent::Engage |
            Intent::Disengage | Intent::Peel | Intent::Setup,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::Automatic,
        800.0f, 0.25f, 210.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::Targeted);
    p.Spells[2].Aim = AimPolicy::TargetPosition;
    p.Spells[2].Priority = 90;
    p.Spells[2].PreserveAutoAttack = true;

    p.Spells[3] = Spell(
        SDK::SpellSlot::R, "Tidal Wave", CastKind::Direction,
        Intent::Damage | Intent::CrowdControl | Intent::Engage |
            Intent::Disengage | Intent::Peel | Intent::Setup | Intent::Interrupt |
            Intent::AntiGapcloser,
        Mode::Combo | Mode::Flee | Mode::Automatic,
        2550.0f, 0.50f, 325.0f, 1200.0f, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[3].Aim = AimPolicy::BestAoe;
    p.Spells[3].Priority = 98;
    p.Spells[3].Hitchance = SDK::HitChance::High;
    p.Spells[3].MinimumAoeTargets = 2;
    p.Spells[3].MaximumEnemiesAtDestination = 3;

    p.Trade = Plan("bubble then alternating heal and damage bounces",
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequireSafePosition),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget | StepRule::AllowDuringWindup),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::AllowDuringWindup));
    p.AllIn = Plan("Tidal Wave setup with empowered follow-up",
        Step(SDK::SpellSlot::R, StepRule::RequireTarget | StepRule::RequireMultiTarget | StepRule::RequireSafePosition),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequireCrowdControl),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::AllowDuringWindup),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget | StepRule::RequireAfterAttack));
    p.Flee = Plan("protect ally and peel the nearest threat",
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::AllowDuringWindup),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget | StepRule::AllowDuringWindup),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequireSafePosition),
        Step(SDK::SpellSlot::R, StepRule::RequireTarget | StepRule::AllowDuringWindup));
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
