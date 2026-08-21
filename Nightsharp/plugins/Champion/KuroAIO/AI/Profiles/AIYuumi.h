#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Yuumi = [] {
    ChampionProfile p{};
    p.ChampionId = SDK::ChampionId::Yuumi;
    p.DisplayName = "Yuumi";
    p.InternalId = "champion.kuroaio.ai.yuumi";
    p.PrimaryArchetype = Archetype::Enchanter;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::AllyTarget | Mechanic::Tether | Mechanic::Channel |
                  Mechanic::Ammo | Mechanic::DirectionalSweet |
                  Mechanic::MissingHealth | Mechanic::Recast;
    p.Ultimate = UltimatePolicy::SaveAlly;

    p.Spells[0] = Spell(
        SDK::SpellSlot::Q, "Prowling Projectile", CastKind::Direction,
        Intent::Damage | Intent::CrowdControl | Intent::Setup,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::LastHit | Mode::Flee | Mode::Automatic,
        1050.0f, 0.20f, 50.0f, 850.0f, true,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[0].Aim = AimPolicy::Prediction;
    p.Spells[0].Priority = 82;
    p.Spells[0].TriggerRange = 1050.0f;
    p.Spells[0].PreserveAutoAttack = true;

    p.Spells[1] = Spell(
        SDK::SpellSlot::W, "You and Me!", CastKind::AllyTarget,
        Intent::Mobility | Intent::Buff | Intent::AllyUtility | Intent::Peel |
            Intent::Disengage | Intent::Recast,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::Automatic,
        1100.0f, 0.0f, 0.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::Targeted);
    p.Spells[1].Priority = 96;
    p.Spells[1].TriggerRange = 1100.0f;
    p.Spells[1].PreserveAutoAttack = true;

    p.Spells[2] = Spell(
        SDK::SpellSlot::E, "Zoomies", CastKind::Self,
        Intent::Heal | Intent::Shield | Intent::Buff | Intent::AllyUtility |
            Intent::Disengage | Intent::Setup,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::Automatic,
        1100.0f, 0.25f, 80.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::Targeted);
    p.Spells[2].Aim = AimPolicy::SelfPosition;
    p.Spells[2].Priority = 99;
    p.Spells[2].TriggerRange = 1100.0f;
    p.Spells[2].PreserveAutoAttack = true;

    p.Spells[3] = Spell(
        SDK::SpellSlot::R, "Final Chapter", CastKind::Direction,
        Intent::Damage | Intent::Heal | Intent::CrowdControl | Intent::Channel |
            Intent::AllyUtility | Intent::Peel | Intent::Setup,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::Automatic,
        1100.0f, 0.25f, 100.0f, 1100.0f, true,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[3].Aim = AimPolicy::Prediction;
    p.Spells[3].Priority = 100;
    p.Spells[3].TriggerRange = 1100.0f;
    p.Spells[3].PlayerHealthPercent = 72.0f;
    p.Spells[3].PreserveAutoAttack = true;

    p.Trade = Plan(
        "Steer the prowling missile while attached, then preserve the bound ally with Zoomies",
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget),
        Step(SDK::SpellSlot::E, StepRule::RequirePlayerLow | StepRule::AllowDuringWindup));
    p.AllIn = Plan(
        "Attach to the safest carry, shield through Zoomies, and channel Final Chapter toward a safe wave",
        Step(SDK::SpellSlot::W, StepRule::RequireTarget | StepRule::RequireSafePosition),
        Step(SDK::SpellSlot::E, StepRule::RequireSafePosition | StepRule::AllowDuringWindup),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget),
        Step(SDK::SpellSlot::R, StepRule::RequireMultiTarget));
    p.Flee = Plan(
        "Attach to a safe ally, spend Zoomies on the endangered partner, and steer Final Chapter for peel",
        Step(SDK::SpellSlot::W, StepRule::RequireTarget | StepRule::RequireSafePosition | StepRule::AllowDuringWindup),
        Step(SDK::SpellSlot::E, StepRule::RequireSafePosition | StepRule::AllowDuringWindup),
        Step(SDK::SpellSlot::R, StepRule::RequirePlayerLow | StepRule::AllowDuringWindup));

    p.PreferredCombatDistance = 800.0f;
    p.EngageHealthPercent = 60.0f;
    p.DefensiveHealthPercent = 48.0f;
    p.UltimateTargetHealthPercent = 74.0f;
    p.UltimateMinimumTargets = 2;
    p.MaximumCommitEnemies = 4;
    p.BaseHumanizerMs = 55;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "YuumiPassive";
    p.FormBuff = "YuumiW";
    p.ChannelBuff = "YuumiR";
    p.UltimateBuff = "YuumiR";
    p.TrackedObjectToken = "YuumiQ";
    p.ThemeFrom = 0xFFFFB7E8u;
    p.ThemeTo = 0xFFB67CFFu;
    p.ThemeSpeed = 0.92f;
    p.TacticalSummary =
        "Reconcile attached and detached You and Me! state, steer Prowling Projectile while bound, protect the Best Friend with Zoomies, and channel safe Final Chapter waves.";
    p.ResearchSummary =
        "Riot 26.15 / CommunityDragon 16.15 Yuumi values, spell names, attached steering and wave behavior are recorded in AI/Research/AIYuumi.md.";
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
