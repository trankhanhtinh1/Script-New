#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Naafiri = [] {
    ChampionProfile p{};
    p.ChampionId = SDK::ChampionId::Naafiri;
    p.DisplayName = "Naafiri";
    p.InternalId = "champion.kuroaio.ai.naafiri";
    p.PrimaryArchetype = Archetype::Assassin;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::Recast | Mechanic::Dash | Mechanic::Execute |
                  Mechanic::ObjectTracking | Mechanic::Mark | Mechanic::Pet |
                  Mechanic::MissingHealth | Mechanic::AutoWeave;
    p.Ultimate = UltimatePolicy::AllIn;
    p.PreferredCombatDistance = 625.0f;
    p.EngageHealthPercent = 38.0f;
    p.DefensiveHealthPercent = 28.0f;
    p.UltimateTargetHealthPercent = 62.0f;
    p.UltimateMinimumTargets = 1;
    p.MaximumCommitEnemies = 2;
    p.BaseHumanizerMs = 62;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "NaafiriP";
    p.MarkBuff = "NaafiriQBleed";
    p.ChannelBuff = "NaafiriW";
    p.UltimateBuff = "NaafiriR";
    p.TrackedObjectToken = "NaafiriPackmate";
    p.ThemeFrom = 0xFFD2353Au;
    p.ThemeTo = 0xFF6B1424u;
    p.TacticalSummary =
        "Pack assassin: land the first dagger, preserve the bleed recast, reject "
        "body-blocked Pursuit paths, and use E/R to reset and recall the pack.";
    p.ResearchSummary =
        "Riot 26.15 / CommunityDragon PC 16.15 Summoner's Rift values, including "
        "the current three-rank Hounds' Pursuit and five-rank Call of the Pack.";

    p.Spells[0] = Spell(
        SDK::SpellSlot::Q, "Darkin Daggers", CastKind::Line,
        Intent::Damage | Intent::Execute | Intent::Heal | Intent::Setup |
            Intent::Recast | Intent::Waveclear | Intent::Jungle | Intent::LastHit,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::LastHit | Mode::Flee | Mode::Automatic,
        900.0f, 0.25f, 150.0f, 1700.0f, false,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotLine);
    p.Spells[0].Aim = AimPolicy::Prediction;
    p.Spells[0].Priority = 93;
    p.Spells[0].Hitchance = SDK::HitChance::High;
    p.Spells[0].PreserveAutoAttack = true;
    p.Spells[0].RecastSpellName = "NaafiriQRecast";

    p.Spells[1] = Spell(
        SDK::SpellSlot::W, "Hounds' Pursuit", CastKind::EnemyTarget,
        Intent::Damage | Intent::CrowdControl | Intent::Mobility | Intent::Engage |
            Intent::Execute | Intent::Shield | Intent::Vision | Intent::Recast |
            Intent::Channel,
        Mode::Combo | Mode::Flee | Mode::Automatic,
        900.0f, 0.75f, 140.0f, 1800.0f, true,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotLine);
    p.Spells[1].Aim = AimPolicy::TargetPosition;
    p.Spells[1].Priority = 98;
    p.Spells[1].DashDistance = 900.0f;
    p.Spells[1].MaximumEnemiesAtDestination = 2;
    p.Spells[1].ComboManaPercent = 12.0f;
    p.Spells[1].RecastSpellName = "NaafiriWRecast";

    p.Spells[2] = Spell(
        SDK::SpellSlot::E, "Eviscerate", CastKind::Position,
        Intent::Damage | Intent::Mobility | Intent::Disengage | Intent::Execute |
            Intent::Waveclear | Intent::Jungle | Intent::LastHit,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::LastHit | Mode::Flee | Mode::Automatic,
        450.0f, 0.0f, 230.0f, 900.0f, false,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotCircle);
    p.Spells[2].Aim = AimPolicy::SafeCursor;
    p.Spells[2].Priority = 88;
    p.Spells[2].DashDistance = 450.0f;
    p.Spells[2].MaximumEnemiesAtDestination = 2;
    p.Spells[2].PreserveAutoAttack = true;

    p.Spells[3] = Spell(
        SDK::SpellSlot::R, "The Call of the Pack", CastKind::Self,
        Intent::Buff | Intent::Mobility | Intent::Disengage | Intent::Setup |
            Intent::Vision,
        Mode::Combo | Mode::Flee | Mode::Automatic,
        0.0f, 0.75f, 0.0f, FLT_MAX, false,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotCircle);
    p.Spells[3].Aim = AimPolicy::SelfPosition;
    p.Spells[3].Priority = 96;
    p.Spells[3].TargetHealthPercent = 62.0f;
    p.Spells[3].PlayerHealthPercent = 38.0f;

    p.Variants[0] = { SDK::SpellSlot::Q, "NaafiriQRecast", p.Spells[0] };
    p.Variants[1] = { SDK::SpellSlot::W, "NaafiriWRecast", p.Spells[1] };
    p.VariantCount = 2;

    p.Trade = Plan("double-dagger trade",
        Step(SDK::SpellSlot::Q,
             StepRule::RequireTarget | StepRule::RequireFirstCast, 0, 720),
        Step(SDK::SpellSlot::Q,
             StepRule::RequireTarget | StepRule::RequireMark |
                 StepRule::RequireRecast,
             750, 4000));
    p.AllIn = Plan("pack pursuit all-in",
        Step(SDK::SpellSlot::R, StepRule::RequireSafePosition, 0, 800),
        Step(SDK::SpellSlot::Q,
             StepRule::RequireTarget | StepRule::RequireFirstCast, 80, 900),
        Step(SDK::SpellSlot::W,
             StepRule::RequireTarget | StepRule::RequireOutsideAaRange |
                 StepRule::RequireSafePosition,
             170, 1500),
        Step(SDK::SpellSlot::E,
             StepRule::RequireTarget | StepRule::RequireSafePosition, 250, 1800),
        Step(SDK::SpellSlot::Q,
             StepRule::RequireTarget | StepRule::RequireMark |
                 StepRule::RequireRecast,
             750, 4000));
    p.Flee = Plan("pack recall escape",
        Step(SDK::SpellSlot::R,
             StepRule::RequireSafePosition | StepRule::RequireNoCrowdControl,
             0, 850),
        Step(SDK::SpellSlot::E,
             StepRule::RequireSafePosition | StepRule::RequireNoCrowdControl,
             80, 950),
        Step(SDK::SpellSlot::Q,
             StepRule::RequireTarget | StepRule::RequireNoCrowdControl,
             150, 1100));
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
