#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Singed = [] {
    ChampionProfile p{};
    p.ChampionId = SDK::ChampionId::Singed;
    p.DisplayName = "Singed";
    p.InternalId = "champion.kuroaio.ai.singed";
    p.PrimaryArchetype = Archetype::Specialist;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::ObjectTracking | Mechanic::Terrain | Mechanic::AutoWeave;
    p.Ultimate = UltimatePolicy::Defensive;
    p.PreferredCombatDistance = 300.0f;
    p.EngageHealthPercent = 55.0f;
    p.DefensiveHealthPercent = 32.0f;
    p.UltimateTargetHealthPercent = 100.0f;
    p.MaximumCommitEnemies = 2;
    p.BaseHumanizerMs = 52;
    p.PassiveBuff = "slipstream";
    p.UltimateBuff = "InsanityPotion";
    p.TacticalSummary =
        "Toggle poison while routing enemies through Mega Adhesive; fling only to a safe endpoint, then use Insanity Potion for proxy and escape posture.";
    p.ResearchSummary =
        "Riot 26.15 and CommunityDragon 16.15 Singed data: Q damage-over-time toggle, W grounded zone, E max-health fling and R 25-second stat potion.";

    p.Spells[0] = Spell(SDK::SpellSlot::Q, "Poison Trail", CastKind::Toggle,
        Intent::Damage | Intent::Waveclear | Intent::Jungle | Intent::LastHit,
        AllModes, 225.0f, 0.0f, 210.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[0].Aim = AimPolicy::SelfPosition;
    p.Spells[0].Priority = 72;
    p.Spells[0].HarassManaPercent = 30.0f;
    p.Spells[0].ClearManaPercent = 24.0f;
    p.Spells[0].PreserveAutoAttack = false;

    p.Spells[1] = Spell(SDK::SpellSlot::W, "Mega Adhesive", CastKind::Circle,
        Intent::CrowdControl | Intent::Setup | Intent::AntiGapcloser | Intent::Disengage,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::Automatic,
        1000.0f, 0.375f, 265.0f, 700.0f, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[1].Aim = AimPolicy::Prediction;
    p.Spells[1].Priority = 84;
    p.Spells[1].MaximumEnemiesAtDestination = 2;
    p.Spells[1].HarassManaPercent = 42.0f;

    p.Spells[2] = Spell(SDK::SpellSlot::E, "Fling", CastKind::EnemyTarget,
        Intent::Damage | Intent::CrowdControl | Intent::Engage | Intent::Disengage |
            Intent::Finisher | Intent::LastHit,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::Automatic,
        125.0f, 0.25f, 100.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::Targeted);
    p.Spells[2].Aim = AimPolicy::BehindTarget;
    p.Spells[2].Priority = 100;
    p.Spells[2].TargetHealthPercent = 65.0f;
    p.Spells[2].MaximumEnemiesAtDestination = 2;
    p.Spells[2].PreserveAutoAttack = false;

    p.Spells[3] = Spell(SDK::SpellSlot::R, "Insanity Potion", CastKind::Self,
        Intent::Buff | Intent::Engage | Intent::Disengage,
        Mode::Combo | Mode::Automatic | Mode::Flee,
        20.0f, 0.25f, 0.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::Targeted);
    p.Spells[3].Aim = AimPolicy::SelfPosition;
    p.Spells[3].Priority = 96;
    p.Spells[3].PlayerHealthPercent = 68.0f;
    p.Spells[3].PreserveAutoAttack = false;

    p.Trade = Plan("poison route with adhesive setup",
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::AllowDuringWindup, 0, 900),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget, 80, 950),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireTargetLow, 180, 1200));
    p.AllIn = Plan("potion proxy fling",
        Step(SDK::SpellSlot::R, StepRule::RequireTarget | StepRule::AllowDuringWindup, 0, 1000),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::AllowDuringWindup, 55, 1100),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget, 110, 1200),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireTargetLow, 230, 1450));
    p.Flee = Plan("potion and adhesive escape",
        Step(SDK::SpellSlot::R, StepRule::RequirePlayerLow | StepRule::AllowDuringWindup, 0, 850),
        Step(SDK::SpellSlot::W, StepRule::RequireSafePosition | StepRule::AllowDuringWindup, 60, 950),
        Step(SDK::SpellSlot::Q, StepRule::RequireSafePosition | StepRule::AllowDuringWindup, 100, 1000));
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
