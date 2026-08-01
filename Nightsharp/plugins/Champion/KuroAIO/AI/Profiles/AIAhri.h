#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

// Ahri is modeled around two guarantees, not raw spell spam: Charm creates a
// reliable first pass and movement (especially Spirit Rush) bends the returning
// Orb through the victim for true damage.  R is therefore recast-controlled and
// the final charge is normally an exit, not a fourth damage button.
inline constexpr ChampionProfile Ahri = [] {
    ChampionProfile p{};
    p.ChampionId = SDK::ChampionId::Ahri;
    p.DisplayName = "Ahri";
    p.InternalId = "champion.kuroaio.ai.ahri";
    p.PrimaryArchetype = Archetype::BurstMage;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::Recast | Mechanic::Ammo | Mechanic::Dash |
                  Mechanic::ReturnProjectile | Mechanic::ObjectTracking |
                  Mechanic::AutoWeave;
    p.Ultimate = UltimatePolicy::RecastControl;

    p.Spells[0] = Spell(
        SDK::SpellSlot::Q, "Orb of Deception", CastKind::Line,
        Intent::Damage | Intent::Waveclear | Intent::Jungle |
            Intent::LastHit | Intent::Setup,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::LastHit | Mode::Automatic,
        970.0f, 0.25f, 100.0f, 1550.0f, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[0].Priority = 84;
    p.Spells[0].Hitchance = SDK::HitChance::High;
    p.Spells[0].MinimumAoeTargets = 3;
    p.Spells[0].HarassManaPercent = 42.0f;
    p.Spells[0].ClearManaPercent = 48.0f;

    p.Spells[1] = Spell(
        SDK::SpellSlot::W, "Fox-Fire", CastKind::Self,
        Intent::Damage | Intent::Buff | Intent::Mobility |
            Intent::LastHit | Intent::Jungle,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::LastHit | Mode::Flee | Mode::Automatic,
        700.0f, 0.0f, 0.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::Targeted);
    p.Spells[1].TriggerRange = 700.0f;
    p.Spells[1].Priority = 72;
    p.Spells[1].WeaveAfterAttack = true;
    p.Spells[1].AllowOnMinions = true;
    p.Spells[1].HarassManaPercent = 46.0f;
    p.Spells[1].ClearManaPercent = 58.0f;

    p.Spells[2] = Spell(
        SDK::SpellSlot::E, "Charm", CastKind::Line,
        Intent::Damage | Intent::CrowdControl | Intent::Setup |
            Intent::Interrupt | Intent::AntiGapcloser | Intent::Peel,
        Mode::Combo | Mode::Harass | Mode::Automatic | Mode::Flee,
        975.0f, 0.25f, 60.0f, 1550.0f, true,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotMissileLine);
    p.Spells[2].Priority = 98;
    p.Spells[2].Hitchance = SDK::HitChance::VeryHigh;
    p.Spells[2].HarassManaPercent = 50.0f;

    p.Spells[3] = Spell(
        SDK::SpellSlot::R, "Spirit Rush", CastKind::Position,
        Intent::Damage | Intent::Mobility | Intent::Engage |
            Intent::Disengage | Intent::Recast,
        Mode::Combo | Mode::Flee | Mode::Automatic,
        450.0f, 0.0f, 0.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[3].Aim = AimPolicy::SafeCursor;
    p.Spells[3].DashDistance = 450.0f;
    p.Spells[3].DesiredDistance = 480.0f;
    p.Spells[3].Priority = 64;
    p.Spells[3].MinimumAmmo = 1;
    p.Spells[3].MaximumEnemiesAtDestination = 2;
    p.Spells[3].RecastSpellName = "AhriTumble";

    p.Trade = Plan(
        "Charm-confirmed trade",
        Step(SDK::SpellSlot::E),
        Step(SDK::SpellSlot::Q, StepRule::RequireCrowdControl),
        Step(SDK::SpellSlot::W,
             StepRule::RequireCrowdControl | StepRule::RequireInsideAaRange));

    p.AllIn = Plan(
        "Guarantee Charm and return Orb",
        Step(SDK::SpellSlot::R,
             StepRule::RequireTarget | StepRule::RequireSafePosition |
                 StepRule::RequireFirstCast),
        Step(SDK::SpellSlot::E),
        Step(SDK::SpellSlot::Q, StepRule::RequireCrowdControl),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget),
        Step(SDK::SpellSlot::R,
             StepRule::RequireTarget | StepRule::RequireSafePosition |
                 StepRule::RequireRecast));

    p.Flee = Plan(
        "Peel then reserve dash",
        Step(SDK::SpellSlot::E, StepRule::RequireTarget),
        Step(SDK::SpellSlot::W, StepRule::None),
        Step(SDK::SpellSlot::R, StepRule::RequireSafePosition));

    p.PreferredCombatDistance = 525.0f;
    p.EngageHealthPercent = 38.0f;
    p.DefensiveHealthPercent = 27.0f;
    p.UltimateTargetHealthPercent = 62.0f;
    p.UltimateMinimumTargets = 2;
    p.MaximumCommitEnemies = 2;
    p.BaseHumanizerMs = 55;
    p.MarkBuff = "AhriSeduce";
    p.UltimateBuff = "AhriTumble";
    p.TrackedObjectToken = "AhriOrb";
    p.ThemeFrom = 0xFFFF76D5u;
    p.ThemeTo = 0xFF6B5DFFu;
    p.ThemeSpeed = 1.08f;
    p.TacticalSummary =
        "Create a guaranteed Charm window, route both Q passes through the "
        "target, use W after target marking/AA, and preserve the last R charge "
        "for exit unless it secures a kill or reset.";
    p.ResearchSummary =
        "CommunityDragon 16.14 and Meraki live kit data, local OKTW missile "
        "implementations, current Shok Ahri coaching, pro combo demonstrations, "
        "and return-projectile geometry tests.";
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
