#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Rakan = [] {
    ChampionProfile p{};
    p.ChampionName = "Rakan";
    p.DisplayName = "Rakan";
    p.InternalId = "champion.kuroaio.ai.rakan";
    p.PrimaryArchetype = Archetype::Catcher;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::Dash | Mechanic::AllyTarget |
                  Mechanic::AutoWeave | Mechanic::DirectionalSweet;
    p.Ultimate = UltimatePolicy::Defensive;

    p.Spells[0] = Spell(
        SDK::SpellSlot::Q, "Gleaming Quill", CastKind::Line,
        Intent::Damage | Intent::Heal | Intent::AllyUtility | Intent::Setup |
            Intent::Peel,
        Mode::Combo | Mode::Harass | Mode::Automatic,
        900.0f, 0.25f, 65.0f, 1450.0f, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[0].Priority = 82;
    p.Spells[0].TriggerRange = 900.0f;
    p.Spells[0].HarassManaPercent = 52.0f;
    p.Spells[0].PreserveAutoAttack = true;

    p.Spells[1] = Spell(
        SDK::SpellSlot::W, "Grand Entrance", CastKind::Position,
        Intent::Damage | Intent::CrowdControl | Intent::Mobility |
            Intent::Engage | Intent::Disengage | Intent::Interrupt |
            Intent::AntiGapcloser | Intent::Peel | Intent::Setup,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::Automatic,
        650.0f, 0.25f, 275.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[1].Priority = 94;
    p.Spells[1].TriggerRange = 650.0f;
    p.Spells[1].MaximumEnemiesAtDestination = 3;
    p.Spells[1].HarassManaPercent = 58.0f;
    p.Spells[1].PreserveAutoAttack = true;

    p.Spells[2] = Spell(
        SDK::SpellSlot::E, "Battle Dance", CastKind::AllyTarget,
        Intent::Mobility | Intent::Shield | Intent::AllyUtility |
            Intent::Disengage | Intent::Peel | Intent::Setup,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::Automatic,
        700.0f, 0.0f, 0.0f, FLT_MAX, false,
        SDK::DamageType::True, SDK::SpellType::Targeted);
    p.Spells[2].Priority = 100;
    p.Spells[2].DashDistance = 700.0f;
    p.Spells[2].MaximumEnemiesAtDestination = 2;
    p.Spells[2].HarassManaPercent = 42.0f;
    p.Spells[2].PreserveAutoAttack = true;

    p.Spells[3] = Spell(
        SDK::SpellSlot::R, "The Quickness", CastKind::Self,
        Intent::CrowdControl | Intent::Mobility | Intent::Engage |
            Intent::Disengage | Intent::Peel | Intent::AntiGapcloser |
            Intent::Setup,
        Mode::Combo | Mode::Flee | Mode::Automatic,
        450.0f, 0.0f, 450.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::Targeted);
    p.Spells[3].Priority = 98;
    p.Spells[3].TriggerRange = 450.0f;
    p.Spells[3].MinimumAoeTargets = 2;
    p.Spells[3].PlayerHealthPercent = 38.0f;
    p.Spells[3].PreserveAutoAttack = true;

    p.Trade = Plan(
        "Q for poke/heal, safe W entry, preserve E for ally exit",
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget),
        Step(SDK::SpellSlot::W,
             StepRule::RequireTarget | StepRule::RequireSafePosition),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireSafePosition));

    p.AllIn = Plan(
        "R movement charm into W knockup, Q sustain and E return",
        Step(SDK::SpellSlot::R, StepRule::RequireMultiTarget | StepRule::AllowDuringWindup),
        Step(SDK::SpellSlot::W,
             StepRule::RequireTarget | StepRule::RequireSafePosition),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireSafePosition));

    p.Flee = Plan(
        "Battle Dance to the safest ally, then peel with W or R",
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireSafePosition),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget | StepRule::RequireSafePosition),
        Step(SDK::SpellSlot::R, StepRule::RequirePlayerLow));

    p.PreferredCombatDistance = 375.0f;
    p.EngageHealthPercent = 52.0f;
    p.DefensiveHealthPercent = 35.0f;
    p.UltimateTargetHealthPercent = 100.0f;
    p.UltimateMinimumTargets = 2;
    p.MaximumCommitEnemies = 3;
    p.BaseHumanizerMs = 42;
    p.PreferSelectedTarget = true;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "RakanPassiveShield";
    p.FormBuff = "RakanE";
    p.UltimateBuff = "RakanR";
    p.ThemeFrom = 0xFFFFC06Au;
    p.ThemeTo = 0xFFB744D6u;
    p.ThemeSpeed = 1.02f;
    p.TacticalSummary =
        "Prioritize ally protection and safe Battle Dance routes, use Grand Entrance "
        "for confirmed knockups, thread Gleaming Quill through enemies for the heal, "
        "and reserve The Quickness for defensive charm movement or multi-target engage.";
    p.ResearchSummary =
        "Riot 26.15 and CommunityDragon PC 16.15 spell data, current Season 16 "
        "support builds, Challenger Rakan engage/peel patterns, ally selection and "
        "dash safety analysis, plus deterministic Q/W/E/R geometry regressions.";
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
