#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Zaahen = [] {
    ChampionProfile p{};
    p.ChampionId = SDK::ChampionId::Zaahen;
    p.DisplayName = "Zaahen, the Unsundered";
    p.InternalId = "champion.kuroaio.ai.zaahen";
    p.PrimaryArchetype = Archetype::Skirmisher;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::Recast | Mechanic::Dash | Mechanic::Mark |
                  Mechanic::Stack | Mechanic::Revive | Mechanic::AutoReset |
                  Mechanic::DirectionalSweet | Mechanic::MissingHealth;
    p.Ultimate = UltimatePolicy::AllIn;
    p.PreferredCombatDistance = 350.0f;
    p.EngageHealthPercent = 34.0f;
    p.DefensiveHealthPercent = 28.0f;
    p.UltimateTargetHealthPercent = 62.0f;
    p.UltimateMinimumTargets = 2;
    p.MaximumCommitEnemies = 3;
    p.BaseHumanizerMs = 55;
    p.PreferSelectedTarget = true;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "ZaahenPassive";
    p.MarkBuff = "ZaahenQ2Ready";
    p.UltimateBuff = "ZaahenRSpellPassive";
    p.TacticalSummary =
        "Determination-fueled skirmisher: prime The Darkin Glaive, pull targets with Dreaded Return, use Aureate Rush sweet spots for safe repositioning, and reserve Grim Deliverance for a lethal or multi-target landing.";
    p.ResearchSummary =
        "Riot 26.15 / CommunityDragon 16.15 Zaahen data: 12-stack Determination, Q recast attack reset, 850-range W pull, 350-range E sweet spot, and 600-range R dash with mitigation and healing.";

    p.Spells[0] = Spell(SDK::SpellSlot::Q, "The Darkin Glaive", CastKind::Self,
        Intent::Damage | Intent::Heal | Intent::Recast | Intent::AutoReset |
            Intent::Setup,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::LastHit | Mode::Automatic,
        0.0f, 0.0f, 100.0f, FLT_MAX, false,
        SDK::DamageType::Physical, SDK::SpellType::Targeted);
    p.Spells[0].Aim = AimPolicy::SelfPosition;
    p.Spells[0].Priority = 92;
    p.Spells[0].RecastSpellName = "ZaahenQ2";
    p.Spells[0].WeaveAfterAttack = true;
    p.Spells[0].PreserveAutoAttack = false;
    p.Spells[0].ClearManaPercent = 0.0f;

    p.Spells[1] = Spell(SDK::SpellSlot::W, "Dreaded Return", CastKind::Direction,
        Intent::Damage | Intent::CrowdControl | Intent::Setup |
            Intent::Interrupt | Intent::AntiGapcloser,
        Mode::Combo | Mode::Harass | Mode::Automatic | Mode::Flee,
        850.0f, 0.50f, 70.0f, 1600.0f, true,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotLine);
    p.Spells[1].Aim = AimPolicy::Prediction;
    p.Spells[1].Priority = 96;
    p.Spells[1].Hitchance = SDK::HitChance::High;

    p.Spells[2] = Spell(SDK::SpellSlot::E, "Aureate Rush", CastKind::Position,
        Intent::Damage | Intent::Mobility | Intent::Engage | Intent::Disengage |
            Intent::Waveclear | Intent::Jungle,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::LastHit | Mode::Flee | Mode::Automatic,
        350.0f, 0.0f, 375.0f, FLT_MAX, false,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotCircle);
    p.Spells[2].Aim = AimPolicy::SafeCursor;
    p.Spells[2].Priority = 84;
    p.Spells[2].DashDistance = 350.0f;
    p.Spells[2].DesiredDistance = 300.0f;
    p.Spells[2].MaximumEnemiesAtDestination = 2;

    p.Spells[3] = Spell(SDK::SpellSlot::R, "Grim Deliverance", CastKind::Position,
        Intent::Damage | Intent::Mobility | Intent::Engage | Intent::Heal |
            Intent::Finisher | Intent::Objective,
        Mode::Combo | Mode::Flee | Mode::Automatic,
        600.0f, 0.50f, 550.0f, 2800.0f, false,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotCircle);
    p.Spells[3].Aim = AimPolicy::BestAoe;
    p.Spells[3].Priority = 100;
    p.Spells[3].MinimumAoeTargets = 2;
    p.Spells[3].MaximumEnemiesAtDestination = 3;
    p.Spells[3].TargetHealthPercent = 62.0f;

    p.Trade = Plan("Pull and glaive trade",
        Step(SDK::SpellSlot::W, StepRule::RequireTarget, 0, 850),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::AllowDuringWindup, 80, 700),
        Step(SDK::SpellSlot::E, StepRule::RequireSafePosition, 180, 600));
    p.AllIn = Plan("Determination all-in",
        Step(SDK::SpellSlot::W, StepRule::RequireTarget, 0, 850),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::AllowDuringWindup, 80, 700),
        Step(SDK::SpellSlot::E, StepRule::RequireSafePosition, 140, 600),
        Step(SDK::SpellSlot::R, StepRule::RequireTarget | StepRule::RequireMultiTarget | StepRule::SkipIfKillableWithout, 240, 900));
    p.Flee = Plan("Rush and return peel",
        Step(SDK::SpellSlot::E, StepRule::RequireSafePosition, 0, 600),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget | StepRule::ManualAssistOnly, 120, 850),
        Step(SDK::SpellSlot::R, StepRule::RequireSafePosition | StepRule::ManualAssistOnly, 180, 900));
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
