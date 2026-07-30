#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile LeeSin = [] {
    ChampionProfile p{};
    p.ChampionName = "LeeSin";
    p.DisplayName = "Lee Sin";
    p.InternalId = "champion.kuroaio.ai.leesin";
    p.PrimaryArchetype = Archetype::Diver;
    p.Resource = ResourceModel::Energy;
    p.Mechanics = Mechanic::Recast | Mechanic::Dash | Mechanic::Mark |
                  Mechanic::AllyTarget | Mechanic::AutoWeave |
                  Mechanic::MissingHealth | Mechanic::DirectionalSweet |
                  Mechanic::ObjectTracking;
    p.Ultimate = UltimatePolicy::MultiTarget;
    p.PreferredCombatDistance = 325.0f;
    p.EngageHealthPercent = 38.0f;
    p.DefensiveHealthPercent = 30.0f;
    p.UltimateTargetHealthPercent = 48.0f;
    p.UltimateMinimumTargets = 2;
    p.MaximumCommitEnemies = 2;
    p.BaseHumanizerMs = 58;
    p.PreferSelectedTarget = true;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "blindmonkpassive_cosmetic";
    p.MarkBuff = "BlindMonkQOne";
    p.TrackedObjectToken = "Ward";
    p.ThemeFrom = 0xFFB5D53Au;
    p.ThemeTo = 0xFF2F9B5Fu;
    p.TacticalSummary =
        "Energy-aware martial diver: land an unblocked Sonic Wave, decide the "
        "Resonating Strike endpoint, use only existing safe Safeguard anchors, "
        "and expose rather than steal the behind-target Dragon's Rage setup.";
    p.ResearchSummary =
        "Riot 26.15 / CommunityDragon 16.15 Summoner's Rift contract; recast "
        "windows, Flurry energy, ward-hop ownership and kick collision are explicit.";

    p.Spells[0] = Spell(
        SDK::SpellSlot::Q, "Sonic Wave / Resonating Strike", CastKind::Line,
        Intent::Damage | Intent::Engage | Intent::Execute | Intent::Recast |
            Intent::Setup | Intent::Jungle | Intent::LastHit,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::LastHit | Mode::Flee | Mode::Automatic,
        1200.0f, 0.25f, 60.0f, 1800.0f, true,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotLine);
    p.Spells[0].Aim = AimPolicy::Prediction;
    p.Spells[0].Priority = 92;
    p.Spells[0].Hitchance = SDK::HitChance::High;
    p.Spells[0].HarassManaPercent = 35.0f;
    p.Spells[0].ClearManaPercent = 30.0f;
    p.Spells[0].WeaveAfterAttack = true;

    p.Spells[1] = Spell(
        SDK::SpellSlot::W, "Safeguard / Iron Will", CastKind::AllyTarget,
        Intent::Mobility | Intent::Shield | Intent::AllyUtility |
            Intent::Disengage | Intent::Recast | Intent::Buff,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::Flee | Mode::Automatic,
        700.0f, 0.0f, 0.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::Targeted);
    p.Spells[1].Aim = AimPolicy::SafeCursor;
    p.Spells[1].Priority = 94;
    p.Spells[1].DashDistance = 700.0f;
    p.Spells[1].MaximumEnemiesAtDestination = 2;
    p.Spells[1].AllowOnMinions = true;
    p.Spells[1].WeaveAfterAttack = true;

    p.Spells[2] = Spell(
        SDK::SpellSlot::E, "Tempest / Cripple", CastKind::Self,
        Intent::Damage | Intent::CrowdControl | Intent::Recast |
            Intent::Waveclear | Intent::Jungle | Intent::Peel,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::LastHit | Mode::Flee | Mode::Automatic,
        450.0f, 0.25f, 450.0f, FLT_MAX, false,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotCircle);
    p.Spells[2].Aim = AimPolicy::SelfPosition;
    p.Spells[2].Priority = 84;
    p.Spells[2].MinimumAoeTargets = 3;
    p.Spells[2].HarassManaPercent = 35.0f;
    p.Spells[2].ClearManaPercent = 35.0f;
    p.Spells[2].WeaveAfterAttack = true;

    p.Spells[3] = Spell(
        SDK::SpellSlot::R, "Dragon's Rage", CastKind::EnemyTarget,
        Intent::Damage | Intent::CrowdControl | Intent::Peel |
            Intent::Interrupt | Intent::Execute | Intent::Setup |
            Intent::AntiGapcloser,
        Mode::Combo | Mode::Flee | Mode::Automatic,
        375.0f, 0.25f, 200.0f, FLT_MAX, false,
        SDK::DamageType::Physical, SDK::SpellType::Targeted);
    p.Spells[3].Aim = AimPolicy::BehindTarget;
    p.Spells[3].Priority = 98;
    p.Spells[3].MinimumAoeTargets = 2;
    p.Spells[3].TargetHealthPercent = 48.0f;

    p.Variants[0] = { SDK::SpellSlot::Q, "BlindMonkQTwo", p.Spells[0] };
    p.Variants[0].Spec.Kind = CastKind::EnemyTarget;
    p.Variants[0].Spec.Range = 1300.0f;
    p.Variants[0].Spec.Collision = false;
    p.Variants[1] = { SDK::SpellSlot::W, "BlindMonkWTwo", p.Spells[1] };
    p.Variants[1].Spec.Kind = CastKind::Self;
    p.Variants[1].Spec.Range = 0.0f;
    p.Variants[2] = { SDK::SpellSlot::E, "BlindMonkETwo", p.Spells[2] };
    p.Variants[2].Spec.Range = 500.0f;
    p.VariantCount = 3;

    p.Trade = Plan("Flurry short trade",
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequireFirstCast, 0, 800),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequireMark |
             StepRule::RequireSafePosition | StepRule::RequireRecast, 180, 1150),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireAfterAttack, 260, 1300),
        Step(SDK::SpellSlot::W, StepRule::RequireSafePosition, 420, 1600));
    p.AllIn = Plan("Marked kick-line all-in",
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequireFirstCast, 0, 750),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequireMark |
             StepRule::RequireSafePosition | StepRule::RequireRecast, 140, 1050),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireAfterAttack, 220, 1200),
        Step(SDK::SpellSlot::R, StepRule::RequireTarget | StepRule::SkipIfKillableWithout, 320, 1450),
        Step(SDK::SpellSlot::E, StepRule::RequireRecast, 400, 1600));
    p.Flee = Plan("Existing-anchor escape",
        Step(SDK::SpellSlot::W, StepRule::RequireSafePosition, 0, 800),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireRecast, 100, 950),
        Step(SDK::SpellSlot::R, StepRule::RequireTarget | StepRule::RequireSafePosition, 180, 1100));
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
