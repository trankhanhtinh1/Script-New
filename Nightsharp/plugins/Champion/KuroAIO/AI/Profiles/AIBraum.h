#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Braum = [] {
    ChampionProfile p{};
    p.ChampionName = "Braum";
    p.DisplayName = "Braum";
    p.InternalId = "champion.kuroaio.ai.braum";
    p.PrimaryArchetype = Archetype::Vanguard;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::Stack | Mechanic::Mark | Mechanic::AllyTarget |
                  Mechanic::Dash | Mechanic::WallInteraction | Mechanic::SpellShield |
                  Mechanic::AutoWeave | Mechanic::DirectionalSweet;
    p.Ultimate = UltimatePolicy::Defensive;
    p.PreferredCombatDistance = 375.0f;
    p.EngageHealthPercent = 58.0f;
    p.DefensiveHealthPercent = 42.0f;
    p.UltimateTargetHealthPercent = 68.0f;
    p.UltimateMinimumTargets = 2;
    p.MaximumCommitEnemies = 3;
    p.BaseHumanizerMs = 48;
    p.PreferSelectedTarget = true;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "BraumPassive";
    p.MarkBuff = "BraumMark";
    p.FormBuff = "BraumEShield";
    p.UltimateBuff = "BraumR";
    p.TacticalSummary =
        "Vanguard support that tracks Concussive Blows per enemy, dashes to safe allies, "
        "blocks incoming projectiles with Unbreakable and fissures selected threats.";
    p.ResearchSummary =
        "Riot 26.15 and CommunityDragon PC 16.15 data model a four-hit per-target passive, "
        "1050 Q projectile, 650 W ally dash/resist, E projectile interception and 1200 R fissure.";

    p.Spells[0] = Spell(SDK::SpellSlot::Q, "Winter's Bite", CastKind::Line,
        Intent::Damage | Intent::CrowdControl | Intent::Engage | Intent::Peel |
            Intent::Disengage | Intent::Setup | Intent::Mark,
        Mode::Combo | Mode::Harass | Mode::Jungle | Mode::Flee | Mode::Automatic,
        1050.0f, 0.25f, 60.0f, 1700.0f, true,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[0].Aim = AimPolicy::Prediction;
    p.Spells[0].Priority = 98;
    p.Spells[0].WeaveAfterAttack = true;
    p.Spells[0].PreserveAutoAttack = true;
    p.Spells[0].HarassManaPercent = 56.0f;

    p.Spells[1] = Spell(SDK::SpellSlot::W, "Stand Behind Me", CastKind::AllyTarget,
        Intent::Mobility | Intent::Buff | Intent::AllyUtility | Intent::Disengage |
            Intent::Peel | Intent::Setup,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::Automatic,
        650.0f, 0.25f, 350.0f, FLT_MAX, false,
        SDK::DamageType::True, SDK::SpellType::Targeted);
    p.Spells[1].Aim = AimPolicy::SafeCursor;
    p.Spells[1].Priority = 100;
    p.Spells[1].PlayerHealthPercent = 60.0f;

    p.Spells[2] = Spell(SDK::SpellSlot::E, "Unbreakable", CastKind::Self,
        Intent::Shield | Intent::SpellShield | Intent::Peel | Intent::Disengage |
            Intent::AntiGapcloser | Intent::AllyUtility,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::Automatic,
        350.0f, 0.10f, 115.0f, FLT_MAX, false,
        SDK::DamageType::True, SDK::SpellType::SkillshotLine);
    p.Spells[2].Aim = AimPolicy::AwayFromThreat;
    p.Spells[2].Priority = 99;
    p.Spells[2].PlayerHealthPercent = 72.0f;

    p.Spells[3] = Spell(SDK::SpellSlot::R, "Glacial Fissure", CastKind::Line,
        Intent::Damage | Intent::CrowdControl | Intent::Engage | Intent::Disengage |
            Intent::Peel | Intent::Interrupt | Intent::AntiGapcloser | Intent::Setup,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::Automatic,
        1200.0f, 0.25f, 115.0f, 1400.0f, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[3].Aim = AimPolicy::Prediction;
    p.Spells[3].Priority = 97;
    p.Spells[3].MinimumAoeTargets = 2;
    p.Spells[3].MaximumEnemiesAtDestination = 3;
    p.Spells[3].PlayerHealthPercent = 68.0f;

    p.Trade = Plan("mark selected target with Q and preserve E for incoming projectiles",
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequireSafePosition, 0, 1000),
        Step(SDK::SpellSlot::W, StepRule::RequirePlayerLow | StepRule::RequireSafePosition, 120, 1200),
        Step(SDK::SpellSlot::E, StepRule::RequireNoCrowdControl | StepRule::AllowDuringWindup, 180, 1300));
    p.AllIn = Plan("stack passive, dash to safe ally, shield the line and fissure the selected threat",
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequireSafePosition, 0, 1000),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget | StepRule::RequireSafePosition, 100, 1200),
        Step(SDK::SpellSlot::E, StepRule::RequireSafePosition | StepRule::AllowDuringWindup, 150, 1250),
        Step(SDK::SpellSlot::R, StepRule::RequireTarget | StepRule::RequireMultiTarget | StepRule::RequireSafePosition, 220, 1400));
    p.Flee = Plan("dash toward an ally, turn Unbreakable into a projectile wall and fissure pursuers",
        Step(SDK::SpellSlot::W, StepRule::RequireTarget | StepRule::RequireSafePosition, 0, 1100),
        Step(SDK::SpellSlot::E, StepRule::RequireSafePosition | StepRule::AllowDuringWindup, 80, 1100),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::AllowDuringWindup, 160, 1200),
        Step(SDK::SpellSlot::R, StepRule::RequireSafePosition, 220, 1400));
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
