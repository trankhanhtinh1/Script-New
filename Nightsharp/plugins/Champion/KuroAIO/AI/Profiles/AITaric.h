#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Taric = [] {
    ChampionProfile p{};
    p.ChampionName = "Taric";
    p.DisplayName = "Taric";
    p.InternalId = "champion.kuroaio.ai.taric";
    p.PrimaryArchetype = Archetype::Enchanter;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::Stack | Mechanic::AllyTarget | Mechanic::Tether |
                  Mechanic::AutoWeave | Mechanic::Charge | Mechanic::MissingHealth;
    p.Ultimate = UltimatePolicy::Defensive;

    p.Spells[0] = Spell(
        SDK::SpellSlot::Q, "Starlight's Touch", CastKind::Self,
        Intent::Heal | Intent::AllyUtility | Intent::Buff,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::LastHit | Mode::Flee | Mode::Automatic,
        325.0f, 0.25f, 325.0f, FLT_MAX, false,
        SDK::DamageType::True, SDK::SpellType::SkillshotCircle);
    p.Spells[0].Priority = 84;
    p.Spells[0].TriggerRange = 325.0f;
    p.Spells[0].PreserveAutoAttack = true;
    p.Spells[0].ChargeBuffName = "TaricQ";
    p.Spells[0].ChargeDurationSeconds = 15.0f;

    p.Spells[1] = Spell(
        SDK::SpellSlot::W, "Bastion", CastKind::AllyTarget,
        Intent::Shield | Intent::AllyUtility | Intent::Buff | Intent::Setup,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::Automatic,
        800.0f, 0.25f, 125.0f, 1700.0f, false,
        SDK::DamageType::True, SDK::SpellType::Targeted);
    p.Spells[1].Priority = 96;
    p.Spells[1].TriggerRange = 800.0f;
    p.Spells[1].PreserveAutoAttack = true;

    p.Spells[2] = Spell(
        SDK::SpellSlot::E, "Dazzle", CastKind::Position,
        Intent::Damage | Intent::CrowdControl | Intent::Engage |
            Intent::Disengage | Intent::Interrupt | Intent::AntiGapcloser |
            Intent::Peel | Intent::Setup,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::Automatic,
        610.0f, 0.25f, 100.0f, 1750.0f, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[2].Priority = 93;
    p.Spells[2].TriggerRange = 610.0f;
    p.Spells[2].WeaveAfterAttack = true;
    p.Spells[2].PreserveAutoAttack = true;
    p.Spells[2].ChargeBuffName = "TaricEChargeBuff";
    p.Spells[2].ChargeDurationSeconds = 1.25f;

    p.Spells[3] = Spell(
        SDK::SpellSlot::R, "Cosmic Radiance", CastKind::Self,
        Intent::Shield | Intent::AllyUtility | Intent::Buff | Intent::Peel |
            Intent::Engage | Intent::Setup,
        Mode::Combo | Mode::Flee | Mode::Automatic,
        400.0f, 2.5f, 400.0f, FLT_MAX, false,
        SDK::DamageType::True, SDK::SpellType::SkillshotCircle);
    p.Spells[3].Priority = 100;
    p.Spells[3].TriggerRange = 400.0f;
    p.Spells[3].MinimumAoeTargets = 2;
    p.Spells[3].PlayerHealthPercent = 42.0f;
    p.Spells[3].PreserveAutoAttack = true;

    p.Trade = Plan(
        "Bastion the carry, heal charged damage and stun only on a predicted line",
        Step(SDK::SpellSlot::W, StepRule::RequireTarget | StepRule::RequireSafePosition),
        Step(SDK::SpellSlot::Q, StepRule::RequirePlayerLow),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireAfterAttack));

    p.AllIn = Plan(
        "Link the engager, weave Bravado, Dazzle the approach and delay Cosmic Radiance",
        Step(SDK::SpellSlot::W, StepRule::RequireTarget | StepRule::RequireSafePosition),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireAfterAttack),
        Step(SDK::SpellSlot::Q, StepRule::RequirePlayerLow),
        Step(SDK::SpellSlot::R, StepRule::RequireMultiTarget | StepRule::AllowDuringWindup));

    p.Flee = Plan(
        "Keep Bastion on a safe ally, heal while retreating and use Dazzle before Radiance",
        Step(SDK::SpellSlot::W, StepRule::RequireTarget | StepRule::RequireSafePosition),
        Step(SDK::SpellSlot::Q, StepRule::RequirePlayerLow),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireSafePosition),
        Step(SDK::SpellSlot::R, StepRule::RequirePlayerLow));

    p.PreferredCombatDistance = 425.0f;
    p.EngageHealthPercent = 58.0f;
    p.DefensiveHealthPercent = 42.0f;
    p.UltimateTargetHealthPercent = 72.0f;
    p.UltimateMinimumTargets = 2;
    p.MaximumCommitEnemies = 3;
    p.BaseHumanizerMs = 55;
    p.PreferSelectedTarget = true;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "TaricPassive";
    p.FormBuff = "TaricWAllyBuff";
    p.ChannelBuff = "TaricEChargeBuff";
    p.UltimateBuff = "TaricR";
    p.TrackedObjectToken = "TaricW";
    p.ThemeFrom = 0xFF69D3FFu;
    p.ThemeTo = 0xFF4385FFu;
    p.ThemeSpeed = 0.92f;
    p.TacticalSummary =
        "Maintain a scored Bastion link on the most valuable threatened ally, spend charged Starlight's Touch heals, "
        "weave Bravado attacks between casts, predict Dazzle's corridor and start Cosmic Radiance early enough for its delayed invulnerability.";
    p.ResearchSummary =
        "CommunityDragon PC 16.15 Taric JSON and Riot patch 26.15 notes; exact Q charge, W link, E corridor and R 2.5 second delay values are recorded in AITaric.md.";
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
