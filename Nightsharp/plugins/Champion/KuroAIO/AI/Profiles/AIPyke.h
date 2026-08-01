#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Pyke = [] {
    ChampionProfile p{};
    p.ChampionId = SDK::ChampionId::Pyke;
    p.DisplayName = "Pyke";
    p.InternalId = "champion.kuroaio.ai.pyke";
    p.PrimaryArchetype = Archetype::Assassin;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::Charge | Mechanic::Dash | Mechanic::Execute |
                  Mechanic::MissingHealth | Mechanic::Mark;
    p.Ultimate = UltimatePolicy::Execute;
    p.PreferredCombatDistance = 550.0f;
    p.EngageHealthPercent = 48.0f;
    p.DefensiveHealthPercent = 42.0f;
    p.UltimateTargetHealthPercent = 45.0f;
    p.UltimateMinimumTargets = 1;
    p.MaximumCommitEnemies = 2;
    p.BaseHumanizerMs = 68;
    p.PreferSelectedTarget = true;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.TrackedObjectToken = "PykeEStunTrail";
    p.TacticalSummary =
        "Ambush assassin: preserve grey health with Ghostwater, charge and throw Bone Skewer, trail a stun through the dash, and spend Death from Below only on an observed execute while respecting shared-gold intent.";
    p.ResearchSummary =
        "Riot 26.15 / CommunityDragon 16.15 baseline; Q charge and throw, W camouflage, E returning trail, and R execute/share behavior are reconciled from spell and buff telemetry.";

    p.Spells[0] = Spell(SDK::SpellSlot::Q, "Bone Skewer", CastKind::Direction,
        Intent::Damage | Intent::CrowdControl | Intent::Setup | Intent::Finisher,
        AllModes, 750.0f, 0.20f, 70.0f, 2000.0f, false,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotLine);
    p.Spells[0].Aim = AimPolicy::Prediction;
    p.Spells[0].Priority = 90;
    p.Spells[0].Hitchance = SDK::HitChance::High;

    p.Spells[1] = Spell(SDK::SpellSlot::W, "Ghostwater", CastKind::Self,
        Intent::Buff | Intent::Mobility | Intent::Vision | Intent::Setup |
            Intent::Disengage | Intent::Peel,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::Automatic,
        600.0f, 0.0f, 0.0f, 0.0f, false,
        SDK::DamageType::Physical, SDK::SpellType::Activated);
    p.Spells[1].Aim = AimPolicy::SafeCursor;
    p.Spells[1].Priority = 72;

    p.Spells[2] = Spell(SDK::SpellSlot::E, "Phantom Undertow", CastKind::Direction,
        Intent::Damage | Intent::CrowdControl | Intent::Mobility |
            Intent::Engage | Intent::Disengage | Intent::Peel | Intent::Setup,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::Automatic,
        550.0f, 0.10f, 110.0f, 1500.0f, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[2].Aim = AimPolicy::SafeCursor;
    p.Spells[2].Priority = 94;

    p.Spells[3] = Spell(SDK::SpellSlot::R, "Death from Below", CastKind::Position,
        Intent::Damage | Intent::Execute | Intent::Finisher | Intent::AllyUtility,
        Mode::Combo | Mode::Flee | Mode::Automatic,
        750.0f, 0.20f, 180.0f, 0.0f, false,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotCircle);
    p.Spells[3].Aim = AimPolicy::Prediction;
    p.Spells[3].Priority = 100;
    p.Spells[3].MinimumAoeTargets = 1;

    p.Trade = Plan("Ghostwater catch",
        Step(SDK::SpellSlot::W, StepRule::RequireSafePosition, 0, 700),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget, 90, 800),
        Step(SDK::SpellSlot::E, StepRule::RequireSafePosition, 130, 650));
    p.AllIn = Plan("Undertow execution",
        Step(SDK::SpellSlot::W, StepRule::RequireSafePosition, 0, 700),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget, 90, 800),
        Step(SDK::SpellSlot::E, StepRule::RequireSafePosition, 160, 650),
        Step(SDK::SpellSlot::R, StepRule::RequireTargetLow | StepRule::HoldForExecute, 220, 800));
    p.Flee = Plan("Ghostwater escape",
        Step(SDK::SpellSlot::W, StepRule::RequireSafePosition, 0, 700),
        Step(SDK::SpellSlot::E, StepRule::RequireSafePosition, 85, 650),
        Step(SDK::SpellSlot::Q, StepRule::ManualAssistOnly, 140, 800));
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
