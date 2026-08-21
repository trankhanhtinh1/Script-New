#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Poppy = [] {
    ChampionProfile p{};
    p.ChampionId = SDK::ChampionId::Poppy;
    p.DisplayName = "Poppy";
    p.InternalId = "champion.kuroaio.ai.poppy";
    p.PrimaryArchetype = Archetype::Vanguard;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::Dash | Mechanic::Recast | Mechanic::WallInteraction |
                  Mechanic::Terrain | Mechanic::DirectionalSweet | Mechanic::ObjectTracking;
    p.Ultimate = UltimatePolicy::Defensive;
    p.PreferredCombatDistance = 275.0f;
    p.EngageHealthPercent = 62.0f;
    p.DefensiveHealthPercent = 42.0f;
    p.UltimateTargetHealthPercent = 38.0f;
    p.UltimateMinimumTargets = 2;
    p.MaximumCommitEnemies = 3;
    p.BaseHumanizerMs = 65;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "PoppyPassive";
    p.TrackedObjectToken = "PoppyBuckler";
    p.TacticalSummary =
        "Defensive yordle vanguard: preserve the buckler throw, deny dashes with Steadfast Presence, and spend Heroic Charge only on observed terrain stuns.";
    p.ResearchSummary =
        "Riot 26.15 / CommunityDragon 16.15 Summoner's Rift values, with passive shield, wall collision and charged Keeper's Verdict state reconciled from events and polling.";

    p.Spells[0] = Spell(SDK::SpellSlot::Q, "Hammer Shock", CastKind::Position,
        Intent::Damage | Intent::CrowdControl | Intent::Waveclear | Intent::Jungle |
            Intent::LastHit,
        AllModes, 430.0f, 0.35f, 90.0f, 0.0f, false,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotLine);
    p.Spells[0].Aim = AimPolicy::Prediction;
    p.Spells[0].Priority = 88;
    p.Spells[0].Hitchance = SDK::HitChance::High;

    p.Spells[1] = Spell(SDK::SpellSlot::W, "Steadfast Presence", CastKind::Self,
        Intent::Buff | Intent::AntiGapcloser | Intent::CrowdControl | Intent::Peel,
        Mode::Combo | Mode::Harass | Mode::Automatic | Mode::Flee,
        400.0f, 0.0f, 400.0f, 0.0f, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[1].Aim = AimPolicy::SelfPosition;
    p.Spells[1].Priority = 100;

    p.Spells[2] = Spell(SDK::SpellSlot::E, "Heroic Charge", CastKind::EnemyTarget,
        Intent::Mobility | Intent::CrowdControl | Intent::Engage | Intent::Peel |
            Intent::Interrupt,
        AllModes, 475.0f, 0.0f, 80.0f, 1600.0f, false,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotLine);
    p.Spells[2].Aim = AimPolicy::TargetPosition;
    p.Spells[2].Priority = 96;

    p.Spells[3] = Spell(SDK::SpellSlot::R, "Keeper's Verdict", CastKind::ChargedLine,
        Intent::Damage | Intent::CrowdControl | Intent::Disengage | Intent::Peel |
            Intent::Interrupt | Intent::Recast,
        Mode::Combo | Mode::Automatic | Mode::Flee,
        1200.0f, 0.35f, 100.0f, 1700.0f, false,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotLine);
    p.Spells[3].Aim = AimPolicy::AwayFromThreat;
    p.Spells[3].Priority = 100;
    p.Spells[3].MinimumAoeTargets = 2;
    p.Spells[3].ChargeBuffName = "PoppyRCharge";
    p.Spells[3].ChargeMinRange = 300;
    p.Spells[3].ChargeMaxRange = 1200;
    p.Spells[3].ChargeDurationSeconds = 4.0f;

    p.Trade = Plan("Buckler wall trade",
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget, 0, 700),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireCrowdControl, 100, 800),
        Step(SDK::SpellSlot::W, StepRule::AllowDuringWindup, 160, 800));
    p.AllIn = Plan("Terrain pin and verdict",
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget, 0, 700),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireCrowdControl, 80, 850),
        Step(SDK::SpellSlot::R, StepRule::RequireMultiTarget | StepRule::RequireRecast, 220, 1800));
    p.Flee = Plan("Defensive knockback",
        Step(SDK::SpellSlot::W, StepRule::RequireSafePosition | StepRule::AllowDuringWindup, 0, 700),
        Step(SDK::SpellSlot::R, StepRule::RequireTarget | StepRule::RequireSafePosition, 100, 1200));
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
