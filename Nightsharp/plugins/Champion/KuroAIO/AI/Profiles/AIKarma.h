#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Karma = [] {
    ChampionProfile p{};
    p.ChampionId = SDK::ChampionId::Karma;
    p.DisplayName = "Karma";
    p.InternalId = "champion.kuroaio.ai.karma";
    p.PrimaryArchetype = Archetype::Enchanter;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::Recast | Mechanic::Tether | Mechanic::AllyTarget |
                  Mechanic::AutoWeave;
    p.Ultimate = UltimatePolicy::RecastControl;
    p.PreferredCombatDistance = 700.0f;
    p.EngageHealthPercent = 58.0f;
    p.DefensiveHealthPercent = 42.0f;
    p.UltimateTargetHealthPercent = 60.0f;
    p.UltimateMinimumTargets = 2;
    p.MaximumCommitEnemies = 3;
    p.BaseHumanizerMs = 55;
    p.PreferSelectedTarget = true;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "KarmaPassive";
    p.MarkBuff = "KarmaMantra";
    p.ChannelBuff = "KarmaSpiritBind";
    p.UltimateBuff = "KarmaMantra";
    p.TacticalSummary =
        "Mantra-aware enchanter: predict Soulflare, hold Spirit Bind through its tether, choose RQ/RW/RE by lethal and peel posture, and shield the threatened ally with speed.";
    p.ResearchSummary =
        "Riot 26.15 / CommunityDragon 16.15 Karma Q/W/E/R data; Mantra state, tether lifecycle and ally shield posture are reconciled from spell, buff and polling observations.";

    p.Spells[0] = Spell(SDK::SpellSlot::Q, "Inner Flame", CastKind::Position,
        Intent::Damage | Intent::CrowdControl | Intent::Waveclear |
            Intent::Jungle | Intent::LastHit,
        AllModes, 950.0f, 0.25f, 90.0f, 902.0f, true,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[0].Aim = AimPolicy::Prediction;
    p.Spells[0].Priority = 86;
    p.Spells[0].PreserveAutoAttack = true;

    p.Spells[1] = Spell(SDK::SpellSlot::W, "Focused Resolve", CastKind::EnemyTarget,
        Intent::Damage | Intent::CrowdControl | Intent::Peel |
            Intent::Interrupt | Intent::AntiGapcloser,
        Mode::Combo | Mode::Harass | Mode::Automatic | Mode::Flee,
        675.0f, 0.25f, 60.0f, 2200.0f, false,
        SDK::DamageType::Magical, SDK::SpellType::Targeted);
    p.Spells[1].Priority = 94;
    p.Spells[1].PreserveAutoAttack = true;

    p.Spells[2] = Spell(SDK::SpellSlot::E, "Inspire", CastKind::AllyTarget,
        Intent::Shield | Intent::Buff | Intent::Mobility | Intent::Disengage |
            Intent::Peel | Intent::AllyUtility,
        AllModes, 600.0f, 0.25f, 100.0f, 20.0f, false,
        SDK::DamageType::Magical, SDK::SpellType::Targeted);
    p.Spells[2].Aim = AimPolicy::TargetPosition;
    p.Spells[2].Priority = 98;
    p.Spells[2].MinimumAoeTargets = 1;
    p.Spells[2].MaximumEnemiesAtDestination = 3;

    p.Spells[3] = Spell(SDK::SpellSlot::R, "Mantra", CastKind::Self,
        Intent::Recast | Intent::Damage | Intent::Shield | Intent::Heal |
            Intent::Buff | Intent::AllyUtility | Intent::Setup,
        AllModes, 0.0f, 0.25f, 0.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::Targeted);
    p.Spells[3].Priority = 100;

    p.Trade = Plan("Q tether trade",
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget, 0, 850),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget | StepRule::AllowDuringWindup, 100, 950),
        Step(SDK::SpellSlot::E, StepRule::RequireSafePosition | StepRule::AllowDuringWindup, 220, 900));
    p.AllIn = Plan("Mantra posture and tether",
        Step(SDK::SpellSlot::R, StepRule::RequireTarget, 0, 450),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget, 80, 950),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget | StepRule::RequireCrowdControl, 180, 1100),
        Step(SDK::SpellSlot::E, StepRule::RequireSafePosition, 280, 950));
    p.Flee = Plan("RE shield-speed peel",
        Step(SDK::SpellSlot::E, StepRule::RequireSafePosition | StepRule::AllowDuringWindup, 0, 850),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget | StepRule::AllowDuringWindup, 90, 900),
        Step(SDK::SpellSlot::R, StepRule::ManualAssistOnly | StepRule::RequireSafePosition, 180, 500));
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
