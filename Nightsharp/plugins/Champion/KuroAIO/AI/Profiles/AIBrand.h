#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Brand = [] {
    ChampionProfile p{};
    p.ChampionId = SDK::ChampionId::Brand;
    p.DisplayName = "Brand";
    p.InternalId = "champion.kuroaio.ai.brand";
    p.PrimaryArchetype = Archetype::BurstMage;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::Stack | Mechanic::Mark | Mechanic::ObjectTracking |
                  Mechanic::Terrain;
    p.Ultimate = UltimatePolicy::MultiTarget;
    p.PreferredCombatDistance = 700.0f;
    p.EngageHealthPercent = 42.0f;
    p.DefensiveHealthPercent = 25.0f;
    p.UltimateTargetHealthPercent = 80.0f;
    p.UltimateMinimumTargets = 2;
    p.MaximumCommitEnemies = 3;
    p.BaseHumanizerMs = 55;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "BrandAblaze";
    p.MarkBuff = "BrandAblaze";
    p.TrackedObjectToken = "BrandAblazeDetonateMarker";
    p.TacticalSummary =
        "Ablaze stack mage: establish a mark before Q stun, use W pillar and E spread to build three stacks, then reserve Pyroclasm for safe clustered bounces.";
    p.ResearchSummary =
        "Riot 26.15 / CommunityDragon 16.15 Brand Q-W-E-R and Blaze passive data; conditional Q stun, spread geometry, bounce routing and detonation safety are event/poll reconciled.";

    p.Spells[0] = Spell(SDK::SpellSlot::Q, "Sear", CastKind::Position,
        Intent::Damage | Intent::CrowdControl | Intent::Setup | Intent::Interrupt,
        CombatModes | Mode::Automatic | Mode::Flee, 1050.0f, 0.25f, 80.0f,
        1600.0f, true, SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[0].Aim = AimPolicy::Prediction;
    p.Spells[0].Priority = 92;
    p.Spells[0].RequiredTargetBuff = "BrandAblaze";
    p.Spells[0].PreserveAutoAttack = true;
    p.Spells[0].ComboManaPercent = 0.0f;

    p.Spells[1] = Spell(SDK::SpellSlot::W, "Pillar of Flame", CastKind::Circle,
        Intent::Damage | Intent::Setup | Intent::Waveclear | Intent::Jungle,
        AllModes, 900.0f, 0.25f, 240.0f, 20.0f, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[1].Aim = AimPolicy::Prediction;
    p.Spells[1].Priority = 86;
    p.Spells[1].MinimumAoeTargets = 1;
    p.Spells[1].ClearManaPercent = 35.0f;

    p.Spells[2] = Spell(SDK::SpellSlot::E, "Conflagration", CastKind::EnemyTarget,
        Intent::Damage | Intent::Setup | Intent::Waveclear | Intent::Jungle |
            Intent::LastHit,
        AllModes, 625.0f, 0.25f, 315.0f, 900.0f, false,
        SDK::DamageType::Magical, SDK::SpellType::Targeted);
    p.Spells[2].Aim = AimPolicy::TargetPosition;
    p.Spells[2].Priority = 78;
    p.Spells[2].PreserveAutoAttack = true;

    p.Spells[3] = Spell(SDK::SpellSlot::R, "Pyroclasm", CastKind::EnemyTarget,
        Intent::Damage | Intent::Finisher | Intent::Setup,
        Mode::Combo | Mode::Automatic, 750.0f, 0.25f, 600.0f, 1000.0f,
        false, SDK::DamageType::Magical, SDK::SpellType::Targeted);
    p.Spells[3].Aim = AimPolicy::TargetPosition;
    p.Spells[3].Priority = 100;
    p.Spells[3].MinimumAoeTargets = 2;
    p.Spells[3].PreserveAutoAttack = false;

    p.Trade = Plan("Ablaze mark into conditional stun",
        Step(SDK::SpellSlot::W, StepRule::RequireTarget, 0, 850),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget, 120, 980),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequireMark, 180, 1250));
    p.AllIn = Plan("Three-stack detonation and clustered Pyroclasm",
        Step(SDK::SpellSlot::W, StepRule::RequireTarget, 0, 850),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget, 110, 1000),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequireMark, 170, 1250),
        Step(SDK::SpellSlot::R, StepRule::RequireTarget | StepRule::RequireMultiTarget, 230, 1500));
    p.Flee = Plan("Sear peel and conservative detonation",
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequireMark, 0, 850),
        Step(SDK::SpellSlot::W, StepRule::RequireSafePosition, 80, 900));
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
