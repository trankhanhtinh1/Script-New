#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Corki = [] {
    ChampionProfile p{};
    p.ChampionName = "Corki";
    p.DisplayName = "Corki";
    p.InternalId = "champion.kuroaio.ai.corki";
    p.PrimaryArchetype = Archetype::Marksman;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::Ammo | Mechanic::Dash |
                  Mechanic::AutoWeave | Mechanic::Stack;
    p.Ultimate = UltimatePolicy::Execute;

    p.Spells[0] = Spell(
        SDK::SpellSlot::Q, "Phosphorus Bomb", CastKind::Circle,
        Intent::Damage | Intent::Vision | Intent::Waveclear |
            Intent::LastHit | Intent::Jungle | Intent::Objective |
            Intent::Finisher,
        Mode::Combo | Mode::Harass | Mode::LaneClear |
            Mode::Jungle | Mode::LastHit | Mode::Automatic,
        825.0f, 0.25f, 250.0f, 1100.0f, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[0].Priority = 98;
    p.Spells[0].Hitchance = SDK::HitChance::High;
    p.Spells[0].PreserveAutoAttack = true;
    p.Spells[0].HarassManaPercent = 42.0f;
    p.Spells[0].ClearManaPercent = 48.0f;
    p.Spells[0].AllowOnMinions = true;

    p.Spells[1] = Spell(
        SDK::SpellSlot::W, "Valkyrie", CastKind::Position,
        Intent::Damage | Intent::Mobility | Intent::Disengage |
            Intent::Peel | Intent::Finisher,
        Mode::Combo | Mode::Flee | Mode::Automatic,
        600.0f, 0.25f, 160.0f, 700.0f, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[1].Priority = 100;
    p.Spells[1].Aim = AimPolicy::SafeCursor;
    p.Spells[1].DashDistance = 600.0f;
    p.Spells[1].DesiredDistance = 550.0f;
    p.Spells[1].MaximumEnemiesAtDestination = 1;

    p.Spells[2] = Spell(
        SDK::SpellSlot::E, "Gatling Gun", CastKind::Cone,
        Intent::Damage | Intent::Setup | Intent::Waveclear |
            Intent::Jungle | Intent::Objective,
        Mode::Combo | Mode::Harass | Mode::LaneClear |
            Mode::Jungle | Mode::Automatic,
        600.0f, 0.25f, 56.0f, FLT_MAX, false,
        SDK::DamageType::Mixed, SDK::SpellType::SkillshotCone);
    p.Spells[2].Priority = 96;
    p.Spells[2].Hitchance = SDK::HitChance::High;
    p.Spells[2].PreserveAutoAttack = true;
    p.Spells[2].HarassManaPercent = 50.0f;
    p.Spells[2].ClearManaPercent = 52.0f;
    p.Spells[2].AllowOnMinions = true;

    p.Spells[3] = Spell(
        SDK::SpellSlot::R, "Missile Barrage", CastKind::Line,
        Intent::Damage | Intent::Execute | Intent::Waveclear |
            Intent::LastHit | Intent::Jungle | Intent::Objective |
            Intent::Finisher,
        Mode::Combo | Mode::Harass | Mode::Automatic,
        1300.0f, 0.175f, 80.0f, 2000.0f, true,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[3].Priority = 97;
    p.Spells[3].Hitchance = SDK::HitChance::High;
    p.Spells[3].PreserveAutoAttack = true;
    p.Spells[3].MinimumAmmo = 1;
    p.Spells[3].HarassManaPercent = 46.0f;

    p.Variants[0] = {
        SDK::SpellSlot::R, "MissileBarrageMissile2", p.Spells[3]
    };
    p.Variants[0].Spec.Name = "The Big One";
    p.Variants[0].Spec.Range = 1500.0f;
    p.Variants[0].Spec.Width = 120.0f;
    p.VariantCount = 1;

    p.Trade = Plan(
        "Gatling only in a stable cone, weave Q after attacks, reserve barrage ammo",
        Step(SDK::SpellSlot::E, StepRule::RequireInsideAaRange),
        Step(SDK::SpellSlot::Q,
             StepRule::RequireTarget | StepRule::AllowDuringWindup),
        Step(SDK::SpellSlot::R,
             StepRule::RequireTarget | StepRule::AllowDuringWindup));
    p.AllIn = p.Trade;
    p.Flee = Plan(
        "Valkyrie to a walkable low-threat cursor-side endpoint",
        Step(SDK::SpellSlot::W,
             StepRule::RequireSafePosition | StepRule::AllowDuringWindup));

    p.PreferredCombatDistance = 575.0f;
    p.EngageHealthPercent = 55.0f;
    p.DefensiveHealthPercent = 34.0f;
    p.UltimateTargetHealthPercent = 24.0f;
    p.MaximumCommitEnemies = 1;
    p.BaseHumanizerMs = 22;
    p.PreferSelectedTarget = true;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.UltimateBuff = "CorkiMissileBarrageCounterBig";
    p.ThemeFrom = 0xFFFF8A3Du;
    p.ThemeTo = 0xFF63D8FFu;
    p.ThemeSpeed = 1.08f;
    p.TacticalSummary =
        "Preserve attacks and mana while landing missile Q, keep Gatling targets "
        "inside the forward cone, treat Valkyrie as endpoint-gated disengage or "
        "confirmed lethal mobility, and spend barrage ammo by observed reach and "
        "Big One value rather than readiness alone.";
    p.ResearchSummary =
        "Pinned to Riot 26.15 and CommunityDragon 16.15: 825-range travelling Q, "
        "300-600 Valkyrie, four-second 56-degree Gatling cone, four-charge R with "
        "1300/1500 normal/Big One reach and event-plus-poll state reconciliation.";
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
