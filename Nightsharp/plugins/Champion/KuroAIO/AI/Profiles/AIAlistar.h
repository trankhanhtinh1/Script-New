#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

// Alistar is modeled as a displacement support, not a W-Q button.  The full
// controller decides whether Headbutt must be buffered, preserved for peel,
// aimed into terrain, used as an insec, or spent on an escape unit; it also
// owns E's four-stack AA timing and R's cleanse-versus-tank economy.
inline constexpr ChampionProfile Alistar = [] {
    ChampionProfile p{};
    p.ChampionName = "Alistar";
    p.DisplayName = "Alistar";
    p.InternalId = "champion.kuroaio.ai.alistar";
    p.PrimaryArchetype = Archetype::Vanguard;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::Dash | Mechanic::Stack |
                  Mechanic::WallInteraction | Mechanic::Cleanse |
                  Mechanic::AutoWeave | Mechanic::DirectionalSweet;
    p.Ultimate = UltimatePolicy::Defensive;

    p.Spells[0] = Spell(
        SDK::SpellSlot::Q, "Pulverize", CastKind::Self,
        Intent::Damage | Intent::CrowdControl | Intent::Engage |
            Intent::Disengage | Intent::Interrupt |
            Intent::AntiGapcloser | Intent::Peel | Intent::Setup,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::Automatic,
        375.0f, 0.25f, 750.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[0].Priority = 96;
    p.Spells[0].TriggerRange = 375.0f;
    p.Spells[0].MinimumAoeTargets = 2;
    p.Spells[0].HarassManaPercent = 48.0f;
    p.Spells[0].PreserveAutoAttack = true;

    p.Spells[1] = Spell(
        SDK::SpellSlot::W, "Headbutt", CastKind::EnemyTarget,
        Intent::Damage | Intent::CrowdControl | Intent::Mobility |
            Intent::Engage | Intent::Disengage | Intent::Interrupt |
            Intent::AntiGapcloser | Intent::Peel | Intent::Setup,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::Automatic,
        650.0f, 0.0f, 100.0f, 1200.0f, false,
        SDK::DamageType::Magical, SDK::SpellType::Targeted);
    p.Spells[1].Priority = 92;
    p.Spells[1].DashDistance = 650.0f;
    p.Spells[1].MaximumEnemiesAtDestination = 3;
    p.Spells[1].HarassManaPercent = 55.0f;
    p.Spells[1].AllowOnMinions = true;
    p.Spells[1].PreserveAutoAttack = true;

    p.Spells[2] = Spell(
        SDK::SpellSlot::E, "Trample", CastKind::Self,
        Intent::Damage | Intent::CrowdControl | Intent::Buff |
            Intent::Peel | Intent::Setup | Intent::AutoReset,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::Automatic,
        350.0f, 0.0f, 700.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[2].Priority = 86;
    p.Spells[2].TriggerRange = 350.0f;
    p.Spells[2].HarassManaPercent = 52.0f;
    p.Spells[2].WeaveAfterAttack = true;
    p.Spells[2].PreserveAutoAttack = true;

    p.Spells[3] = Spell(
        SDK::SpellSlot::R, "Unbreakable Will", CastKind::Self,
        Intent::Buff | Intent::Cleanse | Intent::Disengage |
            Intent::Peel | Intent::Setup,
        Mode::Combo | Mode::Flee | Mode::Automatic,
        0.0f, 0.0f, 0.0f, FLT_MAX, false,
        SDK::DamageType::True, SDK::SpellType::Targeted);
    p.Spells[3].Priority = 100;
    p.Spells[3].TriggerRange = 1100.0f;
    p.Spells[3].PlayerHealthPercent = 42.0f;
    p.Spells[3].PreserveAutoAttack = true;

    p.Trade = Plan(
        "close Q, start Trample, preserve Headbutt for disengage",
        Step(SDK::SpellSlot::Q,
             StepRule::RequireTarget | StepRule::RequireInsideAaRange),
        Step(SDK::SpellSlot::E,
             StepRule::RequireTarget));

    p.AllIn = Plan(
        "choose displacement branch, layer Q/E, tank only on real return damage",
        Step(SDK::SpellSlot::W,
             StepRule::RequireTarget | StepRule::RequireSafePosition),
        Step(SDK::SpellSlot::Q,
             StepRule::RequireTarget | StepRule::AllowDuringWindup),
        Step(SDK::SpellSlot::E,
             StepRule::RequireTarget),
        Step(SDK::SpellSlot::R,
             StepRule::RequirePlayerLow));

    p.Flee = Plan(
        "Q the pursuer, Headbutt an escape unit, keep R for disabling CC",
        Step(SDK::SpellSlot::Q),
        Step(SDK::SpellSlot::W,
             StepRule::RequireTarget | StepRule::RequireSafePosition),
        Step(SDK::SpellSlot::R,
             StepRule::RequirePlayerLow));

    p.PreferredCombatDistance = 285.0f;
    p.EngageHealthPercent = 46.0f;
    p.DefensiveHealthPercent = 34.0f;
    p.UltimateTargetHealthPercent = 100.0f;
    p.UltimateMinimumTargets = 2;
    p.MaximumCommitEnemies = 3;
    p.BaseHumanizerMs = 42;
    p.AllowTurretDiveIfKillable = true;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "AlistarPassiveStacks";
    p.MarkBuff = "AlistarEAttack";
    p.FormBuff = "AlistarE";
    p.UltimateBuff = "FerociousHowl";
    p.ThemeFrom = 0xFFD89BFFu;
    p.ThemeTo = 0xFF7A44D6u;
    p.ThemeSpeed = 0.92f;
    p.TacticalSummary =
        "Peel the carry before forcing an engage, buffer Q only when the full "
        "Headbutt displacement is unwanted, preserve wall pins and insec "
        "angles, prime the fifth Trample pulse inside the AA windup, and use "
        "Unbreakable Will as either a critical cleanse or a timed tank window.";
    p.ResearchSummary =
        "CommunityDragon PC 16.14 bin/game data, Riot 13.3 and 26.14 notes, "
        "current Season 16 build data, Challenger/OTP Alistar material, "
        "Alicopter roaming concepts, pro support peel/dive patterns, combo "
        "demonstrations, and deterministic geometry/timing regressions.";
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
