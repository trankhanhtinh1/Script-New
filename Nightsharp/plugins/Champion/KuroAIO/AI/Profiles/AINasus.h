#pragma once

#include "../AIChampionProfile.h"

#include <cfloat>

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Nasus = [] {
    ChampionProfile p{};
    p.ChampionName = "Nasus";
    p.DisplayName = "Nasus";
    p.InternalId = "champion.kuroaio.ai.nasus";
    p.PrimaryArchetype = Archetype::Juggernaut;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::Stack | Mechanic::Recast | Mechanic::MissingHealth |
                  Mechanic::AutoReset | Mechanic::AutoWeave;
    p.Ultimate = UltimatePolicy::Defensive;
    p.PreferredCombatDistance = 250.0f;
    p.EngageHealthPercent = 55.0f;
    p.DefensiveHealthPercent = 38.0f;
    p.UltimateTargetHealthPercent = 62.0f;
    p.UltimateMinimumTargets = 1;
    p.MaximumCommitEnemies = 2;
    p.BaseHumanizerMs = 62;
    p.PreferSelectedTarget = true;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "NasusSoulEater";
    p.MarkBuff = "NasusW";
    p.FormBuff = "NasusR";
    p.TacticalSummary =
        "Scaling juggernaut: preserve Siphoning Strike for a deliberate last-hit or attack reset, "
        "Wither the selected threat, place Spirit Fire only through a verified target pocket, and "
        "reserve Fury of the Sands for a safe all-in or low-health defense.";
    p.ResearchSummary =
        "Riot live 26.15 / CommunityDragon 16.15 Summoner's Rift values; Q stacks, Wither target "
        "state, Spirit Fire zone and ultimate duration are reconciled from events and polling.";

    p.Spells[0] = Spell(SDK::SpellSlot::Q, "Siphoning Strike", CastKind::Self,
        Intent::Damage | Intent::AutoReset | Intent::LastHit | Intent::Waveclear |
            Intent::Jungle | Intent::Setup,
        AllModes, 300.0f, 0.0f, 0.0f, FLT_MAX, false,
        SDK::DamageType::Physical, SDK::SpellType::Targeted);
    p.Spells[0].Aim = AimPolicy::TargetPosition;
    p.Spells[0].Priority = 100;
    p.Spells[0].WeaveAfterAttack = true;
    p.Spells[0].PreserveAutoAttack = true;
    p.Spells[0].RecastSpellName = "NasusQ";

    p.Spells[1] = Spell(SDK::SpellSlot::W, "Wither", CastKind::EnemyTarget,
        Intent::CrowdControl | Intent::Peel | Intent::AntiGapcloser | Intent::Setup,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::Automatic,
        700.0f, 0.25f, 0.0f, FLT_MAX, false,
        SDK::DamageType::True, SDK::SpellType::Targeted);
    p.Spells[1].Aim = AimPolicy::TargetPosition;
    p.Spells[1].Priority = 98;
    p.Spells[1].TargetHealthPercent = 100.0f;
    p.Spells[1].HarassManaPercent = 55.0f;

    p.Spells[2] = Spell(SDK::SpellSlot::E, "Spirit Fire", CastKind::Position,
        Intent::Damage | Intent::CrowdControl | Intent::Waveclear | Intent::Jungle |
            Intent::LastHit | Intent::Setup,
        AllModes, 650.0f, 0.25f, 350.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[2].Aim = AimPolicy::Prediction;
    p.Spells[2].Priority = 86;
    p.Spells[2].MinimumAoeTargets = 1;
    p.Spells[2].HarassManaPercent = 62.0f;
    p.Spells[2].ClearManaPercent = 38.0f;

    p.Spells[3] = Spell(SDK::SpellSlot::R, "Fury of the Sands", CastKind::Self,
        Intent::Buff | Intent::Damage | Intent::Heal | Intent::Engage | Intent::Disengage |
            Intent::Peel,
        Mode::Combo | Mode::Flee | Mode::Automatic,
        375.0f, 0.25f, 375.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[3].Aim = AimPolicy::SelfPosition;
    p.Spells[3].Priority = 99;
    p.Spells[3].MinimumAoeTargets = 1;
    p.Spells[3].PlayerHealthPercent = 48.0f;

    p.Trade = Plan("Wither, Spirit Fire and Q weave",
        Step(SDK::SpellSlot::W, StepRule::RequireTarget | StepRule::RequireSafePosition, 0, 900),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireSafePosition, 90, 1000),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequireAfterAttack, 170, 1000));
    p.AllIn = Plan("Fury, Wither and Siphoning Strike commit",
        Step(SDK::SpellSlot::R, StepRule::RequireTarget | StepRule::RequireSafePosition, 0, 900),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget, 80, 950),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireSafePosition, 140, 1050),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequireAfterAttack, 230, 1150));
    p.Flee = Plan("Wither peel and defensive Fury",
        Step(SDK::SpellSlot::W, StepRule::RequireTarget | StepRule::RequireSafePosition, 0, 850),
        Step(SDK::SpellSlot::E, StepRule::RequireSafePosition, 80, 900),
        Step(SDK::SpellSlot::R, StepRule::RequirePlayerLow | StepRule::ManualAssistOnly, 150, 900));
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
