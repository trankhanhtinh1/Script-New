#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

// Aurelion Sol is a stateful artillery battlemage, not a generic Q-W-E-R
// caster. Breath of Light owns a first-body collision and uninterrupted
// one-second burst clock; Astral Flight is a sampled route with reset/recast
// decisions; Singularity converts deaths and champion-seconds into Stardust;
// and Falling Star changes delay, radius, CC and shockwave after 75 new stacks.
inline constexpr ChampionProfile AurelionSol = [] {
    ChampionProfile p{};
    p.ChampionId = SDK::ChampionId::AurelionSol;
    p.DisplayName = "Aurelion Sol";
    p.InternalId = "champion.kuroaio.ai.aurelionsol";
    p.PrimaryArchetype = Archetype::Battlemage;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::Channel | Mechanic::Recast |
                  Mechanic::Stack | Mechanic::Transform |
                  Mechanic::Execute | Mechanic::Dash |
                  Mechanic::Terrain | Mechanic::ObjectTracking |
                  Mechanic::WallInteraction;
    p.Ultimate = UltimatePolicy::MultiTarget;

    p.Spells[0] = Spell(
        SDK::SpellSlot::Q, "Breath of Light", CastKind::Direction,
        Intent::Damage | Intent::Channel | Intent::Finisher |
            Intent::Waveclear | Intent::Jungle | Intent::Objective,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::Automatic,
        920.0f, 0.0f, 140.0f, FLT_MAX, true,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[0].Priority = 94;
    p.Spells[0].Hitchance = SDK::HitChance::High;
    p.Spells[0].PreserveAutoAttack = true;
    p.Spells[0].HarassManaPercent = 48.0f;
    p.Spells[0].ClearManaPercent = 58.0f;
    p.Spells[0].ChargeBuffName = "AurelionSolQ";
    p.Spells[0].ChargeDurationSeconds = 3.25f;

    p.Spells[1] = Spell(
        SDK::SpellSlot::W, "Astral Flight", CastKind::Position,
        Intent::Mobility | Intent::Engage | Intent::Disengage |
            Intent::Setup | Intent::Recast | Intent::AllyUtility,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::Automatic,
        1500.0f, 0.40f, 0.0f, FLT_MAX, false,
        SDK::DamageType::True, SDK::SpellType::Targeted);
    p.Spells[1].Priority = 92;
    p.Spells[1].Aim = AimPolicy::SafeCursor;
    p.Spells[1].MaximumEnemiesAtDestination = 1;
    p.Spells[1].PreserveAutoAttack = true;
    p.Spells[1].HarassManaPercent = 56.0f;
    p.Spells[1].RecastSpellName = "AurelionSolWToggle";

    p.Spells[2] = Spell(
        SDK::SpellSlot::E, "Singularity", CastKind::Circle,
        Intent::Damage | Intent::CrowdControl | Intent::Execute |
            Intent::Setup | Intent::Peel | Intent::Waveclear |
            Intent::Jungle | Intent::Objective,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::Flee | Mode::Automatic,
        920.0f, 0.50f, 275.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[2].Priority = 91;
    p.Spells[2].Hitchance = SDK::HitChance::High;
    p.Spells[2].MinimumAoeTargets = 2;
    p.Spells[2].PreserveAutoAttack = true;
    p.Spells[2].HarassManaPercent = 63.0f;
    p.Spells[2].ClearManaPercent = 62.0f;

    p.Spells[3] = Spell(
        SDK::SpellSlot::R, "Falling Star / The Skies Descend",
        CastKind::Circle,
        Intent::Damage | Intent::CrowdControl | Intent::Engage |
            Intent::Disengage | Intent::Interrupt |
            Intent::AntiGapcloser | Intent::Peel |
            Intent::Setup | Intent::Finisher | Intent::Objective,
        Mode::Combo | Mode::Flee | Mode::Automatic,
        1250.0f, 1.25f, 275.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[3].Priority = 98;
    p.Spells[3].Hitchance = SDK::HitChance::VeryHigh;
    p.Spells[3].MinimumAoeTargets = 2;
    p.Spells[3].PreserveAutoAttack = true;
    p.Spells[3].RecastSpellName = "AurelionSolR2";

    p.Trade = Plan(
        "open the first-body line, place E on the exit, then earn one full Q burst",
        Step(SDK::SpellSlot::E,
             StepRule::RequireTarget | StepRule::RequireSafePosition),
        Step(SDK::SpellSlot::Q,
             StepRule::RequireTarget | StepRule::RequireSafePosition));

    p.AllIn = Plan(
        "choose W-E-R-Q or stationary E-R-Q from CC windows, then stop flight before the dive turns unsafe",
        Step(SDK::SpellSlot::W,
             StepRule::RequireTarget | StepRule::RequireSafePosition),
        Step(SDK::SpellSlot::E,
             StepRule::RequireTarget | StepRule::RequireSafePosition),
        Step(SDK::SpellSlot::R,
             StepRule::RequireTarget | StepRule::RequireCrowdControl),
        Step(SDK::SpellSlot::Q,
             StepRule::RequireTarget | StepRule::RequireCrowdControl));

    p.Flee = Plan(
        "drop E on the pursuer, reserve R for committed peel, and fly on a terrain-separated cursor route",
        Step(SDK::SpellSlot::E,
             StepRule::RequireTarget | StepRule::AllowDuringWindup),
        Step(SDK::SpellSlot::R,
             StepRule::RequireTarget | StepRule::AllowDuringWindup),
        Step(SDK::SpellSlot::W,
             StepRule::RequireTarget | StepRule::RequireSafePosition |
                 StepRule::AllowDuringWindup));

    p.PreferredCombatDistance = 790.0f;
    p.EngageHealthPercent = 58.0f;
    p.DefensiveHealthPercent = 38.0f;
    p.UltimateTargetHealthPercent = 100.0f;
    p.UltimateMinimumTargets = 2;
    p.MaximumCommitEnemies = 2;
    p.BaseHumanizerMs = 34;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "AurelionSolPassive";
    p.ChannelBuff = "AurelionSolQ";
    p.FormBuff = "AurelionSolW";
    p.UltimateBuff = "AurelionSolR2";
    p.TrackedObjectToken = "AurelionSol";
    p.ThemeFrom = 0xFF7CE5FFu;
    p.ThemeTo = 0xFF8A52FFu;
    p.ThemeSpeed = 0.62f;
    p.TacticalSummary =
        "Automate movement, cursor-guided steering and attacks; expose the "
        "real first Q body and preserve continuous burst contact; cast W only "
        "on sampled offset routes that retain Q range while avoiding live CC; "
        "place E for champion-seconds, executions, cannon waves and line "
        "opening; track the 75-stack R cycle and account for projectile-wall "
        "impact relocation plus empowered direct/shockwave separation.";
    p.ResearchSummary =
        "Pinned to Riot 26.14 with the latest champion balance in 25.22 and "
        "CommunityDragon PC 16.14. Cross-checked Riot 13.3/14.3/14.9/14.21, "
        "a current 4.3M Master OTP guide, 2026 Challenger/1.3k-LP discussions, "
        "Quantum high-elo VODs, Mobalytics combo notation and every local "
        "Aurelion Sol reference; rejected stale pre-CGU and Arena overrides.";
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
