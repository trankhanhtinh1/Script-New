#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile KhaZix = [] {
    ChampionProfile p{};
    p.ChampionId = SDK::ChampionId::KhaZix;
    p.DisplayName = "Kha'Zix";
    p.InternalId = "champion.kuroaio.ai.khazix";
    p.PrimaryArchetype = Archetype::Assassin;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::Dash | Mechanic::Recast | Mechanic::Evolve |
                  Mechanic::Execute | Mechanic::WallInteraction |
                  Mechanic::Terrain;
    p.Ultimate = UltimatePolicy::RecastControl;
    p.PreferredCombatDistance = 275.0f;
    p.EngageHealthPercent = 55.0f;
    p.DefensiveHealthPercent = 35.0f;
    p.UltimateTargetHealthPercent = 45.0f;
    p.UltimateMinimumTargets = 1;
    p.MaximumCommitEnemies = 2;
    p.BaseHumanizerMs = 55;
    p.PreferSelectedTarget = true;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "KhazixPassive";
    p.UltimateBuff = "KhazixRStealth";
    p.TacticalSummary =
        "Isolation assassin: evolve the live kit, reserve Taste Their Fear for isolated prey, use Void Spike to establish contact, leap only to safe endpoints, and spend Void Assault as a bounded stealth/recast escape or isolated-target setup.";
    p.ResearchSummary =
        "Riot 26.15 / CommunityDragon 16.15 baseline: Q 325 melee isolation strike, W 1000 spike projectiles, E 700 leap, and R 1.25 second stealth with evolved recasts.";

    p.Spells[0] = Spell(SDK::SpellSlot::Q, "Taste Their Fear", CastKind::EnemyTarget,
        Intent::Damage | Intent::Execute | Intent::AutoReset,
        Mode::Combo | Mode::Harass | Mode::Jungle | Mode::LastHit | Mode::Automatic,
        325.0f, 0.0f, 0.0f, FLT_MAX, false,
        SDK::DamageType::Physical, SDK::SpellType::Targeted);
    p.Spells[0].Aim = AimPolicy::TargetPosition;
    p.Spells[0].Priority = 100;
    p.Spells[0].WeaveAfterAttack = true;
    p.Spells[0].PreserveAutoAttack = true;

    p.Spells[1] = Spell(SDK::SpellSlot::W, "Void Spike", CastKind::Position,
        Intent::Damage | Intent::CrowdControl | Intent::Heal | Intent::Setup,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle,
        1000.0f, 0.25f, 70.0f, 1700.0f, true,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotLine);
    p.Spells[1].Aim = AimPolicy::Prediction;
    p.Spells[1].Priority = 82;
    p.Spells[1].Hitchance = SDK::HitChance::High;
    p.Spells[1].WeaveAfterAttack = true;

    p.Spells[2] = Spell(SDK::SpellSlot::E, "Leap", CastKind::Position,
        Intent::Damage | Intent::Mobility | Intent::Engage | Intent::Disengage,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::Automatic,
        700.0f, 0.0f, 250.0f, FLT_MAX, false,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotCircle);
    p.Spells[2].Aim = AimPolicy::TargetPosition;
    p.Spells[2].Priority = 94;
    p.Spells[2].PreserveAutoAttack = true;

    p.Spells[3] = Spell(SDK::SpellSlot::R, "Void Assault", CastKind::Self,
        Intent::Buff | Intent::Recast | Intent::Disengage | Intent::Engage,
        Mode::Combo | Mode::Flee | Mode::Automatic,
        0.0f, 0.0f, 0.0f, FLT_MAX, false,
        SDK::DamageType::Physical, SDK::SpellType::Targeted);
    p.Spells[3].Aim = AimPolicy::SelfPosition;
    p.Spells[3].Priority = 96;
    p.Spells[3].RecastSpellName = "KhazixR";

    p.Trade = Plan("Isolation trade",
        Step(SDK::SpellSlot::W, StepRule::RequireTarget, 0, 1050),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget, 250, 350));
    p.AllIn = Plan("Evolved leap execution",
        Step(SDK::SpellSlot::W, StepRule::RequireTarget, 0, 1050),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget, 250, 350),
        Step(SDK::SpellSlot::E, StepRule::RequireSafePosition, 350, 750),
        Step(SDK::SpellSlot::R, StepRule::RequireSafePosition, 400, 0));
    p.Flee = Plan("Void Assault escape",
        Step(SDK::SpellSlot::R, StepRule::RequireSafePosition, 0, 0),
        Step(SDK::SpellSlot::E, StepRule::RequireSafePosition, 150, 750));
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
