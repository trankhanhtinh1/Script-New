#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile KogMaw = [] {
    ChampionProfile p{};
    p.ChampionId = SDK::ChampionId::KogMaw;
    p.DisplayName = "Kog'Maw";
    p.InternalId = "champion.kuroaio.ai.kogmaw";
    p.PrimaryArchetype = Archetype::Marksman;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::AutoWeave | Mechanic::Stack |
                  Mechanic::MissingHealth | Mechanic::Execute;
    p.Ultimate = UltimatePolicy::Execute;

    p.Spells[0] = Spell(
        SDK::SpellSlot::Q, "Caustic Spittle", CastKind::Line,
        Intent::Damage | Intent::Setup | Intent::Execute |
            Intent::Jungle | Intent::Finisher,
        Mode::Combo | Mode::Harass | Mode::Jungle | Mode::Automatic,
        1200.0f, 0.25f, 70.0f, 1650.0f, true,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[0].Priority = 92;
    p.Spells[0].Hitchance = SDK::HitChance::High;
    p.Spells[0].PreserveAutoAttack = true;
    p.Spells[0].HarassManaPercent = 42.0f;

    p.Spells[1] = Spell(
        SDK::SpellSlot::W, "Bio-Arcane Barrage", CastKind::Self,
        Intent::Buff | Intent::Damage | Intent::AutoReset |
            Intent::Jungle | Intent::Objective,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle,
        0.0f, 0.0f, 0.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[1].Priority = 99;
    p.Spells[1].PreserveAutoAttack = false;
    p.Spells[1].HarassManaPercent = 32.0f;
    p.Spells[1].ClearManaPercent = 38.0f;

    p.Spells[2] = Spell(
        SDK::SpellSlot::E, "Void Ooze", CastKind::Line,
        Intent::Damage | Intent::CrowdControl | Intent::Setup |
            Intent::Peel | Intent::AntiGapcloser | Intent::Waveclear,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::Flee | Mode::Automatic,
        1200.0f, 0.25f, 120.0f, 1200.0f, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[2].Priority = 94;
    p.Spells[2].Hitchance = SDK::HitChance::High;
    p.Spells[2].PreserveAutoAttack = true;
    p.Spells[2].HarassManaPercent = 48.0f;
    p.Spells[2].ClearManaPercent = 55.0f;
    p.Spells[2].AllowOnMinions = true;

    p.Spells[3] = Spell(
        SDK::SpellSlot::R, "Living Artillery", CastKind::Circle,
        Intent::Damage | Intent::Execute | Intent::Vision |
            Intent::Finisher,
        Mode::Combo | Mode::Harass | Mode::Automatic,
        1800.0f, 1.1f, 240.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[3].Priority = 97;
    p.Spells[3].Hitchance = SDK::HitChance::VeryHigh;
    p.Spells[3].PreserveAutoAttack = true;

    p.Trade = Plan(
        "Open W only for a real attack route; weave Q in attack downtime",
        Step(SDK::SpellSlot::W, StepRule::RequireTarget),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget));
    p.AllIn = Plan(
        "W attacks first, Q shred, E catches escape, R executes",
        Step(SDK::SpellSlot::W, StepRule::RequireTarget),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget),
        Step(SDK::SpellSlot::R,
             StepRule::RequireTarget | StepRule::RequireTargetLow));
    p.Flee = Plan(
        "Lay Void Ooze through the committed pursuer",
        Step(SDK::SpellSlot::E,
             StepRule::RequireTarget | StepRule::AllowDuringWindup));

    p.PreferredCombatDistance = 710.0f;
    p.EngageHealthPercent = 58.0f;
    p.DefensiveHealthPercent = 30.0f;
    p.UltimateTargetHealthPercent = 40.0f;
    p.MaximumCommitEnemies = 2;
    p.BaseHumanizerMs = 21;
    p.PreferSelectedTarget = true;
    p.AllowTurretDiveIfKillable = false;
    p.PassiveBuff = "KogMawBioArcaneBarrage";
    p.MarkBuff = "kogmawlivingartillerycost";
    p.ThemeFrom = 0xFFB4F36Bu;
    p.ThemeTo = 0xFF8A4DE8u;
    p.ThemeSpeed = 0.95f;
    p.TacticalSummary =
        "Preserve normal attacks, activate W only when it creates or improves a "
        "real attack route, use Q for shred/lethal downtime, E against committed "
        "movement, and throttle R by artillery stacks and missing-health value.";
    p.ResearchSummary =
        "Ported from TestOrbwalker PortKogMaw.cs and checked against current "
        "CommunityDragon spell data (Q 1200; R 1300/1550/1800).";
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
