#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

// Taliyah is a displacement control mage whose real state is spatial: every Q
// chooses between a five-rock volley and a consumed Worked Ground boulder,
// every W direction changes who is saved or delivered, and E value depends on
// which rows exist when a dash/displacement crosses them.  The full controller
// owns that state; this profile only publishes the spell contract and intent.
inline constexpr ChampionProfile Taliyah = [] {
    ChampionProfile p{};
    p.ChampionName = "Taliyah";
    p.DisplayName = "Taliyah";
    p.InternalId = "champion.kuroaio.ai.taliyah";
    p.PrimaryArchetype = Archetype::Battlemage;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::ObjectTracking | Mechanic::Trap |
                  Mechanic::Terrain | Mechanic::WallInteraction |
                  Mechanic::Channel | Mechanic::AutoWeave;
    p.Ultimate = UltimatePolicy::ManualAssist;

    p.Spells[0] = Spell(
        SDK::SpellSlot::Q, "Threaded Volley", CastKind::Line,
        Intent::Damage | Intent::Execute | Intent::Setup |
            Intent::Waveclear | Intent::LastHit | Intent::Jungle |
            Intent::Objective | Intent::Peel | Intent::Finisher,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::LastHit | Mode::Flee | Mode::Automatic,
        1000.0f, 0.25f, 50.0f, 3600.0f, true,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[0].Priority = 99;
    p.Spells[0].TriggerRange = 1000.0f;
    p.Spells[0].Aim = AimPolicy::Prediction;
    p.Spells[0].Hitchance = SDK::HitChance::VeryHigh;
    p.Spells[0].PreserveAutoAttack = true;
    p.Spells[0].AllowOnMinions = true;
    p.Spells[0].ComboManaPercent = 0.0f;
    p.Spells[0].HarassManaPercent = 38.0f;
    p.Spells[0].ClearManaPercent = 31.0f;

    p.Spells[1] = Spell(
        SDK::SpellSlot::W, "Seismic Shove", CastKind::Vector,
        Intent::CrowdControl | Intent::Setup | Intent::Peel |
            Intent::Interrupt | Intent::AntiGapcloser |
            Intent::Disengage | Intent::AllyUtility,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::Automatic,
        900.0f, 0.75f, 225.0f, FLT_MAX, false,
        SDK::DamageType::True, SDK::SpellType::SkillshotCircle);
    p.Spells[1].Priority = 100;
    p.Spells[1].TriggerRange = 900.0f;
    p.Spells[1].DesiredDistance = 400.0f;
    p.Spells[1].Aim = AimPolicy::BehindTarget;
    p.Spells[1].Hitchance = SDK::HitChance::VeryHigh;
    p.Spells[1].PreserveAutoAttack = true;
    p.Spells[1].ComboManaPercent = 0.0f;
    p.Spells[1].HarassManaPercent = 52.0f;

    p.Spells[2] = Spell(
        SDK::SpellSlot::E, "Unraveled Earth", CastKind::Cone,
        Intent::Damage | Intent::CrowdControl | Intent::Setup |
            Intent::Peel | Intent::AntiGapcloser | Intent::Disengage |
            Intent::Waveclear | Intent::Jungle | Intent::Objective,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::Flee | Mode::Automatic,
        950.0f, 0.25f, 250.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCone);
    p.Spells[2].Priority = 98;
    p.Spells[2].TriggerRange = 950.0f;
    p.Spells[2].Aim = AimPolicy::Prediction;
    p.Spells[2].Hitchance = SDK::HitChance::High;
    p.Spells[2].PreserveAutoAttack = true;
    p.Spells[2].ComboManaPercent = 0.0f;
    p.Spells[2].HarassManaPercent = 55.0f;
    p.Spells[2].ClearManaPercent = 42.0f;

    p.Spells[3] = Spell(
        SDK::SpellSlot::R, "Weaver's Wall", CastKind::Position,
        Intent::Mobility | Intent::Engage | Intent::Disengage |
            Intent::Objective | Intent::AllyUtility | Intent::Channel,
        Mode::Automatic | Mode::Flee,
        6500.0f, 1.0f, 120.0f, 2000.0f, false,
        SDK::DamageType::True, SDK::SpellType::SkillshotLine);
    p.Spells[3].Priority = 100;
    p.Spells[3].TriggerRange = 6500.0f;
    p.Spells[3].Aim = AimPolicy::Cursor;
    p.Spells[3].PreserveAutoAttack = true;
    p.Spells[3].ComboManaPercent = 0.0f;
    p.Spells[3].MaximumEnemiesAtDestination = 2;

    p.Trade = Plan(
        "Big Q slow into W-E-Q; otherwise hold W until commitment",
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget),
        Step(SDK::SpellSlot::W,
             StepRule::RequireTarget | StepRule::AllowDuringWindup),
        Step(SDK::SpellSlot::E,
             StepRule::RequireTarget | StepRule::AllowDuringWindup),
        Step(SDK::SpellSlot::Q,
             StepRule::RequireTarget | StepRule::AllowDuringWindup));

    p.AllIn = Plan(
        "Choose W-E-Q for surprise/follow-up or E-W-Q for dash control and full mine rows",
        Step(SDK::SpellSlot::E, StepRule::RequireTarget),
        Step(SDK::SpellSlot::W,
             StepRule::RequireTarget | StepRule::AllowDuringWindup),
        Step(SDK::SpellSlot::Q,
             StepRule::RequireTarget | StepRule::AllowDuringWindup));

    p.Flee = Plan(
        "E across the committed path, W away from player/carry, then Q while kiting",
        Step(SDK::SpellSlot::E,
             StepRule::RequireTarget | StepRule::AllowDuringWindup),
        Step(SDK::SpellSlot::W,
             StepRule::RequireTarget | StepRule::AllowDuringWindup),
        Step(SDK::SpellSlot::Q,
             StepRule::RequireTarget | StepRule::AllowDuringWindup));

    p.PreferredCombatDistance = 720.0f;
    p.EngageHealthPercent = 58.0f;
    p.DefensiveHealthPercent = 31.0f;
    p.UltimateTargetHealthPercent = 0.0f;
    p.UltimateMinimumTargets = 1;
    p.MaximumCommitEnemies = 2;
    p.BaseHumanizerMs = 31;
    p.PreferSelectedTarget = true;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "TaliyahPassive";
    p.MarkBuff = "TaliyahQGround";
    p.ChannelBuff = "TaliyahR";
    p.UltimateBuff = "TaliyahR";
    p.TrackedObjectToken = "TaliyahQMis";
    p.ThemeFrom = 0xFFB46C3Eu;
    p.ThemeTo = 0xFF62D6E8u;
    p.ThemeSpeed = 0.86f;
    p.TacticalSummary =
        "Resolve accelerating/full-volley versus fixed-speed/boulder Q first body and AoE; track every Worked Ground zone; select E-W or W-E by dash, spacing, row timing and ally follow-up; choose W vectors that convert mines without saving the enemy or delivering a diver; reserve Weaver's Wall for an explicit player key and verified team partition.";
    p.ResearchSummary =
        "Pinned to CommunityDragon PC 16.14 (15 July 2026) and reconciled with Riot 12.9, 12.10, 13.9, 25.12, 25.18, 26.2, 26.5 and 26.9; cross-checked with current OP.GG order, TaliyahMains W-E/E-W discussions, Challenger Season-14/16 guides, KR OTP 26.3-26.4, EUW Challenger 26.13 and Faker/Chovy 2025 POVs.";
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
