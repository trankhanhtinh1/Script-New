#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Galio = [] {
    ChampionProfile p{};
    p.ChampionId = SDK::ChampionId::Galio;
    p.DisplayName = "Galio";
    p.InternalId = "champion.kuroaio.ai.galio";
    p.PrimaryArchetype = Archetype::Vanguard;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::Charge | Mechanic::Channel | Mechanic::Recast |
                  Mechanic::Dash | Mechanic::AllyTarget | Mechanic::DirectionalSweet |
                  Mechanic::AutoReset;
    p.Ultimate = UltimatePolicy::SaveAlly;
    p.PreferredCombatDistance = 475.0f;
    p.EngageHealthPercent = 62.0f;
    p.DefensiveHealthPercent = 38.0f;
    p.UltimateTargetHealthPercent = 65.0f;
    p.UltimateMinimumTargets = 1;
    p.MaximumCommitEnemies = 3;
    p.BaseHumanizerMs = 65;
    p.PreferSelectedTarget = true;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "GalioPassiveReady";
    p.ChannelBuff = "GalioW";
    p.UltimateBuff = "GalioR";
    p.TacticalSummary =
        "Anti-mage vanguard: weave Colossal Smash resets, shape Q gusts, hold and release W taunt, use E knockup with a safe backstep, and land Hero's Entrance only on a defensible ally position.";
    p.ResearchSummary =
        "Riot 26.15 / CommunityDragon 16.15 values with spell and channel state reconciled from process-spell, buff, and polling observations.";

    p.Spells[0] = Spell(SDK::SpellSlot::Q, "Winds of War", CastKind::Line,
        Intent::Damage | Intent::CrowdControl | Intent::Waveclear | Intent::Jungle |
            Intent::LastHit,
        AllModes, 825.0f, 0.25f, 120.0f, 1300.0f, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[0].Hitchance = SDK::HitChance::High;
    p.Spells[0].PreserveAutoAttack = false;

    p.Spells[1] = Spell(SDK::SpellSlot::W, "Shield of Durand", CastKind::ChargedCircle,
        Intent::Damage | Intent::CrowdControl | Intent::Channel | Intent::Peel |
            Intent::Interrupt | Intent::AntiGapcloser,
        Mode::Combo | Mode::Harass | Mode::Automatic | Mode::Flee,
        350.0f, 0.25f, 350.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[1].Aim = AimPolicy::SelfPosition;
    p.Spells[1].Priority = 98;
    p.Spells[1].ChargeBuffName = "GalioW";
    p.Spells[1].ChargeMinRange = 150;
    p.Spells[1].ChargeMaxRange = 350;
    p.Spells[1].ChargeDurationSeconds = 2.0f;
    p.Spells[1].RequiredPlayerBuff = "GalioW";

    p.Spells[2] = Spell(SDK::SpellSlot::E, "Justice Punch", CastKind::Direction,
        Intent::Damage | Intent::CrowdControl | Intent::Mobility | Intent::Engage |
            Intent::Disengage | Intent::Interrupt | Intent::Peel,
        AllModes, 650.0f, 0.25f, 160.0f, 1200.0f, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[2].Aim = AimPolicy::SafeCursor;
    p.Spells[2].Priority = 94;

    p.Spells[3] = Spell(SDK::SpellSlot::R, "Hero's Entrance", CastKind::AllyTarget,
        Intent::Damage | Intent::CrowdControl | Intent::Mobility | Intent::Engage |
            Intent::Disengage | Intent::Peel | Intent::AllyUtility | Intent::Interrupt,
        Mode::Combo | Mode::Automatic | Mode::Flee,
        4000.0f, 1.25f, 650.0f, 1800.0f, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[3].Aim = AimPolicy::TargetPosition;
    p.Spells[3].Priority = 100;
    p.Spells[3].MinimumAoeTargets = 1;
    p.Spells[3].MaximumEnemiesAtDestination = 3;

    p.Trade = Plan("Gust and taunt trade",
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget, 0, 850),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget | StepRule::AllowDuringWindup, 80, 900),
        Step(SDK::SpellSlot::E, StepRule::RequireSafePosition, 180, 900));
    p.AllIn = Plan("Justice entry and Hero landing",
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget, 0, 850),
        Step(SDK::SpellSlot::E, StepRule::RequireSafePosition, 100, 850),
        Step(SDK::SpellSlot::W, StepRule::RequireCrowdControl | StepRule::AllowDuringWindup, 180, 1100),
        Step(SDK::SpellSlot::R, StepRule::RequireTarget | StepRule::RequireSafePosition, 280, 2500));
    p.Flee = Plan("Durand peel",
        Step(SDK::SpellSlot::W, StepRule::RequireSafePosition | StepRule::AllowDuringWindup, 0, 900),
        Step(SDK::SpellSlot::E, StepRule::RequireSafePosition, 90, 850),
        Step(SDK::SpellSlot::R, StepRule::ManualAssistOnly | StepRule::RequireSafePosition, 150, 2500));
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
