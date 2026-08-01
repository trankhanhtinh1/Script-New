#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Yorick = [] {
    ChampionProfile p{};
    p.ChampionId = SDK::ChampionId::Yorick;
    p.DisplayName = "Yorick";
    p.InternalId = "champion.kuroaio.ai.yorick";
    p.PrimaryArchetype = Archetype::Juggernaut;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::Pet | Mechanic::ObjectTracking | Mechanic::Mark |
                  Mechanic::Terrain | Mechanic::WallInteraction |
                  Mechanic::AutoReset | Mechanic::AutoWeave;
    p.Ultimate = UltimatePolicy::MultiTarget;
    p.PreferredCombatDistance = 325.0f;
    p.EngageHealthPercent = 52.0f;
    p.DefensiveHealthPercent = 34.0f;
    p.UltimateTargetHealthPercent = 68.0f;
    p.UltimateMinimumTargets = 1;
    p.MaximumCommitEnemies = 2;
    p.BaseHumanizerMs = 48;
    p.PreferSelectedTarget = true;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "YorickPassive";
    p.MarkBuff = "YorickE";
    p.ChannelBuff = "YorickR";
    p.UltimateBuff = "YorickR";
    p.TrackedObjectToken = "yorickmaiden";
    p.TacticalSummary =
        "Raise Mist Walkers from confirmed graves, use Last Rites as an auto reset, "
        "shape Dark Procession cages, land marked Mourning Mist and release Maiden "
        "only when a split push or fight has a safe retreat and wave.";
    p.ResearchSummary =
        "Riot 26.15 and CommunityDragon 16.15 preserve the grave/walker loop, "
        "targeted Q reset, terrain cage, marked E projectile and autonomous Maiden.";

    p.Spells[0] = Spell(
        SDK::SpellSlot::Q, "Last Rites", CastKind::Self,
        Intent::Damage | Intent::Buff | Intent::AutoReset | Intent::Setup |
            Intent::Waveclear | Intent::Jungle | Intent::LastHit,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::LastHit | Mode::Automatic,
        325.0f, 0.0f, 0.0f, FLT_MAX, false,
        SDK::DamageType::Physical, SDK::SpellType::Targeted);
    p.Spells[0].Aim = AimPolicy::SelfPosition;
    p.Spells[0].Priority = 98;
    p.Spells[0].WeaveAfterAttack = true;
    p.Spells[0].PreserveAutoAttack = false;

    p.Spells[1] = Spell(
        SDK::SpellSlot::W, "Dark Procession", CastKind::Position,
        Intent::CrowdControl | Intent::Engage | Intent::Disengage |
            Intent::Peel | Intent::Setup,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::Automatic,
        650.0f, 0.25f, 210.0f, FLT_MAX, false,
        SDK::DamageType::True, SDK::SpellType::SkillshotCircle);
    p.Spells[1].Aim = AimPolicy::Prediction;
    p.Spells[1].Priority = 94;
    p.Spells[1].MinimumAoeTargets = 1;
    p.Spells[1].PreserveAutoAttack = true;

    p.Spells[2] = Spell(
        SDK::SpellSlot::E, "Mourning Mist", CastKind::Line,
        Intent::Damage | Intent::CrowdControl | Intent::Mark | Intent::Setup |
            Intent::Engage | Intent::Jungle,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::Automatic,
        700.0f, 0.25f, 100.0f, 1200.0f, true,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[2].PreserveAutoAttack = true;

    p.Spells[3] = Spell(
        SDK::SpellSlot::R, "Eulogy of the Isles", CastKind::Position,
        Intent::Damage | Intent::Engage | Intent::Disengage |
            Intent::Setup | Intent::Objective | Intent::Waveclear,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::Automatic,
        700.0f, 0.25f, 320.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[3].Aim = AimPolicy::Prediction;
    p.Spells[3].Priority = 100;
    p.Spells[3].MinimumAoeTargets = 1;
    p.Spells[3].PreserveAutoAttack = true;

    p.Trade = Plan(
        "mark with E, cage the exit and weave empowered Q",
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireSafePosition, 0, 1100),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget | StepRule::RequireSafePosition, 90, 900),
        Step(SDK::SpellSlot::Q, StepRule::RequireInsideAaRange | StepRule::AllowDuringWindup, 150, 700));
    p.AllIn = Plan(
        "Maiden-backed cage, marked mist and Q reset",
        Step(SDK::SpellSlot::R, StepRule::RequireTarget | StepRule::RequireSafePosition, 0, 1300),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireSafePosition, 100, 1100),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget | StepRule::RequireSafePosition, 170, 900),
        Step(SDK::SpellSlot::Q, StepRule::RequireInsideAaRange | StepRule::AllowDuringWindup, 240, 700));
    p.Flee = Plan(
        "wall the pursuer and preserve Maiden retreat",
        Step(SDK::SpellSlot::W, StepRule::RequireTarget | StepRule::RequireSafePosition | StepRule::AllowDuringWindup),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireSafePosition | StepRule::AllowDuringWindup));
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
