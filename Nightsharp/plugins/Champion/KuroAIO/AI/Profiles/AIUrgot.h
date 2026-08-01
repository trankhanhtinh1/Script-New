#pragma once

#include "../AIChampionProfile.h"

namespace Plugins::KuroAIO::AI::Profiles {

inline constexpr ChampionProfile Urgot = [] {
    ChampionProfile p{};
    p.ChampionId = SDK::ChampionId::Urgot;
    p.DisplayName = "Urgot";
    p.InternalId = "champion.kuroaio.ai.urgot";
    p.PrimaryArchetype = Archetype::Juggernaut;
    p.Resource = ResourceModel::Mana;
    p.Mechanics = Mechanic::Mark | Mechanic::ObjectTracking |
                  Mechanic::Execute | Mechanic::Recast | Mechanic::Dash |
                  Mechanic::Terrain | Mechanic::AutoWeave;
    p.Ultimate = UltimatePolicy::Execute;
    p.PreferredCombatDistance = 425.0f;
    p.EngageHealthPercent = 48.0f;
    p.DefensiveHealthPercent = 32.0f;
    p.UltimateTargetHealthPercent = 35.0f;
    p.UltimateMinimumTargets = 1;
    p.MaximumCommitEnemies = 2;
    p.BaseHumanizerMs = 70;
    p.PreferSelectedTarget = true;
    p.AllowTurretDiveIfKillable = false;
    p.ProtectManualChannels = true;
    p.PassiveBuff = "UrgotPassive";
    p.MarkBuff = "UrgotWTarget";
    p.ChannelBuff = "UrgotRRecastChannel";
    p.UltimateBuff = "UrgotR";
    p.TrackedObjectToken = "UrgotPassiveZone";
    p.TacticalSummary =
        "Juggernaut marks a target with Q, controls Purge's toggle and lowest-health lock, trades through a shielded E flip, and reserves Fear Beyond Death for the execute/recast window.";
    p.ResearchSummary =
        "Riot 26.15 / CommunityDragon 16.15 Urgot Q slow/mark, W toggle, six passive legs, E displacement shield and R execute/fear state.";

    p.Spells[0] = Spell(SDK::SpellSlot::Q, "Corrosive Charge", CastKind::Position,
        Intent::Damage | Intent::CrowdControl | Intent::Setup | Intent::Waveclear |
            Intent::Jungle | Intent::LastHit,
        AllModes, 800.0f, 0.30f, 210.0f, 1600.0f, true,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotCircle);
    p.Spells[0].Aim = AimPolicy::Prediction;
    p.Spells[0].Priority = 84;
    p.Spells[0].RequiredTargetBuff = "";

    p.Spells[1] = Spell(SDK::SpellSlot::W, "Purge", CastKind::Toggle,
        Intent::Damage | Intent::Engage | Intent::Waveclear | Intent::Jungle |
            Intent::LastHit,
        Mode::Combo | Mode::Harass | Mode::LaneClear | Mode::Jungle |
            Mode::Automatic | Mode::Flee,
        490.0f, 0.05f, 70.0f, 2500.0f, false,
        SDK::DamageType::Physical, SDK::SpellType::Targeted);
    p.Spells[1].Aim = AimPolicy::SelfPosition;
    p.Spells[1].Priority = 92;
    p.Spells[1].WeaveAfterAttack = true;
    p.Spells[1].PreserveAutoAttack = false;

    p.Spells[2] = Spell(SDK::SpellSlot::E, "Disdain", CastKind::Position,
        Intent::Damage | Intent::Mobility | Intent::CrowdControl | Intent::Engage |
            Intent::Disengage | Intent::Shield | Intent::Peel | Intent::Interrupt,
        AllModes, 475.0f, 0.45f, 120.0f, 1750.0f, true,
        SDK::DamageType::Physical, SDK::SpellType::SkillshotLine);
    p.Spells[2].Aim = AimPolicy::Prediction;
    p.Spells[2].Priority = 97;
    p.Spells[2].MaximumEnemiesAtDestination = 2;

    p.Spells[3] = Spell(SDK::SpellSlot::R, "Fear Beyond Death", CastKind::Position,
        Intent::Damage | Intent::CrowdControl | Intent::Execute | Intent::Recast |
            Intent::Finisher,
        Mode::Combo | Mode::Flee | Mode::Automatic, 2500.0f, 0.50f, 80.0f,
        3200.0f, false, SDK::DamageType::Physical, SDK::SpellType::SkillshotLine);
    p.Spells[3].Aim = AimPolicy::Prediction;
    p.Spells[3].Priority = 100;
    p.Spells[3].TargetHealthPercent = 35.0f;
    p.Spells[3].RecastSpellName = "UrgotRRecast";

    p.Trade = Plan("Marked Purge trade",
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget | StepRule::RequireNoCrowdControl, 0, 850),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget | StepRule::RequireMark, 90, 1500));
    p.AllIn = Plan("Disdain flip into Purge execute",
        Step(SDK::SpellSlot::Q, StepRule::RequireTarget, 0, 850),
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireSafePosition, 120, 1000),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget | StepRule::RequireMark, 250, 2200),
        Step(SDK::SpellSlot::R, StepRule::RequireTarget | StepRule::RequireTargetLow | StepRule::HoldForExecute, 500, 3200));
    p.Flee = Plan("Disdain peel and manual harpoon",
        Step(SDK::SpellSlot::E, StepRule::RequireTarget | StepRule::RequireSafePosition, 0, 850),
        Step(SDK::SpellSlot::W, StepRule::RequireTarget, 110, 1100),
        Step(SDK::SpellSlot::R, StepRule::ManualAssistOnly | StepRule::RequireTargetLow, 180, 1400));
    return p;
}();

} // namespace Plugins::KuroAIO::AI::Profiles
