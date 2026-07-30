#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Yone = [] {
    ChampionProfile p{};
    p.ChampionName = "Yone";
    p.DisplayName = "Yone";
    p.InternalId = "champion.kuroaio.ai.yone";
    p.PrimaryArchetype = Archetype::Skirmisher;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::Dash | Mechanic::Blink | Mechanic::Recast |
                  Mechanic::Stack | Mechanic::Mark | Mechanic::AutoWeave |
                  Mechanic::MissingHealth | Mechanic::WallInteraction;
    p.Ultimate = UltimatePolicy::MultiTarget;
    p.PreferredCombatDistance = 475.0f;
    p.EngageHealthPercent = 42.0f;
    p.DefensiveHealthPercent = 30.0f;
    p.UltimateTargetHealthPercent = 55.0f;
    p.UltimateMinimumTargets = 2;
    p.MaximumCommitEnemies = 2;
    p.BaseHumanizerMs = 58;
    p.PreferSelectedTarget = true;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "YonePassive";
    p.MarkBuff = "YoneE";
    p.TrackedObjectToken = "Yone";
    p.ThemeFrom = 0xFF8B5CFFu;
    p.ThemeTo = 0xFF44D8FFu;
    p.TacticalSummary =
        "Q-stack and W-shield skirmisher: preserve the Spirit Form return, "
        "weave attacks between spells, and reserve R for a safe multi-target line.";
    p.ResearchSummary =
        "Riot 26.15 / CommunityDragon 16.15; Summoner's Rift values only, "
        "with no ARAM: Mayhem modifiers or later-patch mechanics mixed in.";

    p.Spells[0] = Spell(
        SDK::SpellSlot::Q, "Mortal Steel", CastKind::Line,
        Intent::Damage | Intent::CrowdControl | Intent::Setup | Intent::Waveclear |
            Intent::Jungle | Intent::LastHit | Intent::AutoReset,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::LastHit | Mode::Flee | Mode::Automatic,
        475.0f, 0.25f, 60.0f, 1500.0f, true,
        SDK::DamageType::Mixed, SDK::SpellType::SkillshotLine);
    p.Spells[0].Aim = AimPolicy::Prediction;
    p.Spells[0].Priority = 90;
    p.Spells[0].Hitchance = SDK::HitChance::High;
    p.Spells[0].WeaveAfterAttack = true;

    p.Spells[1] = Spell(
        SDK::SpellSlot::W, "Spirit Cleave", CastKind::Cone,
        Intent::Damage | Intent::Shield | Intent::Waveclear | Intent::Jungle |
            Intent::LastHit | Intent::AutoReset,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::LastHit | Mode::Automatic,
        600.0f, 0.25f, 80.0f, FLT_MAX, false,
        SDK::DamageType::Mixed, SDK::SpellType::Cone);
    p.Spells[1].Aim = AimPolicy::BestAoe;
    p.Spells[1].Priority = 84;
    p.Spells[1].MinimumAoeTargets = 1;
    p.Spells[1].WeaveAfterAttack = true;

    p.Spells[2] = Spell(
        SDK::SpellSlot::E, "Soul Unbound", CastKind::Position,
        Intent::Damage | Intent::Mobility | Intent::Disengage | Intent::Setup |
            Intent::Recast | Intent::Execute,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::Automatic,
        400.0f, 0.0f, 80.0f, FLT_MAX, false,
        SDK::DamageType::Mixed, SDK::SpellType::SkillshotCircle);
    p.Spells[2].Aim = AimPolicy::BetweenPlayerAndTarget;
    p.Spells[2].Priority = 95;
    p.Spells[2].DashDistance = 400.0f;
    p.Spells[2].MaximumEnemiesAtDestination = 2;

    p.Spells[3] = Spell(
        SDK::SpellSlot::R, "Fate Sealed", CastKind::Line,
        Intent::Damage | Intent::CrowdControl | Intent::Mobility | Intent::Engage |
            Intent::Execute | Intent::Objective | Intent::Interrupt,
        Mode::Combo | Mode::Flee | Mode::Automatic,
        1000.0f, 0.33f, 120.0f, 1500.0f, false,
        SDK::DamageType::Mixed, SDK::SpellType::SkillshotLine);
    p.Spells[3].Aim = AimPolicy::BestAoe;
    p.Spells[3].Priority = 98;
    p.Spells[3].Hitchance = SDK::HitChance::High;
    p.Spells[3].MinimumAoeTargets = 2;
    p.Spells[3].MaximumEnemiesAtDestination = 3;
    p.Spells[3].TargetHealthPercent = 55.0f;

    p.Variants[0] = { SDK::SpellSlot::Q, "YoneQ3", p.Spells[0] };
    p.Variants[1] = { SDK::SpellSlot::E, "YoneE2", p.Spells[2] };
    p.VariantCount = 2;

    p.Trade = Plan("Q-stack W trade",
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequireAfterAttack, 0, 700),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget | StepRule::RequireAfterAttack, 100, 850),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequireAfterAttack, 170, 1050));
    p.AllIn = Plan("Spirit Form all-in",
        Step(SDK::SpellSlot::E, StepRule::RequireSafePosition, 0, 650),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequireAfterAttack, 90, 800),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget | StepRule::RequireAfterAttack, 160, 950),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequireAfterAttack, 240, 1100),
        Step(SDK::SpellSlot::R, StepRule::RequireTarget | StepRule::RequireMultiTarget, 320, 1450));
    p.Flee = Plan("Spirit return escape",
        Step(SDK::SpellSlot::E, StepRule::RequireSafePosition, 0, 700),
        Step(SDK::SpellSlot::Q, StepRule::RequireNoCrowdControl | StepRule::RequireSafePosition, 80, 900));
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
