#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Quinn = [] {
    ChampionProfile p{};
    p.ChampionId = SDK::ChampionId::Quinn;
    p.DisplayName = "Quinn";
    p.InternalId = "champion.kuroaio.ai.quinn";
    p.PrimaryArchetype = Archetype::Marksman;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::Mark | Mechanic::Dash | Mechanic::Recast |
                  Mechanic::WallInteraction | Mechanic::AutoWeave;
    p.Ultimate = UltimatePolicy::RecastControl;

    p.Spells[0] = Spell(
        SDK::SpellSlot::Q, "Blinding Assault", CastKind::Line,
        Intent::Damage | Intent::CrowdControl | Intent::Setup |
            Intent::Peel | Intent::Finisher,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::LastHit | Mode::Flee | Mode::Automatic,
        1025.0f, 0.25f, 110.0f, 1550.0f, true,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotLine);
    p.Spells[0].Priority = 91;
    p.Spells[0].Hitchance = SDK::HitChance::High;
    p.Spells[0].WeaveAfterAttack = true;
    p.Spells[0].PreserveAutoAttack = true;
    p.Spells[0].HarassManaPercent = 42.0f;
    p.Spells[0].ClearManaPercent = 48.0f;

    p.Spells[1] = Spell(
        SDK::SpellSlot::W, "Heightened Senses", CastKind::Self,
        Intent::Vision | Intent::Buff | Intent::Setup | Intent::AllyUtility,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::Flee | Mode::Automatic,
        2100.0f, 0.0f, 0.0f, FLT_MAX, false,
        SDK::DamageType::True, SDK::SpellType::SkillshotCircle);
    p.Spells[1].Priority = 79;
    p.Spells[1].PreserveAutoAttack = true;
    p.Spells[1].HarassManaPercent = 35.0f;

    p.Spells[2] = Spell(
        SDK::SpellSlot::E, "Vault", CastKind::EnemyTarget,
        Intent::Damage | Intent::CrowdControl | Intent::Mobility |
            Intent::Disengage | Intent::Engage | Intent::Peel |
            Intent::AntiGapcloser | Intent::Setup,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::Automatic,
        675.0f, 0.25f, 0.0f, FLT_MAX, false,
        SDK::DamageType::Physical, SDK::SpellType::Targeted);
    p.Spells[2].Priority = 96;
    p.Spells[2].PreserveAutoAttack = true;

    p.Spells[3] = Spell(
        SDK::SpellSlot::R, "Behind Enemy Lines", CastKind::Self,
        Intent::Mobility | Intent::Damage | Intent::Engage |
            Intent::Disengage | Intent::Recast | Intent::Channel |
            Intent::Setup,
        Mode::Combo | Mode::Flee | Mode::Automatic,
        0.0f, 3.0f, 0.0f, FLT_MAX, false,
        SDK::DamageType::Physical, SDK::SpellType::Targeted);
    p.Spells[3].Priority = 100;
    p.Spells[3].PreserveAutoAttack = true;
    p.Spells[3].RecastSpellName = "QuinnRFinal";

    p.Trade = Plan(
        "mark the chosen target, blind through the first collision, and preserve the attack windup",
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequireNoMark),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireMark));
    p.AllIn = Plan(
        "use Vault only for a safe displacement, then blind and recast a committed Skystrike",
        Step(SDK::SpellSlot::E, StepRule::RequireTarget),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget),
        Step(SDK::SpellSlot::R, StepRule::RequireTarget));
    p.Flee = Plan(
        "Vault a pursuer for peel and keep Behind Enemy Lines movement autonomous",
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::AllowDuringWindup));

    p.PreferredCombatDistance = 575.0f;
    p.EngageHealthPercent = 58.0f;
    p.DefensiveHealthPercent = 34.0f;
    p.UltimateTargetHealthPercent = 42.0f;
    p.UltimateMinimumTargets = 1;
    p.MaximumCommitEnemies = 2;
    p.BaseHumanizerMs = 30;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "QuinnPassiveMarked";
    p.MarkBuff = "QuinnPassiveMarked";
    p.FormBuff = "QuinnR";
    p.ChannelBuff = "QuinnRChannel";
    p.UltimateBuff = "QuinnR";
    p.ThemeFrom = 0xFFB5E7FFu;
    p.ThemeTo = 0xFF4F73FFu;
    p.ThemeSpeed = 0.88f;
    p.TacticalSummary =
        "Reconcile Harrier marks, cast a collision-aware blind line, spend Heightened "
        "Senses for information, Vault only into a safe displacement, and keep Behind "
        "Enemy Lines recasts behind turret and unsafe-mobility gates.";
    p.ResearchSummary =
        "Pinned to Riot live patch 26.15 and CommunityDragon 16.15: Harrier marks a "
        "single enemy for bonus physical damage, Blinding Assault is a colliding line "
        "blind, Heightened Senses reveals the surrounding area, Vault displaces Quinn "
        "and her target, and Behind Enemy Lines is a channel with Skystrike recast.";
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
