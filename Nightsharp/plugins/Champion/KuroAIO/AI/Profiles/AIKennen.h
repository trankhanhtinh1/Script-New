#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Kennen = [] {
    ChampionProfile p{};
    p.ChampionId = SDK::ChampionId::Kennen;
    p.DisplayName = "Kennen";
    p.InternalId = "champion.kuroaio.ai.kennen";
    p.PrimaryArchetype = Archetype::Battlemage;
    p.Resource = ResourceModel::Energy;
    p.Mechanics = Mechanic::Mark | Mechanic::Stack | Mechanic::Dash |
                  Mechanic::Channel | Mechanic::AutoWeave;
    p.Ultimate = UltimatePolicy::MultiTarget;
    p.PreferredCombatDistance = 700.0f;
    p.EngageHealthPercent = 52.0f;
    p.DefensiveHealthPercent = 34.0f;
    p.UltimateTargetHealthPercent = 68.0f;
    p.UltimateMinimumTargets = 2;
    p.MaximumCommitEnemies = 3;
    p.BaseHumanizerMs = 42;
    p.PreferSelectedTarget = true;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "KennenMarkOfTheStorm";
    p.MarkBuff = "KennenMarkOfTheStorm";
    p.ChannelBuff = "KennenLightningRush";
    p.UltimateBuff = "KennenShurikenStorm";
    p.TacticalSummary =
        "Energy-aware storm skirmisher: build Mark of the Storm stacks with Q, W, E and R, reserve the stun for the highest-value target, and enter Lightning Rush only when the cursor route and exit posture are safe.";
    p.ResearchSummary =
        "Riot 26.15 / CommunityDragon 16.15: Kennen Q/W/E/R ranges and energy costs, six-second Mark of the Storm decay, three-stack stun, Lightning Rush posture, and conservative multi-target Slicing Maelstrom safety.";

    p.Spells[0] = Spell(SDK::SpellSlot::Q, "Thundering Shuriken", CastKind::Position,
        Intent::Damage | Intent::CrowdControl | Intent::Setup | Intent::LastHit |
            Intent::Jungle | Intent::Waveclear,
        AllModes, 1050.0f, 0.18f, 50.0f, 1700.0f, true,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[0].Aim = AimPolicy::Prediction;
    p.Spells[0].Priority = 92;
    p.Spells[0].Hitchance = SDK::HitChance::High;
    p.Spells[0].HarassManaPercent = 22.0f;
    p.Spells[0].ClearManaPercent = 18.0f;

    p.Spells[1] = Spell(SDK::SpellSlot::W, "Electrical Surge", CastKind::Self,
        Intent::Damage | Intent::CrowdControl | Intent::Setup | Intent::Waveclear |
            Intent::Jungle | Intent::LastHit | Intent::Peel,
        AllModes, 775.0f, 0.0f, 300.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[1].Aim = AimPolicy::SelfPosition;
    p.Spells[1].Priority = 95;
    p.Spells[1].MinimumAoeTargets = 1;
    p.Spells[1].HarassManaPercent = 36.0f;
    p.Spells[1].ClearManaPercent = 32.0f;

    p.Spells[2] = Spell(SDK::SpellSlot::E, "Lightning Rush", CastKind::Position,
        Intent::Damage | Intent::Mobility | Intent::Engage | Intent::Disengage |
            Intent::AntiGapcloser | Intent::Peel | Intent::Jungle,
        AllModes, 2000.0f, 0.0f, 200.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[2].Aim = AimPolicy::SafeCursor;
    p.Spells[2].Priority = 88;
    p.Spells[2].HarassManaPercent = 42.0f;
    p.Spells[2].ClearManaPercent = 40.0f;

    p.Spells[3] = Spell(SDK::SpellSlot::R, "Slicing Maelstrom", CastKind::Self,
        Intent::Damage | Intent::CrowdControl | Intent::Engage | Intent::Disengage |
            Intent::Peel | Intent::Interrupt | Intent::Setup | Intent::Channel,
        Mode::Combo | Mode::Flee | Mode::Automatic, 550.0f, 0.0f, 550.0f,
        FLT_MAX, false, SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[3].Aim = AimPolicy::SelfPosition;
    p.Spells[3].Priority = 100;
    p.Spells[3].MinimumAoeTargets = 2;
    p.Spells[3].HarassManaPercent = 0.0f;

    p.Trade = Plan("Mark then surge",
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequireSafePosition, 0, 900),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget | StepRule::RequireMark, 90, 900),
        Step(SDK::SpellSlot::E, StepRule::RequireSafePosition | StepRule::AllowDuringWindup, 160, 900));
    p.AllIn = Plan("Storm entry and stun",
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget, 0, 900),
        Step(SDK::SpellSlot::E, StepRule::RequireSafePosition, 80, 900),
        Step(SDK::SpellSlot::R, StepRule::RequireMultiTarget | StepRule::RequireSafePosition, 130, 1500),
        Step(SDK::SpellSlot::W, StepRule::RequireMark | StepRule::AllowDuringWindup, 240, 900));
    p.Flee = Plan("Lightning Rush peel",
        Step(SDK::SpellSlot::E, StepRule::RequireSafePosition | StepRule::AllowDuringWindup, 0, 700),
        Step(SDK::SpellSlot::R, StepRule::RequireMultiTarget | StepRule::AllowDuringWindup, 80, 900),
        Step(SDK::SpellSlot::W, StepRule::RequireMark | StepRule::AllowDuringWindup, 180, 900));
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
