#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Nidalee = [] {
    ChampionProfile p{};
    p.ChampionId = SDK::ChampionId::Nidalee;
    p.DisplayName = "Nidalee";
    p.InternalId = "champion.kuroaio.ai.nidalee";
    p.PrimaryArchetype = Archetype::Specialist;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::Transform | Mechanic::MultiForm | Mechanic::Mark |
                  Mechanic::Trap | Mechanic::Dash | Mechanic::MissingHealth;
    p.Ultimate = UltimatePolicy::RecastControl;
    p.PreferredCombatDistance = 650.0f;
    p.EngageHealthPercent = 58.0f;
    p.DefensiveHealthPercent = 34.0f;
    p.UltimateTargetHealthPercent = 45.0f;
    p.UltimateMinimumTargets = 1;
    p.MaximumCommitEnemies = 2;
    p.BaseHumanizerMs = 62;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "NidaleePassive";
    p.MarkBuff = "NidaleeHunted";
    p.FormBuff = "NidaleeCougarForm";
    p.TacticalSummary =
        "Hunt-mark specialist: use long-range Javelin and Bushwhack to establish a mark, "
        "heal only from an observed low-health state, then transform into Cougar for "
        "marked Pounce, Takedown and Swipe while rejecting unsafe leap endpoints.";
    p.ResearchSummary =
        "Riot 26.15 / CommunityDragon 16.15 Summoner's Rift values; form and Hunt state "
        "are reconciled from spell/buff events and polling.";

    p.Spells[0] = Spell(SDK::SpellSlot::Q, "Javelin Toss",
        CastKind::Position, Intent::Damage | Intent::Setup | Intent::Execute,
        AllModes, 1500.0f, 0.25f, 40.0f, 1300.0f, true,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[0].Aim = AimPolicy::Prediction;
    p.Spells[0].Priority = 94;
    p.Spells[0].Hitchance = SDK::HitChance::High;
    p.Spells[0].PreserveAutoAttack = true;

    p.Spells[1] = Spell(SDK::SpellSlot::W, "Bushwhack",
        CastKind::Position, Intent::Damage | Intent::Setup | Intent::Vision |
            Intent::Waveclear | Intent::Jungle,
        AllModes, 900.0f, 0.25f, 100.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[1].Aim = AimPolicy::TargetPosition;
    p.Spells[1].Priority = 72;
    p.Spells[1].AllowOnMinions = true;

    p.Spells[2] = Spell(SDK::SpellSlot::E, "Primal Surge",
        CastKind::AllyTarget, Intent::Heal | Intent::Buff,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::Automatic,
        600.0f, 0.25f, 0.0f, FLT_MAX, false,
        SDK::DamageType::True, SDK::SpellType::Targeted);
    p.Spells[2].Aim = AimPolicy::SelfPosition;
    p.Spells[2].Priority = 88;
    p.Spells[2].PreserveAutoAttack = true;

    p.Spells[3] = Spell(SDK::SpellSlot::R, "Aspect of the Cougar",
        CastKind::Self, Intent::Recast | Intent::Mobility | Intent::Engage |
            Intent::Disengage,
        AllModes, 0.0f, 0.25f, 0.0f, FLT_MAX, false,
        SDK::DamageType::True, SDK::SpellType::Targeted);
    p.Spells[3].Aim = AimPolicy::SelfPosition;
    p.Spells[3].Priority = 90;

    p.Variants[0] = {SDK::SpellSlot::Q, "Takedown", Spell(
        SDK::SpellSlot::Q, "Takedown", CastKind::EnemyTarget,
        Intent::Damage | Intent::Execute | Intent::AutoReset,
        AllModes, 400.0f, 0.0f, 0.0f, FLT_MAX, false,
        SDK::DamageType::Physical, SDK::SpellType::Targeted)};
    p.Variants[1] = {SDK::SpellSlot::W, "Pounce", Spell(
        SDK::SpellSlot::W, "Pounce", CastKind::Position,
        Intent::Damage | Intent::Mobility | Intent::Engage | Intent::Disengage,
        AllModes, 750.0f, 0.0f, 150.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle)};
    p.Variants[2] = {SDK::SpellSlot::E, "Swipe", Spell(
        SDK::SpellSlot::E, "Swipe", CastKind::Self,
        Intent::Damage | Intent::Waveclear | Intent::Jungle,
        AllModes, 350.0f, 0.0f, 350.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCone)};
    p.VariantCount = 3;

    p.Trade = Plan("Spear and Hunt conversion",
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget, 0, 1000),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget, 80, 1000),
        Step(SDK::SpellSlot::R, StepRule::RequireFirstCast, 130, 700),
        Step(SDK::SpellSlot::W, StepRule::RequireMark | StepRule::RequireSafePosition,
             160, 800),
        Step(SDK::SpellSlot::Q, StepRule::RequireMark | StepRule::RequireTarget,
             220, 800),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget, 260, 800));
    p.AllIn = Plan("Marked Cougar burst",
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequireMark,
             0, 700),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::AllowDuringWindup,
             80, 700),
        Step(SDK::SpellSlot::W, StepRule::RequireMark | StepRule::RequireSafePosition,
             120, 800),
        Step(SDK::SpellSlot::R, StepRule::RequireRecast, 180, 750));
    p.Flee = Plan("Safe Cougar retreat",
        Step(SDK::SpellSlot::R, StepRule::RequireFirstCast, 0, 700),
        Step(SDK::SpellSlot::W, StepRule::RequireSafePosition, 80, 800),
        Step(SDK::SpellSlot::E, StepRule::RequirePlayerLow, 130, 800));
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
