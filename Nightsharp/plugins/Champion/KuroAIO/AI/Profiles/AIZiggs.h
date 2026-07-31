#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Ziggs = [] {
    ChampionProfile p{};
    p.ChampionName = "Ziggs";
    p.DisplayName = "Ziggs";
    p.InternalId = "champion.kuroaio.ai.ziggs";
    p.PrimaryArchetype = Archetype::BurstMage;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::Charge | Mechanic::ObjectTracking | Mechanic::Terrain |
                  Mechanic::WallInteraction;
    p.Ultimate = UltimatePolicy::GlobalExecute;
    p.PreferredCombatDistance = 850.0f;
    p.EngageHealthPercent = 55.0f;
    p.DefensiveHealthPercent = 32.0f;
    p.UltimateTargetHealthPercent = 42.0f;
    p.UltimateMinimumTargets = 2;
    p.MaximumCommitEnemies = 2;
    p.BaseHumanizerMs = 70;
    p.PreferSelectedTarget = true;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.TrackedObjectToken = "ZiggsW";
    p.TacticalSummary =
        "Long-range artillery and layered minefield control: use Q prediction, W displacement only from a safe endpoint, E to deny routes, and R for executes or verified multi-target value.";
    p.ResearchSummary =
        "Riot 26.15 / CommunityDragon 16.15 Summoner's Rift values, with projectile collision, wall and zone state reconciled from events and polling.";

    p.Spells[0] = Spell(SDK::SpellSlot::Q, "Bouncing Bomb", CastKind::Position,
        Intent::Damage | Intent::Setup | Intent::Waveclear | Intent::Jungle |
            Intent::LastHit,
        AllModes, 850.0f, 0.25f, 140.0f, 1700.0f, true,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[0].Aim = AimPolicy::Prediction;
    p.Spells[0].Priority = 86;
    p.Spells[0].Hitchance = SDK::HitChance::High;

    p.Spells[1] = Spell(SDK::SpellSlot::W, "Satchel Charge", CastKind::Position,
        Intent::Damage | Intent::Mobility | Intent::CrowdControl | Intent::Engage |
            Intent::Disengage | Intent::Peel | Intent::Setup,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::Flee | Mode::Automatic,
        1000.0f, 0.25f, 550.0f, 0.0f, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[1].Aim = AimPolicy::SafeCursor;
    p.Spells[1].Priority = 96;
    p.Spells[1].MaximumEnemiesAtDestination = 1;

    p.Spells[2] = Spell(SDK::SpellSlot::E, "Hexplosive Minefield", CastKind::Position,
        Intent::Damage | Intent::CrowdControl | Intent::Setup | Intent::Waveclear |
            Intent::Jungle | Intent::LastHit | Intent::Objective | Intent::Peel,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::LastHit | Mode::Flee | Mode::Automatic,
        900.0f, 0.25f, 500.0f, 0.0f, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[2].Aim = AimPolicy::Prediction;
    p.Spells[2].Priority = 92;
    p.Spells[2].MinimumAoeTargets = 2;

    p.Spells[3] = Spell(SDK::SpellSlot::R, "Mega Inferno Bomb", CastKind::Position,
        Intent::Damage | Intent::Execute | Intent::Finisher | Intent::CrowdControl |
            Intent::Objective | Intent::Setup,
        Mode::Combo | Mode::Harass | Mode::Jungle | Mode::Automatic | Mode::Flee,
        5300.0f, 0.375f, 500.0f, 1550.0f, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[3].Aim = AimPolicy::BestAoe;
    p.Spells[3].Priority = 100;
    p.Spells[3].MinimumAoeTargets = 2;
    p.Spells[3].TargetHealthPercent = 42.0f;

    p.Trade = Plan("Minefield poke",
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget, 0, 850),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget, 110, 900));
    p.AllIn = Plan("Satchel minefield artillery",
        Step(SDK::SpellSlot::E, StepRule::RequireTarget, 0, 900),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget, 90, 850),
        Step(SDK::SpellSlot::W, StepRule::RequireSafePosition, 180, 1000),
        Step(SDK::SpellSlot::R, StepRule::RequireTarget | StepRule::RequireMultiTarget,
             260, 5300));
    p.Flee = Plan("Satchel escape",
        Step(SDK::SpellSlot::W, StepRule::RequireSafePosition, 0, 1000),
        Step(SDK::SpellSlot::E, StepRule::RequireSafePosition, 100, 900),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget, 180, 850));
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
