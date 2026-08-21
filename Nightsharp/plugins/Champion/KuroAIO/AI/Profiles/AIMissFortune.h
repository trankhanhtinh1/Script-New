#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile MissFortune = [] {
    ChampionProfile p{};
    p.ChampionId = SDK::ChampionId::MissFortune;
    p.DisplayName = "Miss Fortune";
    p.InternalId = "champion.kuroaio.ai.missfortune";
    p.PrimaryArchetype = Archetype::Marksman;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::AutoWeave | Mechanic::Mark |
                  Mechanic::Channel;
    p.Ultimate = UltimatePolicy::MultiTarget;

    p.Spells[0] = Spell(
        SDK::SpellSlot::Q, "Double Up", CastKind::EnemyTarget,
        Intent::Damage | Intent::Execute | Intent::AutoReset |
            Intent::LastHit | Intent::Jungle | Intent::Finisher,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::LastHit | Mode::Automatic,
        550.0f, 0.25f, 70.0f, 1400.0f, false,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotLine);
    p.Spells[0].Priority = 96;
    p.Spells[0].WeaveAfterAttack = true;
    p.Spells[0].PreserveAutoAttack = true;
    p.Spells[0].AllowOnMinions = true;
    p.Spells[0].HarassManaPercent = 38.0f;

    p.Spells[1] = Spell(
        SDK::SpellSlot::W, "Strut", CastKind::Self,
        Intent::Buff | Intent::Mobility | Intent::AutoReset |
            Intent::Jungle | Intent::Objective,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle,
        0.0f, 0.0f, 0.0f, FLT_MAX, false,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotCircle);
    p.Spells[1].Priority = 98;
    p.Spells[1].PreserveAutoAttack = false;
    p.Spells[1].HarassManaPercent = 30.0f;

    p.Spells[2] = Spell(
        SDK::SpellSlot::E, "Make It Rain", CastKind::Circle,
        Intent::Damage | Intent::CrowdControl | Intent::Setup |
            Intent::Peel | Intent::AntiGapcloser | Intent::Waveclear,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::Flee | Mode::Automatic,
        1000.0f, 0.50f, 350.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[2].Priority = 93;
    p.Spells[2].Hitchance = SDK::HitChance::High;
    p.Spells[2].PreserveAutoAttack = true;
    p.Spells[2].HarassManaPercent = 52.0f;
    p.Spells[2].ClearManaPercent = 58.0f;
    p.Spells[2].AllowOnMinions = true;

    p.Spells[3] = Spell(
        SDK::SpellSlot::R, "Bullet Time", CastKind::Cone,
        Intent::Damage | Intent::Channel | Intent::Execute |
            Intent::Finisher,
        Mode::Combo | Mode::Automatic,
        1400.0f, 0.25f, 35.0f, FLT_MAX, false,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotCone);
    p.Spells[3].Priority = 100;
    p.Spells[3].Hitchance = SDK::HitChance::High;
    p.Spells[3].MinimumAoeTargets = 2;
    p.Spells[3].PreserveAutoAttack = true;

    p.Trade = Plan(
        "Love Tap attack, W before the next attack, Q in the after-attack window",
        Step(SDK::SpellSlot::W,
             StepRule::RequireTarget | StepRule::AllowDuringWindup),
        Step(SDK::SpellSlot::Q,
             StepRule::RequireTarget | StepRule::RequireAfterAttack));
    p.AllIn = Plan(
        "Slow first only when it secures contact; channel R from safety",
        Step(SDK::SpellSlot::E, StepRule::RequireTarget),
        Step(SDK::SpellSlot::R,
             StepRule::RequireTarget | StepRule::RequireSafePosition));

    p.PreferredCombatDistance = 570.0f;
    p.EngageHealthPercent = 62.0f;
    p.DefensiveHealthPercent = 29.0f;
    p.UltimateTargetHealthPercent = 55.0f;
    p.UltimateMinimumTargets = 2;
    p.MaximumCommitEnemies = 1;
    p.BaseHumanizerMs = 22;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.MarkBuff = "MissFortunePassive";
    p.ChannelBuff = "MissFortuneBulletSound";
    p.ThemeFrom = 0xFFFF4F6Du;
    p.ThemeTo = 0xFFFFC65Au;
    p.ThemeSpeed = 0.92f;
    p.TacticalSummary =
        "Use orbwalker-owned Love Tap swaps only when they do not abandon a "
        "killable target, solve direct and minion-bounce Q routes, preserve AA "
        "windows, and start Bullet Time only on a safe valuable cone.";
    p.ResearchSummary =
        "Ported from TestOrbwalker MissFortune.cs Q bounce/after-attack flow and "
        "cross-checked with current CommunityDragon channel and spell data.";
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
