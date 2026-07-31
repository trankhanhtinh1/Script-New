#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Vayne = [] {
    ChampionProfile p{};
    p.ChampionName = "Vayne";
    p.DisplayName = "Vayne";
    p.InternalId = "champion.kuroaio.ai.vayne";
    p.PrimaryArchetype = Archetype::Marksman;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::AutoWeave | Mechanic::Stack | Mechanic::Dash |
                  Mechanic::WallInteraction | Mechanic::Terrain;
    p.Ultimate = UltimatePolicy::AllIn;

    p.Spells[0] = Spell(
        SDK::SpellSlot::Q, "Tumble", CastKind::Position,
        Intent::Mobility | Intent::Damage | Intent::Disengage |
            Intent::Engage | Intent::AutoReset,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::Flee | Mode::Automatic,
        300.0f, 0.0f, 0.0f, FLT_MAX, false,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotCircle);
    p.Spells[0].Priority = 86;
    p.Spells[0].PreserveAutoAttack = false;
    p.Spells[0].WeaveAfterAttack = true;
    p.Spells[0].HarassManaPercent = 46.0f;
    p.Spells[0].ClearManaPercent = 48.0f;

    p.Spells[1] = Spell(
        SDK::SpellSlot::W, "Silver Bolts", CastKind::Self,
        Intent::Damage | Intent::Execute | Intent::Finisher,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::LastHit | Mode::Automatic,
        0.0f, 0.0f, 0.0f, FLT_MAX, false,
        SDK::DamageType::True, SDK::SpellType::SkillshotCircle);
    p.Spells[1].Priority = 97;
    p.Spells[1].PreserveAutoAttack = true;
    p.Spells[1].RequiredTargetBuff = "VayneSilveredBolts";

    p.Spells[2] = Spell(
        SDK::SpellSlot::E, "Condemn", CastKind::EnemyTarget,
        Intent::Damage | Intent::CrowdControl | Intent::Peel |
            Intent::AntiGapcloser | Intent::Execute | Intent::Setup,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::Automatic,
        550.0f, 0.0f, 0.0f, FLT_MAX, false,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotLine);
    p.Spells[2].Priority = 99;
    p.Spells[2].PreserveAutoAttack = true;

    p.Spells[3] = Spell(
        SDK::SpellSlot::R, "Final Hour", CastKind::Self,
        Intent::Buff | Intent::Damage | Intent::Engage | Intent::Disengage |
            Intent::Setup,
        Mode::Combo | Mode::Automatic,
        0.0f, 0.0f, 0.0f, FLT_MAX, false,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotCircle);
    p.Spells[3].Priority = 100;
    p.Spells[3].PreserveAutoAttack = true;

    p.Trade = Plan(
        "Keep the selected target in attack range, weave Tumble after attacks, and hold Condemn for a wall angle or peel",
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequireAfterAttack |
             StepRule::AllowDuringWindup),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireNoCrowdControl));
    p.AllIn = Plan(
        "Enter Final Hour before a committed exchange, preserve the third Silver Bolts attack, and Condemn into terrain",
        Step(SDK::SpellSlot::R, StepRule::RequireTarget),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequireAfterAttack |
             StepRule::AllowDuringWindup),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireMark));
    p.Flee = Plan(
        "Tumble toward a safe cursor endpoint and Condemn a pursuer only as peel",
        Step(SDK::SpellSlot::Q, StepRule::RequireSafePosition | StepRule::AllowDuringWindup),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::AllowDuringWindup));

    p.PreferredCombatDistance = 550.0f;
    p.EngageHealthPercent = 62.0f;
    p.DefensiveHealthPercent = 34.0f;
    p.UltimateTargetHealthPercent = 72.0f;
    p.UltimateMinimumTargets = 1;
    p.MaximumCommitEnemies = 2;
    p.BaseHumanizerMs = 22;
    p.PreferSelectedTarget = true;
    p.AllowTurretDiveIfKillable = false;
    p.PassiveBuff = "VayneNightHunter";
    p.MarkBuff = "VayneSilveredBolts";
    p.FormBuff = "VayneInquisition";
    p.UltimateBuff = "VayneInquisition";
    p.ThemeFrom = 0xFFB7E2FFu;
    p.ThemeTo = 0xFF4D5CFFu;
    p.ThemeSpeed = 0.95f;
    p.TacticalSummary =
        "Track Silver Bolts from live target buffs, keep the orbwalker's chosen target, "
        "Tumble only after an attack or for a verified safe escape, Condemn on a terrain "
        "angle or immediate peel, and use Final Hour as a stealth-enabled committed posture.";
    p.ResearchSummary =
        "Pinned to Riot 26.15 / CommunityDragon 16.15: Tumble is a 300-unit attack reset, "
        "Silver Bolts procs on the third mark, Condemn pushes 475 units and stuns on terrain, "
        "and Final Hour grants the stealth posture that makes Tumble a safe re-engage tool.";
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
