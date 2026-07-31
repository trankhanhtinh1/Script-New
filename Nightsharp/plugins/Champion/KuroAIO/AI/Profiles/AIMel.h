#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Mel = [] {
    ChampionProfile p{};
    p.ChampionName = "Mel";
    p.DisplayName = "Mel";
    p.InternalId = "champion.kuroaio.ai.mel";
    p.PrimaryArchetype = Archetype::BurstMage;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::Mark | Mechanic::Stack | Mechanic::Execute |
                  Mechanic::SpellShield | Mechanic::Global | Mechanic::AutoWeave;
    p.Ultimate = UltimatePolicy::GlobalExecute;
    p.PreferredCombatDistance = 760.0f;
    p.EngageHealthPercent = 48.0f;
    p.DefensiveHealthPercent = 34.0f;
    p.UltimateTargetHealthPercent = 72.0f;
    p.UltimateMinimumTargets = 1;
    p.MaximumCommitEnemies = 2;
    p.BaseHumanizerMs = 42;
    p.PreferSelectedTarget = true;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "MelPassive";
    p.MarkBuff = "MelPassiveOverwhelm";
    p.ChannelBuff = "MelW";
    p.UltimateBuff = "MelR";

    p.Spells[0] = Spell(SDK::SpellSlot::Q, "Radiant Volley", CastKind::Line,
        Intent::Damage | Intent::Setup | Intent::Waveclear | Intent::Jungle |
            Intent::LastHit,
        AllModes, 950.0f, 0.35f, 140.0f, 1500.0f, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[0].Aim = AimPolicy::Prediction;
    p.Spells[0].Priority = 90;
    p.Spells[0].Hitchance = SDK::HitChance::High;
    p.Spells[0].HarassManaPercent = 42.0f;
    p.Spells[0].ClearManaPercent = 48.0f;
    p.Spells[0].PreserveAutoAttack = true;

    p.Spells[1] = Spell(SDK::SpellSlot::W, "Reflection", CastKind::Self,
        Intent::Shield | Intent::Disengage | Intent::Peel | Intent::AntiGapcloser |
            Intent::Interrupt,
        Mode::Combo | Mode::Flee | Mode::Automatic, 250.0f, 0.0f, 0.0f,
        FLT_MAX, false, SDK::DamageType::True, SDK::SpellType::Targeted);
    p.Spells[1].Aim = AimPolicy::SelfPosition;
    p.Spells[1].Priority = 100;
    p.Spells[1].PreserveAutoAttack = true;

    p.Spells[2] = Spell(SDK::SpellSlot::E, "Solar Snare", CastKind::Line,
        Intent::Damage | Intent::CrowdControl | Intent::Setup | Intent::Peel |
            Intent::Interrupt | Intent::Waveclear | Intent::Jungle,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::LastHit | Mode::Flee | Mode::Automatic,
        1000.0f, 0.25f, 140.0f, 1200.0f, true,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[2].Aim = AimPolicy::Prediction;
    p.Spells[2].Priority = 94;
    p.Spells[2].Hitchance = SDK::HitChance::High;
    p.Spells[2].HarassManaPercent = 48.0f;
    p.Spells[2].ClearManaPercent = 52.0f;
    p.Spells[2].PreserveAutoAttack = true;

    p.Spells[3] = Spell(SDK::SpellSlot::R, "Golden Eclipse", CastKind::Self,
        Intent::Damage | Intent::Execute | Intent::Finisher |
            Intent::Recast,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::Automatic,
        25000.0f, 0.0f, 0.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::Targeted);
    p.Spells[3].Aim = AimPolicy::SelfPosition;
    p.Spells[3].Priority = 100;
    p.Spells[3].MinimumAoeTargets = 1;
    p.Spells[3].PreserveAutoAttack = true;

    p.Trade = Plan("mark with Q/E, preserve Reflection for verified threat",
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequireNoMark),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireCrowdControl,
             80, 800),
        Step(SDK::SpellSlot::R, StepRule::RequireTarget | StepRule::RequireMark |
             StepRule::HoldForExecute, 180, 1000));
    p.AllIn = Plan("root, stack Overwhelm, then global execute",
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireSafePosition),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::AllowDuringWindup,
             80, 800),
        Step(SDK::SpellSlot::R, StepRule::RequireTarget | StepRule::RequireMark,
             160, 1200));
    p.Flee = Plan("reflect the committed projectile and root the pursuer",
        Step(SDK::SpellSlot::W, StepRule::RequireSafePosition | StepRule::AllowDuringWindup),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::AllowDuringWindup));

    p.TacticalSummary =
        "Long-range radiant volleys and Solar Snare build observed Overwhelm marks; "
        "Reflection is reserved for a confirmed incoming projectile or hard crowd control, "
        "and Golden Eclipse is spent only on a live mark/execute or verified multi-target window.";
    p.ResearchSummary =
        "Riot 26.15 and CommunityDragon 16.15 Mel spell metadata, passive Overwhelm "
        "mark/execute behavior, Reflection projectile denial, prediction-safe Q/E lines, "
        "and conservative global Golden Eclipse thresholds.";
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
