#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile TahmKench = [] {
    ChampionProfile p{};
    p.ChampionId = SDK::ChampionId::TahmKench;
    p.DisplayName = "Tahm Kench";
    p.InternalId = "champion.kuroaio.ai.tahmkench";
    p.PrimaryArchetype = Archetype::Vanguard;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::Stack | Mechanic::Channel | Mechanic::AllyTarget |
                  Mechanic::Recast | Mechanic::MissingHealth | Mechanic::Tether;
    p.Ultimate = UltimatePolicy::SaveAlly;
    p.PreferredCombatDistance = 500.0f;
    p.EngageHealthPercent = 62.0f;
    p.DefensiveHealthPercent = 42.0f;
    p.UltimateTargetHealthPercent = 35.0f;
    p.UltimateMinimumTargets = 1;
    p.MaximumCommitEnemies = 2;
    p.BaseHumanizerMs = 60;
    p.PreferSelectedTarget = true;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "TahmKenchPassive";
    p.MarkBuff = "TahmKenchQSlow";
    p.ChannelBuff = "TahmKenchW";
    p.UltimateBuff = "TahmKenchR";
    p.TacticalSummary =
        "Stack Acquired Taste with attacks and Tongue Lash, use Abyssal Dive only through a predicted safe endpoint, convert grey health with Thick Skin, and reserve Devour for a lethal isolated enemy or a threatened ally.";
    p.ResearchSummary =
        "Riot live patch 26.15 with CommunityDragon 16.15 TahmKench.bin.json values and spell-state names recorded in AI/Research/AITahmKench.md.";

    p.Spells[0] = Spell(SDK::SpellSlot::Q, "Tongue Lash", CastKind::Position,
        Intent::Damage | Intent::CrowdControl | Intent::Setup | Intent::Heal,
        AllModes, 900.0f, 0.25f, 70.0f, 2000.0f, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[0].Aim = AimPolicy::Prediction;
    p.Spells[0].Priority = 92;
    p.Spells[0].Hitchance = SDK::HitChance::High;
    p.Spells[0].PreserveAutoAttack = true;

    p.Spells[1] = Spell(SDK::SpellSlot::W, "Abyssal Dive", CastKind::Position,
        Intent::Damage | Intent::CrowdControl | Intent::Mobility | Intent::Engage |
            Intent::Disengage | Intent::Peel | Intent::Setup | Intent::Channel,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::Automatic,
        1200.0f, 1.35f, 250.0f, 1200.0f, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[1].Aim = AimPolicy::SafeCursor;
    p.Spells[1].Priority = 96;
    p.Spells[1].MaximumEnemiesAtDestination = 2;
    p.Spells[1].PreserveAutoAttack = true;

    p.Spells[2] = Spell(SDK::SpellSlot::E, "Thick Skin", CastKind::Self,
        Intent::Shield | Intent::Heal | Intent::Buff | Intent::Peel,
        AllModes, 2400.0f, 0.25f, 0.0f, FLT_MAX, false,
        SDK::DamageType::True, SDK::SpellType::SkillshotCircle);
    p.Spells[2].Aim = AimPolicy::SelfPosition;
    p.Spells[2].Priority = 94;
    p.Spells[2].PlayerHealthPercent = 72.0f;
    p.Spells[2].PreserveAutoAttack = true;

    p.Spells[3] = Spell(SDK::SpellSlot::R, "Devour", CastKind::AnyTarget,
        Intent::Damage | Intent::Shield | Intent::AllyUtility | Intent::Buff |
            Intent::Channel | Intent::Recast | Intent::Peel | Intent::Execute,
        Mode::Combo | Mode::Flee | Mode::Automatic,
        300.0f, 0.25f, 120.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::Targeted);
    p.Spells[3].Aim = AimPolicy::Prediction;
    p.Spells[3].Priority = 100;
    p.Spells[3].MinimumAoeTargets = 1;
    p.Spells[3].PlayerHealthPercent = 55.0f;
    p.Spells[3].PreserveAutoAttack = true;

    p.Trade = Plan("Tongue and stack trade",
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget, 0, 900),
        Step(SDK::SpellSlot::E, StepRule::RequirePlayerLow | StepRule::AllowDuringWindup, 120, 900));
    p.AllIn = Plan("Safe dive into Devour",
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget, 0, 900),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget | StepRule::RequireSafePosition, 120, 1500),
        Step(SDK::SpellSlot::R, StepRule::RequireTarget | StepRule::RequireMark, 280, 1700));
    p.Flee = Plan("Grey health rescue",
        Step(SDK::SpellSlot::E, StepRule::RequirePlayerLow | StepRule::AllowDuringWindup, 0, 700),
        Step(SDK::SpellSlot::W, StepRule::RequireSafePosition, 80, 1000),
        Step(SDK::SpellSlot::R, StepRule::RequireTarget | StepRule::ManualAssistOnly, 140, 1200));
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
