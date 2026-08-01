#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Rumble = [] {
    ChampionProfile profile{};
    profile.ChampionId = SDK::ChampionId::Rumble;
    profile.DisplayName = "Rumble";
    profile.InternalId = "champion.kuroaio.ai.rumble";
    profile.PrimaryArchetype = Archetype::Battlemage;
    profile.Resource = ResourceModel::Special;
    profile.Mechanics = Mechanic::Ammo | Mechanic::Stance |
        Mechanic::DirectionalSweet | Mechanic::AutoWeave;
    profile.Ultimate = UltimatePolicy::MultiTarget;
    profile.PreferredCombatDistance = 500.0f;
    profile.EngageHealthPercent = 62.0f;
    profile.DefensiveHealthPercent = 34.0f;
    profile.UltimateTargetHealthPercent = 45.0f;
    profile.UltimateMinimumTargets = 2;
    profile.MaximumCommitEnemies = 2;
    profile.BaseHumanizerMs = 48;
    profile.PreferSelectedTarget = true;
    profile.AllowTurretDiveIfKillable = false;
    profile.ProtectManualChannels = true;
    profile.TrackedObjectToken = "RumbleCarpetBomb";
    profile.TacticalSummary =
        "Heat-band battlemage: hold Danger Zone for empowered flames, shield "
        "and harpoons; overheat only into a deliberate close-range attack "
        "window; place Equalizer along escape paths without turret pursuit.";
    profile.ResearchSummary =
        "Riot 26.15 / CommunityDragon 16.15; Summoner's Rift baseline.";

    profile.Spells[0] = Spell(
        SDK::SpellSlot::Q, "Flamespitter", CastKind::Cone,
        Intent::Damage | Intent::Waveclear | Intent::Jungle,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::Automatic,
        600.0f, 0.25f, 500.0f, 5000.0f, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotCone);
    profile.Spells[0].Aim = AimPolicy::Prediction;
    profile.Spells[0].Priority = 91;
    profile.Spells[0].MinimumAoeTargets = 3;
    profile.Spells[0].PreserveAutoAttack = true;

    profile.Spells[1] = Spell(
        SDK::SpellSlot::W, "Scrap Shield", CastKind::Self,
        Intent::Shield | Intent::Buff | Intent::Disengage,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::Flee | Mode::Automatic,
        0.0f, 0.0f, 0.0f, FLT_MAX, false,
        SDK::DamageType::Magical, SDK::SpellType::Targeted);
    profile.Spells[1].Aim = AimPolicy::SelfPosition;
    profile.Spells[1].Priority = 84;
    profile.Spells[1].PreserveAutoAttack = true;

    profile.Spells[2] = Spell(
        SDK::SpellSlot::E, "Electro Harpoon", CastKind::Line,
        Intent::Damage | Intent::CrowdControl | Intent::Peel |
            Intent::AntiGapcloser | Intent::LastHit | Intent::Jungle,
        AllModes,
        850.0f, 0.25f, 90.0f, 1200.0f, true,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    profile.Spells[2].Aim = AimPolicy::Prediction;
    profile.Spells[2].Priority = 94;
    profile.Spells[2].MinimumAmmo = 1;
    profile.Spells[2].PreserveAutoAttack = true;
    profile.Spells[2].AllowOnMinions = true;

    profile.Spells[3] = Spell(
        SDK::SpellSlot::R, "The Equalizer", CastKind::Vector,
        Intent::Damage | Intent::CrowdControl | Intent::Engage |
            Intent::Disengage | Intent::Execute | Intent::Peel |
            Intent::Objective,
        Mode::Combo | Mode::Flee | Mode::Automatic,
        1700.0f, 0.5833f, 200.0f, 1600.0f, false,
        SDK::DamageType::Magical, SDK::SpellType::SkillshotLine);
    profile.Spells[3].Aim = AimPolicy::BestAoe;
    profile.Spells[3].Priority = 100;
    profile.Spells[3].MinimumAoeTargets = 2;
    profile.Spells[3].PreserveAutoAttack = true;

    profile.Trade = Plan(
        "Danger Zone harpoon flame trade",
        Step(SDK::SpellSlot::E, StepRule::RequireTarget, 0, 700),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget, 80, 950),
        Step(SDK::SpellSlot::W, StepRule::RequireSafePosition, 120, 1050));
    profile.AllIn = Plan(
        "Equalizer heat conversion",
        Step(SDK::SpellSlot::R,
             StepRule::RequireTarget | StepRule::RequireSafePosition, 0, 900),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget, 80, 1050),
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget, 130, 1150),
        Step(SDK::SpellSlot::W, StepRule::RequireSafePosition, 180, 1250));
    profile.Flee = Plan(
        "Shielded harpoon retreat",
        Step(SDK::SpellSlot::W,
             StepRule::RequireSafePosition | StepRule::AllowDuringWindup,
             0, 650),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget, 40, 800),
        Step(SDK::SpellSlot::R,
             StepRule::RequireTarget | StepRule::ManualAssistOnly,
             100, 1000));
    return profile;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
