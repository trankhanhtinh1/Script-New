#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

// Diana's route is a marked diver rather than an unconditional Q-E-R macro:
// Moonlight is reconciled from Q/buffs, W is a timed shield and orb burst, E
// consumes a mark for its reset, and R is reserved for safe pull density.
inline constexpr ChampionProfile Diana = [] {
    ChampionProfile p{};
    p.ChampionName = "Diana";
    p.DisplayName = "Diana";
    p.InternalId = "champion.kuroaio.ai.diana";
    p.PrimaryArchetype = Archetype::Diver;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::Mark | Mechanic::Dash | Mechanic::AutoWeave |
                  Mechanic::AutoReset | Mechanic::Terrain;
    p.Ultimate = UltimatePolicy::MultiTarget;

    p.Spells[0] = Spell(
        SDK::SpellSlot::Q, "Crescent Strike", CastKind::Line,
        Intent::Damage | Intent::Setup | Intent::Waveclear |
            Intent::Jungle,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::LastHit | Mode::Automatic,
        825.0f, 0.25f, 55.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[0].Priority = 92;
    p.Spells[0].TriggerRange = 825.0f;
    p.Spells[0].Hitchance = SDK::HitChance::High;
    p.Spells[0].HarassManaPercent = 50.0f;
    p.Spells[0].ClearManaPercent = 42.0f;
    p.Spells[0].AllowOnMinions = true;
    p.Spells[0].WeaveAfterAttack = true;

    p.Spells[1] = Spell(
        SDK::SpellSlot::W, "Pale Cascade", CastKind::Self,
        Intent::Damage | Intent::Shield | Intent::Buff | Intent::Peel |
            Intent::Jungle | Intent::Waveclear,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::Flee | Mode::Automatic,
        200.0f, 0.25f, 40.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[1].Priority = 84;
    p.Spells[1].TriggerRange = 200.0f;
    p.Spells[1].MinimumAoeTargets = 1;
    p.Spells[1].PreserveAutoAttack = false;

    p.Spells[2] = Spell(
        SDK::SpellSlot::E, "Lunar Rush", CastKind::EnemyTarget,
        Intent::Damage | Intent::Mobility | Intent::Engage | Intent::Disengage |
            Intent::Setup | Intent::Execute,
        Mode::Combo | Mode::Harass | Mode::Jungle | Mode::Flee | Mode::Automatic,
        825.0f, 0.15f, 40.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[2].Priority = 95;
    p.Spells[2].TriggerRange = 825.0f;
    p.Spells[2].DashDistance = 825.0f;
    p.Spells[2].RequiredTargetBuff = "DianaMoonlight";
    p.Spells[2].MaximumEnemiesAtDestination = 2;
    p.Spells[2].PreserveAutoAttack = true;

    p.Spells[3] = Spell(
        SDK::SpellSlot::R, "Moonfall", CastKind::Self,
        Intent::Damage | Intent::CrowdControl | Intent::Engage | Intent::Peel |
            Intent::Finisher | Intent::Setup,
        Mode::Combo | Mode::Automatic | Mode::Flee,
        475.0f, 0.25f, 100.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[3].Priority = 98;
    p.Spells[3].TriggerRange = 475.0f;
    p.Spells[3].MinimumAoeTargets = 2;
    p.Spells[3].MaximumEnemiesAtDestination = 3;
    p.Spells[3].PreserveAutoAttack = true;

    p.Trade = Plan(
        "Q mark, shield through orb detonation, then marked Lunar Rush",
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget),
        Step(SDK::SpellSlot::E,
             StepRule::RequireTarget | StepRule::RequireMark |
                 StepRule::RequireSafePosition));

    p.AllIn = Plan(
        "crescent mark, shielded marked dash and density-checked Moonfall",
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget),
        Step(SDK::SpellSlot::E,
             StepRule::RequireTarget | StepRule::RequireMark |
                 StepRule::RequireSafePosition),
        Step(SDK::SpellSlot::R,
             StepRule::RequireMultiTarget | StepRule::RequireSafePosition));

    p.Flee = Plan(
        "shield while withdrawing and only dash to a marked safe unit",
        Step(SDK::SpellSlot::W, StepRule::RequirePlayerLow),
        Step(SDK::SpellSlot::E, StepRule::RequireSafePosition),
        Step(SDK::SpellSlot::R, StepRule::RequireMultiTarget));

    p.PreferredCombatDistance = 225.0f;
    p.EngageHealthPercent = 58.0f;
    p.DefensiveHealthPercent = 38.0f;
    p.UltimateTargetHealthPercent = 58.0f;
    p.UltimateMinimumTargets = 2;
    p.MaximumCommitEnemies = 2;
    p.BaseHumanizerMs = 35;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "DianaPassive";
    p.MarkBuff = "DianaMoonlight";
    p.FormBuff = "DianaW";
    p.UltimateBuff = "DianaMoonfall";
    p.ThemeFrom = 0xFF9B5DE5u;
    p.ThemeTo = 0xFF4CC9F0u;
    p.ThemeSpeed = 1.10f;
    p.TacticalSummary =
        "Track the third-hit Moonsilver Blade, lead Crescent Strike around the "
        "crescent path and projectile walls, preserve Pale Cascade's shield and "
        "three orbs, consume real Moonlight on Lunar Rush for its reset, and "
        "pull only a terrain-safe, density-favorable Moonfall burst.";
    p.ResearchSummary =
        "Pinned to Riot patch 26.15 and CommunityDragon game-bin/champion data "
        "16.15, with current SDK spell/buff events, prediction/collision helpers "
        "and deterministic Moonlight, shield, dash and Moonfall regressions.";
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
