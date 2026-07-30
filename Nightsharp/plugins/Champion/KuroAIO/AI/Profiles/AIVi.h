#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

// Vi is a charged-line diver whose apparent Q/R reach is inseparable from the
// endpoint she accepts. W is intentionally descriptive: it has no cast route.
inline constexpr ChampionProfile Vi = [] {
    ChampionProfile p{};
    p.ChampionName = "Vi";
    p.DisplayName = "Vi";
    p.InternalId = "champion.kuroaio.ai.vi";
    p.PrimaryArchetype = Archetype::Diver;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::Charge | Mechanic::Dash | Mechanic::Stack |
                  Mechanic::Mark | Mechanic::Ammo | Mechanic::AutoWeave |
                  Mechanic::AutoReset | Mechanic::Terrain;
    p.Ultimate = UltimatePolicy::ManualAssist;

    p.Spells[0] = Spell(
        SDK::SpellSlot::Q, "Vault Breaker", CastKind::ChargedLine,
        Intent::Damage | Intent::CrowdControl | Intent::Mobility |
            Intent::Engage | Intent::Disengage | Intent::Interrupt |
            Intent::AntiGapcloser | Intent::Waveclear | Intent::Jungle |
            Intent::Setup | Intent::Channel,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::Flee | Mode::Automatic,
        725.0f, 0.0f, 55.0f, 1500.0f, true,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotLine);
    p.Spells[0].Priority = 96;
    p.Spells[0].TriggerRange = 725.0f;
    p.Spells[0].DashDistance = 725.0f;
    p.Spells[0].Hitchance = SDK::HitChance::High;
    p.Spells[0].ChargeBuffName = "ViQ";
    p.Spells[0].ChargeMinRange = 250;
    p.Spells[0].ChargeMaxRange = 725;
    p.Spells[0].ChargeDurationSeconds = 1.25f;
    p.Spells[0].HarassManaPercent = 48.0f;
    p.Spells[0].ClearManaPercent = 55.0f;
    p.Spells[0].AllowOnMinions = true;
    p.Spells[0].PreserveAutoAttack = true;
    p.Spells[0].MaximumEnemiesAtDestination = 2;

    p.Spells[1] = Spell(
        SDK::SpellSlot::W, "Denting Blows", CastKind::None,
        Intent::Damage | Intent::Buff | Intent::Setup | Intent::Objective,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::LastHit | Mode::Automatic,
        0.0f, 0.0f, 0.0f, FLT_MAX, false,
        SDK::DamageType::Physical, SDK::SpellType::Targeted);
    p.Spells[1].Priority = 0;
    p.Spells[1].AllowOnMinions = true;
    p.Spells[1].PreserveAutoAttack = true;

    p.Spells[2] = Spell(
        SDK::SpellSlot::E, "Relentless Force", CastKind::Self,
        Intent::Damage | Intent::Buff | Intent::AutoReset |
            Intent::Waveclear | Intent::Jungle | Intent::LastHit |
            Intent::Objective | Intent::Finisher,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::LastHit | Mode::Automatic,
        225.0f, 0.0f, 535.0f, FLT_MAX, false,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotCone);
    p.Spells[2].Priority = 92;
    p.Spells[2].TriggerRange = 225.0f;
    p.Spells[2].MinimumAmmo = 1;
    p.Spells[2].WeaveAfterAttack = true;
    p.Spells[2].PreserveAutoAttack = false;
    p.Spells[2].AllowOnMinions = true;
    p.Spells[2].HarassManaPercent = 44.0f;
    p.Spells[2].ClearManaPercent = 45.0f;

    p.Spells[3] = Spell(
        SDK::SpellSlot::R, "Cease and Desist", CastKind::EnemyTarget,
        Intent::Damage | Intent::CrowdControl | Intent::Mobility |
            Intent::Engage | Intent::Execute | Intent::Interrupt |
            Intent::Peel | Intent::Setup,
        Mode::Combo | Mode::Flee | Mode::Automatic,
        800.0f, 0.25f, 100.0f, 800.0f, false,
        SDK::DamageType::Physical, SDK::SpellType::Targeted);
    p.Spells[3].Priority = 99;
    p.Spells[3].TriggerRange = 800.0f;
    p.Spells[3].DashDistance = 800.0f;
    p.Spells[3].MaximumEnemiesAtDestination = 2;
    p.Spells[3].PreserveAutoAttack = true;

    p.Trade = Plan(
        "short Q only to a safe edge, AA-E reset, then finish the W proc",
        Step(SDK::SpellSlot::Q,
             StepRule::RequireTarget | StepRule::RequireSafePosition),
        Step(SDK::SpellSlot::E,
             StepRule::RequireTarget | StepRule::RequireAfterAttack));

    p.AllIn = Plan(
        "safe charged Q, AA-E, preserve target through Denting Blows, manual-safe R",
        Step(SDK::SpellSlot::Q,
             StepRule::RequireTarget | StepRule::RequireSafePosition),
        Step(SDK::SpellSlot::E,
             StepRule::RequireTarget | StepRule::RequireAfterAttack),
        Step(SDK::SpellSlot::R,
             StepRule::RequireTarget | StepRule::RequireSafePosition |
                 StepRule::ManualAssistOnly));

    p.Flee = Plan(
        "charge Q toward cursor and release only to a traversable, safer endpoint",
        Step(SDK::SpellSlot::Q,
             StepRule::RequireSafePosition | StepRule::AllowDuringWindup));

    p.PreferredCombatDistance = 175.0f;
    p.EngageHealthPercent = 52.0f;
    p.DefensiveHealthPercent = 30.0f;
    p.UltimateTargetHealthPercent = 38.0f;
    p.UltimateMinimumTargets = 1;
    p.MaximumCommitEnemies = 2;
    p.BaseHumanizerMs = 36;
    p.PreferSelectedTarget = true;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "ViPassiveBuff";
    p.MarkBuff = "ViWMarker";
    p.ChannelBuff = "ViQ";
    p.UltimateBuff = "ViR";
    p.ThemeFrom = 0xFFFF5B98u;
    p.ThemeTo = 0xFF6D7BFFu;
    p.ThemeSpeed = 1.05f;
    p.TacticalSummary =
        "Keep one target through the third Denting Blows hit, use Q only when "
        "both its first collision and dash endpoint are acceptable, proc Blast "
        "Shield into real damage rather than poke, reset attacks with E without "
        "wasting the last charge, and treat R as a lock-on path and landing "
        "commitment instead of a range check.";
    p.ResearchSummary =
        "Pinned to Riot 26.15 and CommunityDragon 16.15: 250-725 Q charge, "
        "1.25-second full growth, 12% maximum-health shield, four-second "
        "Denting Blows shield-cooldown refund, 20% armor shred, two-charge E "
        "attack reset and 800-range target-locked R path.";
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
