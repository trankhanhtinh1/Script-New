#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Rell = [] {
    ChampionProfile p{};
    p.ChampionId = SDK::ChampionId::Rell;
    p.DisplayName = "Rell";
    p.InternalId = "champion.kuroaio.ai.rell";
    p.PrimaryArchetype = Archetype::Vanguard;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::Transform | Mechanic::Stance | Mechanic::AllyTarget |
                  Mechanic::Tether | Mechanic::Channel | Mechanic::AutoWeave |
                  Mechanic::MissingHealth | Mechanic::WallInteraction;
    p.Ultimate = UltimatePolicy::MultiTarget;

    p.Spells[0] = Spell(
        SDK::SpellSlot::Q, "Shattering Strike", CastKind::Line,
        Intent::Damage | Intent::Heal | Intent::CrowdControl | Intent::Setup |
            Intent::Peel | Intent::Waveclear | Intent::Jungle | Intent::LastHit,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::LastHit | Mode::Flee | Mode::Automatic,
        700.0f, 0.40f, 70.0f, 1450.0f, true,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[0].Aim = AimPolicy::Prediction;
    p.Spells[0].Priority = 89;
    p.Spells[0].TriggerRange = 700.0f;
    p.Spells[0].PreserveAutoAttack = true;

    p.Spells[1] = Spell(
        SDK::SpellSlot::W, "Ferromancy", CastKind::Position,
        Intent::Mobility | Intent::Engage | Intent::Disengage | Intent::CrowdControl |
            Intent::Shield | Intent::Buff | Intent::Peel | Intent::AllyUtility,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::Automatic,
        400.0f, 0.25f, 200.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[1].Aim = AimPolicy::SafeCursor;
    p.Spells[1].Priority = 96;
    p.Spells[1].TriggerRange = 400.0f;
    p.Spells[1].PreserveAutoAttack = true;

    p.Spells[2] = Spell(
        SDK::SpellSlot::E, "Attract and Repel", CastKind::AllyTarget,
        Intent::CrowdControl | Intent::AllyUtility | Intent::Peel | Intent::Setup |
            Intent::Buff | Intent::Interrupt,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::Automatic,
        1000.0f, 0.25f, 70.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[2].Aim = AimPolicy::TargetPosition;
    p.Spells[2].Priority = 98;
    p.Spells[2].TriggerRange = 1000.0f;
    p.Spells[2].PreserveAutoAttack = true;

    p.Spells[3] = Spell(
        SDK::SpellSlot::R, "Magnet Storm", CastKind::Self,
        Intent::Damage | Intent::CrowdControl | Intent::Engage | Intent::Peel |
            Intent::Interrupt | Intent::Channel | Intent::AllyUtility,
        Mode::Combo | Mode::Flee | Mode::Automatic,
        450.0f, 0.25f, 450.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[3].Aim = AimPolicy::SelfPosition;
    p.Spells[3].Priority = 100;
    p.Spells[3].TriggerRange = 450.0f;
    p.Spells[3].MinimumAoeTargets = 2;
    p.Spells[3].PreserveAutoAttack = true;

    p.Trade = Plan(
        "Preserve mount state, tether the safest ally, use Q to shatter and heal, then stun the tether line",
        Step(SDK::SpellSlot::E, StepRule::RequireSafePosition | StepRule::AllowDuringWindup),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget | StepRule::AllowDuringWindup));
    p.AllIn = Plan(
        "Crash down only on a safe predicted endpoint, tether the carry for a line stun, then hold Magnet Storm through the channel",
        Step(SDK::SpellSlot::E, StepRule::RequireSafePosition),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget),
        Step(SDK::SpellSlot::R, StepRule::RequireMultiTarget));
    p.Flee = Plan(
        "Keep the ally tethered, mount toward a safe ally or cursor route, and use Q or R only for peel that protects the retreat",
        Step(SDK::SpellSlot::E, StepRule::RequireSafePosition | StepRule::AllowDuringWindup),
        Step(SDK::SpellSlot::W, StepRule::RequireSafePosition | StepRule::AllowDuringWindup),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::AllowDuringWindup),
        Step(SDK::SpellSlot::R, StepRule::RequirePlayerLow));

    p.PreferredCombatDistance = 350.0f;
    p.EngageHealthPercent = 62.0f;
    p.DefensiveHealthPercent = 43.0f;
    p.UltimateTargetHealthPercent = 70.0f;
    p.UltimateMinimumTargets = 2;
    p.MaximumCommitEnemies = 3;
    p.BaseHumanizerMs = 55;
    p.PreferSelectedTarget = true;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "RellPassive";
    p.FormBuff = "RellW";
    p.ChannelBuff = "RellE";
    p.UltimateBuff = "RellR";
    p.TrackedObjectToken = "Rell";
    p.ThemeFrom = 0xFFB6D9FFu;
    p.ThemeTo = 0xFF6F8DFFu;
    p.ThemeSpeed = 0.90f;
    p.TacticalSummary =
        "Reconciles mounted and dismounted Ferromancy state, Q heal and shatter, W endpoint safety, E tether ownership and R magnetic pull channeling.";
    p.ResearchSummary =
        "Riot 26.15 / CommunityDragon 16.15 Rell values, spell names and mount state are recorded in AIRell.md.";
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
