#pragma once

#include "../AIChampionProfile.h"

#include <cfloat>

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Nautilus = [] {
    ChampionProfile p{};
    p.ChampionId = SDK::ChampionId::Nautilus;
    p.DisplayName = "Nautilus";
    p.InternalId = "champion.kuroaio.ai.nautilus";
    p.PrimaryArchetype = Archetype::Vanguard;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::Mark | Mechanic::Dash | Mechanic::WallInteraction |
                  Mechanic::SpellShield | Mechanic::AutoWeave | Mechanic::DirectionalSweet;
    p.Ultimate = UltimatePolicy::MultiTarget;
    p.PreferredCombatDistance = 325.0f;
    p.EngageHealthPercent = 58.0f;
    p.DefensiveHealthPercent = 40.0f;
    p.UltimateTargetHealthPercent = 72.0f;
    p.UltimateMinimumTargets = 2;
    p.MaximumCommitEnemies = 3;
    p.BaseHumanizerMs = 58;
    p.PreferSelectedTarget = true;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "NautilusPassive";
    p.MarkBuff = "NautilusPassiveRoot";
    p.FormBuff = "NautilusW";
    p.ChannelBuff = "NautilusR";
    p.TacticalSummary =
        "Vanguard anchor: fire a collision-safe Dredge Line, preserve the first-hit root, "
        "shield through a selected threat, pulse Riptide, and reserve Depth Charge for a "
        "reachable priority target with safe channel and multi-target follow-through.";
    p.ResearchSummary =
        "Riot live 26.15 / CommunityDragon 16.15 Summoner's Rift values; Q terrain/target "
        "collision, passive root, W shield, E waves and R channel/target tracking reconcile events and polling.";

    p.Spells[0] = Spell(SDK::SpellSlot::Q, "Dredge Line", CastKind::Line,
        Intent::Damage | Intent::CrowdControl | Intent::Engage | Intent::Disengage |
            Intent::Setup | Intent::Peel,
        Mode::Combo | Mode::Harass | Mode::Jungle | Mode::Flee | Mode::Automatic,
        1100.0f, 0.25f, 90.0f, 2000.0f, true,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotLine);
    p.Spells[0].Aim = AimPolicy::Prediction;
    p.Spells[0].Priority = 100;
    p.Spells[0].Hitchance = SDK::HitChance::High;
    p.Spells[0].PreserveAutoAttack = true;

    p.Spells[1] = Spell(SDK::SpellSlot::W, "Titan's Wrath", CastKind::Self,
        Intent::Shield | Intent::Buff | Intent::Damage | Intent::Peel,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::Automatic,
        350.0f, 0.0f, 350.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[1].Aim = AimPolicy::SelfPosition;
    p.Spells[1].Priority = 92;
    p.Spells[1].PlayerHealthPercent = 72.0f;
    p.Spells[1].PreserveAutoAttack = true;

    p.Spells[2] = Spell(SDK::SpellSlot::E, "Riptide", CastKind::Self,
        Intent::Damage | Intent::CrowdControl | Intent::Peel | Intent::Waveclear |
            Intent::Jungle | Intent::LastHit | Intent::AntiGapcloser,
        AllModes, 600.0f, 0.25f, 350.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[2].Aim = AimPolicy::SelfPosition;
    p.Spells[2].Priority = 88;
    p.Spells[2].MinimumAoeTargets = 1;

    p.Spells[3] = Spell(SDK::SpellSlot::R, "Depth Charge", CastKind::EnemyTarget,
        Intent::Damage | Intent::CrowdControl | Intent::Engage | Intent::Peel |
            Intent::Interrupt | Intent::AntiGapcloser | Intent::Setup,
        Mode::Combo | Mode::Flee | Mode::Automatic,
        825.0f, 0.25f, 90.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::Targeted);
    p.Spells[3].Aim = AimPolicy::TargetPosition;
    p.Spells[3].Priority = 99;
    p.Spells[3].MinimumAoeTargets = 2;
    p.Spells[3].TargetHealthPercent = 78.0f;

    p.Trade = Plan("Anchor and root trade",
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequireSafePosition, 0, 1200),
        Step(SDK::SpellSlot::W, StepRule::RequirePlayerLow | StepRule::AllowDuringWindup, 100, 900),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::AllowDuringWindup, 160, 900));
    p.AllIn = Plan("Depth Charge anchor engage",
        Step(SDK::SpellSlot::R, StepRule::RequireTarget | StepRule::RequireMultiTarget, 0, 1300),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequireSafePosition, 110, 1300),
        Step(SDK::SpellSlot::W, StepRule::AllowDuringWindup, 220, 1100),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget, 300, 1050));
    p.Flee = Plan("Anchor peel and shield retreat",
        Step(SDK::SpellSlot::Q, StepRule::RequireSafePosition | StepRule::AllowDuringWindup, 0, 1100),
        Step(SDK::SpellSlot::W, StepRule::RequirePlayerLow | StepRule::AllowDuringWindup, 100, 900),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::AllowDuringWindup, 170, 800));
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
