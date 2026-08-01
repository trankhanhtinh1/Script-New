#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Leblanc = [] {
    ChampionProfile p{};
    p.ChampionId = SDK::ChampionId::Leblanc;
    p.DisplayName = "LeBlanc";
    p.InternalId = "champion.kuroaio.ai.leblanc";
    p.PrimaryArchetype = Archetype::Assassin;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::Recast | Mechanic::Dash | Mechanic::Blink |
                  Mechanic::ObjectTracking | Mechanic::Mark | Mechanic::Pet |
                  Mechanic::Tether | Mechanic::MultiForm;
    p.Ultimate = UltimatePolicy::RecastControl;
    p.PreferredCombatDistance = 620.0f;
    p.EngageHealthPercent = 42.0f;
    p.DefensiveHealthPercent = 32.0f;
    p.UltimateTargetHealthPercent = 65.0f;
    p.UltimateMinimumTargets = 1;
    p.MaximumCommitEnemies = 2;
    p.BaseHumanizerMs = 64;
    p.PreferSelectedTarget = true;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "LeblancP";
    p.MarkBuff = "LeblancQMark";
    p.TrackedObjectToken = "Leblanc";
    p.ThemeFrom = 0xFFA938FFu;
    p.ThemeTo = 0xFF6436C7u;
    p.TacticalSummary =
        "Stateful burst assassin: detonate Sigil, preserve Chain tether, choose the live "
        "Mimic form, and retreat only through a verified Distortion return pad.";
    p.ResearchSummary =
        "Riot 26.15 / CommunityDragon 16.15 Summoner's Rift baseline; Arena and "
        "ARAM-specific modifiers are excluded.";

    p.Spells[0] = Spell(
        SDK::SpellSlot::Q, "Sigil of Malice", CastKind::EnemyTarget,
        Intent::Damage | Intent::Setup | Intent::Execute | Intent::LastHit,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::LastHit | Mode::Automatic,
        700.0f, 0.25f, 0.0f, 2000.0f, false,
        SDK::DamageType::Magical, SDK::SpellType::Targeted);
    p.Spells[0].Aim = AimPolicy::TargetPosition;
    p.Spells[0].Priority = 88;
    p.Spells[0].HarassManaPercent = 38.0f;
    p.Spells[0].ClearManaPercent = 45.0f;

    p.Spells[1] = Spell(
        SDK::SpellSlot::W, "Distortion / Return", CastKind::Position,
        Intent::Damage | Intent::Mobility | Intent::Engage | Intent::Disengage |
            Intent::Waveclear | Intent::Jungle | Intent::Recast,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::Flee | Mode::Automatic,
        600.0f, 0.25f, 240.0f, 1600.0f, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[1].Aim = AimPolicy::Prediction;
    p.Spells[1].Priority = 90;
    p.Spells[1].DashDistance = 600.0f;
    p.Spells[1].MaximumEnemiesAtDestination = 2;
    p.Spells[1].ClearManaPercent = 50.0f;

    p.Spells[2] = Spell(
        SDK::SpellSlot::E, "Ethereal Chains", CastKind::Line,
        Intent::Damage | Intent::CrowdControl | Intent::Setup | Intent::Peel |
            Intent::Interrupt | Intent::AntiGapcloser,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::Automatic,
        925.0f, 0.25f, 110.0f, 1750.0f, true,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[2].Aim = AimPolicy::Prediction;
    p.Spells[2].Priority = 92;
    p.Spells[2].Hitchance = SDK::HitChance::High;
    p.Spells[2].HarassManaPercent = 42.0f;

    p.Spells[3] = Spell(
        SDK::SpellSlot::R, "Mimic", CastKind::AnyTarget,
        Intent::Damage | Intent::CrowdControl | Intent::Mobility | Intent::Engage |
            Intent::Disengage | Intent::Execute | Intent::Setup | Intent::Recast,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::Automatic,
        925.0f, 0.25f, 110.0f, 1750.0f, true,
        SDK::DamageType::Magical, SDK::SpellType::Targeted);
    p.Spells[3].Aim = AimPolicy::Prediction;
    p.Spells[3].Priority = 96;
    p.Spells[3].TargetHealthPercent = 65.0f;
    p.Spells[3].MaximumEnemiesAtDestination = 2;

    p.Variants[0] = { SDK::SpellSlot::W, "LeblancWReturn", p.Spells[1] };
    p.Variants[0].Spec.Intents = Intent::Mobility | Intent::Disengage | Intent::Recast;
    p.Variants[0].Spec.Kind = CastKind::Self;
    p.Variants[1] = { SDK::SpellSlot::R, "LeblancRQ", p.Spells[3] };
    p.Variants[1].Spec.Kind = CastKind::EnemyTarget;
    p.Variants[1].Spec.Range = 700.0f;
    p.Variants[1].Spec.Collision = false;
    p.Variants[2] = { SDK::SpellSlot::R, "LeblancRW", p.Spells[1] };
    p.Variants[3] = { SDK::SpellSlot::R, "LeblancRWReturn", p.Spells[1] };
    p.Variants[3].Spec.Intents = Intent::Mobility | Intent::Disengage | Intent::Recast;
    p.Variants[3].Spec.Kind = CastKind::Self;
    p.Variants[4] = { SDK::SpellSlot::R, "LeblancRE", p.Spells[2] };
    p.VariantCount = 5;

    p.Trade = Plan("Sigil detonation trade",
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequireNoMark, 0, 700),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget | StepRule::RequireMark |
             StepRule::RequireSafePosition, 100, 1000),
        Step(SDK::SpellSlot::W, StepRule::RequireRecast | StepRule::RequireSafePosition, 350, 1500));
    p.AllIn = Plan("Mimic burst and chain",
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequireNoMark, 0, 700),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget | StepRule::RequireMark |
             StepRule::RequireSafePosition, 80, 900),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget, 180, 1150),
        Step(SDK::SpellSlot::R, StepRule::RequireTarget, 260, 1400));
    p.Flee = Plan("Verified return escape",
        Step(SDK::SpellSlot::W, StepRule::RequireRecast | StepRule::RequireSafePosition, 0, 550),
        Step(SDK::SpellSlot::R, StepRule::RequireRecast | StepRule::RequireSafePosition, 50, 650),
        Step(SDK::SpellSlot::W, StepRule::RequireFirstCast | StepRule::RequireSafePosition, 120, 900),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireNoCrowdControl, 220, 1100));
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
