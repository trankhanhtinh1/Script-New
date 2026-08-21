#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile RekSai = [] {
    ChampionProfile p{};
    p.ChampionId = SDK::ChampionId::RekSai;
    p.DisplayName = "Rek'Sai";
    p.InternalId = "champion.kuroaio.ai.reksai";
    p.PrimaryArchetype = Archetype::Diver;
    p.Resource = ResourceModel::Fury;
    p.Mechanics = Mechanic::Transform | Mechanic::Dash | Mechanic::ObjectTracking |
                  Mechanic::Execute | Mechanic::Mark | Mechanic::MissingHealth |
                  Mechanic::WallInteraction;
    p.Ultimate = UltimatePolicy::Execute;
    p.PreferredCombatDistance = 325.0f;
    p.EngageHealthPercent = 52.0f;
    p.DefensiveHealthPercent = 38.0f;
    p.UltimateTargetHealthPercent = 45.0f;
    p.UltimateMinimumTargets = 1;
    p.MaximumCommitEnemies = 3;
    p.BaseHumanizerMs = 55;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "RekSaiPassive";
    p.FormBuff = "RekSaiBurrowed";
    p.MarkBuff = "RekSaiPreySeeker";
    p.TrackedObjectToken = "RekSaiTunnel";
    p.TacticalSummary =
        "Stateful Rek'Sai diver: reconcile burrow posture and tunnels, build fury with safe attacks, use W knock-up and true-damage Furious Bite, and only take marked-target Void Rush when the landing is survivable.";
    p.ResearchSummary =
        "Riot 26.15 / CommunityDragon 16.15 baseline; spell names, burrow buffs, tunnel objects, prey marks and level-scaled damage are observed through events with polling reconciliation.";

    p.Spells[0] = Spell(SDK::SpellSlot::Q, "Queen's Wrath / Prey Seeker",
        CastKind::Position, Intent::Damage | Intent::AutoReset | Intent::Setup |
            Intent::Waveclear | Intent::Jungle,
        AllModes, 1625.0f, 0.40f, 65.0f, 1950.0f, false,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotLine);
    p.Spells[0].Aim = AimPolicy::Prediction;
    p.Spells[0].Priority = 88;

    p.Spells[1] = Spell(SDK::SpellSlot::W, "Burrow / Unburrow",
        CastKind::Self, Intent::CrowdControl | Intent::Mobility | Intent::Disengage |
            Intent::Peel | Intent::Heal,
        AllModes, 165.0f, 0.0f, 165.0f, 0.0f, false,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotCircle);
    p.Spells[1].Aim = AimPolicy::SelfPosition;
    p.Spells[1].Priority = 97;

    p.Spells[2] = Spell(SDK::SpellSlot::E, "Tunnel / Furious Bite",
        CastKind::Position, Intent::Damage | Intent::Mobility | Intent::Execute |
            Intent::Setup | Intent::Jungle,
        AllModes, 2500.0f, 0.25f, 100.0f, 1700.0f, false,
        SDK::DamageType::True, SDK::SpellType::SkillshotLine);
    p.Spells[2].Aim = AimPolicy::SafeCursor;
    p.Spells[2].Priority = 95;

    p.Spells[3] = Spell(SDK::SpellSlot::R, "Void Rush",
        CastKind::EnemyTarget, Intent::Damage | Intent::Execute | Intent::Mobility |
            Intent::Finisher,
        Mode::Combo | Mode::Automatic | Mode::Flee, 1500.0f, 0.25f, 0.0f, 0.0f,
        false, SDK::DamageType::Physical, SDK::SpellType::Targeted);
    p.Spells[3].Aim = AimPolicy::TargetPosition;
    p.Spells[3].Priority = 100;
    p.Spells[3].RequiredTargetBuff = "RekSaiPreySeeker";

    p.Trade = Plan("Burrowed probe and fury trade",
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget, 0, 550),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget | StepRule::AllowDuringWindup, 80, 650),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::HoldForExecute, 160, 850));
    p.AllIn = Plan("Knock-up, bite and marked Void Rush",
        Step(SDK::SpellSlot::W, StepRule::RequireTarget, 0, 600),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::AllowDuringWindup, 80, 700),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::HoldForExecute, 160, 900),
        Step(SDK::SpellSlot::R, StepRule::RequireTarget | StepRule::RequireMark | StepRule::RequireTargetLow, 250, 1400));
    p.Flee = Plan("Burrowed tunnel retreat",
        Step(SDK::SpellSlot::W, StepRule::RequireSafePosition, 0, 450),
        Step(SDK::SpellSlot::E, StepRule::RequireSafePosition, 100, 900));
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
