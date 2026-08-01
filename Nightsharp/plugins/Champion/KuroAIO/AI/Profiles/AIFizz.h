#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Fizz = [] {
    ChampionProfile p{};
    p.ChampionId = SDK::ChampionId::Fizz;
    p.DisplayName = "Fizz";
    p.InternalId = "champion.kuroaio.ai.fizz";
    p.PrimaryArchetype = Archetype::Assassin;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::Dash | Mechanic::Recast | Mechanic::AutoReset |
                  Mechanic::AutoWeave | Mechanic::ReturnProjectile | Mechanic::Terrain;
    p.Ultimate = UltimatePolicy::AllIn;
    p.PreferredCombatDistance = 275.0f;
    p.EngageHealthPercent = 52.0f;
    p.DefensiveHealthPercent = 30.0f;
    p.UltimateTargetHealthPercent = 72.0f;
    p.UltimateMinimumTargets = 1;
    p.MaximumCommitEnemies = 2;
    p.BaseHumanizerMs = 52;
    p.PreferSelectedTarget = true;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "FizzPassive";
    p.MarkBuff = "FizzWActive";
    p.ChannelBuff = "FizzE";
    p.UltimateBuff = "FizzRCling";
    p.TacticalSummary =
        "Assassin route: Urchin Strike dash, Seastone Trident auto reset, timed Playful/Trickster untargetability and a first-champion Chum projectile whose shark grows with travel distance.";
    p.ResearchSummary =
        "Riot patch 26.15 contains no Fizz balance change; CommunityDragon 16.15 retains Q targeted dash, W empowered next attack, E recast hop and R small/medium/large fish multipliers.";

p.Spells[0] = Spell(SDK::SpellSlot::Q, "Urchin Strike", CastKind::EnemyTarget,
        Intent::Damage | Intent::Mobility | Intent::Engage | Intent::Setup | Intent::AutoReset,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::Automatic,
        550.0f, 0.0f, 0.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::Targeted);
    p.Spells[0].Aim = AimPolicy::TargetPosition;
    p.Spells[0].Priority = 88;
    p.Spells[0].WeaveAfterAttack = true;
    p.Spells[0].DashDistance = 550.0f;

    p.Spells[1] = Spell(SDK::SpellSlot::W, "Seastone Trident", CastKind::Self,
        Intent::Damage | Intent::Buff | Intent::AutoReset | Intent::Setup,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::LastHit | Mode::Automatic,
        125.0f, 0.0f, 0.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::Targeted);
    p.Spells[1].Aim = AimPolicy::SelfPosition;
    p.Spells[1].Priority = 96;
    p.Spells[1].WeaveAfterAttack = true;
    p.Spells[1].RequiredTargetBuff = "FizzWActive";

    p.Spells[2] = Spell(SDK::SpellSlot::E, "Playful / Trickster", CastKind::Position,
        Intent::Damage | Intent::Mobility | Intent::Disengage | Intent::Engage |
            Intent::CrowdControl | Intent::Recast | Intent::AntiGapcloser,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::Automatic,
        400.0f, 0.25f, 330.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[2].Aim = AimPolicy::SafeCursor;
    p.Spells[2].Priority = 100;
    p.Spells[2].DashDistance = 400.0f;
    p.Spells[2].MaximumEnemiesAtDestination = 2;
    p.Spells[2].RequiredPlayerBuff = "FizzE";

    p.Spells[3] = Spell(SDK::SpellSlot::R, "Chum the Waters", CastKind::Position,
        Intent::Damage | Intent::CrowdControl | Intent::Engage | Intent::Execute |
            Intent::Finisher | Intent::Setup,
        Mode::Combo | Mode::Harass | Mode::Automatic,
        1300.0f, 0.25f, 110.0f, 1300.0f, true,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[3].Aim = AimPolicy::Prediction;
    p.Spells[3].Priority = 100;
    p.Spells[3].Collision = true;
    p.Spells[3].TargetHealthPercent = 72.0f;

    p.Trade = Plan("W reset Q trade",
        Step(SDK::SpellSlot::W, StepRule::RequireInsideAaRange | StepRule::AllowDuringWindup, 0, 650),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::AllowDuringWindup, 35, 700),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireSafePosition, 80, 900));
    p.AllIn = Plan("Fish Q W E all-in",
        Step(SDK::SpellSlot::R, StepRule::RequireTarget | StepRule::RequireTargetLow, 0, 1400),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::AllowDuringWindup, 80, 900),
        Step(SDK::SpellSlot::W, StepRule::RequireInsideAaRange | StepRule::AllowDuringWindup, 90, 650),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireSafePosition, 150, 1100));
    p.Flee = Plan("Untargetable Trickster escape",
        Step(SDK::SpellSlot::E, StepRule::RequireSafePosition | StepRule::AllowDuringWindup, 0, 700),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequirePlayerLow, 80, 850));
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
