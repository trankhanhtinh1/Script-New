#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

// Blitzcrank is a threat-management catcher, not a max-range hook bot. The
// controller owns moving first-body collision, no-grief pull valuation,
// W-E walk-up pressure, exact E attack timing and R passive/active economy.
inline constexpr ChampionProfile Blitzcrank = [] {
    ChampionProfile p{};
    p.ChampionId = SDK::ChampionId::Blitzcrank;
    p.DisplayName = "Blitzcrank";
    p.InternalId = "champion.kuroaio.ai.blitzcrank";
    p.PrimaryArchetype = Archetype::Catcher;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::ObjectTracking | Mechanic::Mark |
                  Mechanic::Stack | Mechanic::AutoWeave |
                  Mechanic::AutoReset;
    p.Ultimate = UltimatePolicy::MultiTarget;

    p.Spells[0] = Spell(
        SDK::SpellSlot::Q, "Rocket Grab", CastKind::Line,
        Intent::Damage | Intent::CrowdControl | Intent::Engage |
            Intent::Disengage | Intent::Execute | Intent::Setup |
            Intent::Peel | Intent::Interrupt | Intent::Objective,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::Automatic,
        1115.0f, 0.25f, 70.0f, 1800.0f, true,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[0].Priority = 99;
    p.Spells[0].TriggerRange = 1115.0f;
    p.Spells[0].Aim = AimPolicy::Prediction;
    p.Spells[0].Hitchance = SDK::HitChance::High;
    p.Spells[0].PreserveAutoAttack = true;
    p.Spells[0].ComboManaPercent = 0.0f;
    p.Spells[0].HarassManaPercent = 42.0f;

    p.Spells[1] = Spell(
        SDK::SpellSlot::W, "Overdrive", CastKind::Self,
        Intent::Mobility | Intent::Buff | Intent::Engage |
            Intent::Disengage | Intent::Peel | Intent::Setup,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::Automatic,
        0.0f, 0.0f, 0.0f, FLT_MAX, false,
        SDK::DamageType::True, SDK::SpellType::SkillshotCircle);
    p.Spells[1].Priority = 86;
    p.Spells[1].TriggerRange = 650.0f;
    p.Spells[1].Aim = AimPolicy::SelfPosition;
    p.Spells[1].PreserveAutoAttack = true;
    p.Spells[1].ComboManaPercent = 0.0f;
    p.Spells[1].HarassManaPercent = 48.0f;

    p.Spells[2] = Spell(
        SDK::SpellSlot::E, "Power Fist", CastKind::Self,
        Intent::Damage | Intent::CrowdControl | Intent::AutoReset |
            Intent::Setup | Intent::Peel | Intent::Interrupt |
            Intent::Finisher,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::Automatic,
        0.0f, 0.0f, 0.0f, FLT_MAX, false,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotCircle);
    p.Spells[2].Priority = 98;
    p.Spells[2].TriggerRange = 275.0f;
    p.Spells[2].Aim = AimPolicy::SelfPosition;
    p.Spells[2].PreserveAutoAttack = true;
    p.Spells[2].WeaveAfterAttack = true;

    p.Spells[3] = Spell(
        SDK::SpellSlot::R, "Static Field", CastKind::Self,
        Intent::Damage | Intent::CrowdControl | Intent::Execute |
            Intent::Interrupt | Intent::Peel | Intent::Shield |
            Intent::Setup,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::Automatic,
        0.0f, 0.25f, 600.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[3].Priority = 97;
    p.Spells[3].TriggerRange = 600.0f;
    p.Spells[3].Aim = AimPolicy::SelfPosition;
    p.Spells[3].PreserveAutoAttack = true;
    p.Spells[3].MinimumAoeTargets = 2;

    p.Trade = Plan(
        "hold Q pressure; W-E walk-up when safe; Q after knock-up",
        Step(SDK::SpellSlot::W,
             StepRule::RequireTarget | StepRule::RequireSafePosition |
                 StepRule::RequireOutsideAaRange),
        Step(SDK::SpellSlot::E,
             StepRule::RequireTarget | StepRule::RequireInsideAaRange),
        Step(SDK::SpellSlot::Q,
             StepRule::RequireTarget | StepRule::RequireCrowdControl));

    p.AllIn = Plan(
        "clean Q, pre-arm E for escape-ready victim, R only for silence/shield/payoff",
        Step(SDK::SpellSlot::Q,
             StepRule::RequireTarget),
        Step(SDK::SpellSlot::E,
             StepRule::RequireTarget | StepRule::RequireInsideAaRange |
                 StepRule::AllowDuringWindup),
        Step(SDK::SpellSlot::R,
             StepRule::RequireTarget | StepRule::SkipIfKillableWithout),
        Step(SDK::SpellSlot::W,
             StepRule::RequireTarget | StepRule::RequireSafePosition));

    p.Flee = Plan(
        "E nearest pursuer, Q displacement after body check, W before route closes",
        Step(SDK::SpellSlot::E,
             StepRule::RequireTarget | StepRule::RequireInsideAaRange |
                 StepRule::AllowDuringWindup),
        Step(SDK::SpellSlot::Q,
             StepRule::RequireTarget | StepRule::AllowDuringWindup),
        Step(SDK::SpellSlot::W,
             StepRule::RequireSafePosition | StepRule::AllowDuringWindup));

    p.PreferredCombatDistance = 240.0f;
    p.EngageHealthPercent = 52.0f;
    p.DefensiveHealthPercent = 34.0f;
    p.UltimateTargetHealthPercent = 32.0f;
    p.UltimateMinimumTargets = 2;
    p.MaximumCommitEnemies = 3;
    p.BaseHumanizerMs = 42;
    p.PreferSelectedTarget = true;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "ManaBarrier";
    p.MarkBuff = "StaticFieldPassive";
    p.UltimateBuff = "StaticField";
    p.TrackedObjectToken = "RocketGrabMissile";
    p.ThemeFrom = 0xFFFFD15Cu;
    p.ThemeTo = 0xFF53B9FFu;
    p.ThemeSpeed = 0.88f;
    p.TacticalSummary =
        "Use hook pressure before hook casts: walk up with W for guaranteed E, "
        "model every moving first body and the 1115 lollipop endpoint, reject "
        "pulls that deliver engage threats onto a protected carry, choose "
        "immediate/pre-armed E versus AA-E-AA by escape timing, and spend R "
        "only when shield destruction, silence, lethal or AoE beats its passive.";
    p.ResearchSummary =
        "Pinned to Riot/CommunityDragon 16.14 with Riot 25.08 Q/passive, "
        "25.22 E and 13.17 rollback reconciliation; informed by current OP.GG "
        "Q-E-W order, Skill-Capped support guide, Best Blitzcrank NA timing, "
        "Mobalytics combos, Blitzcrank one-trick discussions and 2025-2026 "
        "Keria solo-queue/LCK POV and map-control reviews.";
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
