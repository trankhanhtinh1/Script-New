#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Janna = [] {
    ChampionProfile p{};
    p.ChampionId = SDK::ChampionId::Janna;
    p.DisplayName = "Janna";
    p.InternalId = "champion.kuroaio.ai.janna";
    p.PrimaryArchetype = Archetype::Enchanter;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::Charge | Mechanic::AllyTarget |
                  Mechanic::Channel | Mechanic::DirectionalSweet |
                  Mechanic::WallInteraction | Mechanic::AutoWeave;
    p.Ultimate = UltimatePolicy::Defensive;

    p.Spells[0] = Spell(
        SDK::SpellSlot::Q, "Howling Gale", CastKind::ChargedLine,
        Intent::Damage | Intent::CrowdControl | Intent::Peel |
            Intent::AntiGapcloser | Intent::Interrupt | Intent::Setup,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::Automatic,
        1075.0f, 0.25f, 120.0f, 900.0f, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[0].Priority = 96;
    p.Spells[0].TriggerRange = 1075.0f;
    p.Spells[0].ChargeMinRange = 175;
    p.Spells[0].ChargeMaxRange = 1075;
    p.Spells[0].ChargeDurationSeconds = 3.0f;
    p.Spells[0].ChargeBuffName = "JannaQ";
    p.Spells[0].HarassManaPercent = 52.0f;
    p.Spells[0].PreserveAutoAttack = true;

    p.Spells[1] = Spell(
        SDK::SpellSlot::W, "Zephyr", CastKind::EnemyTarget,
        Intent::Damage | Intent::CrowdControl | Intent::Disengage |
            Intent::Peel | Intent::Setup,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::Automatic,
        650.0f, 0.0f, 0.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::Targeted);
    p.Spells[1].Priority = 78;
    p.Spells[1].TriggerRange = 650.0f;
    p.Spells[1].HarassManaPercent = 58.0f;
    p.Spells[1].PreserveAutoAttack = true;

    p.Spells[2] = Spell(
        SDK::SpellSlot::E, "Eye Of The Storm", CastKind::AllyTarget,
        Intent::Shield | Intent::Buff | Intent::AllyUtility |
            Intent::Peel | Intent::Setup,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::Automatic,
        800.0f, 0.0f, 0.0f, FLT_MAX, false,
        SDK::DamageType::True, SDK::SpellType::Targeted);
    p.Spells[2].Priority = 100;
    p.Spells[2].TriggerRange = 800.0f;
    p.Spells[2].HarassManaPercent = 42.0f;
    p.Spells[2].PreserveAutoAttack = true;

    p.Spells[3] = Spell(
        SDK::SpellSlot::R, "Monsoon", CastKind::Self,
        Intent::Heal | Intent::CrowdControl | Intent::Disengage |
            Intent::Peel | Intent::Channel | Intent::AntiGapcloser,
        Mode::Combo | Mode::Flee | Mode::Automatic,
        725.0f, 0.0f, 725.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::Targeted);
    p.Spells[3].Priority = 110;
    p.Spells[3].TriggerRange = 725.0f;
    p.Spells[3].MinimumAoeTargets = 1;
    p.Spells[3].MaximumEnemiesAtDestination = 3;
    p.Spells[3].PlayerHealthPercent = 40.0f;
    p.Spells[3].PreserveAutoAttack = true;

    p.Trade = Plan(
        "Shield the carry, charge a tornado, then use Zephyr to slow the retreat",
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireSafePosition),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequireSafePosition),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget));

    p.AllIn = Plan(
        "Peel with Monsoon or a full-charge tornado before buffed ally attacks",
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireSafePosition),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequireSafePosition),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget),
        Step(SDK::SpellSlot::R, StepRule::RequireMultiTarget | StepRule::AllowDuringWindup));

    p.Flee = Plan(
        "Shield the endangered ally, release tornado and cursor-direct Monsoon peel",
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireSafePosition),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequireSafePosition),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget),
        Step(SDK::SpellSlot::R, StepRule::RequirePlayerLow));

    p.PreferredCombatDistance = 600.0f;
    p.EngageHealthPercent = 58.0f;
    p.DefensiveHealthPercent = 38.0f;
    p.UltimateTargetHealthPercent = 100.0f;
    p.UltimateMinimumTargets = 1;
    p.MaximumCommitEnemies = 3;
    p.BaseHumanizerMs = 45;
    p.PreferSelectedTarget = true;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "JannaPassive";
    p.ChannelBuff = "JannaR";
    p.UltimateBuff = "JannaR";
    p.ThemeFrom = 0xFF9DD9FFu;
    p.ThemeTo = 0xFF4D88FFu;
    p.ThemeSpeed = 1.04f;
    p.TacticalSummary =
        "Protect the highest-value threatened ally with Eye of the Storm, hold and "
        "release Howling Gale at a meaningful charge, use Zephyr for a targeted slow, "
        "and reserve cursor-directed Monsoon for peel and healing zones.";
    p.ResearchSummary =
        "Riot 26.15 (no Janna balance entry) and CommunityDragon PC 16.15 champion "
        "JSON, charged tornado timing, ally shield/AD scoring, Monsoon zone safety, "
        "projectile-wall checks and deterministic geometry boundaries.";
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
