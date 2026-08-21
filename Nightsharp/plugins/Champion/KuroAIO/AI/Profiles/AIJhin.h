#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Jhin = [] {
    ChampionProfile p{};
    p.ChampionId = SDK::ChampionId::Jhin;
    p.DisplayName = "Jhin";
    p.InternalId = "champion.kuroaio.ai.jhin";
    p.PrimaryArchetype = Archetype::Marksman;
    p.Resource = ResourceModel::Ammo;
    p.Mechanics = Mechanic::Ammo | Mechanic::Mark | Mechanic::Trap |
                  Mechanic::Channel | Mechanic::Recast |
                  Mechanic::AutoWeave | Mechanic::MissingHealth;
    p.Ultimate = UltimatePolicy::SingleTarget;

    p.Spells[0] = Spell(
        SDK::SpellSlot::Q, "Dancing Grenade", CastKind::EnemyTarget,
        Intent::Damage | Intent::Execute | Intent::Waveclear |
            Intent::LastHit | Intent::Jungle | Intent::Finisher,
        Mode::Combo | Mode::Harass | Mode::LaneClear |
            Mode::Jungle | Mode::LastHit | Mode::Automatic,
        550.0f, 0.25f, 40.0f, 1800.0f, false,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotCircle);
    p.Spells[0].Priority = 95;
    p.Spells[0].PreserveAutoAttack = true;
    p.Spells[0].WeaveAfterAttack = true;
    p.Spells[0].HarassManaPercent = 42.0f;
    p.Spells[0].ClearManaPercent = 44.0f;
    p.Spells[0].AllowOnMinions = true;

    p.Spells[1] = Spell(
        SDK::SpellSlot::W, "Deadly Flourish", CastKind::Line,
        Intent::Damage | Intent::CrowdControl | Intent::Execute |
            Intent::Setup | Intent::Peel | Intent::Interrupt,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::Automatic,
        2520.0f, 0.75f, 45.0f, FLT_MAX, true,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotLine);
    p.Spells[1].Priority = 99;
    p.Spells[1].Hitchance = SDK::HitChance::High;
    p.Spells[1].PreserveAutoAttack = true;
    p.Spells[1].RequiredTargetBuff = "jhinespotteddebuff";

    p.Spells[2] = Spell(
        SDK::SpellSlot::E, "Captive Audience", CastKind::Circle,
        Intent::Damage | Intent::CrowdControl | Intent::Setup |
            Intent::Peel | Intent::AntiGapcloser | Intent::Vision,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::Automatic,
        750.0f, 0.25f, 160.0f, 1600.0f, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[2].Priority = 92;
    p.Spells[2].Hitchance = SDK::HitChance::High;
    p.Spells[2].MinimumAmmo = 1;

    p.Spells[3] = Spell(
        SDK::SpellSlot::R, "Curtain Call", CastKind::Line,
        Intent::Damage | Intent::Execute | Intent::Channel |
            Intent::Recast | Intent::Finisher,
        Mode::Combo | Mode::Automatic,
        3400.0f, 0.25f, 80.0f, 5000.0f, true,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotLine);
    p.Spells[3].Priority = 100;
    p.Spells[3].Hitchance = SDK::HitChance::VeryHigh;
    p.Spells[3].RecastSpellName = "JhinRShot";

    p.Trade = Plan(
        "AA first, Q in reload/downtime, W only on a mark or execute",
        Step(SDK::SpellSlot::Q,
             StepRule::RequireTarget | StepRule::RequireAfterAttack),
        Step(SDK::SpellSlot::W,
             StepRule::RequireTarget | StepRule::RequireMark));
    p.AllIn = Plan(
        "E committed path, AA-Q, then marked W catch",
        Step(SDK::SpellSlot::E, StepRule::RequireTarget),
        Step(SDK::SpellSlot::Q,
             StepRule::RequireTarget | StepRule::RequireAfterAttack),
        Step(SDK::SpellSlot::W,
             StepRule::RequireTarget | StepRule::RequireMark));
    p.Flee = Plan(
        "E under the committed pursuer, W only after the mark arms",
        Step(SDK::SpellSlot::E,
             StepRule::RequireTarget | StepRule::AllowDuringWindup),
        Step(SDK::SpellSlot::W,
             StepRule::RequireTarget | StepRule::RequireMark));

    p.PreferredCombatDistance = 575.0f;
    p.EngageHealthPercent = 58.0f;
    p.DefensiveHealthPercent = 32.0f;
    p.UltimateTargetHealthPercent = 28.0f;
    p.MaximumCommitEnemies = 1;
    p.BaseHumanizerMs = 23;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "JhinPassiveReload";
    p.MarkBuff = "jhinespotteddebuff";
    p.ChannelBuff = "JhinRShot";
    p.ThemeFrom = 0xFFFFD0A3u;
    p.ThemeTo = 0xFF9D2B50u;
    p.ThemeSpeed = 0.86f;
    p.TacticalSummary =
        "Keep fourth-shot and reload ownership with the orbwalker; cast Q only "
        "after an attack or during reload, W only on a marked/immobile/lethal line, "
        "place E on committed paths, and solve every Curtain Call shot inside its cone.";
    p.ResearchSummary =
        "Ported from TestOrbwalker AllChampions/Jhin.cs: after-AA Q/W, reload "
        "windows, spotted-debuff W roots, anti-gap E and reach-scored R cone shots; "
        "upgraded with first-champion collision and reachable-target scoring.";
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
