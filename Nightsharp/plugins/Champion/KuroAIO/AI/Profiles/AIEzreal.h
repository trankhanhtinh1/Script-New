#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Ezreal = [] {
    ChampionProfile p{};
    p.ChampionId = SDK::ChampionId::Ezreal;
    p.DisplayName = "Ezreal";
    p.InternalId = "champion.kuroaio.ai.ezreal";
    p.PrimaryArchetype = Archetype::Marksman;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::Mark | Mechanic::Blink |
                  Mechanic::AutoWeave | Mechanic::Global;
    p.Ultimate = UltimatePolicy::ManualAssist;

    p.Spells[0] = Spell(
        SDK::SpellSlot::Q, "Mystic Shot", CastKind::Line,
        Intent::Damage | Intent::Execute | Intent::Waveclear |
            Intent::LastHit | Intent::Jungle | Intent::Objective |
            Intent::Finisher,
        Mode::Combo | Mode::Harass | Mode::LaneClear |
            Mode::Jungle | Mode::LastHit | Mode::Automatic,
        1200.0f, 0.25f, 60.0f, 2000.0f, true,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotLine);
    p.Spells[0].Priority = 99;
    p.Spells[0].Hitchance = SDK::HitChance::High;
    p.Spells[0].PreserveAutoAttack = true;
    p.Spells[0].HarassManaPercent = 38.0f;
    p.Spells[0].ClearManaPercent = 42.0f;
    p.Spells[0].AllowOnMinions = true;

    p.Spells[1] = Spell(
        SDK::SpellSlot::W, "Essence Flux", CastKind::Line,
        Intent::Damage | Intent::Setup | Intent::Buff |
            Intent::Objective,
        Mode::Combo | Mode::Harass | Mode::Automatic,
        1200.0f, 0.25f, 80.0f, 1700.0f, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[1].Priority = 96;
    p.Spells[1].Hitchance = SDK::HitChance::High;
    p.Spells[1].PreserveAutoAttack = true;
    p.Spells[1].HarassManaPercent = 52.0f;

    p.Spells[2] = Spell(
        SDK::SpellSlot::E, "Arcane Shift", CastKind::Position,
        Intent::Damage | Intent::Mobility | Intent::Disengage |
            Intent::Peel | Intent::Execute,
        Mode::Combo | Mode::Flee | Mode::Automatic,
        475.0f, 0.65f, 375.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[2].Priority = 98;
    p.Spells[2].Aim = AimPolicy::SafeCursor;
    p.Spells[2].DashDistance = 475.0f;
    p.Spells[2].DesiredDistance = 650.0f;
    p.Spells[2].MaximumEnemiesAtDestination = 1;

    p.Spells[3] = Spell(
        SDK::SpellSlot::R, "Trueshot Barrage", CastKind::Line,
        Intent::Damage | Intent::Execute | Intent::Waveclear |
            Intent::Objective | Intent::Channel,
        Mode::Combo | Mode::LaneClear | Mode::Automatic,
        20000.0f, 1.0f, 160.0f, 2000.0f, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[3].Priority = 100;
    p.Spells[3].Hitchance = SDK::HitChance::VeryHigh;
    p.Spells[3].PreserveAutoAttack = true;

    p.Trade = Plan(
        "W only with a planned Q or auto detonation; then spam clear Q windows",
        Step(SDK::SpellSlot::W, StepRule::RequireTarget),
        Step(SDK::SpellSlot::Q,
             StepRule::RequireTarget | StepRule::AllowDuringWindup));
    p.AllIn = p.Trade;
    p.Flee = Plan(
        "Arcane Shift toward the safest cursor-side point",
        Step(SDK::SpellSlot::E,
             StepRule::RequireSafePosition | StepRule::AllowDuringWindup));

    p.PreferredCombatDistance = 800.0f;
    p.EngageHealthPercent = 58.0f;
    p.DefensiveHealthPercent = 33.0f;
    p.UltimateTargetHealthPercent = 18.0f;
    p.MaximumCommitEnemies = 1;
    p.BaseHumanizerMs = 20;
    p.PreferSelectedTarget = true;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.MarkBuff = "ezrealwattach";
    p.ThemeFrom = 0xFFFFD34Fu;
    p.ThemeTo = 0xFF5FCBFFu;
    p.ThemeSpeed = 1.02f;
    p.TacticalSummary =
        "Continuously reselect a target with an unblocked Q/W line; preserve "
        "autos, chain W only into a real detonation, blink defensively or for a "
        "single confirmed execute, and reserve R for manual or isolated lethal shots.";
    p.ResearchSummary =
        "Ported from TestOrbwalker AllChampions/Ezreal.cs: dual prediction/collision "
        "Q spam, W-Q/AA detonation, AA-windup preservation, farm last-hit checks, "
        "safe E execute and high-confidence R; upgraded with reach scoring.";
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
