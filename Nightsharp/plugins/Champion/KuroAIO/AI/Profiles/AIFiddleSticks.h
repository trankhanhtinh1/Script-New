#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile FiddleSticks = [] {
    ChampionProfile p{};
    p.ChampionId = SDK::ChampionId::Fiddlesticks;
    p.DisplayName = "Fiddlesticks";
    p.InternalId = "champion.kuroaio.ai.fiddlesticks";
    p.PrimaryArchetype = Archetype::Battlemage;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::Channel | Mechanic::Blink | Mechanic::ObjectTracking |
                  Mechanic::Terrain | Mechanic::MissingHealth | Mechanic::Pet;
    p.Ultimate = UltimatePolicy::MultiTarget;
    p.PreferredCombatDistance = 575.0f;
    p.EngageHealthPercent = 58.0f;
    p.DefensiveHealthPercent = 30.0f;
    p.UltimateTargetHealthPercent = 58.0f;
    p.UltimateMinimumTargets = 2;
    p.MaximumCommitEnemies = 3;
    p.BaseHumanizerMs = 70;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "FiddlesticksEffigy";
    p.ChannelBuff = "FiddlesticksW";
    p.UltimateBuff = "FiddlesticksR";
    p.TrackedObjectToken = "FiddlesticksEffigy";
    p.TacticalSummary =
        "Vision-gated fear mage: track effigy and brush/fog state, drain only while "
        "a W channel is owned and safe, use center Reap silence, and channel "
        "Crowstorm from unseen terrain into a predicted multi-target landing.";
    p.ResearchSummary =
        "Pinned to Riot 26.15 and CommunityDragon 16.15 Summoner's Rift values; "
        "Q 575 target fear, W 650 two-second drain, E 850 cone and R 800-range "
        "1.5-second channel/teleport storm are reconciled from events and polling.";

    p.Spells[0] = Spell(
        SDK::SpellSlot::Q, "Terrify", CastKind::EnemyTarget,
        Intent::Damage | Intent::CrowdControl | Intent::Engage | Intent::Setup |
            Intent::Finisher,
        Mode::Combo | Mode::Harass | Mode::Flee | Mode::Automatic,
        575.0f, 0.25f, 80.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::Targeted);
    p.Spells[0].Aim = AimPolicy::TargetPosition;
    p.Spells[0].Priority = 96;
    p.Spells[0].Hitchance = SDK::HitChance::High;
    p.Spells[0].TargetHealthPercent = 78.0f;
    p.Spells[0].PreserveAutoAttack = true;

    p.Spells[1] = Spell(
        SDK::SpellSlot::W, "Bountiful Harvest", CastKind::Self,
        Intent::Damage | Intent::Heal | Intent::Channel | Intent::Execute |
            Intent::Jungle | Intent::Waveclear | Intent::LastHit,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::LastHit | Mode::Flee | Mode::Automatic,
        650.0f, 0.25f, 650.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[1].Aim = AimPolicy::SelfPosition;
    p.Spells[1].Priority = 92;
    p.Spells[1].RequiredPlayerBuff = "FiddlesticksW";
    p.Spells[1].ClearManaPercent = 30.0f;
    p.Spells[1].HarassManaPercent = 44.0f;
    p.Spells[1].PreserveAutoAttack = false;

    p.Spells[2] = Spell(
        SDK::SpellSlot::E, "Reap", CastKind::Cone,
        Intent::Damage | Intent::CrowdControl | Intent::Setup | Intent::Peel |
            Intent::Interrupt | Intent::AntiGapcloser | Intent::Waveclear |
            Intent::Jungle | Intent::LastHit,
        AllModes, 850.0f, 0.25f, 105.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCone);
    p.Spells[2].Aim = AimPolicy::Prediction;
    p.Spells[2].Priority = 88;
    p.Spells[2].Hitchance = SDK::HitChance::High;
    p.Spells[2].Collision = false;
    p.Spells[2].PreserveAutoAttack = true;

    p.Spells[3] = Spell(
        SDK::SpellSlot::R, "Crowstorm", CastKind::Position,
        Intent::Damage | Intent::Channel | Intent::Mobility | Intent::Engage |
            Intent::Disengage | Intent::Execute | Intent::Vision | Intent::Objective,
        Mode::Combo | Mode::Flee | Mode::Automatic,
        800.0f, 1.50f, 600.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[3].Aim = AimPolicy::BestAoe;
    p.Spells[3].Priority = 100;
    p.Spells[3].MinimumAoeTargets = 2;
    p.Spells[3].MaximumEnemiesAtDestination = 3;
    p.Spells[3].TargetHealthPercent = 60.0f;
    p.Spells[3].PreserveAutoAttack = true;

    p.Trade = Plan("Fear, silence and safe drain",
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequireNoCrowdControl, 0, 900),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireNoCrowdControl, 100, 1000),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget | StepRule::RequireSafePosition, 220, 1800));
    p.AllIn = Plan("Unseen Crowstorm ambush",
        Step(SDK::SpellSlot::R, StepRule::RequireTarget | StepRule::RequireSafePosition, 0, 2200),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget, 1500, 2600),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget, 1600, 2800),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget | StepRule::RequireSafePosition, 1750, 3600));
    p.Flee = Plan("Fear and brush retreat",
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::AllowDuringWindup, 0, 850),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::AllowDuringWindup, 80, 950),
        Step(SDK::SpellSlot::R, StepRule::RequireSafePosition | StepRule::RequireTarget, 180, 2200),
        Step(SDK::SpellSlot::W, StepRule::RequirePlayerLow | StepRule::RequireSafePosition, 300, 1900));
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
