#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Sion = [] {
    ChampionProfile p{};
    p.ChampionId = SDK::ChampionId::Sion;
    p.DisplayName = "Sion";
    p.InternalId = "champion.kuroaio.ai.sion";
    p.PrimaryArchetype = Archetype::Juggernaut;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::Charge | Mechanic::Recast | Mechanic::Revive |
                  Mechanic::WallInteraction | Mechanic::MissingHealth |
                  Mechanic::AutoWeave;
    p.Ultimate = UltimatePolicy::ManualAssist;
    p.PreferredCombatDistance = 250.0f;
    p.EngageHealthPercent = 58.0f;
    p.DefensiveHealthPercent = 34.0f;
    p.UltimateTargetHealthPercent = 52.0f;
    p.UltimateMinimumTargets = 1;
    p.MaximumCommitEnemies = 3;
    p.BaseHumanizerMs = 65;
    p.PreferSelectedTarget = true;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "SionPassive";
    p.ChannelBuff = "SionQ";
    p.UltimateBuff = "SionR";
    p.TacticalSummary =
        "Charge-aware juggernaut: hold Q only for a reachable predicted impact,"
        " shield and detonate W through a safe target pocket, thread E through"
        " its first minion body, and steer R into a verified collision or peel."
        " Reconcile Glory in Death instead of issuing normal spell casts in the"
        " zombie state.";
    p.ResearchSummary =
        "Riot 26.15 / CommunityDragon 16.15 Summoner's Rift values; Q charge"
        " and R movement are observed from runtime spell/buff state, while"
        " wall, turret and target safety are conservative when telemetry is"
        " unavailable.";

    p.Spells[0] = Spell(
        SDK::SpellSlot::Q, "Decimating Smash", CastKind::ChargedCircle,
        Intent::Damage | Intent::CrowdControl | Intent::Setup | Intent::Waveclear |
            Intent::Jungle | Intent::LastHit,
        AllModes, 750.0f, 0.40f, 350.0f, FLT_MAX, false,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotCircle);
    p.Spells[0].Aim = AimPolicy::Prediction;
    p.Spells[0].Priority = 95;
    p.Spells[0].Hitchance = SDK::HitChance::High;
    p.Spells[0].ChargeBuffName = "SionQ";
    p.Spells[0].ChargeMinRange = 140;
    p.Spells[0].ChargeMaxRange = 750;
    p.Spells[0].ChargeDurationSeconds = 2.0f;
    p.Spells[0].PreserveAutoAttack = true;
    p.Spells[0].AllowOnMinions = true;
    p.Spells[0].HarassManaPercent = 42.0f;
    p.Spells[0].ClearManaPercent = 35.0f;

    p.Spells[1] = Spell(
        SDK::SpellSlot::W, "Soul Furnace", CastKind::Self,
        Intent::Shield | Intent::Damage | Intent::Recast | Intent::Waveclear |
            Intent::Jungle | Intent::LastHit | Intent::Peel,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::LastHit | Mode::Flee | Mode::Automatic,
        500.0f, 0.25f, 400.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[1].Aim = AimPolicy::SelfPosition;
    p.Spells[1].Priority = 91;
    p.Spells[1].RecastSpellName = "SionWDetonate";
    p.Spells[1].PreserveAutoAttack = false;
    p.Spells[1].ComboManaPercent = 12.0f;

    p.Spells[2] = Spell(
        SDK::SpellSlot::E, "Roar of the Slayer", CastKind::Line,
        Intent::Damage | Intent::CrowdControl | Intent::Setup | Intent::Peel |
            Intent::AntiGapcloser | Intent::Waveclear | Intent::Jungle |
            Intent::LastHit,
        AllModes, 800.0f, 0.25f, 70.0f, 1800.0f, true,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotLine);
    p.Spells[2].Aim = AimPolicy::Prediction;
    p.Spells[2].Priority = 88;
    p.Spells[2].Hitchance = SDK::HitChance::High;
    p.Spells[2].AllowOnMinions = true;
    p.Spells[2].HarassManaPercent = 50.0f;
    p.Spells[2].ClearManaPercent = 38.0f;

    p.Spells[3] = Spell(
        SDK::SpellSlot::R, "Unstoppable Onslaught", CastKind::Direction,
        Intent::Damage | Intent::CrowdControl | Intent::Mobility |
            Intent::Engage | Intent::Disengage | Intent::Interrupt | Intent::Peel,
        Mode::Combo | Mode::Flee | Mode::Automatic,
        3000.0f, 0.25f, 160.0f, 950.0f, false,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotLine);
    p.Spells[3].Aim = AimPolicy::Prediction;
    p.Spells[3].Priority = 100;
    p.Spells[3].MinimumAoeTargets = 1;
    p.Spells[3].MaximumEnemiesAtDestination = 3;
    p.Spells[3].TargetHealthPercent = 52.0f;

    p.Trade = Plan("Charge, shield and roar",
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget, 0, 900),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget | StepRule::AllowDuringWindup, 120, 900),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget, 180, 850));
    p.AllIn = Plan("Unstoppable collision",
        Step(SDK::SpellSlot::R, StepRule::RequireTarget | StepRule::RequireSafePosition, 0, 2500),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget, 220, 950),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget, 300, 1200),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget, 420, 1100));
    p.Flee = Plan("Zombie-aware retreat",
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::AllowDuringWindup, 0, 850),
        Step(SDK::SpellSlot::R, StepRule::RequireSafePosition | StepRule::ManualAssistOnly, 80, 3000),
        Step(SDK::SpellSlot::W, StepRule::RequirePlayerLow, 140, 700));
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
