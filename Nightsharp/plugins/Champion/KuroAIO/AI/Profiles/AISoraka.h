#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Soraka = [] {
    ChampionProfile p{};
    p.ChampionName = "Soraka";
    p.DisplayName = "Soraka";
    p.InternalId = "champion.kuroaio.ai.soraka";
    p.PrimaryArchetype = Archetype::Enchanter;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::AllyTarget | Mechanic::Global | Mechanic::MissingHealth |
                  Mechanic::ReturnProjectile | Mechanic::Terrain;
    p.Ultimate = UltimatePolicy::SaveAlly;

    p.Spells[0] = Spell(
        SDK::SpellSlot::Q, "Starcall", CastKind::Circle,
        Intent::Damage | Intent::Heal | Intent::CrowdControl | Intent::Setup,
        Mode::Combo | Mode::Harass | Mode::Automatic,
        800.0f, 0.25f, 230.0f, 1750.0f, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[0].Priority = 78;
    p.Spells[0].TriggerRange = 800.0f;
    p.Spells[0].HarassManaPercent = 52.0f;
    p.Spells[0].PreserveAutoAttack = true;

    p.Spells[1] = Spell(
        SDK::SpellSlot::W, "Astral Infusion", CastKind::AllyTarget,
        Intent::Heal | Intent::AllyUtility | Intent::Buff,
        Mode::Combo | Mode::Harass | Mode::Automatic | Mode::Flee,
        550.0f, 0.25f, 0.0f, FLT_MAX, false,
        SDK::DamageType::True, SDK::SpellType::Targeted);
    p.Spells[1].Priority = 100;
    p.Spells[1].TriggerRange = 550.0f;
    p.Spells[1].TargetHealthPercent = 72.0f;
    p.Spells[1].PlayerHealthPercent = 12.0f;
    p.Spells[1].PreserveAutoAttack = true;

    p.Spells[2] = Spell(
        SDK::SpellSlot::E, "Equinox", CastKind::Circle,
        Intent::Damage | Intent::CrowdControl | Intent::Interrupt | Intent::Peel |
            Intent::AntiGapcloser | Intent::Setup,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::Automatic,
        875.0f, 0.25f, 260.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[2].Priority = 92;
    p.Spells[2].TriggerRange = 875.0f;
    p.Spells[2].MinimumAoeTargets = 1;
    p.Spells[2].PreserveAutoAttack = true;

    p.Spells[3] = Spell(
        SDK::SpellSlot::R, "Wish", CastKind::Self,
        Intent::Heal | Intent::AllyUtility | Intent::Peel | Intent::Cleanse,
        Mode::Combo | Mode::Flee | Mode::Automatic,
        25000.0f, 0.25f, 0.0f, FLT_MAX, false,
        SDK::DamageType::True, SDK::SpellType::Targeted);
    p.Spells[3].Priority = 110;
    p.Spells[3].PlayerHealthPercent = 28.0f;
    p.Spells[3].TargetHealthPercent = 34.0f;
    p.Spells[3].MinimumAoeTargets = 1;
    p.Spells[3].PreserveAutoAttack = true;

    p.Trade = Plan(
        "Starcall poke and rejuvenation into health-costed ally infusion",
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget | StepRule::RequireSafePosition),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireSafePosition));
    p.AllIn = Plan(
        "Equinox peel zone, Starcall rejuvenation and emergency Wish",
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireCrowdControl),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget | StepRule::RequireSafePosition),
        Step(SDK::SpellSlot::R, StepRule::RequireTargetLow | StepRule::RequirePlayerLow));
    p.Flee = Plan(
        "Save the lowest safe ally with Wish then zone the pursuer",
        Step(SDK::SpellSlot::R, StepRule::RequireTargetLow | StepRule::RequirePlayerLow),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget | StepRule::RequireSafePosition),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireSafePosition));

    p.PreferredCombatDistance = 650.0f;
    p.EngageHealthPercent = 60.0f;
    p.DefensiveHealthPercent = 28.0f;
    p.UltimateTargetHealthPercent = 34.0f;
    p.UltimateMinimumTargets = 1;
    p.MaximumCommitEnemies = 2;
    p.BaseHumanizerMs = 48;
    p.PreferSelectedTarget = true;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "SorakaPassive";
    p.MarkBuff = "SorakaQRegen";
    p.UltimateBuff = "SorakaR";
    p.ThemeFrom = 0xFF9BE7FFu;
    p.ThemeTo = 0xFFB879FFu;
    p.ThemeSpeed = 0.92f;
    p.TacticalSummary =
        "Keep Starcall contact and its rejuvenation return as the sustain engine, pay "
        "Astral Infusion only from a safe health reserve, use Equinox to silence or "
        "root committed enemies, and cast global Wish for a genuine ally-save window.";
    p.ResearchSummary =
        "CommunityDragon PC 16.15 Soraka spell records, Riot 26.15 baseline, "
        "Q return timing, W health-cost/refund rules, Equinox zone timing, Wish low-health "
        "amplification, and deterministic support geometry regressions.";
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
