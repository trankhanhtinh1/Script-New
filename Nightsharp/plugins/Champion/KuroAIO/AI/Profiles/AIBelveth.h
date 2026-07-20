#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

// Bel'Veth is a four-sector attack-reset skirmisher, not a generic dash
// champion. The descriptive profile configures runtime spell wrappers; the
// controller owns sector cooldowns, W refunds, E's forced victim and global
// coral/form economy.
inline constexpr ChampionProfile Belveth = [] {
    ChampionProfile p{};
    p.ChampionName = "Belveth";
    p.DisplayName = "Bel'Veth";
    p.InternalId = "champion.kuroaio.ai.belveth";
    p.PrimaryArchetype = Archetype::Skirmisher;
    p.Resource = ResourceModel::None;
    p.Mechanics = Mechanic::Dash | Mechanic::Channel |
                  Mechanic::Execute | Mechanic::ObjectTracking |
                  Mechanic::Stack | Mechanic::Transform |
                  Mechanic::MultiForm | Mechanic::WallInteraction |
                  Mechanic::Terrain | Mechanic::AutoWeave |
                  Mechanic::AutoReset | Mechanic::DirectionalSweet |
                  Mechanic::MissingHealth;
    p.Ultimate = UltimatePolicy::ManualAssist;

    p.Spells[0] = Spell(
        SDK::SpellSlot::Q, "Void Surge", CastKind::Direction,
        Intent::Damage | Intent::Mobility | Intent::Engage |
            Intent::Disengage | Intent::Execute | Intent::AutoReset |
            Intent::Waveclear | Intent::Jungle | Intent::LastHit,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::LastHit | Mode::Flee | Mode::Automatic,
        400.0f, 0.0f, 100.0f, FLT_MAX, false,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotLine);
    p.Spells[0].Priority = 98;
    p.Spells[0].TriggerRange = 625.0f;
    p.Spells[0].DashDistance = 625.0f;
    p.Spells[0].Aim = AimPolicy::SafeCursor;
    p.Spells[0].PreserveAutoAttack = true;
    p.Spells[0].WeaveAfterAttack = true;
    p.Spells[0].AllowOnMinions = true;
    p.Spells[0].MaximumEnemiesAtDestination = 2;

    p.Spells[1] = Spell(
        SDK::SpellSlot::W, "Above and Below", CastKind::Line,
        Intent::Damage | Intent::CrowdControl | Intent::Setup |
            Intent::Interrupt | Intent::AntiGapcloser | Intent::Peel |
            Intent::Waveclear | Intent::Jungle,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::Flee | Mode::Automatic,
        660.0f, 0.50f, 200.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[1].Priority = 96;
    p.Spells[1].TriggerRange = 660.0f;
    p.Spells[1].Aim = AimPolicy::Prediction;
    p.Spells[1].Hitchance = SDK::HitChance::High;
    p.Spells[1].PreserveAutoAttack = true;
    p.Spells[1].AllowOnMinions = true;

    p.Spells[2] = Spell(
        SDK::SpellSlot::E, "Royal Maelstrom", CastKind::Self,
        Intent::Damage | Intent::Execute | Intent::Heal |
            Intent::Channel | Intent::Finisher | Intent::Disengage |
            Intent::Jungle | Intent::Objective | Intent::Recast,
        Mode::Combo | Mode::Jungle | Mode::Flee | Mode::Automatic,
        500.0f, 0.0f, 500.0f, FLT_MAX, false,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotCircle);
    p.Spells[2].Priority = 99;
    p.Spells[2].TriggerRange = 500.0f;
    p.Spells[2].Aim = AimPolicy::SelfPosition;
    p.Spells[2].PreserveAutoAttack = true;
    p.Spells[2].PlayerHealthPercent = 58.0f;
    p.Spells[2].TargetHealthPercent = 48.0f;

    p.Spells[3] = Spell(
        SDK::SpellSlot::R, "Endless Banquet", CastKind::AnyTarget,
        Intent::Damage | Intent::Execute | Intent::Heal |
            Intent::Buff | Intent::Mobility | Intent::Objective |
            Intent::Setup,
        Mode::Combo | Mode::Jungle | Mode::Flee | Mode::Automatic,
        450.0f, 1.0f, 500.0f, FLT_MAX, false,
        SDK::DamageType::True, SDK::SpellType::SkillshotCircle);
    p.Spells[3].Priority = 100;
    p.Spells[3].TriggerRange = 500.0f;
    p.Spells[3].Aim = AimPolicy::BestAoe;
    p.Spells[3].PreserveAutoAttack = true;
    p.Spells[3].MaximumEnemiesAtDestination = 2;

    p.Trade = Plan(
        "AA-Q-AA, W only after mobility or to refund the spent sector",
        Step(SDK::SpellSlot::Q,
             StepRule::RequireTarget | StepRule::RequireAfterAttack),
        Step(SDK::SpellSlot::W,
             StepRule::RequireTarget | StepRule::RequireSafePosition));

    p.AllIn = Plan(
        "enter second, AA-Q-AA, reliable W refund, repeat Q weave, E late",
        Step(SDK::SpellSlot::Q,
             StepRule::RequireTarget | StepRule::RequireAfterAttack),
        Step(SDK::SpellSlot::W,
             StepRule::RequireTarget | StepRule::RequireSafePosition),
        Step(SDK::SpellSlot::Q,
             StepRule::RequireTarget),
        Step(SDK::SpellSlot::E,
             StepRule::RequireTarget | StepRule::RequireTargetLow |
                 StepRule::HoldForExecute));

    p.Flee = Plan(
        "peel with reliable W, use cursor-side Q, hold E for real burst",
        Step(SDK::SpellSlot::W,
             StepRule::RequireTarget | StepRule::AllowDuringWindup),
        Step(SDK::SpellSlot::Q,
             StepRule::RequireSafePosition | StepRule::AllowDuringWindup),
        Step(SDK::SpellSlot::E,
             StepRule::RequirePlayerLow | StepRule::AllowDuringWindup));

    p.PreferredCombatDistance = 225.0f;
    p.EngageHealthPercent = 58.0f;
    p.DefensiveHealthPercent = 36.0f;
    p.UltimateTargetHealthPercent = 100.0f;
    p.UltimateMinimumTargets = 1;
    p.MaximumCommitEnemies = 2;
    p.BaseHumanizerMs = 34;
    p.PreferSelectedTarget = true;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "BelvethPassiveStacks";
    p.MarkBuff = "BelvethRPassive";
    p.FormBuff = "BelvethRSteroid";
    p.UltimateBuff = "BelvethRInside";
    p.TrackedObjectToken = "BelvethSpore";
    p.ThemeFrom = 0xFFB67CFFu;
    p.ThemeTo = 0xFF5A2B88u;
    p.ThemeSpeed = 0.82f;
    p.TacticalSummary =
        "Enter teamfights second; preserve AA-Q-AA and the second forward "
        "sector; cast W after mobility or for a real multi-sector refund; "
        "authorize E only against its actual lowest-health-percent victim and "
        "cancel it only after 0.75 seconds for a superior defensive branch; "
        "consume corals by valuing all globally lost corals, AoE, heal, form "
        "duration, enhanced macro and endpoint safety together.";
    p.ResearchSummary =
        "Pinned to live Riot 26.14 and CommunityDragon PC 16.14; reconciled "
        "Riot 25.12, 25.15 and 26.3 notes, current League mechanics, OP.GG "
        "Q-E-W order, Sinerias' Season 16 guide, Sawyer's 2026 guide, "
        "KingKong combo demonstrations and community one-trick edge cases; "
        "the July 2026 PBE midscope is explicitly excluded until live.";
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
