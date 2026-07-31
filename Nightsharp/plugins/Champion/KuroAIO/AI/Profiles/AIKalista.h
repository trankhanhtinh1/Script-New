#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Kalista = [] {
    ChampionProfile p{};
    p.ChampionName = "Kalista";
    p.DisplayName = "Kalista";
    p.InternalId = "champion.kuroaio.ai.kalista";
    p.PrimaryArchetype = Archetype::Marksman;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::Stack | Mechanic::Execute | Mechanic::AllyTarget |
                  Mechanic::AutoWeave | Mechanic::Mark | Mechanic::AutoReset;
    p.Ultimate = UltimatePolicy::SaveAlly;

    p.Spells[0] = Spell(
        SDK::SpellSlot::Q, "Pierce", CastKind::Line,
        Intent::Damage | Intent::CrowdControl | Intent::Setup |
            Intent::LastHit | Intent::Jungle | Intent::Objective,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::LastHit | Mode::Flee | Mode::Automatic,
        1150.0f, 0.25f, 60.0f, 2400.0f, true,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotLine);
    p.Spells[0].Priority = 88;
    p.Spells[0].Hitchance = SDK::HitChance::High;
    p.Spells[0].PreserveAutoAttack = true;
    p.Spells[0].AllowOnMinions = true;
    p.Spells[0].HarassManaPercent = 52.0f;
    p.Spells[0].ClearManaPercent = 48.0f;

    p.Spells[1] = Spell(
        SDK::SpellSlot::W, "Sentinel", CastKind::Line,
        Intent::Vision | Intent::Setup | Intent::Damage,
        Mode::Combo | Mode::Automatic,
        5000.0f, 0.25f, 80.0f, FLT_MAX, false,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotLine);
    p.Spells[1].Priority = 45;
    p.Spells[1].PreserveAutoAttack = true;

    p.Spells[2] = Spell(
        SDK::SpellSlot::E, "Rend", CastKind::Self,
        Intent::Damage | Intent::Execute | Intent::Finisher |
            Intent::Jungle | Intent::Objective | Intent::LastHit | Intent::AutoReset,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::LastHit | Mode::Flee | Mode::Automatic,
        1000.0f, 0.0f, 0.0f, FLT_MAX, false,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotCircle);
    p.Spells[2].Priority = 100;
    p.Spells[2].PreserveAutoAttack = true;
    p.Spells[2].AllowOnMinions = true;
    p.Spells[2].HarassManaPercent = 32.0f;
    p.Spells[2].ClearManaPercent = 30.0f;

    p.Spells[3] = Spell(
        SDK::SpellSlot::R, "Fate's Call", CastKind::AllyTarget,
        Intent::AllyUtility | Intent::Shield | Intent::Engage |
            Intent::Disengage | Intent::Peel | Intent::AntiGapcloser,
        Mode::Combo | Mode::Flee | Mode::Automatic,
        1400.0f, 0.25f, 120.0f, FLT_MAX, false,
        SDK::DamageType::True, SDK::SpellType::SkillshotCircle);
    p.Spells[3].Priority = 99;
    p.Spells[3].Hitchance = SDK::HitChance::VeryHigh;
    p.Spells[3].PreserveAutoAttack = true;

    p.Trade = Plan(
        "Build spears with attacks, then Rend only on lethal, escape or expiry",
        Step(SDK::SpellSlot::Q,
             StepRule::RequireTarget | StepRule::RequireOutsideAaRange),
        Step(SDK::SpellSlot::E,
             StepRule::RequireTarget | StepRule::RequireMark | StepRule::HoldForExecute));
    p.AllIn = Plan(
        "Anchor target, weave Pierce between hops, and reserve Rend for a verified threshold",
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget),
        Step(SDK::SpellSlot::E,
             StepRule::RequireTarget | StepRule::RequireMark | StepRule::HoldForExecute),
        Step(SDK::SpellSlot::R, StepRule::RequireTarget | StepRule::AllowDuringWindup));
    p.Flee = Plan(
        "Rend the committed pursuer and call the Oathsworn ally only for a verified save",
        Step(SDK::SpellSlot::E,
             StepRule::RequireTarget | StepRule::RequireMark | StepRule::AllowDuringWindup),
        Step(SDK::SpellSlot::R,
             StepRule::RequireTarget | StepRule::RequirePlayerLow | StepRule::AllowDuringWindup));

    p.PreferredCombatDistance = 525.0f;
    p.EngageHealthPercent = 58.0f;
    p.DefensiveHealthPercent = 38.0f;
    p.UltimateTargetHealthPercent = 35.0f;
    p.UltimateMinimumTargets = 1;
    p.MaximumCommitEnemies = 2;
    p.BaseHumanizerMs = 24;
    p.PreferSelectedTarget = true;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "KalistaPassive";
    p.MarkBuff = "kalistaexpungemarker";
    p.UltimateBuff = "kalistacoopstrikeally";
    p.ThemeFrom = 0xFFB7E7FFu;
    p.ThemeTo = 0xFF5846D8u;
    p.ThemeSpeed = 1.08f;
    p.TacticalSummary =
        "Own the reachable spear target, preserve the current attack/windup, hop "
        "toward safe cursor space after confirmed attacks, and Rend only when "
        "the live stack damage is lethal, expiring, escaping or an objective is "
        "conservatively secured. Fate's Call is an Oathsworn save first and a "
        "manual/verified engage second.";
    p.ResearchSummary =
        "Riot 26.15 / CommunityDragon 16.15 Kalista: Martial Poise movement "
        "intent, Pierce line prediction/collision, Sentinel mark vocabulary, "
        "Expunge marker stack formula and 50% epic-monster modifier, plus "
        "Oathsworn Fate's Call save policy. Runtime aliases remain reconciled "
        "through polling because buff/object telemetry is not stable across clients.";
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
