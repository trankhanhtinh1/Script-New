#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Talon = [] {
    ChampionProfile p{};
    p.ChampionId = SDK::ChampionId::Talon;
    p.DisplayName = "Talon";
    p.InternalId = "champion.kuroaio.ai.talon";
    p.PrimaryArchetype = Archetype::Assassin;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::Recast | Mechanic::Dash | Mechanic::Execute |
                  Mechanic::Mark | Mechanic::Terrain |
                  Mechanic::WallInteraction | Mechanic::ObjectTracking |
                  Mechanic::ReturnProjectile | Mechanic::AutoWeave;
    p.Ultimate = UltimatePolicy::SingleTarget;
    p.PreferredCombatDistance = 450.0f;
    p.EngageHealthPercent = 42.0f;
    p.DefensiveHealthPercent = 28.0f;
    p.UltimateTargetHealthPercent = 60.0f;
    p.UltimateMinimumTargets = 1;
    p.MaximumCommitEnemies = 2;
    p.BaseHumanizerMs = 62;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "TalonPassiveBleed";
    p.MarkBuff = "TalonPassiveStack";
    p.TrackedObjectToken = "Talon";
    p.ThemeFrom = 0xFF5B4BFFu;
    p.ThemeTo = 0xFFE5B7FFu;
    p.TacticalSummary = "Blade assassin: build three Hemorrhage stacks, preserve W return blades, use terrain traversal only for a safe route, and return R blades before re-entering danger.";
    p.ResearchSummary = "Riot 26.15 / CommunityDragon 16.15 Summoner's Rift baseline; ARAM: Mayhem and arena modifiers excluded.";

    p.Spells[0] = Spell(SDK::SpellSlot::Q, "Noxian Diplomacy", CastKind::EnemyTarget,
        Intent::Damage | Intent::Engage | Intent::Execute | Intent::AutoReset,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle | Mode::LastHit | Mode::Flee | Mode::Automatic,
        575.0f, 0.0f, 0.0f, FLT_MAX, false, SDK::DamageType::Physical, SDK::SpellType::Targeted);
    p.Spells[0].Aim = AimPolicy::BetweenPlayerAndTarget;
    p.Spells[0].Priority = 90;
    p.Spells[0].WeaveAfterAttack = true;

    p.Spells[1] = Spell(SDK::SpellSlot::W, "Rake", CastKind::Line,
        Intent::Damage | Intent::CrowdControl | Intent::Setup | Intent::Waveclear | Intent::Jungle | Intent::LastHit,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle | Mode::LastHit | Mode::Automatic,
        650.0f, 0.25f, 75.0f, 2500.0f, true, SDK::DamageType::Physical, SDK::SpellType::SkillshotLine);
    p.Spells[1].Aim = AimPolicy::Prediction;
    p.Spells[1].Priority = 86;
    p.Spells[1].Hitchance = SDK::HitChance::High;
    p.Spells[1].HarassManaPercent = 40.0f;
    p.Spells[1].ClearManaPercent = 35.0f;

    p.Spells[2] = Spell(SDK::SpellSlot::E, "Assassin's Path", CastKind::Position,
        Intent::Mobility | Intent::Engage | Intent::Disengage | Intent::Setup,
        Mode::Combo | Mode::Flee | Mode::Automatic,
        725.0f, 0.0f, 0.0f, FLT_MAX, false, SDK::DamageType::Physical, SDK::SpellType::Targeted);
    p.Spells[2].Priority = 78;
    p.Spells[2].DashDistance = 725.0f;
    p.Spells[2].MaximumEnemiesAtDestination = 2;

    p.Spells[3] = Spell(SDK::SpellSlot::R, "Shadow Assault", CastKind::Self,
        Intent::Damage | Intent::Disengage | Intent::Engage | Intent::Execute,
        Mode::Combo | Mode::Flee | Mode::Automatic,
        550.0f, 0.0f, 100.0f, FLT_MAX, false, SDK::DamageType::Physical, SDK::SpellType::Targeted);
    p.Spells[3].Aim = AimPolicy::BestAoe;
    p.Spells[3].Priority = 96;
    p.Spells[3].MinimumAoeTargets = 1;
    p.Spells[3].TargetHealthPercent = 60.0f;

    p.Trade = Plan("Rake and Noxian short trade",
        Step(SDK::SpellSlot::W, StepRule::RequireTarget, 0, 700),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequireAfterAttack, 120, 900));
    p.AllIn = Plan("Terrain-assisted blade all-in",
        Step(SDK::SpellSlot::E, StepRule::RequireSafePosition, 0, 800),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget, 80, 900),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequireAfterAttack, 180, 1000),
        Step(SDK::SpellSlot::R, StepRule::RequireTarget | StepRule::RequireSafePosition, 260, 1300));
    p.Flee = Plan("Terrain and blade return escape",
        Step(SDK::SpellSlot::E, StepRule::RequireSafePosition, 0, 800),
        Step(SDK::SpellSlot::R, StepRule::RequireSafePosition, 120, 900));
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
