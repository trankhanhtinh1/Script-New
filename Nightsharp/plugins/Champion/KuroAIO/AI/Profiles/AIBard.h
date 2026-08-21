#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

// Bard is a catcher whose real kit is collision ordering, mixed-team stasis,
// terrain topology and map tempo. The profile is descriptive; AIBardController
// owns every Q continuation, shrine, portal and no-grief R decision.
inline constexpr ChampionProfile Bard = [] {
    ChampionProfile p{};
    p.ChampionId = SDK::ChampionId::Bard;
    p.DisplayName = "Bard";
    p.InternalId = "champion.kuroaio.ai.bard";
    p.PrimaryArchetype = Archetype::Catcher;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::Stack | Mechanic::Pet |
                  Mechanic::ObjectTracking | Mechanic::AutoWeave |
                  Mechanic::Ammo | Mechanic::Terrain |
                  Mechanic::WallInteraction | Mechanic::AllyTarget |
                  Mechanic::Dash;
    p.Ultimate = UltimatePolicy::MultiTarget;

    p.Spells[0] = Spell(
        SDK::SpellSlot::Q, "Cosmic Binding", CastKind::Line,
        Intent::Damage | Intent::CrowdControl | Intent::Peel |
            Intent::Interrupt | Intent::AntiGapcloser | Intent::Setup |
            Intent::Waveclear | Intent::Jungle,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::Flee | Mode::Automatic,
        850.0f, 0.25f, 60.0f, 1500.0f, true,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[0].Priority = 98;
    p.Spells[0].Aim = AimPolicy::BehindTarget;
    p.Spells[0].Hitchance = SDK::HitChance::High;
    p.Spells[0].PreserveAutoAttack = true;
    p.Spells[0].WeaveAfterAttack = true;
    p.Spells[0].AllowOnMinions = true;
    p.Spells[0].HarassManaPercent = 48.0f;
    p.Spells[0].ClearManaPercent = 62.0f;

    p.Spells[1] = Spell(
        SDK::SpellSlot::W, "Caretaker's Shrine", CastKind::AnyTarget,
        Intent::Heal | Intent::Buff | Intent::AllyUtility |
            Intent::Disengage | Intent::Vision | Intent::Setup,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::Automatic,
        800.0f, 0.25f, 100.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[1].Priority = 94;
    p.Spells[1].Aim = AimPolicy::SafeCursor;
    p.Spells[1].MinimumAmmo = 1;
    p.Spells[1].PreserveAutoAttack = true;
    p.Spells[1].HarassManaPercent = 28.0f;

    p.Spells[2] = Spell(
        SDK::SpellSlot::E, "Magical Journey", CastKind::Direction,
        Intent::Mobility | Intent::Engage | Intent::Disengage |
            Intent::Setup | Intent::AllyUtility | Intent::Vision,
        Mode::Combo | Mode::Flee | Mode::Automatic,
        900.0f, 0.25f, 150.0f, FLT_MAX, false,
        SDK::DamageType::True, SDK::SpellType::SkillshotLine);
    p.Spells[2].Priority = 92;
    p.Spells[2].Aim = AimPolicy::SafeCursor;
    p.Spells[2].DashDistance = 2600.0f;
    p.Spells[2].MaximumEnemiesAtDestination = 2;
    p.Spells[2].PreserveAutoAttack = true;

    p.Spells[3] = Spell(
        SDK::SpellSlot::R, "Tempered Fate", CastKind::Circle,
        Intent::CrowdControl | Intent::Engage | Intent::Disengage |
            Intent::Peel | Intent::Setup | Intent::AllyUtility |
            Intent::Objective | Intent::Vision,
        Mode::Combo | Mode::Flee | Mode::Automatic,
        3400.0f, 0.50f, 350.0f, 2100.0f, false,
        SDK::DamageType::True, SDK::SpellType::SkillshotCircle);
    p.Spells[3].Priority = 99;
    p.Spells[3].Aim = AimPolicy::BestAoe;
    p.Spells[3].Hitchance = SDK::HitChance::VeryHigh;
    p.Spells[3].MinimumAoeTargets = 1;
    p.Spells[3].PreserveAutoAttack = true;

    p.Trade = Plan(
        "meep auto, hold for a real wall or second-body Q, then auto again",
        Step(SDK::SpellSlot::Q,
             StepRule::RequireTarget | StepRule::RequireAfterAttack));

    p.AllIn = Plan(
        "clean R catch, time Q for stasis exit, then weave meeps autonomously",
        Step(SDK::SpellSlot::R,
             StepRule::RequireTarget | StepRule::RequireSafePosition),
        Step(SDK::SpellSlot::Q,
             StepRule::RequireTarget | StepRule::RequireCrowdControl),
        Step(SDK::SpellSlot::E,
             StepRule::RequireSafePosition));

    p.Flee = Plan(
        "speed the retreat, expose a safe cursor-side portal, then Q/R pursuers",
        Step(SDK::SpellSlot::W,
             StepRule::RequirePlayerLow | StepRule::AllowDuringWindup),
        Step(SDK::SpellSlot::E,
             StepRule::RequireSafePosition | StepRule::AllowDuringWindup),
        Step(SDK::SpellSlot::Q,
             StepRule::RequireTarget | StepRule::AllowDuringWindup),
        Step(SDK::SpellSlot::R,
             StepRule::RequireTarget | StepRule::RequireMultiTarget |
                 StepRule::AllowDuringWindup));

    p.PreferredCombatDistance = 500.0f;
    p.EngageHealthPercent = 58.0f;
    p.DefensiveHealthPercent = 35.0f;
    p.UltimateTargetHealthPercent = 100.0f;
    p.UltimateMinimumTargets = 1;
    p.MaximumCommitEnemies = 2;
    p.BaseHumanizerMs = 34;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "BardPChimes";
    p.MarkBuff = "BardQInitialTargetDebuff";
    p.UltimateBuff = "BardRStasis";
    p.TrackedObjectToken = "BardWHealthPack";
    p.ThemeFrom = 0xFFFFD56Au;
    p.ThemeTo = 0xFF5EDFD4u;
    p.ThemeSpeed = 0.54f;
    p.TacticalSummary =
        "Automate movement, portal entry, attacks and target choice while preserving "
        "Flash and Smite safety. Use meep slow before ordinary Q, preserve Q for a true "
        "second-body/wall stun, treat chimes as route bonuses, cast E only into "
        "verified terrain, and reject any R that freezes allied damage.";
    p.ResearchSummary =
        "Pinned to Riot 26.13 and CommunityDragon PC 16.14, including the "
        "current meep nerf and 25.21 W AP ratios; reconciled live bin data, "
        "the League mechanics reference, Lathyrus' Bard Bible and 26.14 build "
        "update, Polypuff's 36-trick breakdown, current BardMains guidance, "
        "Mobalytics combos and Keria's 2026 Bard pro view.";
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
