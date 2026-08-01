#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

// Akshan is an auto-weaving marksman whose real decision tree is built around
// passive shot ownership and terrain.  Q changes length after every hit, E has
// three input phases and an orbital collision path, W is a roam/revive tool
// rather than a combat steroid, and R must solve blockers while it channels.
inline constexpr ChampionProfile Akshan = [] {
    ChampionProfile p{};
    p.ChampionId = SDK::ChampionId::Akshan;
    p.DisplayName = "Akshan";
    p.InternalId = "champion.kuroaio.ai.akshan";
    p.PrimaryArchetype = Archetype::Marksman;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::Recast | Mechanic::Dash | Mechanic::Channel |
                  Mechanic::Execute | Mechanic::ObjectTracking |
                  Mechanic::Mark | Mechanic::Stack | Mechanic::Revive |
                  Mechanic::AutoWeave | Mechanic::ReturnProjectile |
                  Mechanic::MissingHealth | Mechanic::Terrain;
    p.Ultimate = UltimatePolicy::RecastControl;

    p.Spells[0] = Spell(
        SDK::SpellSlot::Q, "Avengerang", CastKind::Line,
        Intent::Damage | Intent::Setup | Intent::Vision |
            Intent::Waveclear | Intent::Jungle | Intent::LastHit,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::LastHit | Mode::Flee | Mode::Automatic,
        850.0f, 0.25f, 70.0f, 1500.0f, false,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotLine);
    p.Spells[0].Priority = 88;
    p.Spells[0].Hitchance = SDK::HitChance::High;
    p.Spells[0].MinimumAoeTargets = 3;
    p.Spells[0].HarassManaPercent = 38.0f;
    p.Spells[0].ClearManaPercent = 52.0f;
    p.Spells[0].PreserveAutoAttack = true;
    p.Spells[0].AllowOnMinions = true;

    p.Spells[1] = Spell(
        SDK::SpellSlot::W, "Going Rogue", CastKind::Self,
        Intent::Buff | Intent::Mobility | Intent::Vision |
            Intent::Setup | Intent::AllyUtility,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::Automatic,
        0.0f, 0.50f, 0.0f, FLT_MAX, false,
        SDK::DamageType::True, SDK::SpellType::Targeted);
    p.Spells[1].Priority = 42;
    p.Spells[1].TriggerRange = 5500.0f;
    p.Spells[1].HarassManaPercent = 45.0f;
    p.Spells[1].PreserveAutoAttack = true;

    p.Spells[2] = Spell(
        SDK::SpellSlot::E, "Heroic Swing", CastKind::Direction,
        Intent::Damage | Intent::Mobility | Intent::Engage |
            Intent::Disengage | Intent::Setup | Intent::Recast,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::Automatic,
        800.0f, 0.10f, 50.0f, 2500.0f, false,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotLine);
    p.Spells[2].Priority = 82;
    p.Spells[2].Aim = AimPolicy::SafeCursor;
    p.Spells[2].DashDistance = 350.0f;
    p.Spells[2].MaximumEnemiesAtDestination = 2;
    p.Spells[2].PreserveAutoAttack = true;
    p.Spells[2].RecastSpellName = "AkshanE2";

    p.Spells[3] = Spell(
        SDK::SpellSlot::R, "Comeuppance", CastKind::EnemyTarget,
        Intent::Damage | Intent::Execute | Intent::Channel |
            Intent::Vision | Intent::Recast,
        Mode::Combo | Mode::Automatic,
        2500.0f, 0.0f, 40.0f, 3200.0f, true,
        SDK::DamageType::Physical, SDK::SpellType::Targeted);
    p.Spells[3].Priority = 74;
    p.Spells[3].TargetHealthPercent = 58.0f;
    p.Spells[3].PreserveAutoAttack = true;
    p.Spells[3].RecastSpellName = "AkshanRCancel";

    SpellSpec e2 = p.Spells[2];
    e2.Name = "Heroic Swing Orbit";
    e2.Kind = CastKind::Position;
    e2.Range = 800.0f;
    e2.TriggerRange = 800.0f;
    e2.Delay = 0.0f;
    e2.Speed = 1200.0f;
    e2.Shape = SDK::SpellType::SkillshotCircle;
    e2.Priority = 97;
    e2.PreserveAutoAttack = false;
    p.Variants[p.VariantCount++] = {
        SDK::SpellSlot::E, "AkshanE2", e2
    };

    SpellSpec e3 = e2;
    e3.Name = "Heroic Swing Dismount";
    e3.Range = 350.0f;
    e3.TriggerRange = 350.0f;
    e3.Speed = 3000.0f;
    e3.Shape = SDK::SpellType::SkillshotLine;
    e3.Aim = AimPolicy::SafeCursor;
    e3.DashDistance = 350.0f;
    e3.Priority = 100;
    p.Variants[p.VariantCount++] = {
        SDK::SpellSlot::E, "AkshanE3", e3
    };

    SpellSpec r2 = p.Spells[3];
    r2.Name = "Comeuppance Release";
    r2.Kind = CastKind::Self;
    r2.Range = FLT_MAX;
    r2.TriggerRange = FLT_MAX;
    r2.Delay = 0.0f;
    r2.Collision = false;
    r2.Shape = SDK::SpellType::Targeted;
    r2.Priority = 100;
    r2.PreserveAutoAttack = false;
    p.Variants[p.VariantCount++] = {
        SDK::SpellSlot::R, "AkshanRCancel", r2
    };

    p.Trade = Plan(
        "double shot, extended Q and return alignment",
        Step(SDK::SpellSlot::Q,
             StepRule::RequireTarget | StepRule::RequireAfterAttack),
        Step(SDK::SpellSlot::E,
             StepRule::RequireTarget | StepRule::RequireOutsideAaRange |
                 StepRule::RequireSafePosition));

    p.AllIn = Plan(
        "mark target, solve swing orbit, reserve blocker-free R",
        Step(SDK::SpellSlot::Q,
             StepRule::RequireTarget | StepRule::RequireAfterAttack),
        Step(SDK::SpellSlot::E,
             StepRule::RequireTarget | StepRule::RequireSafePosition),
        Step(SDK::SpellSlot::R,
             StepRule::RequireTarget | StepRule::HoldForExecute));

    p.Flee = Plan(
        "safe terrain swing, Q haste, then wall camouflage",
        Step(SDK::SpellSlot::E, StepRule::RequireSafePosition),
        Step(SDK::SpellSlot::Q),
        Step(SDK::SpellSlot::W, StepRule::RequireSafePosition));

    p.PreferredCombatDistance = 535.0f;
    p.EngageHealthPercent = 38.0f;
    p.DefensiveHealthPercent = 30.0f;
    p.UltimateTargetHealthPercent = 58.0f;
    p.UltimateMinimumTargets = 1;
    p.MaximumCommitEnemies = 2;
    p.BaseHumanizerMs = 50;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "AkshanPassiveShield";
    p.MarkBuff = "AkshanPassiveDebuff";
    p.ChannelBuff = "AkshanR";
    p.FormBuff = "AkshanW";
    p.UltimateBuff = "AkshanR";
    p.TrackedObjectToken = "AkshanQMissile";
    p.ThemeFrom = 0xFFE7B84Du;
    p.ThemeTo = 0xFF35C8D0u;
    p.ThemeSpeed = 1.05f;
    p.TacticalSummary =
        "Preserve or cancel the passive second shot by danger, route Q through "
        "units and its moving return, swing only on a simulated safe orbit, "
        "hunt Scoundrels without overriding player pathing, and release R only "
        "after solving its first blocker.";
    p.ResearchSummary =
        "CommunityDragon PC 16.14 bin/game data, Riot 26.1 and 26.14 notes, "
        "current matchup/skill statistics, Challenger Akshan swing geometry, "
        "high-elo combo material, Akshan-main edge cases, and pure geometry "
        "regressions.";
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
