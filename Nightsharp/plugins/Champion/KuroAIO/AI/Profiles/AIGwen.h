#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

// Gwen's profile is descriptive only. AIGwenController owns Snip Snip! stacks,
// Hallowed Mist placement, Skip 'n Slash endpoints and Needlework recasts.
inline constexpr ChampionProfile Gwen = [] {
    ChampionProfile p{};
    p.ChampionName = "Gwen";
    p.DisplayName = "Gwen";
    p.InternalId = "champion.kuroaio.ai.gwen";
    p.PrimaryArchetype = Archetype::Skirmisher;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::Stack | Mechanic::Dash | Mechanic::Recast |
                  Mechanic::AutoReset | Mechanic::AutoWeave |
                  Mechanic::DirectionalSweet | Mechanic::MissingHealth;
    p.Ultimate = UltimatePolicy::RecastControl;

    p.Spells[0] = Spell(
        SDK::SpellSlot::Q, "Snip Snip!", CastKind::Direction,
        Intent::Damage | Intent::Execute | Intent::Waveclear |
            Intent::Jungle | Intent::LastHit,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::LastHit | Mode::Automatic,
        450.0f, 0.50f, 210.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCone);
    p.Spells[0].Priority = 94;
    p.Spells[0].TriggerRange = 475.0f;
    p.Spells[0].Aim = AimPolicy::Prediction;
    p.Spells[0].PreserveAutoAttack = true;
    p.Spells[0].AllowOnMinions = true;
    p.Spells[0].ComboManaPercent = 0.0f;
    p.Spells[0].HarassManaPercent = 35.0f;
    p.Spells[0].ClearManaPercent = 45.0f;

    p.Spells[1] = Spell(
        SDK::SpellSlot::W, "Hallowed Mist", CastKind::Self,
        Intent::Buff | Intent::Disengage | Intent::Peel |
            Intent::Recast,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::Automatic,
        0.0f, 0.0f, 370.0f, FLT_MAX, false,
        SDK::DamageType::True, SDK::SpellType::SkillshotCircle);
    p.Spells[1].Priority = 99;
    p.Spells[1].TriggerRange = 1300.0f;
    p.Spells[1].Aim = AimPolicy::SelfPosition;
    p.Spells[1].PreserveAutoAttack = true;
    p.Spells[1].RecastSpellName = "GwenWRecast";

    p.Spells[2] = Spell(
        SDK::SpellSlot::E, "Skip 'n Slash", CastKind::Position,
        Intent::Mobility | Intent::Engage | Intent::Disengage |
            Intent::Buff | Intent::AutoReset | Intent::Waveclear |
            Intent::Jungle,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::Flee,
        350.0f, 0.0f, 20.0f, 800.0f, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[2].Priority = 96;
    p.Spells[2].TriggerRange = 650.0f;
    p.Spells[2].DashDistance = 350.0f;
    p.Spells[2].Aim = AimPolicy::SafeCursor;
    p.Spells[2].PreserveAutoAttack = false;
    p.Spells[2].WeaveAfterAttack = true;
    p.Spells[2].MaximumEnemiesAtDestination = 2;

    p.Spells[3] = Spell(
        SDK::SpellSlot::R, "Needlework", CastKind::Line,
        Intent::Damage | Intent::CrowdControl | Intent::Execute |
            Intent::Heal | Intent::Recast | Intent::Finisher,
        Mode::Combo | Mode::Flee | Mode::Automatic,
        1200.0f, 0.25f, 120.0f, 1800.0f, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[3].Priority = 98;
    p.Spells[3].TriggerRange = 1300.0f;
    p.Spells[3].Aim = AimPolicy::Prediction;
    p.Spells[3].Hitchance = SDK::HitChance::High;
    p.Spells[3].PreserveAutoAttack = true;
    p.Spells[3].RecastSpellName = "GwenRRecast";
    p.Spells[3].MinimumAoeTargets = 2;

    p.Trade = Plan(
        "stack scissors with attacks, center Q, E only to a safe AA endpoint",
        Step(SDK::SpellSlot::Q,
             StepRule::RequireTarget | StepRule::RequireInsideAaRange),
        Step(SDK::SpellSlot::E,
             StepRule::RequireTarget | StepRule::RequireAfterAttack |
                 StepRule::RequireSafePosition));

    p.AllIn = Plan(
        "R1 slow, E-AA, centered stacked Q, preserve and aim both R recasts",
        Step(SDK::SpellSlot::R,
             StepRule::RequireTarget | StepRule::RequireFirstCast),
        Step(SDK::SpellSlot::E,
             StepRule::RequireTarget | StepRule::RequireSafePosition),
        Step(SDK::SpellSlot::Q,
             StepRule::RequireTarget | StepRule::RequireInsideAaRange),
        Step(SDK::SpellSlot::R,
             StepRule::RequireTarget | StepRule::RequireRecast));

    p.Flee = Plan(
        "mist ranged pressure, dash to a safe cursor endpoint, slow pursuers",
        Step(SDK::SpellSlot::W,
             StepRule::AllowDuringWindup),
        Step(SDK::SpellSlot::E,
             StepRule::RequireSafePosition | StepRule::AllowDuringWindup),
        Step(SDK::SpellSlot::R,
             StepRule::RequireTarget | StepRule::AllowDuringWindup));

    p.PreferredCombatDistance = 300.0f;
    p.EngageHealthPercent = 62.0f;
    p.DefensiveHealthPercent = 36.0f;
    p.UltimateTargetHealthPercent = 72.0f;
    p.UltimateMinimumTargets = 2;
    p.MaximumCommitEnemies = 2;
    p.BaseHumanizerMs = 42;
    p.PreferSelectedTarget = true;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "GwenQStacker";
    p.FormBuff = "GwenW_GwenInsideW";
    p.UltimateBuff = "GwenRRecast";
    p.ThemeFrom = 0xFF72D8FFu;
    p.ThemeTo = 0xFFC991FFu;
    p.ThemeSpeed = 0.76f;
    p.TacticalSummary =
        "Build four Snip stacks without dropping attacks, place Q's narrow "
        "center on predicted movement, use mist only against outside pressure, "
        "dash to evaluated attack endpoints, and independently predict all "
        "three Needlework casts before their recast window expires.";
    p.ResearchSummary =
        "Pinned to Riot 26.15 and CommunityDragon PC 16.15: Q has one base "
        "mini-snip plus one per stack and a 50-percent center true conversion; "
        "W uses the 370 gameplay radius; E dashes 350; R fires 1/3/5 piercing "
        "needles with 120 width and 1800 speed.";
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
