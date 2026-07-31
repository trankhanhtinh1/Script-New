#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Olaf = [] {
    ChampionProfile p{};
    p.ChampionName = "Olaf";
    p.DisplayName = "Olaf";
    p.InternalId = "champion.kuroaio.ai.olaf";
    p.PrimaryArchetype = Archetype::Diver;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::AutoWeave | Mechanic::AutoReset | Mechanic::Execute |
                  Mechanic::ObjectTracking | Mechanic::Channel;
    p.Ultimate = UltimatePolicy::Defensive;
    p.PreferredCombatDistance = 325.0f;
    p.EngageHealthPercent = 48.0f;
    p.DefensiveHealthPercent = 24.0f;
    p.UltimateTargetHealthPercent = 100.0f;
    p.MaximumCommitEnemies = 2;
    p.BaseHumanizerMs = 48;
    p.PassiveBuff = "OlafBerserkerRage";
    p.UltimateBuff = "OlafRagnarok";
    p.TrackedObjectToken = "OlafAxe";
    p.TacticalSummary =
        "Axe-return bruiser: recover Undertow axes, reserve Reckless Swing for lethal or low-health trades, and use Ragnarok only to cross crowd control while attacking.";
    p.ResearchSummary =
        "Riot 26.15 and CommunityDragon 16.15 Olaf Q/W/E/R data; axe lifecycle, true-damage health cost, and Ragnarok immunity are event-reconciled.";

    p.Spells[0] = Spell(SDK::SpellSlot::Q, "Undertow", CastKind::Position,
        Intent::Damage | Intent::CrowdControl | Intent::Setup | Intent::Disengage |
            Intent::Waveclear | Intent::Jungle,
        AllModes, 1000.0f, 0.25f, 100.0f, 1600.0f, true,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotLine);
    p.Spells[0].Aim = AimPolicy::Prediction;
    p.Spells[0].Priority = 90;
    p.Spells[0].RequiredTargetBuff = "olafaxepickup";
    p.Spells[0].HarassManaPercent = 35.0f;
    p.Spells[0].ClearManaPercent = 30.0f;

    p.Spells[1] = Spell(SDK::SpellSlot::W, "Tough It Out", CastKind::Self,
        Intent::Shield | Intent::Buff | Intent::AutoReset,
        Mode::Combo | Mode::Automatic | Mode::Jungle | Mode::Flee,
        0.0f, 0.0f, 0.0f, FLT_MAX, false,
        SDK::DamageType::True, SDK::SpellType::Targeted);
    p.Spells[1].Priority = 76;
    p.Spells[1].PlayerHealthPercent = 70.0f;

    p.Spells[2] = Spell(SDK::SpellSlot::E, "Reckless Swing", CastKind::EnemyTarget,
        Intent::Damage | Intent::Execute | Intent::Finisher | Intent::LastHit |
            Intent::Jungle,
        AllModes, 325.0f, 0.0f, 0.0f, FLT_MAX, false,
        SDK::DamageType::True, SDK::SpellType::Targeted);
    p.Spells[2].Aim = AimPolicy::TargetPosition;
    p.Spells[2].Priority = 100;
    p.Spells[2].WeaveAfterAttack = true;
    p.Spells[2].TargetHealthPercent = 52.0f;

    p.Spells[3] = Spell(SDK::SpellSlot::R, "Ragnarok", CastKind::Self,
        Intent::Cleanse | Intent::Buff | Intent::Engage | Intent::Disengage,
        Mode::Combo | Mode::Automatic | Mode::Flee,
        0.0f, 0.0f, 0.0f, FLT_MAX, false,
        SDK::DamageType::True, SDK::SpellType::Targeted);
    p.Spells[3].Priority = 98;
    p.Spells[3].PlayerHealthPercent = 68.0f;
    p.Spells[3].PreserveAutoAttack = false;

    p.Trade = Plan("Undertow poke and true-damage check",
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget, 0, 850),
        Step(SDK::SpellSlot::E, StepRule::RequireTargetLow | StepRule::RequireAfterAttack, 120, 900));
    p.AllIn = Plan("axe chase with Ragnarok commit",
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget, 0, 900),
        Step(SDK::SpellSlot::R, StepRule::RequireTarget | StepRule::AllowDuringWindup, 60, 1000),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget | StepRule::AllowDuringWindup, 90, 1100),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireTargetLow, 130, 1300));
    p.Flee = Plan("Ragnarok peel and axe slow",
        Step(SDK::SpellSlot::R, StepRule::RequirePlayerLow | StepRule::AllowDuringWindup, 0, 800),
        Step(SDK::SpellSlot::Q, StepRule::RequireSafePosition, 80, 900));
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
