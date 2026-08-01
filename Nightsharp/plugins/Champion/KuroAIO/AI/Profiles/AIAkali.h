#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

// Akali is an energy-and-escape-route state machine.  Each damaging ability
// opens a passive-ring decision; W is a scarce energy/stealth resource; E2 is
// optional until its destination is safe; and R2 is held for missing-health
// execution or a planned exit instead of being fired as soon as it unlocks.
inline constexpr ChampionProfile Akali = [] {
    ChampionProfile p{};
    p.ChampionId = SDK::ChampionId::Akali;
    p.DisplayName = "Akali";
    p.InternalId = "champion.kuroaio.ai.akali";
    p.PrimaryArchetype = Archetype::Assassin;
    p.Resource = ResourceModel::Energy;
    p.Mechanics = Mechanic::Recast | Mechanic::Dash | Mechanic::Execute |
                  Mechanic::ObjectTracking | Mechanic::Mark |
                  Mechanic::AutoWeave | Mechanic::MissingHealth |
                  Mechanic::Terrain;
    p.Ultimate = UltimatePolicy::RecastControl;

    p.Spells[0] = Spell(
        SDK::SpellSlot::Q, "Five Point Strike", CastKind::Cone,
        Intent::Damage | Intent::CrowdControl | Intent::Setup |
            Intent::Waveclear | Intent::Jungle | Intent::LastHit,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::LastHit | Mode::Automatic,
        500.0f, 0.20f, 350.0f, 3200.0f, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCone);
    p.Spells[0].Priority = 86;
    p.Spells[0].Hitchance = SDK::HitChance::High;
    p.Spells[0].MinimumAoeTargets = 3;
    p.Spells[0].HarassManaPercent = 42.0f;
    p.Spells[0].ClearManaPercent = 55.0f;

    p.Spells[1] = Spell(
        SDK::SpellSlot::W, "Twilight Shroud", CastKind::Position,
        Intent::Buff | Intent::Mobility | Intent::Disengage |
            Intent::Setup,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::Automatic,
        250.0f, 0.0f, 350.0f, FLT_MAX, false,
        SDK::DamageType::True, SDK::SpellType::SkillshotCircle);
    p.Spells[1].Aim = AimPolicy::SafeCursor;
    p.Spells[1].TriggerRange = 850.0f;
    p.Spells[1].Priority = 92;
    p.Spells[1].MaximumEnemiesAtDestination = 3;

    p.Spells[2] = Spell(
        SDK::SpellSlot::E, "Shuriken Flip", CastKind::Line,
        Intent::Damage | Intent::Mobility | Intent::Setup |
            Intent::Disengage | Intent::Jungle | Intent::Objective |
            Intent::Recast,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle | Mode::Flee |
            Mode::Automatic,
        825.0f, 0.25f, 120.0f, 1800.0f, true,
        SDK::DamageType::Magical,
        SDK::SpellType::SkillshotMissileLine);
    p.Spells[2].Priority = 80;
    p.Spells[2].Hitchance = SDK::HitChance::VeryHigh;
    p.Spells[2].DashDistance = 400.0f;
    p.Spells[2].MaximumEnemiesAtDestination = 2;
    p.Spells[2].RecastSpellName = "AkaliEb";

    p.Spells[3] = Spell(
        SDK::SpellSlot::R, "Perfect Execution", CastKind::EnemyTarget,
        Intent::Damage | Intent::Mobility | Intent::Engage |
            Intent::Disengage | Intent::Execute | Intent::Recast,
        Mode::Combo | Mode::Flee | Mode::Automatic,
        675.0f, 0.0f, 110.0f, 1500.0f, false,
        SDK::DamageType::Magical, SDK::SpellType::Targeted);
    p.Spells[3].Priority = 75;
    p.Spells[3].DashDistance = 750.0f;
    p.Spells[3].MaximumEnemiesAtDestination = 2;
    p.Spells[3].RecastSpellName = "AkaliRb";

    SpellSpec e2 = p.Spells[2];
    e2.Name = "Shuriken Flip Recast";
    e2.Kind = CastKind::Self;
    e2.Range = FLT_MAX;
    e2.TriggerRange = FLT_MAX;
    e2.Delay = 0.0f;
    e2.Speed = 1500.0f;
    e2.Collision = false;
    e2.Shape = SDK::SpellType::Targeted;
    e2.Hitchance = SDK::HitChance::Immobile;
    e2.Priority = 96;
    p.Variants[p.VariantCount++] = {
        SDK::SpellSlot::E, "AkaliEb", e2
    };

    SpellSpec r2 = p.Spells[3];
    r2.Name = "Perfect Execution Recast";
    r2.Kind = CastKind::Direction;
    r2.Range = 800.0f;
    r2.TriggerRange = 910.0f;
    r2.Delay = 0.0f;
    r2.Speed = 3000.0f;
    r2.Shape = SDK::SpellType::SkillshotLine;
    r2.Aim = AimPolicy::SafeCursor;
    r2.DashDistance = 800.0f;
    r2.Priority = 100;
    p.Variants[p.VariantCount++] = {
        SDK::SpellSlot::R, "AkaliRb", r2
    };

    p.Trade = Plan(
        "Q ring, passive kama, second Q",
        Step(SDK::SpellSlot::Q),
        Step(SDK::SpellSlot::Q,
             StepRule::RequireTarget | StepRule::RequireAfterAttack));

    p.AllIn = Plan(
        "R1-E1 route with passive and held R2",
        Step(SDK::SpellSlot::R,
             StepRule::RequireTarget | StepRule::RequireSafePosition |
                 StepRule::RequireFirstCast),
        Step(SDK::SpellSlot::E,
             StepRule::RequireTarget | StepRule::AllowDuringWindup),
        Step(SDK::SpellSlot::E,
             StepRule::RequireTarget | StepRule::RequireRecast |
                 StepRule::RequireSafePosition),
        Step(SDK::SpellSlot::Q),
        Step(SDK::SpellSlot::R,
             StepRule::RequireTarget | StepRule::RequireRecast |
                 StepRule::HoldForExecute));

    p.Flee = Plan(
        "Shroud, safe backflip, held R2",
        Step(SDK::SpellSlot::W, StepRule::RequireSafePosition),
        Step(SDK::SpellSlot::E, StepRule::RequireSafePosition),
        Step(SDK::SpellSlot::R,
             StepRule::RequireSafePosition | StepRule::RequireRecast));

    p.PreferredCombatDistance = 475.0f;
    p.EngageHealthPercent = 34.0f;
    p.DefensiveHealthPercent = 27.0f;
    p.UltimateTargetHealthPercent = 76.0f;
    p.UltimateMinimumTargets = 1;
    p.MaximumCommitEnemies = 2;
    p.BaseHumanizerMs = 48;
    p.PassiveBuff = "AkaliPWeapon";
    p.MarkBuff = "AkaliE";
    p.FormBuff = "AkaliWStealthTracker";
    p.TrackedObjectToken = "AkaliWSmoke";
    p.ThemeFrom = 0xFF44E0A0u;
    p.ThemeTo = 0xFF7A35D8u;
    p.ThemeSpeed = 1.15f;
    p.TacticalSummary =
        "Budget energy before every branch, cash each safe passive kama, use "
        "W once at the fight's real danger point, treat E2 as an optional "
        "commit, and hold R2 for missing-health execution or exit.";
    p.ResearchSummary =
        "CommunityDragon 16.14 bin/kit data, Meraki live mechanics, current "
        "Challenger long-form guide, local EnsoulSharp passive/dash event audit, "
        "high-elo Akali matchup material, current combo catalogues, Akali-main "
        "edge cases, and pure geometry tests.";
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
