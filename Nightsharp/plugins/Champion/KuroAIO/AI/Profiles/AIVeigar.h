#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Veigar = [] {
    ChampionProfile p{};
    p.ChampionName = "Veigar";
    p.DisplayName = "Veigar";
    p.InternalId = "champion.kuroaio.ai.veigar";
    p.PrimaryArchetype = Archetype::BurstMage;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::Stack | Mechanic::Charge | Mechanic::Execute |
                  Mechanic::MissingHealth | Mechanic::ObjectTracking |
                  Mechanic::Terrain;
    p.Ultimate = UltimatePolicy::Execute;
    p.PreferredCombatDistance = 700.0f;
    p.EngageHealthPercent = 50.0f;
    p.DefensiveHealthPercent = 32.0f;
    p.UltimateTargetHealthPercent = 55.0f;
    p.MaximumCommitEnemies = 2;
    p.BaseHumanizerMs = 68;
    p.PreferSelectedTarget = true;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "VeigarPassive";
    p.TrackedObjectToken = "VeigarEventHorizon";
    p.ThemeFrom = 0xFF5D35B5u;
    p.ThemeTo = 0xFFBA70FFu;
    p.TacticalSummary =
        "Phenomenal Evil burst mage: harvest Q last hits and ability hits for AP, use a delayed W meteor after cage control, and hold Primordial Burst for a verified missing-health execute.";
    p.ResearchSummary =
        "Riot 26.15 / CommunityDragon 16.15 Summoner's Rift values; passive stack telemetry, Event Horizon edge stun, Dark Matter impact timing and execute scaling are reconciled from events and polling.";

    p.Spells[0] = Spell(SDK::SpellSlot::Q, "Baleful Strike", CastKind::Line,
        Intent::Damage | Intent::LastHit | Intent::Setup | Intent::Waveclear |
            Intent::Jungle,
        AllModes, 1000.0f, 0.25f, 70.0f, 2200.0f, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[0].Aim = AimPolicy::Prediction;
    p.Spells[0].Priority = 88;
    p.Spells[0].Hitchance = SDK::HitChance::High;
    p.Spells[0].HarassManaPercent = 38.0f;
    p.Spells[0].ClearManaPercent = 48.0f;

    p.Spells[1] = Spell(SDK::SpellSlot::W, "Dark Matter", CastKind::Circle,
        Intent::Damage | Intent::Setup | Intent::Waveclear | Intent::Jungle,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::Automatic,
        950.0f, 1.20f, 450.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[1].Aim = AimPolicy::Prediction;
    p.Spells[1].Priority = 82;
    p.Spells[1].ChargeBuffName = "VeigarDarkMatter";
    p.Spells[1].ChargeDurationSeconds = 1.20f;
    p.Spells[1].ComboManaPercent = 35.0f;
    p.Spells[1].HarassManaPercent = 52.0f;

    p.Spells[2] = Spell(SDK::SpellSlot::E, "Event Horizon", CastKind::Position,
        Intent::CrowdControl | Intent::Setup | Intent::Disengage | Intent::AntiGapcloser,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::Automatic,
        700.0f, 0.50f, 780.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[2].Aim = AimPolicy::BetweenPlayerAndTarget;
    p.Spells[2].Priority = 96;
    p.Spells[2].MinimumAoeTargets = 1;
    p.Spells[2].MaximumEnemiesAtDestination = 2;

    p.Spells[3] = Spell(SDK::SpellSlot::R, "Primordial Burst", CastKind::EnemyTarget,
        Intent::Damage | Intent::Execute | Intent::Finisher,
        Mode::Combo | Mode::Automatic | Mode::Flee,
        650.0f, 0.25f, 0.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::Targeted);
    p.Spells[3].Aim = AimPolicy::TargetPosition;
    p.Spells[3].Priority = 100;
    p.Spells[3].TargetHealthPercent = 58.0f;
    p.Spells[3].PreserveAutoAttack = false;

    p.Trade = Plan("Cage edge and Q poke",
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireSafePosition, 0, 900),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::AllowDuringWindup, 100, 1100),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget | StepRule::RequireCrowdControl, 180, 1400));
    p.AllIn = Plan("Cage meteor execute",
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireSafePosition, 0, 950),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget | StepRule::RequireCrowdControl, 120, 1250),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget, 210, 1400),
        Step(SDK::SpellSlot::R, StepRule::RequireTarget | StepRule::RequireTargetLow | StepRule::HoldForExecute, 280, 1600));
    p.Flee = Plan("Event Horizon peel",
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireSafePosition, 0, 900),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::AllowDuringWindup, 120, 1000));
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
