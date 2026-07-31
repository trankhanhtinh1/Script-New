#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Zilean = [] {
    ChampionProfile p{};
    p.ChampionName = "Zilean";
    p.DisplayName = "Zilean";
    p.InternalId = "champion.kuroaio.ai.zilean";
    p.PrimaryArchetype = Archetype::Enchanter;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::Recast | Mechanic::AllyTarget | Mechanic::Revive |
                  Mechanic::ObjectTracking | Mechanic::AutoWeave;
    p.Ultimate = UltimatePolicy::SaveAlly;
    p.PreferredCombatDistance = 650.0f;
    p.EngageHealthPercent = 42.0f;
    p.DefensiveHealthPercent = 34.0f;
    p.UltimateTargetHealthPercent = 32.0f;
    p.UltimateMinimumTargets = 1;
    p.MaximumCommitEnemies = 2;
    p.BaseHumanizerMs = 62;
    p.PreferSelectedTarget = true;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "HeightenedLearning";
    p.UltimateBuff = "ChronoShift";
    p.TacticalSummary =
        "Attach a predicted Time Bomb, Rewind only to complete a verified double-bomb stun, use Time Warp for ally speed or enemy slow, and reserve Chronoshift for a scored lethal window.";
    p.ResearchSummary =
        "Riot 26.15 with CommunityDragon 16.15 ZileanQ/ZileanW/TimeWarp/ChronoShift values; bomb, buff, cast and manual ownership state are reconciled by polling and events.";

    p.Spells[0] = Spell(SDK::SpellSlot::Q, "Time Bomb", CastKind::Position,
        Intent::Damage | Intent::CrowdControl | Intent::Setup | Intent::Waveclear |
            Intent::Jungle | Intent::LastHit,
        AllModes, 900.0f, 0.25f, 120.0f, 2000.0f, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[0].Aim = AimPolicy::Prediction;
    p.Spells[0].Priority = 92;
    p.Spells[0].MinimumAoeTargets = 1;
    p.Spells[0].PreserveAutoAttack = true;

    p.Spells[1] = Spell(SDK::SpellSlot::W, "Rewind", CastKind::Self,
        Intent::Recast | Intent::Setup | Intent::CrowdControl,
        Mode::Combo | Mode::Harass | Mode::Automatic, 600.0f, 0.0f, 0.0f,
        FLT_MAX, false, SDK::DamageType::True, SDK::SpellType::SkillshotLine);
    p.Spells[1].Priority = 98;
    p.Spells[1].ComboManaPercent = 18.0f;
    p.Spells[1].RequiredPlayerBuff = "";

    p.Spells[2] = Spell(SDK::SpellSlot::E, "Time Warp", CastKind::AnyTarget,
        Intent::Buff | Intent::CrowdControl | Intent::Disengage | Intent::Peel |
            Intent::Engage | Intent::AllyUtility,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::Automatic, 550.0f,
        0.0f, 210.0f, FLT_MAX, false, SDK::DamageType::Magical,
        SDK::SpellType::Targeted);
    p.Spells[2].Aim = AimPolicy::TargetPosition;
    p.Spells[2].Priority = 88;
    p.Spells[2].PreserveAutoAttack = true;

    p.Spells[3] = Spell(SDK::SpellSlot::R, "Chronoshift", CastKind::AllyTarget,
        Intent::Revive | Intent::Shield | Intent::Heal | Intent::AllyUtility,
        Mode::Combo | Mode::Automatic, 900.0f, 0.0f, 300.0f, FLT_MAX, false,
        SDK::DamageType::True, SDK::SpellType::Targeted);
    p.Spells[3].Aim = AimPolicy::TargetPosition;
    p.Spells[3].Priority = 100;
    p.Spells[3].PlayerHealthPercent = 100.0f;
    p.Spells[3].TargetHealthPercent = 38.0f;
    p.Spells[3].PreserveAutoAttack = true;

    p.Trade = Plan("Bomb and slow trade",
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget, 0, 900),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget, 120, 900));
    p.AllIn = Plan("Verified double bomb and Chronoshift",
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget, 0, 900),
        Step(SDK::SpellSlot::W, StepRule::RequireRecast | StepRule::RequireTarget,
             80, 1000),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequireRecast,
             110, 1100),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget, 190, 1000),
        Step(SDK::SpellSlot::R, StepRule::RequireTarget | StepRule::RequirePlayerLow,
             240, 900));
    p.Flee = Plan("Time Warp escape",
        Step(SDK::SpellSlot::E, StepRule::RequireSafePosition, 0, 700),
        Step(SDK::SpellSlot::R, StepRule::RequirePlayerLow, 80, 900));
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
