#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Lillia = [] {
    ChampionProfile p{};
    p.ChampionName = "Lillia";
    p.DisplayName = "Lillia";
    p.InternalId = "champion.kuroaio.ai.lillia";
    p.PrimaryArchetype = Archetype::Battlemage;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::ObjectTracking | Mechanic::Mark | Mechanic::Stack |
                  Mechanic::WallInteraction | Mechanic::DirectionalSweet;
    p.Ultimate = UltimatePolicy::MultiTarget;
    p.PreferredCombatDistance = 390.0f;
    p.EngageHealthPercent = 48.0f;
    p.DefensiveHealthPercent = 34.0f;
    p.UltimateTargetHealthPercent = 70.0f;
    p.UltimateMinimumTargets = 1;
    p.MaximumCommitEnemies = 3;
    p.BaseHumanizerMs = 62;
    p.PreferSelectedTarget = true;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "LilliaPassive";
    p.MarkBuff = "LilliaRSleepMark";
    p.ChannelBuff = "LilliaR";
    p.UltimateBuff = "LilliaR";
    p.TrackedObjectToken = "LilliaE";
    p.TacticalSummary =
        "Battlemage skirmisher: build Dream-Laden Bough movement stacks, favor Q outer-ring true damage, reserve W center strikes for reliable setups, bowl E through clear corridors, and sleep marked targets while kiting away from unsafe clusters.";
    p.ResearchSummary =
        "Riot 26.15 has no Lillia kit changes; values and runtime state use the CommunityDragon 16.15 Lillia dossier.";

    p.Spells[0] = Spell(SDK::SpellSlot::Q, "Blooming Blows", CastKind::Circle,
        Intent::Damage | Intent::Waveclear | Intent::Jungle | Intent::LastHit | Intent::Buff,
        AllModes, 450.0f, 0.25f, 450.0f, FLT_MAX, false,
        SDK::DamageType::True, SDK::SpellType::SkillshotCircle);
    p.Spells[0].Aim = AimPolicy::TargetPosition;
    p.Spells[0].Priority = 92;
    p.Spells[0].PreserveAutoAttack = true;
    p.Spells[0].HarassManaPercent = 52.0f;
    p.Spells[0].ClearManaPercent = 42.0f;

    p.Spells[1] = Spell(SDK::SpellSlot::W, "Watch Out! Eep!", CastKind::Circle,
        Intent::Damage | Intent::CrowdControl | Intent::Setup | Intent::Finisher,
        Mode::Combo | Mode::Harass | Mode::Automatic,
        500.0f, 0.75f, 160.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[1].Aim = AimPolicy::Prediction;
    p.Spells[1].Priority = 96;
    p.Spells[1].PreserveAutoAttack = true;
    p.Spells[1].HarassManaPercent = 60.0f;

    p.Spells[2] = Spell(SDK::SpellSlot::E, "Swirlseed", CastKind::Line,
        Intent::Damage | Intent::CrowdControl | Intent::Setup | Intent::Peel |
            Intent::Interrupt | Intent::Vision,
        Mode::Combo | Mode::Harass | Mode::Automatic | Mode::Flee,
        1600.0f, 0.40f, 110.0f, 1400.0f, true,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[2].Aim = AimPolicy::Prediction;
    p.Spells[2].Priority = 88;
    p.Spells[2].Hitchance = SDK::HitChance::High;
    p.Spells[2].PreserveAutoAttack = true;

    p.Spells[3] = Spell(SDK::SpellSlot::R, "Lilting Lullaby", CastKind::Self,
        Intent::CrowdControl | Intent::Damage | Intent::Engage | Intent::Disengage |
            Intent::Finisher | Intent::Peel,
        Mode::Combo | Mode::Automatic | Mode::Flee,
        1600.0f, 0.25f, 0.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[3].Aim = AimPolicy::SelfPosition;
    p.Spells[3].Priority = 100;
    p.Spells[3].MinimumAoeTargets = 1;
    p.Spells[3].PlayerHealthPercent = 45.0f;

    p.Trade = Plan("Outer-ring Dream trade",
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::AllowDuringWindup, 0, 850),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget, 80, 1500),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget, 180, 1000));
    p.AllIn = Plan("Marked sleep kite collapse",
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget, 0, 800),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget, 100, 1700),
        Step(SDK::SpellSlot::R, StepRule::RequireTarget | StepRule::RequireMark | StepRule::RequireSafePosition, 160, 1800),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget | StepRule::RequireSafePosition, 300, 1000),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget, 700, 1000));
    p.Flee = Plan("Dream-speed kite and sleep peel",
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::AllowDuringWindup, 0, 700),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireSafePosition, 80, 1600),
        Step(SDK::SpellSlot::R, StepRule::RequireTarget | StepRule::RequireMark | StepRule::RequireSafePosition, 150, 1800));
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
