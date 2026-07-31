#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Lux = [] {
    ChampionProfile p{};
    p.ChampionName = "Lux";
    p.DisplayName = "Lux";
    p.InternalId = "champion.kuroaio.ai.lux";
    p.PrimaryArchetype = Archetype::BurstMage;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::Mark | Mechanic::Global | Mechanic::AllyTarget |
                  Mechanic::ReturnProjectile | Mechanic::Channel | Mechanic::WallInteraction;
    p.Ultimate = UltimatePolicy::GlobalExecute;
    p.PreferredCombatDistance = 850.0f;
    p.EngageHealthPercent = 48.0f;
    p.DefensiveHealthPercent = 30.0f;
    p.UltimateTargetHealthPercent = 58.0f;
    p.UltimateMinimumTargets = 1;
    p.MaximumCommitEnemies = 2;
    p.BaseHumanizerMs = 38;
    p.PreferSelectedTarget = true;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "LuxIllumination";
    p.MarkBuff = "LuxIllumination";
    p.TacticalSummary =
        "Mark targets with Illumination, root only on Q's first legal collision, "
        "send Prismatic Wave through the threatened ally for its returning shield, "
        "hold a visible Lucent Singularity zone for slow/detonation value and use "
        "Final Spark only on a safe predicted beam or explicit manual request.";
    p.ResearchSummary =
        "Riot 26.15 notes (no Lux balance change) and CommunityDragon 16.15 Lux "
        "spell-bin values: Q 1175/80/1200, W 1200/150/25-100+40% AP doubled on return, "
        "E 1100/295/20-45% slow/5 seconds, and R 3340/190/3000 with a 1.375 second cast.";

    p.Spells[0] = Spell(
        SDK::SpellSlot::Q, "Light Binding", CastKind::Line,
        Intent::Damage | Intent::CrowdControl | Intent::Setup |
            Intent::Interrupt,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::Automatic,
        1175.0f, 0.25f, 80.0f, 1200.0f, true,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[0].Aim = AimPolicy::Prediction;
    p.Spells[0].Priority = 94;
    p.Spells[0].Hitchance = SDK::HitChance::High;
    p.Spells[0].PreserveAutoAttack = true;
    p.Spells[0].RequiredTargetBuff = "";
    p.Spells[0].HarassManaPercent = 42.0f;

    p.Spells[1] = Spell(
        SDK::SpellSlot::W, "Prismatic Barrier", CastKind::Direction,
        Intent::Shield | Intent::Buff | Intent::AllyUtility | Intent::Peel,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::Automatic,
        1200.0f, 0.25f, 150.0f, 1200.0f, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[1].Aim = AimPolicy::Prediction;
    p.Spells[1].Priority = 88;
    p.Spells[1].PreserveAutoAttack = true;
    p.Spells[1].HarassManaPercent = 52.0f;

    p.Spells[2] = Spell(
        SDK::SpellSlot::E, "Lucent Singularity", CastKind::Circle,
        Intent::Damage | Intent::CrowdControl | Intent::Setup |
            Intent::Waveclear | Intent::Jungle | Intent::LastHit | Intent::Vision,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::LastHit | Mode::Flee | Mode::Automatic,
        1100.0f, 0.25f, 295.0f, 1300.0f, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[2].Aim = AimPolicy::Prediction;
    p.Spells[2].Priority = 86;
    p.Spells[2].MinimumAoeTargets = 1;
    p.Spells[2].PreserveAutoAttack = true;
    p.Spells[2].HarassManaPercent = 48.0f;
    p.Spells[2].ClearManaPercent = 38.0f;

    p.Spells[3] = Spell(
        SDK::SpellSlot::R, "Final Spark", CastKind::Line,
        Intent::Damage | Intent::Execute | Intent::Finisher |
            Intent::Channel | Intent::Objective,
        Mode::Combo | Mode::Automatic,
        3340.0f, 1.375f, 190.0f, 3000.0f, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[3].Aim = AimPolicy::Prediction;
    p.Spells[3].Priority = 100;
    p.Spells[3].Hitchance = SDK::HitChance::VeryHigh;
    p.Spells[3].PreserveAutoAttack = false;
    p.Spells[3].HarassManaPercent = 100.0f;

    p.Trade = Plan(
        "Q first collision into an Illumination auto or E zone detonation",
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::AllowDuringWindup),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget));
    p.AllIn = Plan(
        "Q mark, E slow/detonation, return-shield W and safe Final Spark",
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireCrowdControl),
        Step(SDK::SpellSlot::W, StepRule::RequireSafePosition),
        Step(SDK::SpellSlot::R, StepRule::RequireTarget | StepRule::HoldForExecute));
    p.Flee = Plan(
        "Return Prismatic Barrier through the player and Q peel",
        Step(SDK::SpellSlot::W, StepRule::RequireSafePosition | StepRule::AllowDuringWindup),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::AllowDuringWindup));
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
