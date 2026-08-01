#pragma once

#include "../AIChampionProfile.h"

#include <cfloat>

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Renekton = [] {
    ChampionProfile p{};
    p.ChampionId = SDK::ChampionId::Renekton;
    p.DisplayName = "Renekton";
    p.InternalId = "champion.kuroaio.ai.renekton";
    p.PrimaryArchetype = Archetype::Diver;
    p.Resource = ResourceModel::Fury;
    p.Mechanics = Mechanic::Recast | Mechanic::Dash | Mechanic::AutoReset |
                  Mechanic::MissingHealth | Mechanic::AutoWeave;
    p.Ultimate = UltimatePolicy::AllIn;
    p.PreferredCombatDistance = 300.0f;
    p.EngageHealthPercent = 58.0f;
    p.DefensiveHealthPercent = 30.0f;
    p.UltimateTargetHealthPercent = 55.0f;
    p.UltimateMinimumTargets = 2;
    p.MaximumCommitEnemies = 2;
    p.BaseHumanizerMs = 55;
    p.PreferSelectedTarget = true;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.TacticalSummary =
        "Fury-gated diver: choose empowered Q sustain, W stun or E armor shred, "
        "chain Slice/Dice only through verified targets, and reserve Dominus for a "
        "defensive or multi-target all-in window.";
    p.ResearchSummary =
        "Riot 26.15 / CommunityDragon 16.15 Summoner's Rift values; fury, spell "
        "stage and armor-shred state are reconciled from events and polling.";

    p.Spells[0] = Spell(
        SDK::SpellSlot::Q, "Cull the Meek", CastKind::Self,
        Intent::Damage | Intent::Heal | Intent::Waveclear | Intent::Jungle |
            Intent::LastHit | Intent::Setup,
        AllModes, 325.0f, 0.25f, 325.0f, FLT_MAX, false,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotCircle);
    p.Spells[0].Aim = AimPolicy::SelfPosition;
    p.Spells[0].Priority = 88;
    p.Spells[0].PreserveAutoAttack = true;

    p.Spells[1] = Spell(
        SDK::SpellSlot::W, "Ruthless Predator", CastKind::EnemyTarget,
        Intent::Damage | Intent::CrowdControl | Intent::AutoReset | Intent::Finisher |
            Intent::Interrupt | Intent::AntiGapcloser,
        Mode::Combo | Mode::Harass | Mode::Automatic | Mode::Flee,
        300.0f, 0.0f, 0.0f, FLT_MAX, false,
        SDK::DamageType::Physical, SDK::SpellType::Targeted);
    p.Spells[1].Aim = AimPolicy::TargetPosition;
    p.Spells[1].Priority = 98;
    p.Spells[1].WeaveAfterAttack = true;

    p.Spells[2] = Spell(
        SDK::SpellSlot::E, "Slice and Dice", CastKind::Position,
        Intent::Damage | Intent::Mobility | Intent::Engage | Intent::Disengage |
            Intent::Waveclear | Intent::Jungle,
        AllModes, 450.0f, 0.0f, 110.0f, FLT_MAX, false,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotLine);
    p.Spells[2].Aim = AimPolicy::SafeCursor;
    p.Spells[2].DashDistance = 450.0f;
    p.Spells[2].MaximumEnemiesAtDestination = 2;
    p.Spells[2].Priority = 94;
    p.Spells[2].RecastSpellName = "RenektonDice";

    p.Spells[3] = Spell(
        SDK::SpellSlot::R, "Dominus", CastKind::Self,
        Intent::Damage | Intent::Buff | Intent::Heal | Intent::Engage |
            Intent::Disengage | Intent::Peel,
        Mode::Combo | Mode::Flee | Mode::Automatic,
        375.0f, 0.25f, 375.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[3].Aim = AimPolicy::SelfPosition;
    p.Spells[3].Priority = 100;
    p.Spells[3].MinimumAoeTargets = 2;
    p.Spells[3].PlayerHealthPercent = 55.0f;

    p.Trade = Plan(
        "Fury stun and sustain trade",
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireSafePosition, 0, 700),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget | StepRule::RequireAfterAttack, 80, 800),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::AllowDuringWindup, 140, 800),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireSafePosition, 220, 900));
    p.AllIn = Plan(
        "Empowered W/Dice Dominus commit",
        Step(SDK::SpellSlot::R, StepRule::RequireTarget | StepRule::RequirePlayerLow, 0, 700),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireSafePosition, 50, 850),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget | StepRule::RequireAfterAttack, 100, 900),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::AllowDuringWindup, 170, 900),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireSafePosition, 250, 950));
    p.Flee = Plan(
        "Dice disengage and defensive Dominus",
        Step(SDK::SpellSlot::E, StepRule::RequireSafePosition, 0, 700),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget, 70, 700),
        Step(SDK::SpellSlot::R, StepRule::RequirePlayerLow | StepRule::ManualAssistOnly, 120, 700));
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
