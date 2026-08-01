#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Evelynn = [] {
    ChampionProfile p{};
    p.ChampionId = SDK::ChampionId::Evelynn;
    p.DisplayName = "Evelynn";
    p.InternalId = "champion.kuroaio.ai.evelynn";
    p.PrimaryArchetype = Archetype::Assassin;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::Recast | Mechanic::Dash | Mechanic::Blink |
                  Mechanic::Execute | Mechanic::Mark | Mechanic::MissingHealth |
                  Mechanic::WallInteraction | Mechanic::Terrain;
    p.Ultimate = UltimatePolicy::Execute;
    p.PreferredCombatDistance = 325.0f;
    p.EngageHealthPercent = 48.0f;
    p.DefensiveHealthPercent = 35.0f;
    p.UltimateTargetHealthPercent = 30.0f;
    p.UltimateMinimumTargets = 1;
    p.MaximumCommitEnemies = 2;
    p.BaseHumanizerMs = 55;
    p.PreferSelectedTarget = true;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "EvelynnPassiveDemonShade";
    p.MarkBuff = "EvelynnW";
    p.UltimateBuff = "EvelynnRThresholdTracker";
    p.TacticalSummary =
        "Stealth assassin: wait for Demon Shade after level six, prime Allure only when the charm can arm, mark with Hate Spike, enter on empowered Whiplash, and reserve Last Caress for an execute or a safe escape.";
    p.ResearchSummary =
        "Riot 26.15 / CommunityDragon 16.15 baseline from raw CommunityDragon Evelynn data: Q 800 missile plus 550 recasts, W 1200-1600 charm mark and 2.5 second arm, E 210 range entry with empowered 450 movement, and R 700 execute/teleport with a 30 percent threshold.";

    p.Spells[0] = Spell(SDK::SpellSlot::Q, "Hate Spike", CastKind::Direction,
        Intent::Damage | Intent::Setup | Intent::Recast | Intent::Finisher,
        AllModes, 800.0f, 0.25f, 60.0f, 2400.0f, true,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[0].Aim = AimPolicy::Prediction;
    p.Spells[0].Priority = 90;
    p.Spells[0].Hitchance = SDK::HitChance::High;
    p.Spells[0].RecastSpellName = "EvelynnQ2";
    p.Spells[0].WeaveAfterAttack = true;

    p.Spells[1] = Spell(SDK::SpellSlot::W, "Allure", CastKind::EnemyTarget,
        Intent::CrowdControl | Intent::Setup | Intent::Vision | Intent::Damage,
        Mode::Combo | Mode::Harass | Mode::Jungle | Mode::Automatic,
        1600.0f, 0.15f, 100.0f, 2400.0f, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[1].Aim = AimPolicy::TargetPosition;
    p.Spells[1].Priority = 78;
    p.Spells[1].RequiredPlayerBuff = "EvelynnPassiveDemonShade";
    p.Spells[1].RecastSpellName = "EvelynnWApplyMark";
    p.Spells[1].ChargeBuffName = "EvelynnW";
    p.Spells[1].ChargeDurationSeconds = 2.5f;

    p.Spells[2] = Spell(SDK::SpellSlot::E, "Whiplash", CastKind::EnemyTarget,
        Intent::Damage | Intent::Mobility | Intent::Engage | Intent::Disengage |
            Intent::AutoReset,
        Mode::Combo | Mode::Harass | Mode::Jungle | Mode::Flee | Mode::Automatic,
        210.0f, 0.25f, 200.0f, 902.0f, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[2].Aim = AimPolicy::TargetPosition;
    p.Spells[2].Priority = 95;
    p.Spells[2].RequiredTargetBuff = "EvelynnW";
    p.Spells[2].WeaveAfterAttack = true;
    p.Spells[2].PreserveAutoAttack = true;

    p.Spells[3] = Spell(SDK::SpellSlot::R, "Last Caress", CastKind::Position,
        Intent::Damage | Intent::Execute | Intent::Finisher | Intent::Disengage,
        Mode::Combo | Mode::Flee | Mode::Automatic,
        700.0f, 0.35f, 700.0f, 1300.0f, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[3].Aim = AimPolicy::AwayFromThreat;
    p.Spells[3].Priority = 100;
    p.Spells[3].TargetHealthPercent = 30.0f;
    p.Spells[3].PlayerHealthPercent = 35.0f;

    p.Trade = Plan("Allure charm trade",
        Step(SDK::SpellSlot::W, StepRule::RequireNoMark, 0, 2600),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequireMark, 2500, 900),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireMark, 2600, 800));
    p.AllIn = Plan("Demon Shade execution",
        Step(SDK::SpellSlot::W, StepRule::RequireNoMark, 0, 2600),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequireMark, 2500, 900),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireMark, 2600, 800),
        Step(SDK::SpellSlot::R, StepRule::RequireTargetLow | StepRule::HoldForExecute, 2750, 900));
    p.Flee = Plan("Last Caress escape",
        Step(SDK::SpellSlot::R, StepRule::RequireSafePosition | StepRule::RequirePlayerLow, 0, 500),
        Step(SDK::SpellSlot::W, StepRule::RequireSafePosition, 80, 650));
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
