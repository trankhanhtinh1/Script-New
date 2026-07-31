#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Seraphine = [] {
    ChampionProfile p{};
    p.ChampionName = "Seraphine";
    p.DisplayName = "Seraphine";
    p.InternalId = "champion.kuroaio.ai.seraphine";
    p.PrimaryArchetype = Archetype::Catcher;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::Stack | Mechanic::AllyTarget | Mechanic::Execute |
                  Mechanic::DirectionalSweet | Mechanic::AutoWeave | Mechanic::Global;
    p.Ultimate = UltimatePolicy::MultiTarget;
    p.PreferredCombatDistance = 850.0f;
    p.EngageHealthPercent = 52.0f;
    p.DefensiveHealthPercent = 42.0f;
    p.UltimateTargetHealthPercent = 100.0f;
    p.UltimateMinimumTargets = 2;
    p.MaximumCommitEnemies = 2;
    p.BaseHumanizerMs = 42;
    p.PreferSelectedTarget = true;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "SeraphinePassiveNotes";
    p.MarkBuff = "SeraphinePassiveNotes";
    p.FormBuff = "SeraphineW";
    p.UltimateBuff = "SeraphineR";
    p.TacticalSummary =
        "Track Notes and Echo readiness, execute low-health targets with Beat Drop, protect allies "
        "with Surround Sound, escalate Beat Drop from slow to root/stun, and extend Encore through champions.";
    p.ResearchSummary =
        "Riot 26.15 live notes and CommunityDragon PC 16.15 Seraphine spell-bin values: Q 900/350 "
        "area with 25% execute threshold, W 800 shield/heal/speed, E 1300/80 line, and R 1200/160 "
        "charm line whose range extends through champion contacts.";

    p.Spells[0] = Spell(SDK::SpellSlot::Q, "High Note", CastKind::Circle,
        Intent::Damage | Intent::Execute | Intent::Setup,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle | Mode::LastHit | Mode::Automatic,
        900.0f, 0.25f, 350.0f, 1200.0f, true,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[0].Aim = AimPolicy::Prediction;
    p.Spells[0].Priority = 92;
    p.Spells[0].MinimumAoeTargets = 1;
    p.Spells[0].PreserveAutoAttack = true;
    p.Spells[0].HarassManaPercent = 46.0f;
    p.Spells[0].ClearManaPercent = 38.0f;

    p.Spells[1] = Spell(SDK::SpellSlot::W, "Surround Sound", CastKind::Self,
        Intent::Shield | Intent::Heal | Intent::Mobility | Intent::Buff | Intent::AllyUtility | Intent::Peel,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::Automatic,
        800.0f, 0.25f, 800.0f, FLT_MAX, false,
        SDK::DamageType::True, SDK::SpellType::Targeted);
    p.Spells[1].Priority = 100;
    p.Spells[1].TriggerRange = 800.0f;
    p.Spells[1].PlayerHealthPercent = 58.0f;
    p.Spells[1].TargetHealthPercent = 58.0f;
    p.Spells[1].PreserveAutoAttack = true;

    p.Spells[2] = Spell(SDK::SpellSlot::E, "Beat Drop", CastKind::Line,
        Intent::Damage | Intent::CrowdControl | Intent::Setup | Intent::Peel | Intent::Interrupt,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::Automatic,
        1300.0f, 0.25f, 80.0f, FLT_MAX, true,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[2].Aim = AimPolicy::Prediction;
    p.Spells[2].Priority = 88;
    p.Spells[2].Hitchance = SDK::HitChance::High;
    p.Spells[2].PreserveAutoAttack = true;
    p.Spells[2].HarassManaPercent = 55.0f;

    p.Spells[3] = Spell(SDK::SpellSlot::R, "Encore", CastKind::Line,
        Intent::Damage | Intent::CrowdControl | Intent::Engage | Intent::Peel | Intent::Interrupt,
        Mode::Combo | Mode::Flee | Mode::Automatic,
        1200.0f, 0.25f, 160.0f, 1200.0f, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[3].Aim = AimPolicy::Prediction;
    p.Spells[3].Priority = 100;
    p.Spells[3].MinimumAoeTargets = 2;
    p.Spells[3].MaximumEnemiesAtDestination = 3;
    p.Spells[3].PreserveAutoAttack = true;

    p.Trade = Plan("Beat Drop setup into execute-weighted High Note while preserving W for a real shield window",
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireOutsideAaRange),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget));
    p.AllIn = Plan("Encore through champions, echoed Beat Drop control, High Note execute, and Surround Sound sustain",
        Step(SDK::SpellSlot::R, StepRule::RequireMultiTarget | StepRule::RequireSafePosition),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::HoldForExecute),
        Step(SDK::SpellSlot::W, StepRule::RequirePlayerLow));
    p.Flee = Plan("Surround Sound first, then Beat Drop peel and Encore only for a safe defensive line",
        Step(SDK::SpellSlot::W, StepRule::RequireSafePosition | StepRule::AllowDuringWindup),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::AllowDuringWindup),
        Step(SDK::SpellSlot::R, StepRule::RequireTarget));
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
