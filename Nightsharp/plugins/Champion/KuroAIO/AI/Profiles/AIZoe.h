#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Zoe = [] {
    ChampionProfile p{};
    p.ChampionId = SDK::ChampionId::Zoe;
    p.DisplayName = "Zoe";
    p.InternalId = "champion.kuroaio.ai.zoe";
    p.PrimaryArchetype = Archetype::BurstMage;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::Recast | Mechanic::ObjectTracking | Mechanic::Terrain |
                  Mechanic::ReturnProjectile | Mechanic::Possession | Mechanic::AutoWeave;
    p.Ultimate = UltimatePolicy::RecastControl;
    p.PreferredCombatDistance = 650.0f;
    p.EngageHealthPercent = 46.0f;
    p.DefensiveHealthPercent = 29.0f;
    p.UltimateTargetHealthPercent = 58.0f;
    p.UltimateMinimumTargets = 1;
    p.MaximumCommitEnemies = 2;
    p.BaseHumanizerMs = 58;
    p.PreferSelectedTarget = true;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.MarkBuff = "zoeqpassive";
    p.UltimateBuff = "ZoePortalJump";
    p.TrackedObjectToken = "ZoeQMissile";

    p.Spells[0] = Spell(SDK::SpellSlot::Q, "Paddle Star", CastKind::Position,
        Intent::Damage | Intent::Recast | Intent::Waveclear | Intent::Jungle |
            Intent::LastHit | Intent::Setup,
        AllModes, 1600.0f, 0.25f, 50.0f, 1200.0f, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[0].Aim = AimPolicy::Prediction;
    p.Spells[0].Priority = 92;
    p.Spells[0].Hitchance = SDK::HitChance::High;
    p.Spells[0].RecastSpellName = "ZoeQ2";
    p.Spells[0].HarassManaPercent = 38.0f;
    p.Spells[0].ClearManaPercent = 48.0f;

    p.Spells[1] = Spell(SDK::SpellSlot::W, "Spell Thief", CastKind::Self,
        Intent::Damage | Intent::Buff | Intent::Mobility,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::Automatic,
        800.0f, 0.0f, 0.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::Targeted);
    p.Spells[1].Priority = 74;
    p.Spells[1].WeaveAfterAttack = true;
    p.Spells[1].PreserveAutoAttack = true;
    p.Spells[1].RecastSpellName = "ZoeW";
    p.Spells[1].HarassManaPercent = 42.0f;

    p.Spells[2] = Spell(SDK::SpellSlot::E, "Sleepy Trouble Bubble", CastKind::Position,
        Intent::Damage | Intent::CrowdControl | Intent::Setup | Intent::Interrupt |
            Intent::AntiGapcloser | Intent::Peel,
        Mode::Combo | Mode::Harass | Mode::Automatic | Mode::Flee,
        1400.0f, 0.30f, 55.0f, 1700.0f, true,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[2].Aim = AimPolicy::Prediction;
    p.Spells[2].Priority = 100;
    p.Spells[2].Hitchance = SDK::HitChance::VeryHigh;
    p.Spells[2].HarassManaPercent = 48.0f;

    p.Spells[3] = Spell(SDK::SpellSlot::R, "Portal Jump", CastKind::Position,
        Intent::Mobility | Intent::Engage | Intent::Disengage | Intent::Recast |
            Intent::Setup,
        Mode::Combo | Mode::Flee | Mode::Automatic,
        575.0f, 0.0f, 0.0f, FLT_MAX, false,
        SDK::DamageType::True, SDK::SpellType::SkillshotCircle);
    p.Spells[3].Aim = AimPolicy::SafeCursor;
    p.Spells[3].DashDistance = 575.0f;
    p.Spells[3].DesiredDistance = 525.0f;
    p.Spells[3].Priority = 66;
    p.Spells[3].RecastSpellName = "ZoeR2";

    p.Trade = Plan("Bubble-confirmed Paddle trade",
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireSafePosition),
        Step(SDK::SpellSlot::Q, StepRule::RequireCrowdControl | StepRule::RequireTarget),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget | StepRule::AllowDuringWindup));
    p.AllIn = Plan("Sleep into return Paddle burst",
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireSafePosition),
        Step(SDK::SpellSlot::R, StepRule::RequireSafePosition | StepRule::RequireFirstCast),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget),
        Step(SDK::SpellSlot::Q, StepRule::RequireRecast | StepRule::RequireTarget),
        Step(SDK::SpellSlot::R, StepRule::RequireRecast | StepRule::RequireSafePosition));
    p.Flee = Plan("Bubble peel and portal return",
        Step(SDK::SpellSlot::E, StepRule::RequireSafePosition),
        Step(SDK::SpellSlot::R, StepRule::RequireSafePosition | StepRule::RequireFirstCast));

    p.TacticalSummary =
        "Collision-aware Sleepy Trouble Bubble creates the sleep window; Paddle Star is deliberately recast from a safe return angle, Spell Thief follows observed pickups without guessing their shape, and Portal Jump only blinks to a wall-safe endpoint with an explicit return plan.";
    p.ResearchSummary =
        "Riot 26.15 / CommunityDragon 16.15 spell metadata and buff names, validated against live Zoe projectile naming, wall-extension behavior, Q return timing, W stolen-spell variants, and conservative portal endpoint safety.";
    p.ThemeFrom = 0xFFFF8ACBu;
    p.ThemeTo = 0xFF9D62FFu;
    p.ThemeSpeed = 1.12f;
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
