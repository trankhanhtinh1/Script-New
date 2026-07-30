#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Pantheon = [] {
    ChampionProfile p{};
    p.ChampionName = "Pantheon";
    p.DisplayName = "Pantheon";
    p.InternalId = "champion.kuroaio.ai.pantheon";
    p.PrimaryArchetype = Archetype::Diver;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::Charge | Mechanic::Recast | Mechanic::Dash |
                  Mechanic::Channel | Mechanic::Execute | Mechanic::Global |
                  Mechanic::Stack | Mechanic::AutoWeave |
                  Mechanic::DirectionalSweet;
    p.Ultimate = UltimatePolicy::ManualAssist;

    p.Spells[0] = Spell(
        SDK::SpellSlot::Q, "Comet Spear", CastKind::ChargedLine,
        Intent::Damage | Intent::Execute | Intent::Waveclear |
            Intent::LastHit | Intent::Jungle | Intent::Objective |
            Intent::Finisher,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::LastHit | Mode::Automatic,
        1200.0f, 0.25f, 60.0f, 2700.0f, false,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotLine);
    p.Spells[0].Priority = 96;
    p.Spells[0].Hitchance = SDK::HitChance::High;
    p.Spells[0].ChargeBuffName = "PantheonQ";
    p.Spells[0].ChargeMinRange = 575;
    p.Spells[0].ChargeMaxRange = 1200;
    p.Spells[0].ChargeDurationSeconds = 0.80f;
    p.Spells[0].PreserveAutoAttack = true;
    p.Spells[0].HarassManaPercent = 38.0f;
    p.Spells[0].ClearManaPercent = 45.0f;
    p.Spells[0].AllowOnMinions = true;

    p.Spells[1] = Spell(
        SDK::SpellSlot::W, "Shield Vault", CastKind::EnemyTarget,
        Intent::Damage | Intent::CrowdControl | Intent::Mobility |
            Intent::Engage | Intent::Interrupt | Intent::AntiGapcloser |
            Intent::Setup,
        Mode::Combo | Mode::Jungle | Mode::Flee | Mode::Automatic,
        600.0f, 0.25f, 0.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[1].Priority = 98;
    p.Spells[1].PreserveAutoAttack = true;
    p.Spells[1].AllowOnMinions = true;
    p.Spells[1].MaximumEnemiesAtDestination = 2;

    p.Spells[2] = Spell(
        SDK::SpellSlot::E, "Aegis Assault", CastKind::Direction,
        Intent::Damage | Intent::Shield | Intent::Disengage |
            Intent::Peel | Intent::Waveclear | Intent::Jungle |
            Intent::Recast,
        Mode::Combo | Mode::LaneClear | Mode::Jungle | Mode::Flee |
            Mode::Automatic,
        525.0f, 0.10f, 375.0f, FLT_MAX, false,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotCone);
    p.Spells[2].Priority = 100;
    p.Spells[2].Aim = AimPolicy::AwayFromThreat;
    p.Spells[2].PreserveAutoAttack = true;
    p.Spells[2].AllowOnMinions = true;
    p.Spells[2].MaximumEnemiesAtDestination = 3;
    p.Spells[2].ClearManaPercent = 55.0f;
    p.Spells[2].RecastSpellName = "PantheonE2";

    p.Spells[3] = Spell(
        SDK::SpellSlot::R, "Grand Starfall", CastKind::Position,
        Intent::Damage | Intent::Mobility | Intent::Engage |
            Intent::Setup | Intent::Channel,
        Mode::Combo,
        5500.0f, 2.20f, 450.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[3].Priority = 90;
    p.Spells[3].Aim = AimPolicy::SafeCursor;
    p.Spells[3].Hitchance = SDK::HitChance::VeryHigh;
    p.Spells[3].MaximumEnemiesAtDestination = 2;

    p.Trade = Plan(
        "Tap Q for cooldown economy; spend Mortal Will only on a safe W or clean Q",
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget),
        Step(SDK::SpellSlot::W,
             StepRule::RequireTarget | StepRule::RequireSafePosition),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget));
    p.AllIn = Plan(
        "Empowered W, protected triple attack, tap Q, then directional E exit",
        Step(SDK::SpellSlot::W,
             StepRule::RequireTarget | StepRule::RequireSafePosition),
        Step(SDK::SpellSlot::Q,
             StepRule::RequireTarget | StepRule::RequireAfterAttack),
        Step(SDK::SpellSlot::E,
             StepRule::RequireTarget | StepRule::RequireSafePosition));
    p.Flee = Plan(
        "Face Aegis Assault into the committed pursuer; stun only when it does not deepen danger",
        Step(SDK::SpellSlot::E,
             StepRule::RequireTarget | StepRule::AllowDuringWindup),
        Step(SDK::SpellSlot::W,
             StepRule::RequireTarget | StepRule::RequirePlayerLow |
                 StepRule::AllowDuringWindup));

    p.PreferredCombatDistance = 525.0f;
    p.EngageHealthPercent = 58.0f;
    p.DefensiveHealthPercent = 30.0f;
    p.UltimateTargetHealthPercent = 38.0f;
    p.MaximumCommitEnemies = 2;
    p.BaseHumanizerMs = 28;
    p.PreferSelectedTarget = true;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "PantheonPassiveReady";
    p.ChannelBuff = "PantheonQ";
    p.UltimateBuff = "PantheonRJump";
    p.ThemeFrom = 0xFFD6A543u;
    p.ThemeTo = 0xFF6E86C7u;
    p.ThemeSpeed = 0.90f;
    p.TacticalSummary =
        "Reconcile five Mortal Will actions; reserve empowered E for committed "
        "damage, prefer empowered W only at a safe vault endpoint, tap Q inside "
        "575 for its cooldown refund, throw Q only after the 0.35 second hold, "
        "and keep Grand Starfall manual with explicit landing safety.";
    p.ResearchSummary =
        "Riot 26.15 / CommunityDragon 16.15: five-action Mortal Will, Q tap/"
        "0.35-second throw split and first-body falloff, targeted W with the "
        "empowered triple attack, source-facing E block/recast, and 5500-range "
        "manual R landing geometry.";
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
