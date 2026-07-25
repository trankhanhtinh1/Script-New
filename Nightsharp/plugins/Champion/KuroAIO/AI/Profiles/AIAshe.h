#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

// Ashe is not represented as a generic line-skillshot marksman. Volley owns
// seven through eleven independently colliding five-degree rays; Ranger's
// Focus is an after-attack reset gated by real Focus state; Hawkshot is a
// two-charge information planner; and Crystal Arrow stops on the first enemy
// champion while its travel distance changes both speed and stun duration.
inline constexpr ChampionProfile Ashe = [] {
    ChampionProfile p{};
    p.ChampionName = "Ashe";
    p.DisplayName = "Ashe";
    p.InternalId = "champion.kuroaio.ai.ashe";
    p.PrimaryArchetype = Archetype::Marksman;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::Stack | Mechanic::Ammo | Mechanic::Global |
                  Mechanic::Mark | Mechanic::ObjectTracking |
                  Mechanic::AutoWeave | Mechanic::AutoReset;
    p.Ultimate = UltimatePolicy::MultiTarget;

    p.Spells[0] = Spell(
        SDK::SpellSlot::Q, "Ranger's Focus", CastKind::Self,
        Intent::Damage | Intent::Buff | Intent::AutoReset |
            Intent::Waveclear | Intent::Jungle | Intent::Objective,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::Automatic,
        0.0f, 0.0f, 0.0f, FLT_MAX, false,
        SDK::DamageType::Physical, SDK::SpellType::Targeted);
    p.Spells[0].Priority = 93;
    p.Spells[0].TriggerRange = 650.0f;
    p.Spells[0].RequiredPlayerBuff = "asheqcastready";
    p.Spells[0].ForbiddenPlayerBuff = "AsheQBuff";
    p.Spells[0].WeaveAfterAttack = true;
    p.Spells[0].PreserveAutoAttack = true;
    p.Spells[0].HarassManaPercent = 24.0f;
    p.Spells[0].ClearManaPercent = 42.0f;

    p.Spells[1] = Spell(
        SDK::SpellSlot::W, "Volley", CastKind::Cone,
        Intent::Damage | Intent::CrowdControl | Intent::Setup |
            Intent::Peel | Intent::Disengage | Intent::Finisher |
            Intent::Waveclear | Intent::Jungle,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::Flee | Mode::Automatic,
        1200.0f, 0.25f, 20.0f, 1500.0f, true,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotCone);
    p.Spells[1].Priority = 91;
    p.Spells[1].Hitchance = SDK::HitChance::High;
    p.Spells[1].MinimumAoeTargets = 2;
    p.Spells[1].WeaveAfterAttack = true;
    p.Spells[1].PreserveAutoAttack = true;
    p.Spells[1].HarassManaPercent = 48.0f;
    p.Spells[1].ClearManaPercent = 63.0f;

    p.Spells[2] = Spell(
        SDK::SpellSlot::E, "Hawkshot", CastKind::Position,
        Intent::Vision | Intent::Objective | Intent::Setup |
            Intent::AllyUtility,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::Flee | Mode::Automatic,
        25000.0f, 0.0f, 1000.0f, 1400.0f, false,
        SDK::DamageType::True, SDK::SpellType::SkillshotLine);
    p.Spells[2].Priority = 84;
    p.Spells[2].MinimumAmmo = 1;
    p.Spells[2].PreserveAutoAttack = true;

    p.Spells[3] = Spell(
        SDK::SpellSlot::R, "Enchanted Crystal Arrow", CastKind::Line,
        Intent::Damage | Intent::CrowdControl | Intent::Engage |
            Intent::Disengage | Intent::Interrupt |
            Intent::AntiGapcloser | Intent::Peel | Intent::Setup |
            Intent::Finisher | Intent::AllyUtility,
        Mode::Combo | Mode::Flee | Mode::Automatic,
        25000.0f, 0.25f, 130.0f, 1500.0f, true,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[3].Priority = 98;
    p.Spells[3].Hitchance = SDK::HitChance::VeryHigh;
    p.Spells[3].TriggerRange = 400.0f;
    p.Spells[3].MinimumAoeTargets = 2;
    p.Spells[3].PreserveAutoAttack = true;

    p.Trade = Plan(
        "thread a side Volley ray, take the max-range auto, then reset with Q",
        Step(SDK::SpellSlot::W,
             StepRule::RequireTarget | StepRule::RequireSafePosition),
        Step(SDK::SpellSlot::Q,
             StepRule::RequireTarget | StepRule::RequireAfterAttack |
                 StepRule::RequireInsideAaRange));

    p.AllIn = Plan(
        "choose W-R or R-W by commitment, then let the player attack before Q reset",
        Step(SDK::SpellSlot::R,
             StepRule::RequireTarget | StepRule::RequireSafePosition),
        Step(SDK::SpellSlot::W,
             StepRule::RequireTarget | StepRule::RequireCrowdControl),
        Step(SDK::SpellSlot::Q,
             StepRule::RequireTarget | StepRule::RequireAfterAttack |
                 StepRule::RequireInsideAaRange));

    p.Flee = Plan(
        "slow the committed pursuer with a clear Volley ray and spend R only for hard peel",
        Step(SDK::SpellSlot::W,
             StepRule::RequireTarget | StepRule::AllowDuringWindup),
        Step(SDK::SpellSlot::R,
             StepRule::RequireTarget | StepRule::AllowDuringWindup));

    p.PreferredCombatDistance = 590.0f;
    p.EngageHealthPercent = 52.0f;
    p.DefensiveHealthPercent = 38.0f;
    p.UltimateTargetHealthPercent = 100.0f;
    p.UltimateMinimumTargets = 2;
    p.MaximumCommitEnemies = 3;
    p.BaseHumanizerMs = 34;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "ashepassiveslow";
    p.MarkBuff = "ashepassiveslow";
    p.FormBuff = "AsheQBuff";
    p.TrackedObjectToken = "EnchantedCrystalArrow";
    p.ThemeFrom = 0xFFB7F5FFu;
    p.ThemeTo = 0xFF4D8DFFu;
    p.ThemeSpeed = 0.76f;
    p.TacticalSummary =
        "Keep movement and target attacks player-owned; observe four Focus "
        "stacks and reset only after a valuable attack; simulate every live "
        "Volley ray and its first blocker; reserve Hawkshot charges for "
        "multi-camp jungle tracking, objectives and no-facecheck routes; and "
        "fire Crystal Arrow only through a verified first champion with "
        "distance stun, explosion value and allied follow-up.";
    p.ResearchSummary =
        "Pinned to Riot 26.14/26.10/26.1 and CommunityDragon PC 16.14. "
        "Cross-checked current pro builds/replays, Challenger Ashe guides, "
        "AsheMains OTP jungle-tracking and reset discussions, and all three "
        "local Ashe controllers; rejected their fixed-line W, 2500-range E/R "
        "and unconditional Q/R automation.";
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
