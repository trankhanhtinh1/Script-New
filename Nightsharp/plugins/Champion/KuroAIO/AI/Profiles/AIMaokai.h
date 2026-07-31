#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

// Maokai is a zone-controlling vanguard.  Q and W are not generic damage
// buttons: Q's displacement is a peel/interrupt or a deliberate wall-facing
// knockback, W is a root dash whose endpoint must be safe, E is a tracked
// sapling/brush state and R is an advancing wave planned for peel or engage.
inline constexpr ChampionProfile Maokai = [] {
    ChampionProfile p{};
    p.ChampionName = "Maokai";
    p.DisplayName = "Maokai";
    p.InternalId = "champion.kuroaio.ai.maokai";
    p.PrimaryArchetype = Archetype::Vanguard;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::ObjectTracking | Mechanic::Dash |
                  Mechanic::Terrain | Mechanic::Trap |
                  Mechanic::DirectionalSweet | Mechanic::AutoWeave;
    p.Ultimate = UltimatePolicy::MultiTarget;

    p.Spells[0] = Spell(
        SDK::SpellSlot::Q, "Bramble Smash", CastKind::Direction,
        Intent::Damage | Intent::CrowdControl | Intent::Disengage |
            Intent::AntiGapcloser | Intent::Peel | Intent::Interrupt | Intent::Waveclear,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::LastHit | Mode::Flee | Mode::Automatic,
        650.0f, 0.25f, 110.0f, 1600.0f, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[0].Priority = 91;
    p.Spells[0].TriggerRange = 650.0f;
    p.Spells[0].MaximumEnemiesAtDestination = 3;
    p.Spells[0].ComboManaPercent = 22.0f;
    p.Spells[0].HarassManaPercent = 48.0f;
    p.Spells[0].ClearManaPercent = 32.0f;
    p.Spells[0].PreserveAutoAttack = true;

    p.Spells[1] = Spell(
        SDK::SpellSlot::W, "Twisted Advance", CastKind::EnemyTarget,
        Intent::Damage | Intent::CrowdControl | Intent::Mobility |
            Intent::Engage | Intent::Disengage | Intent::Peel | Intent::Interrupt | Intent::Setup,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::Automatic,
        525.0f, 0.0f, 100.0f, 1300.0f, false,
        SDK::DamageType::Magical, SDK::SpellType::Targeted);
    p.Spells[1].Priority = 96;
    p.Spells[1].DashDistance = 525.0f;
    p.Spells[1].MaximumEnemiesAtDestination = 2;
    p.Spells[1].ComboManaPercent = 38.0f;
    p.Spells[1].HarassManaPercent = 58.0f;
    p.Spells[1].AllowOnMinions = false;
    p.Spells[1].PreserveAutoAttack = true;

    p.Spells[2] = Spell(
        SDK::SpellSlot::E, "Sapling Toss", CastKind::Position,
        Intent::Damage | Intent::CrowdControl | Intent::Setup |
            Intent::Waveclear | Intent::Jungle | Intent::Vision,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::LastHit | Mode::Flee | Mode::Automatic,
        1100.0f, 0.25f, 120.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[2].Priority = 74;
    p.Spells[2].TriggerRange = 1100.0f;
    p.Spells[2].ComboManaPercent = 34.0f;
    p.Spells[2].HarassManaPercent = 62.0f;
    p.Spells[2].ClearManaPercent = 55.0f;
    p.Spells[2].MaximumEnemiesAtDestination = 4;
    p.Spells[2].PreserveAutoAttack = true;

    p.Spells[3] = Spell(
        SDK::SpellSlot::R, "Nature's Grasp", CastKind::Direction,
        Intent::Damage | Intent::CrowdControl | Intent::Engage |
            Intent::Disengage | Intent::Peel | Intent::Interrupt | Intent::AntiGapcloser | Intent::Setup,
        Mode::Combo | Mode::Flee | Mode::Automatic,
        3000.0f, 0.25f, 150.0f, 750.0f, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[3].Priority = 100;
    p.Spells[3].TriggerRange = 3000.0f;
    p.Spells[3].MinimumAoeTargets = 2;
    p.Spells[3].ComboManaPercent = 66.0f;
    p.Spells[3].PlayerHealthPercent = 36.0f;
    p.Spells[3].PreserveAutoAttack = true;

    p.Trade = Plan(
        "E brush zone, Q slow/knockback, W only for a safe root trade",
        Step(SDK::SpellSlot::E, StepRule::RequireTarget),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::AllowDuringWindup),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget | StepRule::RequireSafePosition));

    p.AllIn = Plan(
        "R wave or W root, Q displacement and empowered E zone",
        Step(SDK::SpellSlot::R, StepRule::RequireTarget | StepRule::RequireMultiTarget),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget | StepRule::RequireSafePosition),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::AllowDuringWindup));

    p.Flee = Plan(
        "Q pursuer away, R choke peel, never spend W into unsafe enemy density",
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget),
        Step(SDK::SpellSlot::R, StepRule::RequireTarget | StepRule::RequireMultiTarget),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget | StepRule::RequireSafePosition));

    p.PreferredCombatDistance = 325.0f;
    p.EngageHealthPercent = 48.0f;
    p.DefensiveHealthPercent = 30.0f;
    p.UltimateTargetHealthPercent = 100.0f;
    p.UltimateMinimumTargets = 2;
    p.MaximumCommitEnemies = 3;
    p.BaseHumanizerMs = 48;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "MaokaiPassiveReady";
    p.FormBuff = "MaokaiSapling";
    p.UltimateBuff = "MaokaiRRoot";
    p.TrackedObjectToken = "MaokaiSapling";
    p.ThemeFrom = 0xFF8DC26Fu;
    p.ThemeTo = 0xFF315C39u;
    p.ThemeSpeed = 0.86f;
    p.TacticalSummary =
        "Track sapling lifetime and empowered brush zones, use W as a safe root dash, "
        "Q to peel or displace toward a chosen endpoint, and send R along an "
        "advancing path for multi-target engage or urgent carry peel.";
    p.ResearchSummary =
        "Riot patch 26.15 and CommunityDragon PC 16.15 data, including the jungle "
        "Q monster/AP tuning, sapling zone behavior, W root dash and R wave timing.";
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
