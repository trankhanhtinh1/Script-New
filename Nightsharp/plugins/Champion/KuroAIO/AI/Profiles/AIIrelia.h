#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Irelia = [] {
    ChampionProfile p{};
    p.ChampionName = "Irelia";
    p.DisplayName = "Irelia";
    p.InternalId = "champion.kuroaio.ai.irelia";
    p.PrimaryArchetype = Archetype::Skirmisher;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::Dash | Mechanic::Charge | Mechanic::Channel |
                  Mechanic::Recast | Mechanic::Stack | Mechanic::Mark |
                  Mechanic::AutoWeave | Mechanic::AutoReset;
    p.Ultimate = UltimatePolicy::AllIn;
    p.PreferredCombatDistance = 425.0f;
    p.EngageHealthPercent = 42.0f;
    p.DefensiveHealthPercent = 30.0f;
    p.UltimateTargetHealthPercent = 62.0f;
    p.UltimateMinimumTargets = 2;
    p.MaximumCommitEnemies = 2;
    p.BaseHumanizerMs = 52;
    p.PreferSelectedTarget = true;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "ireliapassivestacks";
    p.MarkBuff = "ireliamark";
    p.ChannelBuff = "ireliawdefense";
    p.TrackedObjectToken = "IreliaE";
    p.ThemeFrom = 0xFF58C9FFu;
    p.ThemeTo = 0xFFD7ECFFu;
    p.TacticalSummary =
        "Four-stack skirmisher: route reset-safe Bladesurges through marked or "
        "lethal units, charge W into committed damage, cross E blades through "
        "prediction, and use R marks without dashing into an unsafe blade cage.";
    p.ResearchSummary =
        "Riot 26.15 / CommunityDragon 16.15 Summoner's Rift contract; state is "
        "reconciled from spell, buff and runtime-name telemetry.";

    p.Spells[0] = Spell(
        SDK::SpellSlot::Q, "Bladesurge", CastKind::EnemyTarget,
        Intent::Damage | Intent::Mobility | Intent::Heal | Intent::Execute |
            Intent::Waveclear | Intent::Jungle | Intent::LastHit |
            Intent::AutoReset,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::LastHit | Mode::Flee | Mode::Automatic,
        600.0f, 0.0f, 0.0f, 1400.0f, false,
        SDK::DamageType::Physical, SDK::SpellType::Targeted);
    p.Spells[0].Aim = AimPolicy::TargetPosition;
    p.Spells[0].Priority = 94;
    p.Spells[0].DashDistance = 600.0f;
    p.Spells[0].AllowOnMinions = true;
    p.Spells[0].MaximumEnemiesAtDestination = 2;
    p.Spells[0].ComboManaPercent = 8.0f;
    p.Spells[0].HarassManaPercent = 35.0f;
    p.Spells[0].ClearManaPercent = 30.0f;

    p.Spells[1] = Spell(
        SDK::SpellSlot::W, "Defiant Dance", CastKind::ChargedLine,
        Intent::Damage | Intent::Buff | Intent::Channel | Intent::Waveclear |
            Intent::Jungle | Intent::Peel,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::Flee | Mode::Automatic,
        825.0f, 0.25f, 120.0f, FLT_MAX, false,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotLine);
    p.Spells[1].Aim = AimPolicy::Prediction;
    p.Spells[1].Priority = 88;
    p.Spells[1].ChargeBuffName = "ireliawdefense";
    p.Spells[1].ChargeMinRange = 825;
    p.Spells[1].ChargeMaxRange = 825;
    p.Spells[1].ChargeDurationSeconds = 1.5f;
    p.Spells[1].PreserveAutoAttack = true;

    p.Spells[2] = Spell(
        SDK::SpellSlot::E, "Flawless Duet", CastKind::Position,
        Intent::Damage | Intent::CrowdControl | Intent::Setup |
            Intent::Interrupt | Intent::AntiGapcloser | Intent::Recast,
        Mode::Combo | Mode::Harass | Mode::Jungle | Mode::Flee |
            Mode::Automatic,
        850.0f, 0.25f, 70.0f, 2000.0f, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[2].Aim = AimPolicy::BehindTarget;
    p.Spells[2].Priority = 96;
    p.Spells[2].Hitchance = SDK::HitChance::High;
    p.Spells[2].RecastSpellName = "IreliaE2";

    p.Spells[3] = Spell(
        SDK::SpellSlot::R, "Vanguard's Edge", CastKind::Line,
        Intent::Damage | Intent::CrowdControl | Intent::Engage |
            Intent::Setup | Intent::Objective,
        Mode::Combo | Mode::Automatic,
        950.0f, 0.40f, 160.0f, 2000.0f, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[3].Aim = AimPolicy::BestAoe;
    p.Spells[3].Priority = 98;
    p.Spells[3].Hitchance = SDK::HitChance::VeryHigh;
    p.Spells[3].MinimumAoeTargets = 2;
    p.Spells[3].MaximumEnemiesAtDestination = 2;
    p.Spells[3].TargetHealthPercent = 62.0f;

    p.Variants[0] = { SDK::SpellSlot::E, "IreliaE2", p.Spells[2] };
    p.Variants[1] = { SDK::SpellSlot::W, "ireliawdefense", p.Spells[1] };
    p.VariantCount = 2;

    p.Trade = Plan("Duet reset trade",
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireFirstCast, 0, 700),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireRecast, 80, 1100),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequireMark | StepRule::RequireSafePosition, 150, 1250));
    p.AllIn = Plan("Vanguard reset chain",
        Step(SDK::SpellSlot::R, StepRule::RequireTarget | StepRule::RequireSafePosition, 0, 850),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequireMark | StepRule::RequireSafePosition, 120, 1000),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireFirstCast, 180, 1100),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireRecast, 250, 1350),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequireMark | StepRule::RequireSafePosition, 330, 1500));
    p.Flee = Plan("Reset dash escape",
        Step(SDK::SpellSlot::Q, StepRule::RequireSafePosition | StepRule::RequireTargetLow, 0, 800),
        Step(SDK::SpellSlot::E, StepRule::RequireNoCrowdControl, 100, 1050),
        Step(SDK::SpellSlot::W, StepRule::AllowDuringWindup | StepRule::RequirePlayerLow, 160, 1150));
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
