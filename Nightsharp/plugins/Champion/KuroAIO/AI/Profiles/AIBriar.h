#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {
inline constexpr ChampionProfile Briar = [] {
    ChampionProfile p{};
    p.ChampionName = "Briar";
    p.DisplayName = "Briar";
    p.InternalId = "champion.kuroaio.ai.briar";
    p.PrimaryArchetype = Archetype::Diver;
    p.Resource = ResourceModel::None;
    p.Mechanics = Mechanic::Recast | Mechanic::Charge | Mechanic::Global | Mechanic::MissingHealth |
                  Mechanic::AutoWeave | Mechanic::Stack;
    p.Ultimate = UltimatePolicy::GlobalExecute;
    p.PreferredCombatDistance = 275.0f;
    p.EngageHealthPercent = 48.0f;
    p.DefensiveHealthPercent = 34.0f;
    p.UltimateTargetHealthPercent = 55.0f;
    p.UltimateMinimumTargets = 1;
    p.MaximumCommitEnemies = 2;
    p.BaseHumanizerMs = 55;
    p.PreferSelectedTarget = true;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "BriarPassive";
    p.FormBuff = "BriarW/BriarE";
    p.UltimateBuff = "BriarR";
    p.TrackedObjectToken = "BriarR";
    p.TacticalSummary = "Blood-cursed diver: Head Rush stuns, Blood Frenzy forces a target and Snack Attack heals, Chilling Scream mitigates and knocks back, while Certain Death marks a safe global engage.";
    p.ResearchSummary = "Riot 26.15 / CommunityDragon 16.15 data models health-based Briar spell gates, W recast healing, E charge reduction/knockback and global R berserk safety.";

    p.Spells[0] = Spell(SDK::SpellSlot::Q, "Head Rush", CastKind::EnemyTarget,
        Intent::Damage | Intent::CrowdControl | Intent::Engage | Intent::AutoReset,
        AllModes, 475.0f, 0.15f, 0.0f, FLT_MAX, false,
        SDK::DamageType::Physical, SDK::SpellType::Targeted);
    p.Spells[0].Priority = 92;
    p.Spells[0].WeaveAfterAttack = true;
    p.Spells[0].PreserveAutoAttack = true;

    p.Spells[1] = Spell(SDK::SpellSlot::W, "Blood Frenzy", CastKind::EnemyTarget,
        Intent::Damage | Intent::Mobility | Intent::Buff | Intent::Recast | Intent::Heal | Intent::AutoReset | Intent::Jungle,
        AllModes, 350.0f, 0.10f, 0.0f, FLT_MAX, false,
        SDK::DamageType::Physical, SDK::SpellType::Targeted);
    p.Spells[1].Priority = 96;
    p.Spells[1].PreserveAutoAttack = false;
    p.Spells[1].RecastSpellName = "BriarW2";
    p.Spells[1].RequiredPlayerBuff = "BriarW";

    p.Spells[2] = Spell(SDK::SpellSlot::E, "Chilling Scream", CastKind::Self,
        Intent::Damage | Intent::CrowdControl | Intent::Disengage | Intent::Recast | Intent::Channel | Intent::Heal | Intent::AntiGapcloser,
        AllModes, 600.0f, 0.25f, 220.0f, FLT_MAX, false,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotCone);
    p.Spells[2].Priority = 90;
    p.Spells[2].PreserveAutoAttack = false;
    p.Spells[2].ChargeBuffName = "BriarE";
    p.Spells[2].ChargeMinRange = 400;
    p.Spells[2].ChargeMaxRange = 600;
    p.Spells[2].ChargeDurationSeconds = 1.0f;

    p.Spells[3] = Spell(SDK::SpellSlot::R, "Certain Death", CastKind::Line,
        Intent::Damage | Intent::CrowdControl | Intent::Engage | Intent::Buff | Intent::Execute | Intent::Setup,
        AllModes, 12000.0f, 0.40f, 160.0f, 2200.0f, true,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotLine);
    p.Spells[3].Aim = AimPolicy::Prediction;
    p.Spells[3].Priority = 88;
    p.Spells[3].MaximumEnemiesAtDestination = 2;

    p.Trade = Plan("head rush into controlled frenzy and scream release",
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequireAfterAttack, 0, 700),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget | StepRule::RequireSafePosition, 80, 1100),
        Step(SDK::SpellSlot::W, StepRule::RequireRecast | StepRule::RequirePlayerLow, 450, 2200),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::AllowDuringWindup, 600, 2400));
    p.AllIn = Plan("certain death mark followed by frenzy and stun peel",
        Step(SDK::SpellSlot::R, StepRule::RequireTarget | StepRule::RequireSafePosition, 0, 1200),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget | StepRule::RequireSafePosition, 180, 1500),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequireAfterAttack, 320, 1800),
        Step(SDK::SpellSlot::W, StepRule::RequireRecast | StepRule::RequirePlayerLow, 700, 3000),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireSafePosition, 900, 3400));
    p.Flee = Plan("scream damage reduction and safe disengage",
        Step(SDK::SpellSlot::E, StepRule::RequireNoCrowdControl | StepRule::AllowDuringWindup, 0, 1800),
        Step(SDK::SpellSlot::W, StepRule::RequireRecast | StepRule::RequirePlayerLow, 300, 1900),
        Step(SDK::SpellSlot::R, StepRule::RequireTarget | StepRule::RequireSafePosition, 450, 1300));
    return p;
}();
} // namespace Plugins::KuroAIO::AI::Profiles
