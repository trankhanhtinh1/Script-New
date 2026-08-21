#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Ornn = [] {
    ChampionProfile p{};
    p.ChampionId = SDK::ChampionId::Ornn;
    p.DisplayName = "Ornn";
    p.InternalId = "champion.kuroaio.ai.ornn";
    p.PrimaryArchetype = Archetype::Vanguard;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::Charge | Mechanic::Recast | Mechanic::Terrain |
                  Mechanic::ObjectTracking | Mechanic::DirectionalSweet |
                  Mechanic::MissingHealth;
    p.Ultimate = UltimatePolicy::RecastControl;
    p.PreferredCombatDistance = 650.0f;
    p.EngageHealthPercent = 58.0f;
    p.DefensiveHealthPercent = 32.0f;
    p.UltimateTargetHealthPercent = 48.0f;
    p.UltimateMinimumTargets = 2;
    p.MaximumCommitEnemies = 3;
    p.BaseHumanizerMs = 65;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.TrackedObjectToken = "OrnnQSpawn";
    p.TacticalSummary =
        "Terrain-aware forge vanguard: create Q pillar setup, use brittle and wall-impact E to control a fight, and call or redirect the Ram only from a safe observed state.";
    p.ResearchSummary =
        "Riot 26.15 / CommunityDragon 16.15 Summoner's Rift values, with spell and pillar state reconciled from events and polling.";

    p.Spells[0] = Spell(SDK::SpellSlot::Q, "Volcanic Rupture", CastKind::Position,
        Intent::Damage | Intent::CrowdControl | Intent::Setup |
            Intent::Waveclear | Intent::Jungle,
        AllModes, 800.0f, 0.30f, 65.0f, 1200.0f, false,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotLine);
    p.Spells[0].Aim = AimPolicy::Prediction;
    p.Spells[0].Priority = 88;
    p.Spells[0].Hitchance = SDK::HitChance::High;

    p.Spells[1] = Spell(SDK::SpellSlot::W, "Bellows Breath", CastKind::Direction,
        Intent::Damage | Intent::CrowdControl | Intent::Setup | Intent::Peel,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::Automatic | Mode::Flee,
        560.0f, 0.25f, 175.0f, 0.0f, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[1].Aim = AimPolicy::BetweenPlayerAndTarget;
    p.Spells[1].Priority = 94;

    p.Spells[2] = Spell(SDK::SpellSlot::E, "Searing Charge", CastKind::Direction,
        Intent::Mobility | Intent::CrowdControl | Intent::Engage |
            Intent::Disengage | Intent::Peel,
        AllModes, 650.0f, 0.0f, 175.0f, 1600.0f, false,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotLine);
    p.Spells[2].Aim = AimPolicy::SafeCursor;
    p.Spells[2].Priority = 96;

    p.Spells[3] = Spell(SDK::SpellSlot::R, "Call of the Forge God", CastKind::Vector,
        Intent::Damage | Intent::CrowdControl | Intent::Recast |
            Intent::Interrupt | Intent::Setup,
        Mode::Combo | Mode::Flee | Mode::Automatic,
        2500.0f, 0.50f, 120.0f, 1200.0f, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[3].Aim = AimPolicy::BestAoe;
    p.Spells[3].Priority = 100;
    p.Spells[3].MinimumAoeTargets = 2;

    p.Trade = Plan("Brittle trade",
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget, 0, 850),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget | StepRule::AllowDuringWindup, 90, 700),
        Step(SDK::SpellSlot::E, StepRule::RequireSafePosition, 150, 700));
    p.AllIn = Plan("Pillar charge and Ram",
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget, 0, 850),
        Step(SDK::SpellSlot::E, StepRule::RequireSafePosition, 100, 800),
        Step(SDK::SpellSlot::R, StepRule::RequireMultiTarget, 220, 2500),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget, 260, 700));
    p.Flee = Plan("Brittle peel",
        Step(SDK::SpellSlot::W, StepRule::RequireSafePosition, 0, 700),
        Step(SDK::SpellSlot::E, StepRule::RequireSafePosition, 80, 700),
        Step(SDK::SpellSlot::R, StepRule::RequireMultiTarget | StepRule::RequireSafePosition, 150, 2500));
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
