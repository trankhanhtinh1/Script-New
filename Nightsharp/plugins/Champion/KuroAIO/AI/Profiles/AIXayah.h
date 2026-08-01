#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Xayah = [] {
    ChampionProfile p{};
    p.ChampionId = SDK::ChampionId::Xayah;
    p.DisplayName = "Xayah";
    p.InternalId = "champion.kuroaio.ai.xayah";
    p.PrimaryArchetype = Archetype::Marksman;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::ObjectTracking | Mechanic::AutoWeave |
                  Mechanic::DirectionalSweet | Mechanic::ReturnProjectile |
                  Mechanic::SpellShield;
    p.Ultimate = UltimatePolicy::Defensive;

    p.Spells[0] = Spell(
        SDK::SpellSlot::Q, "Double Daggers", CastKind::Line,
        Intent::Damage | Intent::Setup | Intent::Waveclear |
            Intent::Jungle | Intent::LastHit,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::LastHit | Mode::Automatic,
        1100.0f, 0.25f, 70.0f, 2000.0f, true,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotLine);
    p.Spells[0].Priority = 90;
    p.Spells[0].Hitchance = SDK::HitChance::High;
    p.Spells[0].PreserveAutoAttack = true;
    p.Spells[0].HarassManaPercent = 48.0f;
    p.Spells[0].ClearManaPercent = 55.0f;
    p.Spells[0].AllowOnMinions = true;

    p.Spells[1] = Spell(
        SDK::SpellSlot::W, "Deadly Plumage", CastKind::Self,
        Intent::Buff | Intent::Damage | Intent::Engage | Intent::Disengage,
        Mode::Combo | Mode::Harass | Mode::Automatic,
        0.0f, 0.0f, 0.0f, FLT_MAX, false,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotCircle);
    p.Spells[1].Priority = 88;
    p.Spells[1].PreserveAutoAttack = true;
    p.Spells[1].HarassManaPercent = 46.0f;

    p.Spells[2] = Spell(
        SDK::SpellSlot::E, "Bladecaller", CastKind::Self,
        Intent::Damage | Intent::CrowdControl | Intent::Execute |
            Intent::Finisher | Intent::Peel | Intent::Waveclear |
            Intent::Jungle | Intent::LastHit,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::LastHit | Mode::Flee | Mode::Automatic,
        0.0f, 0.25f, 0.0f, FLT_MAX, false,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotCircle);
    p.Spells[2].Priority = 100;
    p.Spells[2].PreserveAutoAttack = true;
    p.Spells[2].HarassManaPercent = 42.0f;
    p.Spells[2].ClearManaPercent = 48.0f;

    p.Spells[3] = Spell(
        SDK::SpellSlot::R, "Featherstorm", CastKind::Direction,
        Intent::Damage | Intent::Disengage | Intent::Peel |
            Intent::AntiGapcloser | Intent::Execute,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::Automatic,
        1100.0f, 0.25f, 80.0f, 2000.0f, false,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotLine);
    p.Spells[3].Priority = 99;
    p.Spells[3].Hitchance = SDK::HitChance::High;
    p.Spells[3].PreserveAutoAttack = true;

    p.Trade = Plan(
        "Q to create feathers, maintain the auto attack, then recall a safe line",
        Step(SDK::SpellSlot::Q,
             StepRule::RequireTarget | StepRule::RequireOutsideAaRange |
                 StepRule::AllowDuringWindup),
        Step(SDK::SpellSlot::E,
             StepRule::RequireTarget | StepRule::RequireMark));
    p.AllIn = Plan(
        "W for a committed attack window, Q and autos for feathers, then E root or execute",
        Step(SDK::SpellSlot::W, StepRule::RequireTarget),
        Step(SDK::SpellSlot::Q,
             StepRule::RequireTarget | StepRule::RequireAfterAttack),
        Step(SDK::SpellSlot::E,
             StepRule::RequireTarget | StepRule::RequireMark));
    p.Flee = Plan(
        "R only for invulnerability or a verified peel line, then E the pursuer",
        Step(SDK::SpellSlot::R,
             StepRule::RequireTarget | StepRule::AllowDuringWindup),
        Step(SDK::SpellSlot::E,
             StepRule::RequireTarget | StepRule::RequireMark |
                 StepRule::AllowDuringWindup));

    p.PreferredCombatDistance = 575.0f;
    p.EngageHealthPercent = 62.0f;
    p.DefensiveHealthPercent = 34.0f;
    p.UltimateTargetHealthPercent = 48.0f;
    p.MaximumCommitEnemies = 2;
    p.BaseHumanizerMs = 22;
    p.PreferSelectedTarget = true;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "XayahPassive";
    p.UltimateBuff = "XayahR";
    p.TrackedObjectToken = "XayahFeather";
    p.ThemeFrom = 0xFFE14A9Bu;
    p.ThemeTo = 0xFF8E66FFu;
    p.ThemeSpeed = 0.92f;
    p.TacticalSummary =
        "Track every live feather from spell, missile and object events, keep "
        "the orbwalker attack route intact, and recall only a predictive line "
        "that reaches a target. W is reserved for a real attack posture while "
        "R is defensive unless it creates a lethal or multi-target feather line.";
    p.ResearchSummary =
        "Pinned to Riot 26.15 / CommunityDragon 16.15: Q Double Daggers leaves "
        "two feathers, passive attacks leave feathers, E Bladecaller recalls all "
        "feathers and roots after three pass-throughs, W buffs attack posture, "
        "and R supplies a defensive untargetable feather spread.";
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
