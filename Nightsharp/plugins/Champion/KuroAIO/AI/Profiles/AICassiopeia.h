#pragma once
#include "../AIChampionProfile.h"
namespace Plugins::KuroAIO::AI::Profiles {
inline constexpr ChampionProfile Cassiopeia = [] {
    ChampionProfile p{};
    p.ChampionName = "Cassiopeia";
    p.DisplayName = "Cassiopeia";
    p.InternalId = "champion.kuroaio.ai.cassiopeia";
    p.PrimaryArchetype = Archetype::Battlemage;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::Stack | Mechanic::ObjectTracking | Mechanic::Mark |
                  Mechanic::MissingHealth | Mechanic::DirectionalSweet;
    p.Ultimate = UltimatePolicy::MultiTarget;
    p.PreferredCombatDistance = 650.0f;
    p.EngageHealthPercent = 48.0f;
    p.DefensiveHealthPercent = 32.0f;
    p.UltimateTargetHealthPercent = 45.0f;
    p.UltimateMinimumTargets = 2;
    p.MaximumCommitEnemies = 3;
    p.BaseHumanizerMs = 65;
    p.PreferSelectedTarget = true;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "CassiopeiaPassive";
    p.MarkBuff = "CassiopeiaE";
    p.TrackedObjectToken = "Cassiopeia";
    p.ThemeFrom = 0xFF6B48D8u;
    p.ThemeTo = 0xFF3AD6C4u;
    p.TacticalSummary = "Poison-state battlemage: maintain poison for Twin Fang resets, use Miasma to deny movement, and reserve Petrifying Gaze for a verified directional stun line.";
    p.ResearchSummary = "Riot 26.15 / CommunityDragon 16.15; Summoner's Rift values only and no ARAM Mayhem modifiers.";
    p.Spells[0] = Spell(SDK::SpellSlot::Q, "Noxious Blast", CastKind::Position,
        Intent::Damage | Intent::Setup | Intent::Waveclear | Intent::Jungle,
        AllModes, 850.0f, 0.75f, 160.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[0].Aim = AimPolicy::Prediction; p.Spells[0].Priority = 83; p.Spells[0].Hitchance = SDK::HitChance::High;
    p.Spells[1] = Spell(SDK::SpellSlot::W, "Miasma", CastKind::Position,
        Intent::Damage | Intent::CrowdControl | Intent::Disengage | Intent::Setup | Intent::Waveclear,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle | Mode::Flee | Mode::Automatic,
        700.0f, 0.0f, 160.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[1].Aim = AimPolicy::BestAoe; p.Spells[1].Priority = 90;
    p.Spells[2] = Spell(SDK::SpellSlot::E, "Twin Fang", CastKind::EnemyTarget,
        Intent::Damage | Intent::Execute | Intent::AutoReset | Intent::LastHit,
        AllModes, 700.0f, 0.0f, 0.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::Targeted);
    p.Spells[2].Aim = AimPolicy::TargetPosition; p.Spells[2].Priority = 96; p.Spells[2].WeaveAfterAttack = true;
    p.Spells[3] = Spell(SDK::SpellSlot::R, "Petrifying Gaze", CastKind::Cone,
        Intent::Damage | Intent::CrowdControl | Intent::Peel | Intent::Execute | Intent::Interrupt,
        Mode::Combo | Mode::Flee | Mode::Automatic,
        825.0f, 0.50f, 80.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::Cone);
    p.Spells[3].Aim = AimPolicy::BestAoe; p.Spells[3].Priority = 98; p.Spells[3].MinimumAoeTargets = 2;
    p.Trade = Plan("Poison Twin Fang trade", Step(SDK::SpellSlot::Q, StepRule::RequireSafePosition, 0, 800), Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireAfterAttack, 100, 1000));
    p.AllIn = Plan("Miasma poison all-in", Step(SDK::SpellSlot::W, StepRule::RequireSafePosition, 0, 800), Step(SDK::SpellSlot::Q, StepRule::RequireTarget, 80, 950), Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireAfterAttack, 150, 1200), Step(SDK::SpellSlot::R, StepRule::RequireMultiTarget, 300, 1500));
    p.Flee = Plan("Miasma peel", Step(SDK::SpellSlot::W, StepRule::RequireSafePosition, 0, 800), Step(SDK::SpellSlot::R, StepRule::RequireSafePosition | StepRule::ManualAssistOnly, 150, 1100));
    return p;
}();
}
