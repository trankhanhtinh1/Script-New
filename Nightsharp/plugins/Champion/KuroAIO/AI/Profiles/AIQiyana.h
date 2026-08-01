#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Qiyana = [] {
    ChampionProfile p{};
    p.ChampionId = SDK::ChampionId::Qiyana;
    p.DisplayName = "Qiyana";
    p.InternalId = "champion.kuroaio.ai.qiyana";
    p.PrimaryArchetype = Archetype::Assassin;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::Recast | Mechanic::Dash | Mechanic::Execute |
                  Mechanic::ObjectTracking | Mechanic::Mark |
                  Mechanic::AutoWeave | Mechanic::MissingHealth |
                  Mechanic::Terrain | Mechanic::WallInteraction;
    p.Ultimate = UltimatePolicy::MultiTarget;
    p.PreferredCombatDistance = 450.0f;
    p.EngageHealthPercent = 35.0f;
    p.DefensiveHealthPercent = 28.0f;
    p.UltimateTargetHealthPercent = 55.0f;
    p.UltimateMinimumTargets = 2;
    p.MaximumCommitEnemies = 2;
    p.BaseHumanizerMs = 62;
    p.PreferSelectedTarget = true;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "QiyanaPassive";
    p.MarkBuff = "QiyanaPassive";
    p.TrackedObjectToken = "Qiyana";
    p.ThemeFrom = 0xFF2C8CFFu;
    p.ThemeTo = 0xFF38D8D0u;
    p.TacticalSummary =
        "Element-aware assassin: preserve Grass safety, use Ice to catch, "
        "Rock below half health, and commit R only through real terrain.";
    p.ResearchSummary =
        "Riot 26.15 / CommunityDragon 16.15; Qiyana has no Summoner's Rift "
        "kit delta in this patch and ARAM: Mayhem modifiers are excluded.";

    p.Spells[0] = Spell(
        SDK::SpellSlot::Q, "Elemental Wrath / Edge of Ixtal", CastKind::Line,
        Intent::Damage | Intent::CrowdControl | Intent::Setup | Intent::Waveclear |
            Intent::Jungle | Intent::LastHit | Intent::Execute,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::LastHit | Mode::Flee | Mode::Automatic,
        650.0f, 0.25f, 100.0f, 1600.0f, true,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotLine);
    p.Spells[0].Aim = AimPolicy::Prediction;
    p.Spells[0].Priority = 88;
    p.Spells[0].Hitchance = SDK::HitChance::High;
    p.Spells[0].MinimumAoeTargets = 2;
    p.Spells[0].HarassManaPercent = 38.0f;
    p.Spells[0].ClearManaPercent = 35.0f;
    p.Spells[0].WeaveAfterAttack = true;

    p.Spells[1] = Spell(
        SDK::SpellSlot::W, "Terrashape", CastKind::Position,
        Intent::Damage | Intent::Mobility | Intent::Disengage | Intent::Setup |
            Intent::Buff | Intent::AutoReset,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::Flee | Mode::Automatic,
        1100.0f, 0.0f, 366.0f, 1000.0f, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[1].Aim = AimPolicy::SafeCursor;
    p.Spells[1].Priority = 92;
    p.Spells[1].DashDistance = 300.0f;
    p.Spells[1].MaximumEnemiesAtDestination = 2;
    p.Spells[1].WeaveAfterAttack = true;

    p.Spells[2] = Spell(
        SDK::SpellSlot::E, "Audacity", CastKind::EnemyTarget,
        Intent::Damage | Intent::Mobility | Intent::Engage | Intent::Setup |
            Intent::Jungle | Intent::Objective,
        Mode::Combo | Mode::Harass | Mode::Jungle | Mode::Flee | Mode::Automatic,
        650.0f, 0.25f, 75.0f, FLT_MAX, false,
        SDK::DamageType::Physical, SDK::SpellType::Targeted);
    p.Spells[2].Aim = AimPolicy::BetweenPlayerAndTarget;
    p.Spells[2].Priority = 80;
    p.Spells[2].DashDistance = 100.0f;
    p.Spells[2].MaximumEnemiesAtDestination = 2;
    p.Spells[2].WeaveAfterAttack = true;

    p.Spells[3] = Spell(
        SDK::SpellSlot::R, "Supreme Display of Talent", CastKind::Vector,
        Intent::Damage | Intent::CrowdControl | Intent::Engage | Intent::Peel |
            Intent::Execute | Intent::Objective | Intent::Interrupt,
        Mode::Combo | Mode::Flee | Mode::Automatic,
        950.0f, 0.25f, 220.0f, 1000.0f, false,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotLine);
    p.Spells[3].Aim = AimPolicy::BestAoe;
    p.Spells[3].Priority = 96;
    p.Spells[3].MinimumAoeTargets = 2;
    p.Spells[3].MaximumEnemiesAtDestination = 4;
    p.Spells[3].TargetHealthPercent = 60.0f;

    p.Variants[0] = { SDK::SpellSlot::Q, "QiyanaQ_Rock", p.Spells[0] };
    p.Variants[1] = { SDK::SpellSlot::Q, "QiyanaQ_Water", p.Spells[0] };
    p.Variants[2] = { SDK::SpellSlot::Q, "QiyanaQ_Grass", p.Spells[0] };
    p.VariantCount = 3;

    p.Trade = Plan("Elemental short trade",
        Step(SDK::SpellSlot::W, StepRule::RequireSafePosition, 0, 700),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequireAfterAttack, 140, 950));
    p.AllIn = Plan("Terrain detonation all-in",
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireSafePosition, 0, 700),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequireAfterAttack, 90, 900),
        Step(SDK::SpellSlot::W, StepRule::RequireSafePosition, 150, 1050),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequireAfterAttack, 280, 1250),
        Step(SDK::SpellSlot::R, StepRule::RequireTarget | StepRule::RequireMultiTarget, 350, 1450));
    p.Flee = Plan("Grass and cursor escape",
        Step(SDK::SpellSlot::Q, StepRule::RequireNoCrowdControl | StepRule::RequireSafePosition, 0, 700),
        Step(SDK::SpellSlot::W, StepRule::RequireSafePosition, 80, 850),
        Step(SDK::SpellSlot::E, StepRule::RequireSafePosition, 150, 950),
        Step(SDK::SpellSlot::R, StepRule::RequireSafePosition | StepRule::ManualAssistOnly, 250, 1200));
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
