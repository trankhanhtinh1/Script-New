#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile MonkeyKing = [] {
    ChampionProfile p{};
    p.ChampionName = "MonkeyKing";
    p.DisplayName = "Wukong";
    p.InternalId = "champion.kuroaio.ai.monkeyking";
    p.PrimaryArchetype = Archetype::Diver;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::Dash | Mechanic::Channel | Mechanic::Recast |
                  Mechanic::ObjectTracking | Mechanic::Mark | Mechanic::AutoReset |
                  Mechanic::DirectionalSweet;
    p.Ultimate = UltimatePolicy::RecastControl;
    p.PreferredCombatDistance = 250.0f;
    p.EngageHealthPercent = 48.0f;
    p.DefensiveHealthPercent = 30.0f;
    p.UltimateTargetHealthPercent = 62.0f;
    p.UltimateMinimumTargets = 2;
    p.MaximumCommitEnemies = 3;
    p.BaseHumanizerMs = 45;
    p.PreferSelectedTarget = true;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "MonkeyKingStoneSkin";
    p.MarkBuff = "MonkeyKingQArmorReduction";
    p.UltimateBuff = "MonkeyKingSpinToWin";
    p.TrackedObjectToken = "MonkeyKingDecoy";
    p.TacticalSummary =
        "Wukong diver: preserve Q armor shred for the empowered attack, use Decoy as a tracked stealth/clone state, choose a safe Nimbus Strike endpoint, and own the Cyclone channel with a deliberate recast.";
    p.ResearchSummary =
        "Pinned to Riot 26.15 / CommunityDragon 16.15 Summoner's Rift spell data; clone and spin states are reconciled from events plus polling when names or telemetry are unavailable.";

    p.Spells[0] = Spell(SDK::SpellSlot::Q, "Crushing Blow", CastKind::EnemyTarget,
        Intent::Damage | Intent::AutoReset | Intent::Setup,
        AllModes, 300.0f, 0.0f, 0.0f, FLT_MAX, false,
        SDK::DamageType::Physical, SDK::SpellType::Targeted);
    p.Spells[0].Aim = AimPolicy::TargetPosition;
    p.Spells[0].Priority = 96;
    p.Spells[0].WeaveAfterAttack = true;
    p.Spells[0].PreserveAutoAttack = false;

    p.Spells[1] = Spell(SDK::SpellSlot::W, "Decoy", CastKind::Self,
        Intent::Damage | Intent::Mobility | Intent::Disengage | Intent::Peel |
            Intent::Setup | Intent::AutoReset,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::Automatic,
        275.0f, 0.0f, 0.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[1].Aim = AimPolicy::SafeCursor;
    p.Spells[1].Priority = 88;
    p.Spells[1].DashDistance = 275.0f;
    p.Spells[1].PreserveAutoAttack = true;

    p.Spells[2] = Spell(SDK::SpellSlot::E, "Nimbus Strike", CastKind::EnemyTarget,
        Intent::Damage | Intent::Mobility | Intent::Engage | Intent::Disengage |
            Intent::Setup | Intent::Waveclear | Intent::Jungle,
        AllModes, 625.0f, 0.0f, 110.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::Targeted);
    p.Spells[2].Aim = AimPolicy::Prediction;
    p.Spells[2].Priority = 94;
    p.Spells[2].DashDistance = 625.0f;
    p.Spells[2].MaximumEnemiesAtDestination = 2;
    p.Spells[2].PreserveAutoAttack = true;

    p.Spells[3] = Spell(SDK::SpellSlot::R, "Cyclone", CastKind::Self,
        Intent::Damage | Intent::CrowdControl | Intent::Engage | Intent::Disengage |
            Intent::Interrupt | Intent::Peel | Intent::Channel | Intent::Recast,
        Mode::Combo | Mode::Flee | Mode::Automatic,
        315.0f, 0.0f, 315.0f, FLT_MAX, false,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotCircle);
    p.Spells[3].Aim = AimPolicy::SelfPosition;
    p.Spells[3].Priority = 100;
    p.Spells[3].MinimumAoeTargets = 2;
    p.Spells[3].TargetHealthPercent = 62.0f;
    p.Spells[3].RecastSpellName = "MonkeyKingSpinToWin";
    p.Spells[3].PreserveAutoAttack = true;

    p.Trade = Plan("Q armor shred trade",
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::AllowDuringWindup),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireSafePosition),
        Step(SDK::SpellSlot::W, StepRule::RequireSafePosition));
    p.AllIn = Plan("Decoy entry and Cyclone",
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireSafePosition),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::AllowDuringWindup),
        Step(SDK::SpellSlot::R, StepRule::RequireTarget | StepRule::RequireSafePosition |
             StepRule::RequireMultiTarget));
    p.Flee = Plan("Decoy and Nimbus disengage",
        Step(SDK::SpellSlot::W, StepRule::RequireSafePosition),
        Step(SDK::SpellSlot::E, StepRule::RequireSafePosition),
        Step(SDK::SpellSlot::R, StepRule::ManualAssistOnly));

    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
