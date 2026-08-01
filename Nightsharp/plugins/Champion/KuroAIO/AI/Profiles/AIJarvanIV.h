#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile JarvanIV = [] {
    ChampionProfile p{};
    p.ChampionId = SDK::ChampionId::JarvanIV;
    p.DisplayName = "Jarvan IV";
    p.InternalId = "champion.kuroaio.ai.jarvaniv";
    p.PrimaryArchetype = Archetype::Diver;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::Dash | Mechanic::ObjectTracking |
                  Mechanic::Terrain | Mechanic::Recast |
                  Mechanic::AutoWeave | Mechanic::DirectionalSweet;
    p.Ultimate = UltimatePolicy::RecastControl;

    p.Spells[0] = Spell(
        SDK::SpellSlot::Q, "Dragon Strike", CastKind::Line,
        Intent::Damage | Intent::CrowdControl | Intent::Mobility |
            Intent::Engage | Intent::Disengage | Intent::Interrupt |
            Intent::AntiGapcloser | Intent::Peel | Intent::Setup |
            Intent::Waveclear | Intent::Jungle,
        AllModes,
        770.0f, 0.25f, 70.0f, FLT_MAX, false,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotLine);
    p.Spells[0].Priority = 94;
    p.Spells[0].HarassManaPercent = 45.0f;
    p.Spells[0].ClearManaPercent = 52.0f;
    p.Spells[0].DashDistance = 770.0f;
    p.Spells[0].MaximumEnemiesAtDestination = 3;
    p.Spells[0].PreserveAutoAttack = true;

    p.Spells[1] = Spell(
        SDK::SpellSlot::W, "Golden Aegis", CastKind::Self,
        Intent::Shield | Intent::CrowdControl | Intent::Disengage |
            Intent::AntiGapcloser | Intent::Peel | Intent::Setup,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::Automatic,
        625.0f, 0.125f, 625.0f, 1500.0f, false,
        SDK::DamageType::True, SDK::SpellType::SkillshotCircle);
    p.Spells[1].Priority = 90;
    p.Spells[1].TriggerRange = 625.0f;
    p.Spells[1].PlayerHealthPercent = 62.0f;
    p.Spells[1].HarassManaPercent = 50.0f;
    p.Spells[1].PreserveAutoAttack = true;

    p.Spells[2] = Spell(
        SDK::SpellSlot::E, "Demacian Standard", CastKind::Circle,
        Intent::Damage | Intent::Buff | Intent::Mobility |
            Intent::Engage | Intent::Disengage | Intent::Setup |
            Intent::Vision | Intent::Waveclear | Intent::Jungle |
            Intent::AllyUtility,
        AllModes,
        860.0f, 0.25f, 175.0f, 1450.0f, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[2].Priority = 92;
    p.Spells[2].HarassManaPercent = 52.0f;
    p.Spells[2].ClearManaPercent = 55.0f;
    p.Spells[2].DashDistance = 860.0f;
    p.Spells[2].MaximumEnemiesAtDestination = 3;
    p.Spells[2].PreserveAutoAttack = true;

    p.Spells[3] = Spell(
        SDK::SpellSlot::R, "Cataclysm", CastKind::EnemyTarget,
        Intent::Damage | Intent::CrowdControl | Intent::Mobility |
            Intent::Engage | Intent::Execute | Intent::Peel |
            Intent::Setup | Intent::Recast,
        Mode::Combo | Mode::Flee | Mode::Automatic,
        650.0f, 0.25f, 340.0f, FLT_MAX, false,
        SDK::DamageType::Physical, SDK::SpellType::Targeted);
    p.Spells[3].Priority = 98;
    p.Spells[3].MinimumAoeTargets = 2;
    p.Spells[3].TargetHealthPercent = 55.0f;
    p.Spells[3].MaximumEnemiesAtDestination = 3;
    p.Spells[3].PreserveAutoAttack = true;
    p.Spells[3].RecastSpellName = "JarvanIVCataclysm";

    p.Trade = Plan(
        "proc Martial Cadence, Q armor shred, preserve E for exit",
        Step(SDK::SpellSlot::Q,
             StepRule::RequireTarget | StepRule::RequireInsideAaRange),
        Step(SDK::SpellSlot::W,
             StepRule::RequireTarget));

    p.AllIn = Plan(
        "safe E-Q knock-up, passive attack, W stick, terrain-safe Cataclysm",
        Step(SDK::SpellSlot::E,
             StepRule::RequireTarget | StepRule::RequireSafePosition),
        Step(SDK::SpellSlot::Q,
             StepRule::RequireTarget | StepRule::AllowDuringWindup),
        Step(SDK::SpellSlot::W,
             StepRule::RequireTarget),
        Step(SDK::SpellSlot::R,
             StepRule::RequireTarget | StepRule::RequireSafePosition |
                 StepRule::SkipIfKillableWithout));

    p.Flee = Plan(
        "place flag toward cursor, Q through it, shield pursuer pressure",
        Step(SDK::SpellSlot::E,
             StepRule::RequireSafePosition),
        Step(SDK::SpellSlot::Q,
             StepRule::RequireSafePosition | StepRule::AllowDuringWindup),
        Step(SDK::SpellSlot::W));

    p.PreferredCombatDistance = 190.0f;
    p.EngageHealthPercent = 47.0f;
    p.DefensiveHealthPercent = 31.0f;
    p.UltimateTargetHealthPercent = 55.0f;
    p.UltimateMinimumTargets = 2;
    p.MaximumCommitEnemies = 3;
    p.BaseHumanizerMs = 45;
    p.PreferSelectedTarget = true;
    p.AllowTurretDiveIfKillable = true;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "JarvanIVMartialCadenceCheck";
    p.MarkBuff = "JarvanIVDragonStrikeDebuff";
    p.UltimateBuff = "JarvanIVCataclysm";
    p.TrackedObjectToken = "JarvanIVDemacianStandard";
    p.ThemeFrom = 0xFFF3C55Bu;
    p.ThemeTo = 0xFF30549Bu;
    p.ThemeSpeed = 0.90f;
    p.TacticalSummary =
        "Spend Martial Cadence before replacing an auto with a spell, place "
        "Demacian Standard only where Dragon Strike has a safe knock-up line, "
        "time Golden Aegis against real nearby pressure, and cast or collapse "
        "Cataclysm only after evaluating its landing point and trapped units.";
    p.ResearchSummary =
        "Pinned Riot 26.15 and CommunityDragon PC 16.15 spell-bin data, "
        "Summoner's Rift flag/dash and arena terrain behavior, passive "
        "per-target cadence, endpoint safety, and deterministic regressions.";
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
