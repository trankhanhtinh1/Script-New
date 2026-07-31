#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Sivir = [] {
    ChampionProfile p{};
    p.ChampionName = "Sivir";
    p.DisplayName = "Sivir";
    p.InternalId = "champion.kuroaio.ai.sivir";
    p.PrimaryArchetype = Archetype::Marksman;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::ReturnProjectile | Mechanic::SpellShield |
                  Mechanic::AllyTarget | Mechanic::AutoWeave |
                  Mechanic::AutoReset;
    p.Ultimate = UltimatePolicy::MultiTarget;

    p.Spells[0] = Spell(
        SDK::SpellSlot::Q, "Boomerang Blade", CastKind::Line,
        Intent::Damage | Intent::Setup | Intent::Finisher |
            Intent::Waveclear | Intent::Jungle | Intent::LastHit,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::LastHit | Mode::Automatic,
        1250.0f, 0.25f, 100.0f, 1350.0f, true,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotLine);
    p.Spells[0].Priority = 94;
    p.Spells[0].Hitchance = SDK::HitChance::High;
    p.Spells[0].WeaveAfterAttack = true;
    p.Spells[0].PreserveAutoAttack = true;
    p.Spells[0].AllowOnMinions = true;
    p.Spells[0].HarassManaPercent = 48.0f;
    p.Spells[0].ClearManaPercent = 35.0f;

    p.Spells[1] = Spell(
        SDK::SpellSlot::W, "Ricochet", CastKind::Self,
        Intent::Damage | Intent::Buff | Intent::AutoReset |
            Intent::Waveclear | Intent::Jungle | Intent::Objective,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::LastHit | Mode::Automatic,
        0.0f, 0.0f, 0.0f, FLT_MAX, false,
        SDK::DamageType::Physical, SDK::SpellType::Targeted);
    p.Spells[1].Priority = 91;
    p.Spells[1].WeaveAfterAttack = true;
    p.Spells[1].PreserveAutoAttack = false;
    p.Spells[1].HarassManaPercent = 42.0f;
    p.Spells[1].ClearManaPercent = 54.0f;

    p.Spells[2] = Spell(
        SDK::SpellSlot::E, "Spell Shield", CastKind::Self,
        Intent::Shield | Intent::Cleanse | Intent::AntiGapcloser |
            Intent::Peel | Intent::Disengage,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::Automatic,
        0.0f, 0.0f, 0.0f, FLT_MAX, false,
        SDK::DamageType::True, SDK::SpellType::Targeted);
    p.Spells[2].Priority = 100;
    p.Spells[2].PreserveAutoAttack = true;
    p.Spells[2].PlayerHealthPercent = 100.0f;

    p.Spells[3] = Spell(
        SDK::SpellSlot::R, "On the Hunt", CastKind::Self,
        Intent::Buff | Intent::AllyUtility | Intent::Engage |
            Intent::Disengage | Intent::Peel | Intent::AntiGapcloser,
        Mode::Combo | Mode::Flee | Mode::Automatic,
        0.0f, 0.0f, 0.0f, FLT_MAX, false,
        SDK::DamageType::True, SDK::SpellType::Targeted);
    p.Spells[3].Priority = 98;
    p.Spells[3].PreserveAutoAttack = true;

    p.Trade = Plan(
        "Keep the orbwalker target primary, cast Ricochet on a real attack, then thread Q",
        Step(SDK::SpellSlot::W,
             StepRule::RequireTarget | StepRule::RequireInsideAaRange |
                 StepRule::AllowDuringWindup),
        Step(SDK::SpellSlot::Q,
             StepRule::RequireTarget | StepRule::RequireSafePosition));
    p.AllIn = Plan(
        "Use the outgoing and returning Q line while the team moves under On the Hunt",
        Step(SDK::SpellSlot::R,
             StepRule::RequireTarget | StepRule::RequireSafePosition),
        Step(SDK::SpellSlot::W,
             StepRule::RequireTarget | StepRule::RequireInsideAaRange |
                 StepRule::AllowDuringWindup),
        Step(SDK::SpellSlot::Q,
             StepRule::RequireTarget | StepRule::RequireSafePosition));
    p.Flee = Plan(
        "Shield the committed spell, then accelerate the player and nearby allies away",
        Step(SDK::SpellSlot::E,
             StepRule::RequireNoCrowdControl | StepRule::AllowDuringWindup),
        Step(SDK::SpellSlot::R,
             StepRule::RequireSafePosition | StepRule::AllowDuringWindup));

    p.PreferredCombatDistance = 575.0f;
    p.EngageHealthPercent = 62.0f;
    p.DefensiveHealthPercent = 36.0f;
    p.UltimateTargetHealthPercent = 100.0f;
    p.UltimateMinimumTargets = 2;
    p.MaximumCommitEnemies = 3;
    p.BaseHumanizerMs = 28;
    p.PreferSelectedTarget = true;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "SivirPassive";
    p.UltimateBuff = "SivirR";
    p.TrackedObjectToken = "SivirQ";
    p.ThemeFrom = 0xFFFFC04Fu;
    p.ThemeTo = 0xFF4DA6FFu;
    p.ThemeSpeed = 0.82f;
    p.TacticalSummary =
        "Treat Q as two independently colliding passes, keep W on the real attack target, "
        "time E against an observed impact window, and reserve R for a coordinated ally "
        "engage, threatened retreat or committed anti-gapclose movement.";
    p.ResearchSummary =
        "Pinned to Riot 26.15 / CommunityDragon 16.15: Q 1250-range returning line, "
        "W three-hit ricochet attack buff, E one-instance spell shield and R team movement "
        "speed. Conservative automation rejects blind R and preserves mana for E in danger.";
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
