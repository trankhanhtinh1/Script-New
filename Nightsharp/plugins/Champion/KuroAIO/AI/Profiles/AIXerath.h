#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Xerath = [] {
    ChampionProfile p{};
    p.ChampionId = SDK::ChampionId::Xerath;
    p.DisplayName = "Xerath";
    p.InternalId = "champion.kuroaio.ai.xerath";
    p.PrimaryArchetype = Archetype::BurstMage;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::Charge | Mechanic::Channel | Mechanic::Ammo |
                  Mechanic::Recast | Mechanic::DirectionalSweet |
                  Mechanic::WallInteraction | Mechanic::SpellShield;
    p.Ultimate = UltimatePolicy::RecastControl;
    p.PreferredCombatDistance = 900.0f;
    p.EngageHealthPercent = 55.0f;
    p.DefensiveHealthPercent = 28.0f;
    p.UltimateTargetHealthPercent = 48.0f;
    p.UltimateMinimumTargets = 1;
    p.MaximumCommitEnemies = 2;
    p.BaseHumanizerMs = 64;
    p.PreferSelectedTarget = true;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "XerathAscended2OnHit";
    p.ChannelBuff = "XerathRRampUp";
    p.UltimateBuff = "XerathRShots";
    p.TacticalSummary =
        "Long-range artillery mage: charge Arcanopulse to the required distance, use Eye of Destruction center hits after control, and spend Rite of the Arcane ammo only from a safe channel.";
    p.ResearchSummary =
        "Riot 26.15 / CommunityDragon 16.15 values: Q 75/115/155/195/235 plus 85 percent AP with a 4 second charge and 750 to 1125 range; W 50/85/120/155/190 plus 65 percent AP with a 1.667 center multiplier; E 70/100/130/160/190 plus 45 percent AP, 70 width first-collision missile and 0.75 to 2.25 second distance stun; R 4/5/6 shots, 170/220/270 plus 45 percent AP and 20/25/30 plus 5 percent AP per prior hit over a 10 second channel.";

    p.Spells[0] = Spell(
        SDK::SpellSlot::Q, "Arcanopulse", CastKind::ChargedLine,
        Intent::Damage | Intent::Setup | Intent::Waveclear |
            Intent::Jungle | Intent::LastHit | Intent::Channel,
        AllModes, 1125.0f, 0.25f, 145.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[0].Aim = AimPolicy::Prediction;
    p.Spells[0].Priority = 90;
    p.Spells[0].Hitchance = SDK::HitChance::High;
    p.Spells[0].ChargeMinRange = 750;
    p.Spells[0].ChargeMaxRange = 1125;
    p.Spells[0].ChargeDurationSeconds = 4.0f;
    p.Spells[0].ChargeBuffName = "XerathArcanopulseChargeUp";
    p.Spells[0].PreserveAutoAttack = true;
    p.Spells[0].HarassManaPercent = 42.0f;
    p.Spells[0].ClearManaPercent = 35.0f;

    p.Spells[1] = Spell(
        SDK::SpellSlot::W, "Eye of Destruction", CastKind::Circle,
        Intent::Damage | Intent::Setup | Intent::Waveclear | Intent::Jungle,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::LastHit | Mode::Automatic,
        1000.0f, 0.50f, 250.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[1].Aim = AimPolicy::Prediction;
    p.Spells[1].Priority = 86;
    p.Spells[1].Hitchance = SDK::HitChance::High;
    p.Spells[1].PreserveAutoAttack = true;
    p.Spells[1].HarassManaPercent = 54.0f;

    p.Spells[2] = Spell(
        SDK::SpellSlot::E, "Shocking Orb", CastKind::Line,
        Intent::Damage | Intent::CrowdControl | Intent::Setup |
            Intent::Interrupt | Intent::AntiGapcloser | Intent::Peel,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::Automatic,
        1050.0f, 0.25f, 70.0f, 1600.0f, true,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[2].Aim = AimPolicy::Prediction;
    p.Spells[2].Priority = 98;
    p.Spells[2].Hitchance = SDK::HitChance::High;
    p.Spells[2].PreserveAutoAttack = true;

    p.Spells[3] = Spell(
        SDK::SpellSlot::R, "Rite of the Arcane", CastKind::Position,
        Intent::Damage | Intent::Finisher | Intent::Channel | Intent::Recast |
            Intent::Interrupt,
        Mode::Combo | Mode::Flee | Mode::Automatic,
        5000.0f, 10.0f, 200.0f, 20.0f, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[3].Aim = AimPolicy::TargetPosition;
    p.Spells[3].Priority = 100;
    p.Spells[3].MinimumAmmo = 1;
    p.Spells[3].TargetHealthPercent = 48.0f;
    p.Spells[3].PreserveAutoAttack = true;
    p.Spells[3].RecastSpellName = "XerathLocusPulse";

    p.Trade = Plan(
        "Orb catch into center barrage and charged beam",
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireSafePosition, 0, 900),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget | StepRule::RequireCrowdControl, 120, 1100),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::AllowDuringWindup, 280, 1500));
    p.AllIn = Plan(
        "Shocking Orb setup, center Eye, charged Arcanopulse and safe artillery",
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireSafePosition, 0, 900),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget, 150, 1200),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget, 260, 1500),
        Step(SDK::SpellSlot::R, StepRule::RequireTarget | StepRule::RequireSafePosition |
             StepRule::HoldForExecute, 600, 2800));
    p.Flee = Plan(
        "Shocking Orb peel while preserving a manual artillery channel",
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireSafePosition |
             StepRule::AllowDuringWindup, 0, 850),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::AllowDuringWindup, 120, 1100));
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
