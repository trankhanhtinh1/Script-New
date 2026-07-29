#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Caitlyn = [] {
    ChampionProfile p{};
    p.ChampionName = "Caitlyn";
    p.DisplayName = "Caitlyn";
    p.InternalId = "champion.kuroaio.ai.caitlyn";
    p.PrimaryArchetype = Archetype::Marksman;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::Trap | Mechanic::Mark |
                  Mechanic::AutoWeave | Mechanic::Dash |
                  Mechanic::Channel;
    p.Ultimate = UltimatePolicy::ManualAssist;

    p.Spells[0] = Spell(
        SDK::SpellSlot::Q, "Piltover Peacemaker", CastKind::Line,
        Intent::Damage | Intent::Execute | Intent::Waveclear |
            Intent::LastHit | Intent::Jungle | Intent::Finisher,
        Mode::Combo | Mode::Harass | Mode::LaneClear |
            Mode::Jungle | Mode::LastHit | Mode::Automatic,
        1250.0f, 0.625f, 60.0f, 2200.0f, false,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotLine);
    p.Spells[0].Priority = 94;
    p.Spells[0].Hitchance = SDK::HitChance::High;
    p.Spells[0].PreserveAutoAttack = true;
    p.Spells[0].HarassManaPercent = 45.0f;
    p.Spells[0].ClearManaPercent = 48.0f;
    p.Spells[0].AllowOnMinions = true;

    p.Spells[1] = Spell(
        SDK::SpellSlot::W, "Yordle Snap Trap", CastKind::Circle,
        Intent::CrowdControl | Intent::Setup | Intent::Peel |
            Intent::AntiGapcloser | Intent::Vision,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::Automatic,
        820.0f, 1.0f, 100.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[1].Priority = 96;
    p.Spells[1].Hitchance = SDK::HitChance::High;
    p.Spells[1].MinimumAmmo = 1;
    p.Spells[1].HarassManaPercent = 52.0f;

    p.Spells[2] = Spell(
        SDK::SpellSlot::E, "90 Caliber Net", CastKind::Line,
        Intent::Damage | Intent::Mobility | Intent::Disengage |
            Intent::Peel | Intent::AntiGapcloser | Intent::Execute,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::Automatic,
        800.0f, 0.125f, 70.0f, 1600.0f, true,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[2].Priority = 98;
    p.Spells[2].Hitchance = SDK::HitChance::High;
    p.Spells[2].DashDistance = 390.0f;
    p.Spells[2].DesiredDistance = 650.0f;
    p.Spells[2].MaximumEnemiesAtDestination = 1;
    p.Spells[2].HarassManaPercent = 38.0f;

    p.Spells[3] = Spell(
        SDK::SpellSlot::R, "Ace in the Hole", CastKind::EnemyTarget,
        Intent::Damage | Intent::Execute | Intent::Channel,
        Mode::Combo | Mode::Automatic,
        2000.0f, 1.0f, 80.0f, 3200.0f, false,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotLine);
    p.Spells[3].Priority = 100;
    p.Spells[3].PreserveAutoAttack = true;

    p.Trade = Plan(
        "AA first; Q only in real downtime; trap committed movement",
        Step(SDK::SpellSlot::W, StepRule::RequireTarget),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget));
    p.AllIn = Plan(
        "E recoil only to a safe firing point, then W-Q-headshot",
        Step(SDK::SpellSlot::E,
             StepRule::RequireTarget | StepRule::RequireSafePosition),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget));
    p.Flee = Plan(
        "Trap the committed pursuer and recoil away with E",
        Step(SDK::SpellSlot::W,
             StepRule::RequireTarget | StepRule::AllowDuringWindup),
        Step(SDK::SpellSlot::E,
             StepRule::RequireTarget | StepRule::AllowDuringWindup));

    p.PreferredCombatDistance = 650.0f;
    p.EngageHealthPercent = 60.0f;
    p.DefensiveHealthPercent = 31.0f;
    p.UltimateTargetHealthPercent = 20.0f;
    p.MaximumCommitEnemies = 1;
    p.BaseHumanizerMs = 22;
    p.PreferSelectedTarget = true;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.MarkBuff = "caitlynyordletrapinternal";
    p.ChannelBuff = "CaitlynAceintheHole";
    p.ThemeFrom = 0xFF6F8CFFu;
    p.ThemeTo = 0xFFF2C5FFu;
    p.ThemeSpeed = 0.90f;
    p.TacticalSummary =
        "Prefer attack and trapped-headshot reach; Q only outside an available "
        "attack or for lethal; trap immobile/committed paths; cast E only when "
        "its recoil landing is safe and still creates damage; R only on a clean "
        "isolated execute or explicit manual request.";
    p.ResearchSummary =
        "Ported from TestOrbwalker AllChampions/Caitlyn.cs: W target spacing, "
        "E recoil reach check, Q downtime/execute gate and trap/headshot state; "
        "adapted to KuroAI reach-scored target selection.";
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
