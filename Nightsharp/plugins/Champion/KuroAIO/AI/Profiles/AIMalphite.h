#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Malphite = [] {
    ChampionProfile p{};
    p.ChampionName = "Malphite";
    p.DisplayName = "Malphite";
    p.InternalId = "champion.kuroaio.ai.malphite";
    p.PrimaryArchetype = Archetype::Vanguard;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::Mark | Mechanic::AutoReset | Mechanic::Dash |
                  Mechanic::Terrain | Mechanic::WallInteraction;
    p.Ultimate = UltimatePolicy::MultiTarget;
    p.PreferredCombatDistance = 475.0f;
    p.EngageHealthPercent = 55.0f;
    p.DefensiveHealthPercent = 30.0f;
    p.UltimateTargetHealthPercent = 70.0f;
    p.UltimateMinimumTargets = 2;
    p.MaximumCommitEnemies = 3;
    p.BaseHumanizerMs = 65;
    p.PreferSelectedTarget = true;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "MalphiteShield";
    p.MarkBuff = "SeismicShardBuff";
    p.UltimateBuff = "UnstoppableForce";
    p.TacticalSummary =
        "Armor vanguard that waits for Granite Shield, steals movement speed with Q, "
        "weaves Thunderclap's attack reset, cripples attack speed with E, and reserves "
        "Unstoppable Force for a predicted multi-target or lethal landing.";
    p.ResearchSummary =
        "Riot 26.15 / CommunityDragon 16.15: 10% max-health Granite Shield with "
        "8/7/6-second refresh, 625-range 1200-speed Q, 350-radius W reset, "
        "400-radius E cripple, and 1000-range 270-radius R.";

    p.Spells[0] = Spell(SDK::SpellSlot::Q, "Seismic Shard", CastKind::EnemyTarget,
        Intent::Damage | Intent::Setup | Intent::CrowdControl,
        AllModes, 625.0f, 0.25f, 45.0f, 1200.0f, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[0].Aim = AimPolicy::Prediction;
    p.Spells[0].Priority = 76;
    p.Spells[0].RequiredTargetBuff = "";
    p.Spells[0].WeaveAfterAttack = false;

    p.Spells[1] = Spell(SDK::SpellSlot::W, "Thunderclap", CastKind::Self,
        Intent::Damage | Intent::AutoReset | Intent::Buff,
        CombatModes | FarmModes, 350.0f, 0.05f, 350.0f, FLT_MAX, false,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotCircle);
    p.Spells[1].Aim = AimPolicy::SelfPosition;
    p.Spells[1].Priority = 84;
    p.Spells[1].WeaveAfterAttack = true;
    p.Spells[1].PreserveAutoAttack = false;

    p.Spells[2] = Spell(SDK::SpellSlot::E, "Ground Slam", CastKind::Self,
        Intent::Damage | Intent::CrowdControl | Intent::Peel | Intent::Waveclear |
            Intent::Jungle | Intent::LastHit,
        AllModes, 400.0f, 0.24f, 400.0f, FLT_MAX, false,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotCircle);
    p.Spells[2].Aim = AimPolicy::SelfPosition;
    p.Spells[2].Priority = 82;

    p.Spells[3] = Spell(SDK::SpellSlot::R, "Unstoppable Force", CastKind::Position,
        Intent::Damage | Intent::CrowdControl | Intent::Mobility | Intent::Engage |
            Intent::Peel | Intent::Interrupt,
        Mode::Combo | Mode::Automatic | Mode::Flee, 1000.0f, 0.25f, 270.0f,
        700.0f, false, SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[3].Aim = AimPolicy::BestAoe;
    p.Spells[3].Priority = 100;
    p.Spells[3].MinimumAoeTargets = 2;
    p.Spells[3].MaximumEnemiesAtDestination = 3;
    p.Spells[3].ComboManaPercent = 0.0f;
    p.Spells[3].PreserveAutoAttack = false;

    p.Trade = Plan("Shard and reset trade",
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget, 0, 850),
        Step(SDK::SpellSlot::W, StepRule::RequireInsideAaRange | StepRule::AllowDuringWindup, 120, 950),
        Step(SDK::SpellSlot::E, StepRule::RequireInsideAaRange, 180, 1000));
    p.AllIn = Plan("Unstoppable Force armor engage",
        Step(SDK::SpellSlot::R, StepRule::RequireTarget | StepRule::RequireSafePosition |
             StepRule::RequireMultiTarget, 0, 2200),
        Step(SDK::SpellSlot::E, StepRule::RequireCrowdControl | StepRule::AllowDuringWindup, 180, 1200),
        Step(SDK::SpellSlot::W, StepRule::RequireInsideAaRange | StepRule::AllowDuringWindup, 240, 1300),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget, 320, 1400));
    p.Flee = Plan("Shard slow and Ground Slam peel",
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequireSafePosition, 0, 900),
        Step(SDK::SpellSlot::E, StepRule::RequireSafePosition | StepRule::AllowDuringWindup, 90, 900));
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
