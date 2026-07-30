#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

// Xin Zhao is an attack-cadence diver. His controller owns passive third-hit
// healing, the Q reset/knockup chain, W challenge geometry, radius-correct E
// landing and R's challenged-target exception rather than treating the kit as
// an unconditional E-Q-W-R sequence.
inline constexpr ChampionProfile XinZhao = [] {
    ChampionProfile p{};
    p.ChampionName = "XinZhao";
    p.DisplayName = "Xin Zhao";
    p.InternalId = "champion.kuroaio.ai.xinzhao";
    p.PrimaryArchetype = Archetype::Diver;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::Dash | Mechanic::Mark | Mechanic::Stack |
                  Mechanic::AutoWeave | Mechanic::AutoReset;
    p.Ultimate = UltimatePolicy::Defensive;

    p.Spells[0] = Spell(
        SDK::SpellSlot::Q, "Three Talon Strike", CastKind::Self,
        Intent::Damage | Intent::CrowdControl | Intent::Buff |
            Intent::AutoReset | Intent::Setup | Intent::Jungle,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::LastHit | Mode::Flee,
        375.0f, 0.0f, 0.0f, FLT_MAX, false,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotCircle);
    p.Spells[0].Priority = 94;
    p.Spells[0].TriggerRange = 375.0f;
    p.Spells[0].DesiredDistance = 175.0f;
    p.Spells[0].AllowOnMinions = true;
    p.Spells[0].WeaveAfterAttack = true;
    p.Spells[0].PreserveAutoAttack = false;

    p.Spells[1] = Spell(
        SDK::SpellSlot::W, "Wind Becomes Lightning", CastKind::Line,
        Intent::Damage | Intent::CrowdControl | Intent::Setup |
            Intent::Waveclear | Intent::Jungle | Intent::Peel,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::LastHit | Mode::Flee | Mode::Automatic,
        1000.0f, 0.60f, 50.0f, FLT_MAX, false,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotLine);
    p.Spells[1].Priority = 91;
    p.Spells[1].TriggerRange = 1000.0f;
    p.Spells[1].Hitchance = SDK::HitChance::High;
    p.Spells[1].HarassManaPercent = 44.0f;
    p.Spells[1].ClearManaPercent = 58.0f;
    p.Spells[1].AllowOnMinions = true;
    p.Spells[1].PreserveAutoAttack = true;

    p.Spells[2] = Spell(
        SDK::SpellSlot::E, "Audacious Charge", CastKind::EnemyTarget,
        Intent::Damage | Intent::CrowdControl | Intent::Mobility |
            Intent::Engage | Intent::Setup | Intent::Jungle |
            Intent::Disengage,
        Mode::Combo | Mode::Harass | Mode::Jungle | Mode::Flee,
        650.0f, 0.15f, 250.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[2].Priority = 93;
    p.Spells[2].TriggerRange = 1100.0f;
    p.Spells[2].DashDistance = 1100.0f;
    p.Spells[2].HarassManaPercent = 55.0f;
    p.Spells[2].MaximumEnemiesAtDestination = 2;
    p.Spells[2].AllowOnMinions = true;
    p.Spells[2].PreserveAutoAttack = true;
    p.Spells[2].RequiredTargetBuff = "XinZhaoWMark";

    p.Spells[3] = Spell(
        SDK::SpellSlot::R, "Crescent Guard", CastKind::Self,
        Intent::Damage | Intent::CrowdControl | Intent::Disengage |
            Intent::Peel | Intent::Setup | Intent::Finisher,
        Mode::Combo | Mode::Flee | Mode::Automatic,
        500.0f, 0.33f, 500.0f, FLT_MAX, false,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotCircle);
    p.Spells[3].Priority = 98;
    p.Spells[3].TriggerRange = 500.0f;
    p.Spells[3].MinimumAoeTargets = 2;
    p.Spells[3].MaximumEnemiesAtDestination = 2;
    p.Spells[3].PreserveAutoAttack = true;

    p.Trade = Plan(
        "W challenge, optional safe long E, Q reset and preserve third strike",
        Step(SDK::SpellSlot::W, StepRule::RequireTarget),
        Step(SDK::SpellSlot::E,
             StepRule::RequireTarget | StepRule::RequireMark |
                 StepRule::RequireSafePosition),
        Step(SDK::SpellSlot::Q,
             StepRule::RequireTarget | StepRule::RequireAfterAttack));

    p.AllIn = Plan(
        "challenge entry, complete Q knockup, then isolate with Crescent Guard",
        Step(SDK::SpellSlot::W, StepRule::RequireTarget),
        Step(SDK::SpellSlot::E,
             StepRule::RequireTarget | StepRule::RequireSafePosition),
        Step(SDK::SpellSlot::Q,
             StepRule::RequireTarget | StepRule::RequireAfterAttack),
        Step(SDK::SpellSlot::R,
             StepRule::RequireTarget | StepRule::RequireMark |
                 StepRule::RequireSafePosition));

    p.Flee = Plan(
        "sweep pursuers, then charge only to a cursor-improving safe unit",
        Step(SDK::SpellSlot::R,
             StepRule::RequireMultiTarget | StepRule::RequirePlayerLow),
        Step(SDK::SpellSlot::E, StepRule::RequireSafePosition),
        Step(SDK::SpellSlot::W));

    p.PreferredCombatDistance = 175.0f;
    p.EngageHealthPercent = 48.0f;
    p.DefensiveHealthPercent = 38.0f;
    p.UltimateTargetHealthPercent = 42.0f;
    p.UltimateMinimumTargets = 2;
    p.MaximumCommitEnemies = 2;
    p.BaseHumanizerMs = 35;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "XinZhaoP";
    p.MarkBuff = "XinZhaoWMark";
    p.FormBuff = "XinZhaoQ";
    p.UltimateBuff = "XinZhaoRRangedImmunity";
    p.ThemeFrom = 0xFF4674D7u;
    p.ThemeTo = 0xFFD2B15Bu;
    p.ThemeSpeed = 1.05f;
    p.TacticalSummary =
        "Treat every third basic attack as damage plus healing, reset one auto "
        "with Q and protect its third-hit knockup, use W's narrow piercing "
        "thrust to create the three-second Challenge, land E at the real "
        "radius-adjusted endpoint, and cast R only when its protected target "
        "and knockback geometry produce a safe isolation or block real range.";
    p.ResearchSummary =
        "Riot 26.15 and pinned CommunityDragon 16.15 champion/game-bin data, "
        "current Xin Zhao mechanics, runtime spell/buff identities, local SDK "
        "and legacy-route audits, plus deterministic passive/Q/W/E/R regressions.";
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
