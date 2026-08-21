#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Hecarim = [] {
    ChampionProfile p{};
    p.ChampionId = SDK::ChampionId::Hecarim;
    p.DisplayName = "Hecarim";
    p.InternalId = "champion.kuroaio.ai.hecarim";
    p.PrimaryArchetype = Archetype::Diver;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::Stack | Mechanic::Charge | Mechanic::Dash |
                  Mechanic::Channel | Mechanic::Terrain | Mechanic::AutoWeave;
    p.Ultimate = UltimatePolicy::AllIn;
    p.PreferredCombatDistance = 350.0f;
    p.EngageHealthPercent = 55.0f;
    p.DefensiveHealthPercent = 42.0f;
    p.UltimateTargetHealthPercent = 55.0f;
    p.MaximumCommitEnemies = 2;
    p.BaseHumanizerMs = 55;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "HecarimPassive";
    p.MarkBuff = "HecarimRamp";
    p.ChannelBuff = "HecarimCharge";
    p.UltimateBuff = "HecarimUlt";
    p.TacticalSummary =
        "Rampage stack diver: preserve Q stacks, cast Spirit of Dread for sustained healing, "
        "charge through safe paths, and fear with a terrain-safe Onslaught of Shadows.";
    p.ResearchSummary =
        "Riot 26.15 / CommunityDragon 16.15 Hecarim Q stacks, W healing zone, E charge/ram, "
        "and R spectral wave endpoint safety are modeled.";

    p.Spells[0] = Spell(SDK::SpellSlot::Q, "Rampage", CastKind::Self,
        Intent::Damage | Intent::Waveclear | Intent::Jungle | Intent::LastHit | Intent::AutoReset,
        AllModes, 375.0f, 0.1f, 375.0f, 1450.0f, false,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotCircle);
    p.Spells[0].Priority = 92;
    p.Spells[0].PreserveAutoAttack = true;
    p.Spells[0].ComboManaPercent = 0.0f;
    p.Spells[0].HarassManaPercent = 35.0f;
    p.Spells[0].ClearManaPercent = 25.0f;

    p.Spells[1] = Spell(SDK::SpellSlot::W, "Spirit of Dread", CastKind::Self,
        Intent::Damage | Intent::Heal | Intent::Buff | Intent::Jungle,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle | Mode::Automatic,
        525.0f, 0.1f, 575.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCircle);
    p.Spells[1].Priority = 78;
    p.Spells[1].PlayerHealthPercent = 72.0f;
    p.Spells[1].HarassManaPercent = 45.0f;
    p.Spells[1].ClearManaPercent = 35.0f;

    p.Spells[2] = Spell(SDK::SpellSlot::E, "Devastating Charge", CastKind::Direction,
        Intent::Damage | Intent::Mobility | Intent::Engage | Intent::Disengage | Intent::CrowdControl,
        Mode::Combo | Mode::Harass | Mode::Jungle | Mode::Automatic | Mode::Flee,
        1350.0f, 0.5f, 120.0f, 1200.0f, false,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotLine);
    p.Spells[2].Aim = AimPolicy::Prediction;
    p.Spells[2].Priority = 88;
    p.Spells[2].PreserveAutoAttack = false;

    p.Spells[3] = Spell(SDK::SpellSlot::R, "Onslaught of Shadows", CastKind::Position,
        Intent::Damage | Intent::CrowdControl | Intent::Mobility | Intent::Engage |
            Intent::Disengage | Intent::Finisher,
        Mode::Combo | Mode::Automatic | Mode::Flee,
        1000.0f, 0.01f, 200.0f, 1200.0f, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    p.Spells[3].Aim = AimPolicy::Prediction;
    p.Spells[3].Priority = 100;
    p.Spells[3].MaximumEnemiesAtDestination = 2;
    p.Spells[3].PreserveAutoAttack = false;

    p.Trade = Plan("maintain Rampage stacks and sustain in Spirit of Dread",
        Step(SDK::SpellSlot::W, StepRule::RequireMultiTarget | StepRule::RequirePlayerLow, 0, 900),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequireAfterAttack, 40, 850),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::AllowDuringWindup, 100, 1100));
    p.AllIn = Plan("charge through a safe fear endpoint",
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::AllowDuringWindup, 0, 1000),
        Step(SDK::SpellSlot::W, StepRule::RequireMultiTarget, 80, 1100),
        Step(SDK::SpellSlot::R, StepRule::RequireTarget | StepRule::RequireSafePosition, 140, 1200),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::AllowDuringWindup, 200, 1450));
    p.Flee = Plan("charge and spectral fear peel",
        Step(SDK::SpellSlot::E, StepRule::RequirePlayerLow | StepRule::AllowDuringWindup, 0, 900),
        Step(SDK::SpellSlot::R, StepRule::RequireTarget | StepRule::RequireSafePosition, 100, 1100));
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
