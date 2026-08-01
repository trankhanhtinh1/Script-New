#pragma once

#include "../AIChampionProfile.h"

#include <cfloat>

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Shen = [] {
    ChampionProfile p{};
    p.ChampionId = SDK::ChampionId::Shen;
    p.DisplayName = "Shen";
    p.InternalId = "champion.kuroaio.ai.shen";
    p.PrimaryArchetype = Archetype::Tank;
    p.Resource = ResourceModel::Energy;
    p.Mechanics = Mechanic::AllyTarget | Mechanic::Dash | Mechanic::Channel |
                  Mechanic::ObjectTracking | Mechanic::Mark | Mechanic::AutoWeave |
                  Mechanic::WallInteraction;
    p.Ultimate = UltimatePolicy::SaveAlly;
    p.PreferredCombatDistance = 275.0f;
    p.EngageHealthPercent = 45.0f;
    p.DefensiveHealthPercent = 38.0f;
    p.UltimateTargetHealthPercent = 58.0f;
    p.UltimateMinimumTargets = 1;
    p.MaximumCommitEnemies = 3;
    p.BaseHumanizerMs = 50;
    p.PreferSelectedTarget = true;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "ShenPassiveShield";
    p.MarkBuff = "ShenQ";
    p.UltimateBuff = "ShenR";
    p.TrackedObjectToken = "ShenSpiritBlade";
    p.TacticalSummary =
        "Energy tank with an observed passive shield, target-tracked Spirit Blade, "
        "ally-safe Spirit's Refuge, endpoint-validated Shadow Dash and channel-aware Stand United.";
    p.ResearchSummary =
        "Riot 26.15 and CommunityDragon 16.15 pin Shen's passive shield, Q blade pull, "
        "W dodge zone, E taunt dash and global ally channel policy.";

    p.Spells[0] = Spell(
        SDK::SpellSlot::Q, "Twilight Assault", CastKind::EnemyTarget,
        Intent::Damage | Intent::Buff | Intent::AutoReset | Intent::Jungle |
            Intent::LastHit | Intent::Setup,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::LastHit | Mode::Flee | Mode::Automatic,
        475.0f, 0.25f, 0.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::Targeted);
    p.Spells[0].Priority = 92;
    p.Spells[0].WeaveAfterAttack = true;
    p.Spells[0].PreserveAutoAttack = true;
    p.Spells[0].HarassManaPercent = 30.0f;
    p.Spells[0].ClearManaPercent = 25.0f;
    p.Spells[0].AllowOnMinions = true;

    p.Spells[1] = Spell(
        SDK::SpellSlot::W, "Spirit's Refuge", CastKind::Position,
        Intent::Shield | Intent::Buff | Intent::Peel | Intent::Disengage |
            Intent::AllyUtility | Intent::Jungle | Intent::Objective,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::Flee | Mode::Automatic,
        400.0f, 0.25f, 120.0f, FLT_MAX, false,
        SDK::DamageType::True, SDK::SpellType::SkillshotCircle);
    p.Spells[1].Aim = AimPolicy::TargetPosition;
    p.Spells[1].Priority = 98;
    p.Spells[1].MinimumAoeTargets = 1;
    p.Spells[1].HarassManaPercent = 38.0f;
    p.Spells[1].ClearManaPercent = 35.0f;
    p.Spells[1].PlayerHealthPercent = 72.0f;

    p.Spells[2] = Spell(
        SDK::SpellSlot::E, "Shadow Dash", CastKind::Position,
        Intent::Mobility | Intent::Engage | Intent::CrowdControl |
            Intent::Disengage | Intent::AntiGapcloser | Intent::Interrupt |
            Intent::Peel | Intent::Setup,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::Automatic,
        600.0f, 0.2f, 110.0f, FLT_MAX, false,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotLine);
    p.Spells[2].Aim = AimPolicy::Prediction;
    p.Spells[2].Priority = 95;
    p.Spells[2].MaximumEnemiesAtDestination = 2;
    p.Spells[2].HarassManaPercent = 45.0f;
    p.Spells[2].ClearManaPercent = 35.0f;
    p.Spells[2].PlayerHealthPercent = 75.0f;

    p.Spells[3] = Spell(
        SDK::SpellSlot::R, "Stand United", CastKind::AllyTarget,
        Intent::Shield | Intent::AllyUtility | Intent::Disengage |
            Intent::Peel | Intent::Objective | Intent::Channel,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::Automatic,
        0.0f, 3.0f, 0.0f, FLT_MAX, false,
        SDK::DamageType::True, SDK::SpellType::Targeted);
    p.Spells[3].Priority = 100;
    p.Spells[3].PlayerHealthPercent = 100.0f;
    p.Spells[3].PreserveAutoAttack = true;

    p.Trade = Plan(
        "Q blade pull and empowered attacks, W only for committed damage, taunt on reach",
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequireAfterAttack),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget | StepRule::RequirePlayerLow),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireSafePosition));
    p.AllIn = Plan(
        "safe taunt entry into Q weave and ally-safe refuge, reserve R for a threatened ally",
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireSafePosition),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::AllowDuringWindup),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget | StepRule::RequirePlayerLow));
    p.Flee = Plan(
        "Spirit's Refuge against autos, taunt away from danger and channel only to save an ally",
        Step(SDK::SpellSlot::W, StepRule::AllowDuringWindup),
        Step(SDK::SpellSlot::E, StepRule::RequireSafePosition | StepRule::AllowDuringWindup),
        Step(SDK::SpellSlot::R, StepRule::RequireTarget | StepRule::ManualAssistOnly));
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
