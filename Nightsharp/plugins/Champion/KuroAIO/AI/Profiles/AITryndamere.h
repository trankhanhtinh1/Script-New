#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Tryndamere = [] {
    ChampionProfile p{};
    p.ChampionName = "Tryndamere";
    p.DisplayName = "Tryndamere";
    p.InternalId = "champion.kuroaio.ai.tryndamere";
    p.PrimaryArchetype = Archetype::Skirmisher;
    p.Resource = ResourceModel::Fury;
    p.Mechanics = Mechanic::Stack | Mechanic::MissingHealth | Mechanic::Dash |
                  Mechanic::AutoWeave | Mechanic::AutoReset | Mechanic::WallInteraction;
    p.Ultimate = UltimatePolicy::Defensive;
    p.PreferredCombatDistance = 175.0f;
    p.EngageHealthPercent = 42.0f;
    p.DefensiveHealthPercent = 30.0f;
    p.UltimateTargetHealthPercent = 30.0f;
    p.MaximumCommitEnemies = 2;
    p.BaseHumanizerMs = 55;
    p.PassiveBuff = "TryndamerePassive";
    p.UltimateBuff = "UndyingRage";
    p.TrackedObjectToken = "Tryndamere";
    p.TacticalSummary =
        "Fury-driven melee skirmisher: weave attacks into E spins, debuff fleeing enemies with W, heal only when missing health, and reserve Undying Rage for a timed kill or escape.";
    p.ResearchSummary =
        "Riot 26.15 / CommunityDragon 16.15 Tryndamere Q/W/E/R values, fury accounting, E terrain line, and five-second Undying Rage are event-reconciled.";

    p.Spells[0] = Spell(SDK::SpellSlot::Q, "Bloodlust", CastKind::Self,
        Intent::Heal | Intent::Buff,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::LastHit | Mode::Flee | Mode::Automatic,
        0.0f, 0.10f, 0.0f, FLT_MAX, false,
        SDK::DamageType::True, SDK::SpellType::Targeted);
    p.Spells[0].Priority = 68;
    p.Spells[0].PlayerHealthPercent = 48.0f;
    p.Spells[0].HarassManaPercent = 0.0f;
    p.Spells[0].ClearManaPercent = 0.0f;

    p.Spells[1] = Spell(SDK::SpellSlot::W, "Mocking Shout", CastKind::Self,
        Intent::Damage | Intent::CrowdControl | Intent::Peel |
            Intent::Setup | Intent::Disengage,
        Mode::Combo | Mode::Harass | Mode::Jungle | Mode::Flee | Mode::Automatic,
        850.0f, 0.125f, 830.0f, FLT_MAX, false,
        SDK::DamageType::Physical, SDK::SpellType::Targeted);
    p.Spells[1].Priority = 82;
    p.Spells[1].TargetHealthPercent = 75.0f;
    p.Spells[1].PreserveAutoAttack = true;

    p.Spells[2] = Spell(SDK::SpellSlot::E, "Spinning Slash", CastKind::Position,
        Intent::Damage | Intent::Mobility | Intent::Engage | Intent::Disengage |
            Intent::Waveclear | Intent::Jungle | Intent::LastHit | Intent::AutoReset,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::LastHit | Mode::Flee | Mode::Automatic,
        650.0f, 0.16f, 160.0f, 700.0f, false,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotLine);
    p.Spells[2].Aim = AimPolicy::Prediction;
    p.Spells[2].Priority = 94;
    p.Spells[2].WeaveAfterAttack = true;
    p.Spells[2].PreserveAutoAttack = true;

    p.Spells[3] = Spell(SDK::SpellSlot::R, "Undying Rage", CastKind::Self,
        Intent::Buff | Intent::Engage | Intent::Disengage | Intent::Finisher,
        Mode::Combo | Mode::Automatic | Mode::Flee,
        0.0f, 0.07f, 0.0f, FLT_MAX, false,
        SDK::DamageType::True, SDK::SpellType::Targeted);
    p.Spells[3].Priority = 100;
    p.Spells[3].PlayerHealthPercent = 30.0f;
    p.Spells[3].PreserveAutoAttack = false;

    p.Trade = Plan("fury trade with W debuff and E weave",
        Step(SDK::SpellSlot::W, StepRule::RequireTarget | StepRule::RequireInsideAaRange, 0, 650),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireAfterAttack, 110, 900),
        Step(SDK::SpellSlot::Q, StepRule::RequirePlayerLow | StepRule::AllowDuringWindup, 180, 1100));
    p.AllIn = Plan("Undying Rage kill window",
        Step(SDK::SpellSlot::W, StepRule::RequireTarget, 0, 650),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireAfterAttack, 100, 1000),
        Step(SDK::SpellSlot::R, StepRule::RequirePlayerLow | StepRule::AllowDuringWindup, 180, 1600),
        Step(SDK::SpellSlot::Q, StepRule::RequirePlayerLow | StepRule::AllowDuringWindup, 250, 1900));
    p.Flee = Plan("R survival then cursor spin",
        Step(SDK::SpellSlot::R, StepRule::RequirePlayerLow | StepRule::AllowDuringWindup, 0, 650),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget | StepRule::AllowDuringWindup, 40, 750),
        Step(SDK::SpellSlot::E, StepRule::RequireSafePosition | StepRule::AllowDuringWindup, 100, 900));
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
