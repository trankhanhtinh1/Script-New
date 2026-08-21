#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Garen = [] {
    ChampionProfile p{};
    p.ChampionId = SDK::ChampionId::Garen;
    p.DisplayName = "The Might of Demacia";
    p.InternalId = "champion.kuroaio.ai.garen";
    p.PrimaryArchetype = Archetype::Juggernaut;
    p.Resource = ResourceModel::None;
    p.Mechanics = Mechanic::ObjectTracking | Mechanic::MissingHealth |
                  Mechanic::AutoReset;
    p.Ultimate = UltimatePolicy::Execute;
    p.PreferredCombatDistance = 175.0f;
    p.EngageHealthPercent = 42.0f;
    p.DefensiveHealthPercent = 38.0f;
    p.UltimateTargetHealthPercent = 50.0f;
    p.UltimateMinimumTargets = 1;
    p.MaximumCommitEnemies = 3;
    p.BaseHumanizerMs = 70;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "GarenPassive";
    p.MarkBuff = "GarenQAttack";
    p.ChannelBuff = "GarenE";
    p.UltimateBuff = "GarenR";
    p.TacticalSummary =
        "Resource-free juggernaut: arm Decisive Strike only for a real attack,"
        " spin Judgment through tracked bodies, reserve Courage for incoming pressure,"
        " and finish targets only across the missing-health Demacian Justice boundary.";
    p.ResearchSummary =
        "Riot 26.15 / CommunityDragon 16.15: Q grants movement and a silencing"
        " empowered attack, W supplies a brief shield and 30% damage reduction,"
        " E is a three-second area spin with a bonus isolated-target hit profile,"
        " R is true damage scaling with target missing health, and Perseverance"
        " restores health after a safe no-damage interval without a mana resource.";

    p.Spells[0] = Spell(
        SDK::SpellSlot::Q, "Decisive Strike", CastKind::Self,
        Intent::Damage | Intent::CrowdControl | Intent::Mobility |
            Intent::AutoReset | Intent::Buff,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::LastHit | Mode::Flee | Mode::Automatic,
        0.0f, 0.0f, 0.0f, FLT_MAX, false,
        SDK::DamageType::Physical, SDK::SpellType::Targeted);
    p.Spells[0].Aim = AimPolicy::SelfPosition;
    p.Spells[0].Priority = 94;
    p.Spells[0].WeaveAfterAttack = true;
    p.Spells[0].PreserveAutoAttack = true;
    p.Spells[0].ComboManaPercent = 0.0f;
    p.Spells[0].HarassManaPercent = 0.0f;
    p.Spells[0].ClearManaPercent = 0.0f;

    p.Spells[1] = Spell(
        SDK::SpellSlot::W, "Courage", CastKind::Self,
        Intent::Shield | Intent::Buff | Intent::Peel | Intent::Disengage,
        Mode::Combo | Mode::Harass | Mode::Jungle | Mode::Flee | Mode::Automatic,
        0.0f, 0.0f, 0.0f, FLT_MAX, false,
        SDK::DamageType::True, SDK::SpellType::Targeted);
    p.Spells[1].Aim = AimPolicy::SelfPosition;
    p.Spells[1].Priority = 96;
    p.Spells[1].PlayerHealthPercent = 70.0f;
    p.Spells[1].PreserveAutoAttack = false;

    p.Spells[2] = Spell(
        SDK::SpellSlot::E, "Judgment", CastKind::Toggle,
        Intent::Damage | Intent::CrowdControl | Intent::Waveclear |
            Intent::Jungle | Intent::LastHit | Intent::Buff,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::Automatic,
        325.0f, 0.0f, 325.0f, FLT_MAX, false,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotCircle);
    p.Spells[2].Aim = AimPolicy::SelfPosition;
    p.Spells[2].Priority = 90;
    p.Spells[2].MinimumAoeTargets = 1;
    p.Spells[2].PreserveAutoAttack = false;
    p.Spells[2].AllowOnMinions = true;
    p.Spells[2].ComboManaPercent = 0.0f;
    p.Spells[2].HarassManaPercent = 0.0f;
    p.Spells[2].ClearManaPercent = 0.0f;

    p.Spells[3] = Spell(
        SDK::SpellSlot::R, "Demacian Justice", CastKind::EnemyTarget,
        Intent::Damage | Intent::Execute | Intent::Finisher,
        Mode::Combo | Mode::Automatic,
        400.0f, 0.25f, 0.0f, FLT_MAX, false,
        SDK::DamageType::True, SDK::SpellType::Targeted);
    p.Spells[3].Aim = AimPolicy::TargetPosition;
    p.Spells[3].Priority = 100;
    p.Spells[3].TargetHealthPercent = 55.0f;
    p.Spells[3].PreserveAutoAttack = false;
    p.Spells[3].ComboManaPercent = 0.0f;
    p.Spells[3].HarassManaPercent = 0.0f;

    p.Trade = Plan(
        "Silence, spin and safe Courage trade",
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequireInsideAaRange |
             StepRule::AllowDuringWindup, 0, 800),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireInsideAaRange, 120, 1000),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget | StepRule::RequireSafePosition, 240, 1100));
    p.AllIn = Plan(
        "Judgment into missing-health Demacian Justice",
        Step(SDK::SpellSlot::W, StepRule::RequireTarget | StepRule::RequireSafePosition, 0, 700),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequireInsideAaRange |
             StepRule::AllowDuringWindup, 40, 850),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireInsideAaRange, 120, 1300),
        Step(SDK::SpellSlot::R, StepRule::RequireTarget | StepRule::RequireTargetLow |
             StepRule::HoldForExecute, 300, 1600));
    p.Flee = Plan(
        "Courage peel and Decisive Strike retreat",
        Step(SDK::SpellSlot::W, StepRule::RequireTarget | StepRule::RequireSafePosition, 0, 500),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::AllowDuringWindup, 80, 800));
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
