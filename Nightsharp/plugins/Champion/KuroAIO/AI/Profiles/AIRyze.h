#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

// Ryze is a short-range reset battlemage. The full controller owns first-body
// Q collision, four-second Rune/Flux state, direct versus wave-bridge spell
// branches, mana reserves, auto weaving and player-authorized Realm Warp.
inline constexpr ChampionProfile Ryze = [] {
    ChampionProfile p{};
    p.ChampionId = SDK::ChampionId::Ryze;
    p.DisplayName = "Ryze";
    p.InternalId = "champion.kuroaio.ai.ryze";
    p.PrimaryArchetype = Archetype::Battlemage;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::Mark | Mechanic::Stack |
                  Mechanic::AutoWeave | Mechanic::Channel |
                  Mechanic::Blink | Mechanic::ObjectTracking;
    p.Ultimate = UltimatePolicy::ManualAssist;

    p.Spells[0] = Spell(
        SDK::SpellSlot::Q, "Overload", CastKind::Line,
        Intent::Damage | Intent::Execute | Intent::Waveclear |
            Intent::LastHit | Intent::Jungle | Intent::Objective |
            Intent::Setup | Intent::Finisher | Intent::Mobility,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::LastHit | Mode::Flee | Mode::Automatic,
        1000.0f, 0.25f, 55.0f, 1700.0f, true,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[0].Priority = 99;
    p.Spells[0].TriggerRange = 1000.0f;
    p.Spells[0].Aim = AimPolicy::Prediction;
    p.Spells[0].Hitchance = SDK::HitChance::High;
    p.Spells[0].PreserveAutoAttack = true;
    p.Spells[0].ComboManaPercent = 0.0f;
    p.Spells[0].HarassManaPercent = 40.0f;
    p.Spells[0].ClearManaPercent = 36.0f;
    p.Spells[0].AllowOnMinions = true;

    p.Spells[1] = Spell(
        SDK::SpellSlot::W, "Rune Prison", CastKind::EnemyTarget,
        Intent::Damage | Intent::CrowdControl | Intent::Setup |
            Intent::Peel | Intent::Interrupt | Intent::AntiGapcloser |
            Intent::Disengage | Intent::Jungle,
        Mode::Combo | Mode::Harass | Mode::Jungle | Mode::Flee |
            Mode::Automatic,
        550.0f, 0.25f, 20.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[1].Priority = 98;
    p.Spells[1].TriggerRange = 550.0f;
    p.Spells[1].Aim = AimPolicy::TargetPosition;
    p.Spells[1].PreserveAutoAttack = true;
    p.Spells[1].ComboManaPercent = 0.0f;
    p.Spells[1].HarassManaPercent = 58.0f;

    p.Spells[2] = Spell(
        SDK::SpellSlot::E, "Spell Flux", CastKind::EnemyTarget,
        Intent::Damage | Intent::Setup | Intent::CrowdControl |
            Intent::Waveclear | Intent::LastHit | Intent::Jungle |
            Intent::Objective | Intent::Peel | Intent::Interrupt,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::LastHit | Mode::Flee | Mode::Automatic,
        550.0f, 0.25f, 350.0f, 3500.0f, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[2].Priority = 97;
    p.Spells[2].TriggerRange = 550.0f;
    p.Spells[2].Aim = AimPolicy::TargetPosition;
    p.Spells[2].PreserveAutoAttack = true;
    p.Spells[2].ComboManaPercent = 0.0f;
    p.Spells[2].HarassManaPercent = 42.0f;
    p.Spells[2].ClearManaPercent = 36.0f;
    p.Spells[2].AllowOnMinions = true;

    p.Spells[3] = Spell(
        SDK::SpellSlot::R, "Realm Warp", CastKind::Position,
        Intent::Mobility | Intent::Engage | Intent::Disengage |
            Intent::Objective | Intent::Waveclear | Intent::AllyUtility |
            Intent::Channel,
        Mode::Automatic | Mode::Flee,
        3000.0f, 2.10f, 365.0f, FLT_MAX, false,
        SDK::DamageType::True, SDK::SpellType::SkillshotCircle);
    p.Spells[3].Priority = 100;
    p.Spells[3].TriggerRange = 3000.0f;
    p.Spells[3].Aim = AimPolicy::Cursor;
    p.Spells[3].PreserveAutoAttack = true;
    p.Spells[3].ComboManaPercent = 0.0f;
    p.Spells[3].MaximumEnemiesAtDestination = 2;

    p.Trade = Plan(
        "Q-E-Q short trade; preserve W and two-rune escape when threatened",
        Step(SDK::SpellSlot::Q,
             StepRule::RequireTarget),
        Step(SDK::SpellSlot::E,
             StepRule::RequireTarget | StepRule::AllowDuringWindup),
        Step(SDK::SpellSlot::Q,
             StepRule::RequireTarget | StepRule::AllowDuringWindup));

    p.AllIn = Plan(
        "Q-E-Q-W-Q-E-Q only into a committed safe target; E-W-Q for root and speed",
        Step(SDK::SpellSlot::Q,
             StepRule::RequireTarget),
        Step(SDK::SpellSlot::E,
             StepRule::RequireTarget | StepRule::AllowDuringWindup),
        Step(SDK::SpellSlot::Q,
             StepRule::RequireTarget | StepRule::AllowDuringWindup),
        Step(SDK::SpellSlot::W,
             StepRule::RequireTarget),
        Step(SDK::SpellSlot::Q,
             StepRule::RequireTarget | StepRule::AllowDuringWindup),
        Step(SDK::SpellSlot::E,
             StepRule::RequireTarget | StepRule::AllowDuringWindup),
        Step(SDK::SpellSlot::Q,
             StepRule::RequireTarget | StepRule::AllowDuringWindup));

    p.Flee = Plan(
        "E-W root pursuer then consume two runes with Q for movement speed",
        Step(SDK::SpellSlot::E,
             StepRule::RequireTarget | StepRule::AllowDuringWindup),
        Step(SDK::SpellSlot::W,
             StepRule::RequireTarget | StepRule::RequireMark),
        Step(SDK::SpellSlot::Q,
             StepRule::RequireTarget | StepRule::AllowDuringWindup));

    p.PreferredCombatDistance = 500.0f;
    p.EngageHealthPercent = 64.0f;
    p.DefensiveHealthPercent = 31.0f;
    p.UltimateTargetHealthPercent = 0.0f;
    p.UltimateMinimumTargets = 1;
    p.MaximumCommitEnemies = 2;
    p.BaseHumanizerMs = 30;
    p.PreferSelectedTarget = true;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "RyzePassive";
    p.MarkBuff = "RyzeE";
    p.ChannelBuff = "RyzeRChannel";
    p.UltimateBuff = "RyzeR";
    p.TrackedObjectToken = "Ryze_Q_Mis";
    p.ThemeFrom = 0xFF4D8DFFu;
    p.ThemeTo = 0xFFB8F0FFu;
    p.ThemeSpeed = 0.92f;
    p.TacticalSummary =
        "Resolve every moving Q first body; track each four-second Flux and "
        "Rune; choose QEQWQEQ, QEWQ, EWQ, WEQ or QEQ by commitment, mobility, "
        "mana and escape need; bridge E-Q through the wave; weave only in real "
        "downtime; keep Realm Warp under an explicit player key and safe-arrival gate.";
    p.ResearchSummary =
        "Pinned to Riot/CommunityDragon 16.14 and Riot 25.11/25.13/26.3/26.12; "
        "cross-checked with current OP.GG order, Mobalytics combo catalog, "
        "RyzeMains 2026 combo/spacing discussions, Strompest season-16 guides, "
        "Mysterias matchup curriculum and Faker patch-26.11/26.12 replays.";
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
